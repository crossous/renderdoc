# D3D12 VRAM Optimization for RenderDoc Capture

## Problem

When capturing frames in UE5 with Lumen/Nanite/VSM/RVT enabled, the editor alone consumes >10GB VRAM. RenderDoc's frame capture needs to copy all dirty resources' initial states into readback buffers, adding 5GB+ more. This causes OOM crashes — often on the very first capture attempt.

## Root Cause

During capture, RenderDoc calls `Prepare_InitialState` for every dirty resource. For D3D12:

1. Each `Resource_Resource` (buffer/texture) or `Resource_Heap` is copied via `CopyResource`/`CopyTextureRegion` into a readback buffer allocated from 128MB readback heaps (`m_InitialStateHeaps`).
2. All resources are prepared before any are serialized, so the peak memory = sum of all readback buffers simultaneously in memory.

The Vulkan backend already had two optimizations for this. D3D12 had neither.

## Optimizations Implemented

### Optimization 1: Postpone/Skip (Automatic, No User Config Needed)

**What it does:** Resources that haven't been written to for >3 seconds are deferred (postponed) or skipped entirely during capture, avoiding readback buffer allocation for them.

**Mechanism:** The base `ResourceManager` already has a `Postpone/Skip` system driven by `ResourceRefTimes`:
- Resources with no write in the last 3000ms (`PERSISTENT_RESOURCE_AGE`) are **postponed** — their initial state is prepared lazily during serialization instead of upfront.
- Resources only referenced via `CompleteWriteAndDiscard` for 3000ms+ (`SKIP_RESOURCE_AGE`) are **skipped** entirely — no readback copy at all.

The system requires each backend to override `IsResourceTrackedForPersistency()` to opt in. D3D12 never did this (the base class returns `false`).

**Files changed:**

`renderdoc/driver/d3d12/d3d12_manager.h` — Declaration:
```cpp
bool IsResourceTrackedForPersistency(ID3D12DeviceChild *const &res) override;
```

`renderdoc/driver/d3d12/d3d12_manager.cpp` — Implementation:
```cpp
bool D3D12ResourceManager::IsResourceTrackedForPersistency(ID3D12DeviceChild *const &res)
{
  D3D12ResourceType type = IdentifyTypeByPtr((ID3D12Object *)res);
  return type == Resource_Resource || type == Resource_Heap;
}
```

Only `Resource_Resource` (buffers/textures) and `Resource_Heap` are tracked — these are the VRAM-heavy types. DescriptorHeap, PipelineState, QueryHeap, etc. are excluded (small, fast to copy).

**Bug fix — `End_PrepareInitialBatch` (必须配套实现):**

仅实现 `IsResourceTrackedForPersistency` 会导致 UE 中 EndFrameCapture 崩溃（`E_INVALIDARG`）。原因：

1. postponed 资源在活跃捕获期间被引用（dirty ref）时，`Prepare_InitialStateIfPostponed(midframe=true)` 会调用 `Begin_PrepareInitialBatch()` → `Prepare_InitialState()` → `End_PrepareInitialBatch()`
2. `Prepare_InitialState` 通过 `GetInitialStateList()` → `GetNewList()` 在 `m_Alloc` 上创建一个 recording 状态的 command list
3. 如果 `End_PrepareInitialBatch` 是空实现（基类默认 no-op），这个 command list 不会被 close/execute/flush
4. `EndFrameCapture` 截取 backbuffer 截图时调用 `GetNewList()`，`m_Alloc` 上已有 recording command list，D3D12 不允许同一 allocator 同时有两个 recording command list → `E_INVALIDARG` 崩溃

Vulkan 后端不会崩溃是因为它实现了 `End_PrepareInitialBatch`（`CloseInitStateCmd` + `SubmitCmds` + `FlushQ`）。D3D12 必须做同样的事。

`renderdoc/driver/d3d12/d3d12_manager.h` — 声明：
```cpp
void Begin_PrepareInitialBatch() override;
void End_PrepareInitialBatch() override;
```

`renderdoc/driver/d3d12/d3d12_manager.cpp` — 实现：
```cpp
void D3D12ResourceManager::Begin_PrepareInitialBatch()
{
}

void D3D12ResourceManager::End_PrepareInitialBatch()
{
  if(m_Device->initStateCurList)
  {
    m_Device->CloseInitialStateList();
    m_Device->ExecuteLists(NULL, true);
    m_Device->FlushLists();
  }
}
```

`End_PrepareInitialBatch` 会在以下场景被调用：
- `PrepareInitialContents()`（`StartFrameCapture` 阶段）批量准备结束后
- `InsertInitialContentsChunks()`（`EndFrameCapture` 序列化阶段）处理 postponed 资源后
- `Prepare_InitialStateIfPostponed(midframe=true)`（活跃捕获期间单个 postponed 资源被引用时）

所有场景下都能正确关闭 command list，避免 allocator 冲突。

