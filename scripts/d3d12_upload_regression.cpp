// Headless capture/replay regression inputs for D3D12 mapped upload resources.
// The mixed-pointer Map cases keep a valid native mapping until the final Unmap.
#include <d3d12.h>
#include <dxgi1_4.h>
#include <emmintrin.h>
#include <initguid.h>
#include <windows.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include "api/app/renderdoc_app.h"

extern "C" {
__declspec(dllexport) extern const UINT D3D12SDKVersion = 616;
__declspec(dllexport) extern const char *D3D12SDKPath = ".\\sdk616\\";
}

using Microsoft::WRL::ComPtr;
static void Check(HRESULT hr)
{
  if(FAILED(hr))
  {
    fprintf(stderr, "HRESULT %08x\n", unsigned(hr));
    ExitProcess(2);
  }
}

static D3D12_RESOURCE_DESC BufferDesc(UINT64 size)
{
  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Width = size;
  desc.Height = 1;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  return desc;
}

int wmain(int argc, wchar_t **argv)
{
  if(argc < 3)
    return 2;
  bool extended = false, gpuUpload = false, compatibilityWrites = false;
  for(int i = 3; i < argc; i++)
  {
    extended |= wcscmp(argv[i], L"--extended") == 0;
    gpuUpload |= wcscmp(argv[i], L"--gpu-upload") == 0;
    compatibilityWrites |= wcscmp(argv[i], L"--compat-upload-writes") == 0;
  }
  RENDERDOC_API_1_7_0 *rdoc = NULL;
  if(wcscmp(argv[1], L"-") != 0)
  {
    HMODULE module = LoadLibraryW(argv[1]);
    if(!module)
    {
      fprintf(stderr, "LoadLibrary failed %lu\n", GetLastError());
      return 2;
    }
    pRENDERDOC_GetAPI getAPI = (pRENDERDOC_GetAPI)GetProcAddress(module, "RENDERDOC_GetAPI");
    if(!getAPI || !getAPI(eRENDERDOC_API_Version_1_7_0, (void **)&rdoc))
      return 2;
    char path[32768] = {};
    WideCharToMultiByte(CP_UTF8, 0, argv[2], -1, path, sizeof(path), NULL, NULL);
    rdoc->SetCaptureFilePathTemplate(path);
  }
  ComPtr<ID3D12Device> device;
  Check(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
  ComPtr<IDXGIFactory4> dxgi;
  ComPtr<IDXGIAdapter1> adapter;
  if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgi))) &&
     SUCCEEDED(dxgi->EnumAdapterByLuid(device->GetAdapterLuid(), IID_PPV_ARGS(&adapter))))
  {
    DXGI_ADAPTER_DESC1 desc = {};
    adapter->GetDesc1(&desc);
    char name[512] = {};
    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), NULL, NULL);
    printf("Adapter: %s (vendor %04x device %04x)\n", name, desc.VendorId, desc.DeviceId);
  }
  D3D12_FEATURE_DATA_D3D12_OPTIONS16 options16 = {};
  HRESULT featureResult =
      device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &options16, sizeof(options16));
  printf("GPU_UPLOAD supported: %u (HRESULT %08x)\n", options16.GPUUploadHeapSupported,
         unsigned(featureResult));
  if(gpuUpload)
  {
    // Probe the driver's creation result independently of the advertised capability. Failure is
    // expected on unsupported environments; this does not falsify the feature query for an app.
    for(D3D12_RESOURCE_STATES state :
        {D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ})
    {
      D3D12_HEAP_PROPERTIES props = {};
      props.Type = D3D12_HEAP_TYPE_GPU_UPLOAD;
      D3D12_RESOURCE_DESC desc = BufferDesc(64 * 1024);
      ComPtr<ID3D12Resource> probe;
      HRESULT creation = device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, state,
                                                         NULL, IID_PPV_ARGS(&probe));
      printf("GPU_UPLOAD CreateCommittedResource state %u: HRESULT %08x\n", unsigned(state),
             unsigned(creation));
    }
  }
  if(gpuUpload && (!options16.GPUUploadHeapSupported || FAILED(featureResult)))
    return 5;
  D3D12_HEAP_TYPE inputType = gpuUpload ? D3D12_HEAP_TYPE_GPU_UPLOAD : D3D12_HEAP_TYPE_UPLOAD;
  auto makeBuffer = [&](UINT64 size, D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES properties = {};
    properties.Type = type;
    properties.CreationNodeMask = properties.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC desc = BufferDesc(size);
    ComPtr<ID3D12Resource> resource;
    Check(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &desc,
                                          type == D3D12_HEAP_TYPE_READBACK
                                              ? D3D12_RESOURCE_STATE_COPY_DEST
                                              : D3D12_RESOURCE_STATE_GENERIC_READ,
                                          NULL, IID_PPV_ARGS(&resource)));
    return resource;
  };
  D3D12_COMMAND_QUEUE_DESC qdesc = {};
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> list;
  ComPtr<ID3D12Fence> fence;
  Check(device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&queue)));
  Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
  Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), NULL,
                                  IID_PPV_ARGS(&list)));
  Check(list->Close());
  Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
  HANDLE event = CreateEventW(NULL, FALSE, FALSE, NULL);
  UINT64 signal = 0;
  auto output = makeBuffer(4096, D3D12_HEAP_TYPE_READBACK);
  output->SetName(L"UploadRegressionOutput");
  std::vector<ComPtr<ID3D12Resource>> keepAlive;
  std::vector<UINT> expected;
  bool passed = true;
  uint32_t captureIndex = 0;
  auto finishCapture = [&]() {
    if(rdoc && !rdoc->EndFrameCapture(NULL, NULL))
      ExitProcess(4);
    void *bytes = NULL;
    Check(output->Map(0, NULL, &bytes));
    printf("Native output:");
    for(size_t i = 0; i < expected.size(); i++)
    {
      UINT value = ((UINT *)bytes)[i];
      printf(" %u", value);
      passed &= value == expected[i];
    }
    printf("\n");
    D3D12_RANGE noWrite = {0, 0};
    output->Unmap(0, &noWrite);
    if(rdoc)
    {
      char path[32768] = {};
      uint32_t length = sizeof(path);
      uint64_t timestamp = 0;
      if(!rdoc->GetCapture(captureIndex++, path, &length, &timestamp))
        ExitProcess(4);
      printf("Capture: %s\n", path);
      std::string expectedPath = std::string(path) + ".expected.bin";
      FILE *file = NULL;
      if(fopen_s(&file, expectedPath.c_str(), "wb") || !file)
        ExitProcess(4);
      fwrite(expected.data(), sizeof(UINT), expected.size(), file);
      fclose(file);
    }
  };
  auto submitValue = [&](ID3D12Resource *source, UINT64 offset, UINT value) {
    Check(allocator->Reset());
    Check(list->Reset(allocator.Get(), NULL));
    list->CopyBufferRegion(output.Get(), expected.size() * sizeof(UINT), source, offset,
                           sizeof(UINT));
    Check(list->Close());
    ID3D12CommandList *commands[] = {list.Get()};
    queue->ExecuteCommandLists(1, commands);
    Check(queue->Signal(fence.Get(), ++signal));
    Check(fence->SetEventOnCompletion(signal, event));
    if(WaitForSingleObject(event, 10000) != WAIT_OBJECT_0)
      ExitProcess(3);
    expected.push_back(value);
  };
  D3D12_RANGE noRead = {0, 0};
  if(rdoc)
    rdoc->StartFrameCapture(NULL, NULL);
  for(int mode = 0; mode < 3; mode++)
  {
    auto input = makeBuffer(4096, inputType);
    input->SetName(mode == 0   ? L"PointerThenNull"
                   : mode == 1 ? L"NullThenPointer"
                               : L"TwoPointers");
    UINT *pointer = NULL;
    if(mode == 1)
      Check(input->Map(0, &noRead, NULL));
    Check(input->Map(0, &noRead, (void **)&pointer));
    if(mode == 0)
      Check(input->Map(0, &noRead, NULL));
    if(mode == 2)
    {
      void *other = NULL;
      Check(input->Map(0, &noRead, &other));
    }
    *pointer = 111;
    input->Unmap(0, NULL);    // One valid mapping remains, including in mixed-pointer cases.
    *pointer = 222 + mode;
    submitValue(input.Get(), 0, *pointer);
    *pointer = 333 + mode;
    submitValue(input.Get(), 0, *pointer);
    input->Unmap(0, NULL);
    keepAlive.push_back(input);
  }
  // Two distant changed pages, a final partial page, and repeated submissions.
  auto large = makeBuffer(2 * 1024 * 1024 + 17, inputType);
  large->SetName(L"SparseMappedWrites");
  unsigned char *mapped = NULL;
  Check(large->Map(0, &noRead, (void **)&mapped));
  memset(mapped, 0, 2 * 1024 * 1024 + 17);
  UINT *first = (UINT *)(mapped + 64 * 1024 + 28);
  UINT *last = (UINT *)(mapped + 2 * 1024 * 1024 + 12);
  *first = 444;
  *last = 555;
  submitValue(large.Get(), 64 * 1024 + 28, 444);
  submitValue(large.Get(), 2 * 1024 * 1024 + 12, 555);
  *first = 666;
  *last = 777;
  submitValue(large.Get(), 64 * 1024 + 28, 666);
  submitValue(large.Get(), 2 * 1024 * 1024 + 12, 777);
  if(extended)
  {
    // Correctly ordered non-temporal writes must be captured before the native Unmap.
    // Exercise a worker writer too: join completes its Map/write/fence/Unmap before submission.
    for(UINT64 bytes : {UINT64(4096), UINT64(2 * 1024 * 1024 + 17)})
    {
      auto streamed = makeBuffer(bytes, inputType);
      streamed->SetName(L"FencedStreamingStores");
      for(UINT mode = 0; mode < 2; mode++)
      {
        UINT value = 920 + mode + (bytes > 4096 ? 2 : 0);
        auto write = [&]() {
          void *destination = NULL;
          Check(streamed->Map(0, &noRead, &destination));
          memset(destination, 0, size_t(bytes));
          _mm_stream_si128((__m128i *)destination, _mm_set1_epi32(value));
          _mm_sfence();
          D3D12_RANGE written = {0, size_t(bytes)};
          streamed->Unmap(0, &written);
        };
        if(mode == 0)
          write();
        else
        {
          std::thread worker(write);
          worker.join();
        }
        submitValue(streamed.Get(), 0, value);
      }
      keepAlive.push_back(streamed);
    }
    // A failed Map must not alter another subresource's live mapping count.
    void *invalidPointer = NULL;
    if(SUCCEEDED(large->Map(1, &noRead, &invalidPointer)))
      return 6;

    // Empty written ranges must not suppress real writes. The capture code cannot rely on them.
    auto remapped = makeBuffer(4096, inputType);
    remapped->SetName(L"EmptyWrittenRange");
    UINT *pointer = NULL;
    Check(remapped->Map(0, &noRead, (void **)&pointer));
    *pointer = 901;
    remapped->Unmap(0, &noRead);
    submitValue(remapped.Get(), 0, 901);
    Check(remapped->Map(0, &noRead, (void **)&pointer));
    *pointer = 902;
    remapped->Unmap(0, &noRead);
    submitValue(remapped.Get(), 0, 902);
    keepAlive.push_back(remapped);

    // NULL-only maps must remain safe both on the last Unmap and on resource destruction.
    auto nullOnly = makeBuffer(4096, inputType);
    Check(nullOnly->Map(0, &noRead, NULL));
    Check(nullOnly->Map(0, &noRead, NULL));
    nullOnly->Unmap(0, &noRead);
    nullOnly->Unmap(0, &noRead);
    Check(nullOnly->Map(0, &noRead, NULL));
    nullOnly.Reset();

    // The anchor mapping remains valid while other threads make nested NULL-pointer maps.
    auto threaded = makeBuffer(4096, inputType);
    threaded->SetName(L"ThreadedNestedMaps");
    Check(threaded->Map(0, &noRead, (void **)&pointer));
    std::vector<std::thread> workers;
    for(int thread = 0; thread < 4; thread++)
      workers.emplace_back([&]() {
        for(int i = 0; i < 100; i++)
        {
          Check(threaded->Map(0, &noRead, NULL));
          threaded->Unmap(0, &noRead);
        }
      });
    for(auto &worker : workers)
      worker.join();
    *pointer = 903;
    submitValue(threaded.Get(), 0, 903);
    threaded->Unmap(0, NULL);
    keepAlive.push_back(threaded);

    // Map one placed resource but submit a different overlapping resource from the same heap.
    D3D12_HEAP_DESC heapDesc = {};
    heapDesc.SizeInBytes = 3 * 1024 * 1024;
    heapDesc.Properties.Type = inputType;
    heapDesc.Properties.CreationNodeMask = heapDesc.Properties.VisibleNodeMask = 1;
    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
    ComPtr<ID3D12Heap> heap;
    Check(device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap)));
    D3D12_RESOURCE_DESC placedDesc = BufferDesc(2 * 1024 * 1024 + 17);
    ComPtr<ID3D12Resource> writer, reader;
    Check(device->CreatePlacedResource(
        heap.Get(), 0, &placedDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&writer)));
    Check(device->CreatePlacedResource(
        heap.Get(), 0, &placedDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&reader)));
    writer->SetName(L"PlacedAliasWriter");
    reader->SetName(L"PlacedAliasReader");
    Check(writer->Map(0, &noRead, (void **)&pointer));
    memset(pointer, 0, size_t(placedDesc.Width));
    pointer[0] = 904;
    submitValue(reader.Get(), 0, 904);
    pointer[0] = 905;
    submitValue(reader.Get(), 0, 905);
    writer->Unmap(0, NULL);
    keepAlive.push_back(writer);
    keepAlive.push_back(reader);

    if(compatibilityWrites)
    {
      // Deliberately model writes outside the tracked Map lifetime, as observed in the game.
      // This is a driver-specific compatibility fixture, not a valid-API regression claim.
      // The process is disposable; compare native output before testing the capture strategy.
      printf("Testing compatibility-only writes after Unmap (not a valid-API fixture).\n");
      for(UINT mode = 0; mode < 2; mode++)
      {
        ComPtr<ID3D12Resource> upload, alias;
        if(mode == 0)
          upload = makeBuffer(2 * 1024 * 1024 + 17, inputType);
        else
        {
          Check(device->CreatePlacedResource(heap.Get(), 0, &placedDesc,
                                             D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                             IID_PPV_ARGS(&upload)));
          Check(device->CreatePlacedResource(heap.Get(), 0, &placedDesc,
                                             D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                             IID_PPV_ARGS(&alias)));
        }
        upload->SetName(mode == 0 ? L"CompatCommittedWriter" : L"CompatPlacedWriter");
        if(alias)
          alias->SetName(L"CompatPlacedReader");
        unsigned char *cached = NULL;
        Check(upload->Map(0, &noRead, (void **)&cached));
        memset(cached, 0, size_t(placedDesc.Width));
        UINT *left = (UINT *)(cached + 64 * 1024 + 28);
        UINT *right = (UINT *)(cached + 2 * 1024 * 1024 + 12);
        *left = 930;
        *right = 931;
        upload->Unmap(0, NULL);
        ID3D12Resource *source = alias ? alias.Get() : upload.Get();
        submitValue(source, 64 * 1024 + 28, 930);
        *left = 932;
        *right = 933;
        _mm_sfence();
        submitValue(source, 64 * 1024 + 28, 932);
        submitValue(source, 2 * 1024 * 1024 + 12, 933);
        *left = 934;
        *right = 935;
        _mm_sfence();
        submitValue(source, 64 * 1024 + 28, 934);
        submitValue(source, 2 * 1024 * 1024 + 12, 935);
        keepAlive.push_back(upload);
        if(alias)
          keepAlive.push_back(alias);
      }
    }

    // Two independent queues read successive CPU updates with explicit fence ordering.
    D3D12_COMMAND_QUEUE_DESC otherDesc = {};
    otherDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    ComPtr<ID3D12CommandQueue> otherQueue;
    ComPtr<ID3D12CommandAllocator> otherAllocator;
    ComPtr<ID3D12GraphicsCommandList> otherList;
    Check(device->CreateCommandQueue(&otherDesc, IID_PPV_ARGS(&otherQueue)));
    Check(device->CreateCommandAllocator(otherDesc.Type, IID_PPV_ARGS(&otherAllocator)));
    Check(device->CreateCommandList(0, otherDesc.Type, otherAllocator.Get(), NULL,
                                    IID_PPV_ARGS(&otherList)));
    Check(otherList->Close());
    *first = 906;
    Check(otherList->Reset(otherAllocator.Get(), NULL));
    otherList->CopyBufferRegion(output.Get(), expected.size() * sizeof(UINT), large.Get(),
                                64 * 1024 + 28, sizeof(UINT));
    Check(otherList->Close());
    Check(otherQueue->Wait(fence.Get(), signal));
    ID3D12CommandList *commands[] = {otherList.Get()};
    otherQueue->ExecuteCommandLists(1, commands);
    Check(otherQueue->Signal(fence.Get(), ++signal));
    Check(queue->Wait(fence.Get(), signal));
    Check(fence->SetEventOnCompletion(signal, event));
    if(WaitForSingleObject(event, 10000) != WAIT_OBJECT_0)
      ExitProcess(3);
    expected.push_back(906);
    *first = 907;
    submitValue(large.Get(), 64 * 1024 + 28, 907);

    // Discard content is undefined in the application. Replay is checked separately against
    // RenderDoc's diagnostic pattern, including every byte across chunk boundaries and tails.
    for(UINT64 size :
        {UINT64(1024 * 1024 - 1), UINT64(1024 * 1024), UINT64(1024 * 1024 + 17),
         UINT64(1024 * 1024 + 1000), UINT64(8 * 1024 * 1024 + 17), UINT64(8 * 1024 * 1024 + 1000)})
    {
      D3D12_RESOURCE_DESC desc = BufferDesc(size);
      desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
      D3D12_HEAP_PROPERTIES props = {};
      props.Type = D3D12_HEAP_TYPE_DEFAULT;
      ComPtr<ID3D12Resource> discard;
      Check(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
                                            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL,
                                            IID_PPV_ARGS(&discard)));
      discard->SetName((L"DiscardRegression_" + std::to_wstring(size)).c_str());
      Check(allocator->Reset());
      Check(list->Reset(allocator.Get(), NULL));
      list->DiscardResource(discard.Get(), NULL);
      Check(list->Close());
      ID3D12CommandList *commands[] = {list.Get()};
      queue->ExecuteCommandLists(1, commands);
      Check(queue->Signal(fence.Get(), ++signal));
      Check(fence->SetEventOnCompletion(signal, event));
      if(WaitForSingleObject(event, 10000) != WAIT_OBJECT_0)
        ExitProcess(3);
      keepAlive.push_back(discard);
    }
  }
  finishCapture();
  if(extended)
  {
    // Start another capture with the same persistent mapping. Shadows from the first capture
    // must not suppress the initial bytes of the second, and event seeking must restore them.
    expected.clear();
    if(rdoc)
      rdoc->StartFrameCapture(NULL, NULL);
    *first = 908;
    *last = 909;
    submitValue(large.Get(), 64 * 1024 + 28, 908);
    submitValue(large.Get(), 2 * 1024 * 1024 + 12, 909);
    *first = 910;
    *last = 911;
    submitValue(large.Get(), 64 * 1024 + 28, 910);
    submitValue(large.Get(), 2 * 1024 * 1024 + 12, 911);
    finishCapture();
  }
  large->Unmap(0, NULL);
  CloseHandle(event);
  return passed ? 0 : 1;
}