**Verification:** In RenderDoc's log output during capture, look for:
```
Prepared X dirty resources, postponed Y, skipped Z
```
With this change, Y and Z should be non-zero for scenes with stable resources.

---

### Optimization 2: softMemoryLimit Flush-to-Disk (User-Configured)

**What it does:** When the cumulative size of prepared readback buffers exceeds a user-specified threshold (in MB), RenderDoc flushes all pending initial states to a temporary file on disk, releases the GPU readback heaps, and continues. This caps peak VRAM usage during capture.

**User configuration:**

| Method | How |
|---|---|
| RenderDoc UI | Capture Settings → "Soft Memory Limit (MB)" |
| RenderDoc API | `RENDERDOC_SetCaptureOptionU32(eRENDERDOC_Option_SoftMemoryLimit, 500)` |
| UE5 Plugin | Project Settings → Plugins → RenderDoc → "Soft memory limit (MB)" |
| UE5 Console | `renderdoc.SoftMemoryLimit 500` |

Default = 0 (disabled). Recommended: 200–500 MB for large scenes.

**Files changed:**

#### `renderdoc/driver/d3d12/d3d12_device.h`

Added members after `m_LastInitialStateHeapOffset`:
```cpp
// softMemoryLimit support: flush initial states to disk in batches
rdcarray<ResourceId> m_PreparedNotSerialisedInitStates;
rdcarray<rdcstr> m_InitTempFiles;
uint64_t m_TotalInitialStateBytes = 0;
void FlushInitialStatesToDisk();
void FreeInitialStateHeaps();
```

#### `renderdoc/driver/d3d12/d3d12_device.cpp`

`FreeInitialStateHeaps()` — Releases all 128MB readback heaps:
```cpp
void WrappedID3D12Device::FreeInitialStateHeaps()
{
  for(ID3D12Heap *h : m_InitialStateHeaps)
    h->Release();
  m_InitialStateHeaps.clear();
  m_LastInitialStateHeapOffset = 0;
}
```

`FlushInitialStatesToDisk()` — Core flush logic:
1. 如果 `initStateCurList` 非空则 `CloseInitialStateList()` + `ExecuteLists()` + `FlushLists()` 确保 GPU copy 完成（注意必须先检查 NULL，因为 `Prepare_InitialState` 内部某些路径如 non-resident 资源会提前关闭 list）
2. Creates a temp file: `<capture_dir>/rdoc_<tick>_<tid>.bin`
3. Iterates `m_PreparedNotSerialisedInitStates`, serializing each initial state to the temp file
4. Records each resource's file offset via `rm->SetInitialFileStore(flushId, tempFile, start, end)`
5. Clears in-memory initial contents (releases readback buffer references)
6. Calls `FreeInitialStateHeaps()` to release GPU memory
7. Resets `m_TotalInitialStateBytes = 0`

Cleanup — Added to both capture completion paths (~line 3426 and ~3489):
```cpp
// clean up temp files from softMemoryLimit flush
for(const rdcstr &f : m_InitTempFiles)
  FileIO::Delete(f);
m_InitTempFiles.clear();
m_TotalInitialStateBytes = 0;
```

#### `renderdoc/driver/d3d12/d3d12_initstate.cpp`

**Pre-allocation check** — Added at the start of `Prepare_InitialState`'s `Resource_Resource`/`Resource_Heap` branch, before any readback buffer is allocated:
```cpp
// Estimate resource size
uint64_t estimatedSize = 0;
if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
  estimatedSize = desc.Width;
else
{
  estimatedSize = GetByteSize((int)desc.Width, (int)desc.Height,
                              (int)desc.DepthOrArraySize, desc.Format, 0);
  if(desc.MipLevels > 1) estimatedSize *= 2;
  if(desc.SampleDesc.Count > 1) estimatedSize *= desc.SampleDesc.Count;
}

// Flush if adding this resource would exceed the limit
uint32_t softMemoryLimit = RenderDoc::Inst().GetCaptureOptions().softMemoryLimit;
if(softMemoryLimit > 0 && !m_Device->m_PreparedNotSerialisedInitStates.empty() &&
   m_Device->m_TotalInitialStateBytes + estimatedSize >
       (uint64_t)softMemoryLimit * 1024 * 1024ULL)
{
  m_Device->FlushInitialStatesToDisk();
}
```

**Post-allocation tracking** — Added after `SetInitialContents()`:
```cpp
uint32_t softMemoryLimit = RenderDoc::Inst().GetCaptureOptions().softMemoryLimit;
if(softMemoryLimit > 0)
{
  uint64_t resourceBytes = 0;
  if(initContents.resource)
    resourceBytes = ((ID3D12Resource *)initContents.resource)->GetDesc().Width;
  else if(initContents.tag == D3D12InitialContents::MapDirect)
    resourceBytes = initContents.dataSize;

  m_Device->m_TotalInitialStateBytes += resourceBytes;
  m_Device->m_PreparedNotSerialisedInitStates.push_back(GetResID(res));
}
```

**Verification:** Set softMemoryLimit to 500, capture a heavy UE5 scene. The log should show:
```
Flushing batch of N initial states to disk (XXXX bytes allocated)
```
And peak VRAM usage should be noticeably lower.

---

### Optimization 3: OOM Error Hint

**What it does:** When an OOM error occurs during initial state preparation and `softMemoryLimit == 0` (disabled), the error message now includes a hint telling the user to enable it.

**Files changed:** `renderdoc/driver/d3d12/d3d12_initstate.cpp` — All 7 `SET_ERROR_RESULT(error, ResultCode::OutOfMemory, ...)` calls now append:
```
Consider setting 'Soft Memory Limit' in capture settings to reduce GPU memory usage during capture.
```
Only shown when `softMemoryLimit == 0`.

---

### UE5 Plugin Integration

To allow setting `softMemoryLimit` from the UE5 editor (without the RenderDoc UI), these files were modified:

#### `Engine/Source/ThirdParty/RenderDoc/renderdoc_app.h`

Added the missing enum value (UE's bundled header is from an older RenderDoc version):
```cpp
eRENDERDOC_Option_SoftMemoryLimit = 13,
```

#### `Engine/Plugins/Developer/RenderDocPlugin/Source/RenderDocPlugin/Public/RenderDocPluginSettings.h`

Added UPROPERTY under "Frame Capture Settings":
```cpp
UPROPERTY(config, EditAnywhere, Category = "Frame Capture Settings", meta = (
    ConsoleVariable = "renderdoc.SoftMemoryLimit", DisplayName = "Soft memory limit (MB)",
    ToolTip = "If > 0, RenderDoc will flush initial states to disk in batches during capture to keep GPU memory usage under this limit (in MB). Recommended: 200-500 for large scenes. 0 = disabled.",
    ClampMin = 0, ConfigRestartRequired = false))
int32 SoftMemoryLimit;
```

#### `Engine/Plugins/Developer/RenderDocPlugin/Source/RenderDocPlugin/Private/RenderDocPluginModule.cpp`

Added CVar:
```cpp
static TAutoConsoleVariable<int32> CVarRenderDocSoftMemoryLimit(
    TEXT("renderdoc.SoftMemoryLimit"), 0,
    TEXT("If > 0, RenderDoc will flush initial states to disk in batches during capture..."));
```

Added `SetCaptureOptionU32` call at both initialization and `BeginFrameCapture`:
```cpp
RenderDocAPI->SetCaptureOptionU32(eRENDERDOC_Option_SoftMemoryLimit,
    FMath::Max(0, CVarRenderDocSoftMemoryLimit.GetValueOnAnyThread()));
```

---

## Data Flow Summary

```
UE5 Editor UI  ──(UPROPERTY config)──> CVar renderdoc.SoftMemoryLimit
                                           │
                                           ▼
RenderDocPluginModule  ──(SetCaptureOptionU32)──> renderdoc.dll CaptureOptions.softMemoryLimit
                                                       │
              ┌────────────────────────────────────────┘
              ▼
Prepare_InitialState (per resource)
  1. Estimate resource size
  2. If totalBytes + estimatedSize > softMemoryLimit → FlushInitialStatesToDisk()
  3. Allocate readback buffer, issue GPU CopyResource
  4. Track: totalBytes += resourceBytes, record ResourceId
              │
              ▼
FlushInitialStatesToDisk()
  1. Execute pending GPU commands (CloseInitialStateList + ExecuteLists + FlushLists)
  2. Serialize all tracked resources to temp file on disk
  3. Record file offsets via SetInitialFileStore()
  4. Clear in-memory initial contents
  5. Release readback heaps (FreeInitialStateHeaps)
  6. Reset totalBytes = 0
              │
              ▼
Serialization phase (InsertInitialContentsChunks)
  - Resources flushed to disk: read back from temp file via GetInitialFileStore()
  - Resources still in memory: serialize normally
              │
              ▼
Capture complete
  - Delete temp files
  - Final .rdc file contains all initial states regardless of flush path
```

## Replay-Side Note

Replay-side VRAM optimization is a separate concern handled by the existing `InitPolicy` / `ReplayOptimisationLevel` mechanism. Setting Replay Optimisation Level to "Fastest" (`eInitPolicy_Fastest`) only restores resources that are read-before-written, significantly reducing replay VRAM. This was NOT modified in this change.

## Reference Implementation

The D3D12 implementation mirrors the Vulkan backend's existing approach:
- Vulkan: `vk_initstate.cpp:102-157` (softMemoryLimit check), `vk_core.h` (member variables)
- Vulkan: `vk_manager.h` → `IsPostponableRes()` (equivalent of `IsResourceTrackedForPersistency`)
- GL: `gl_manager.cpp` → `IsResourceTrackedForPersistency` (texture/buffer only)
