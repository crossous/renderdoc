/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "replay/replay_driver.h"
#include "metal_common.h"

class WrappedMTLDevice;

class MetalReplay : public IReplayDriver
{
public:
  MetalReplay(WrappedMTLDevice *wrappedMTLDevice);
  virtual ~MetalReplay();

  void Shutdown();
  bool IsRemoteProxy() { return m_Proxy; }
  void SetProxy(bool proxy) { m_Proxy = proxy; }
  RDResult FatalErrorCheck() { return m_FatalError; }
  IReplayDriver *MakeDummyDriver();

  APIProperties GetAPIProperties();
  DriverInformation GetDriverInfo() { return m_DriverInfo; }
  rdcarray<GPUDevice> GetAvailableGPUs();

  ResourceDescription &GetResourceDesc(ResourceId id);
  rdcarray<ResourceDescription> GetResources() { return m_Resources; }
  rdcarray<DescriptorStoreDescription> GetDescriptorStores() { return {}; }

  void AddBuffer(ResourceId id, uint64_t length);
  void AddTexture(ResourceId id, MTL::Texture *texture, bool swapBuffer);
  void AddShaderLibrary(ResourceId id, const rdcstr &source);
  void AddShader(ResourceId id, ResourceId library, MTL::Function *function,
                 const rdcstr &entryPoint);
  void AddRenderPipeline(ResourceId id, const RDMTL::RenderPipelineDescriptor &descriptor,
                         MTL::RenderPipelineReflection *reflection);
  void AddDepthStencilState(ResourceId id, const RDMTL::DepthStencilDescriptor &descriptor);
  void AddSamplerState(ResourceId id, const RDMTL::SamplerDescriptor &descriptor);
  void BeginRenderPass(const RDMTL::RenderPassDescriptor &descriptor);
  void EndRenderPass();
  void BindRenderPipeline(ResourceId id);
  void BindDepthStencilState(ResourceId id);
  void BindVertexBuffer(uint32_t index, ResourceId id, uint64_t offset);
  void BindFragmentTexture(uint32_t index, ResourceId id);
  void BindFragmentSampler(uint32_t index, ResourceId id);
  void BindIndexBuffer(ResourceId id, uint64_t offset, MTL::IndexType indexType);
  void SetViewport(const MTL::Viewport &viewport);
  void SetScissor(const MTL::ScissorRect &scissor);
  void SetFrontFacingWinding(MTL::Winding winding);
  void SetCullMode(MTL::CullMode cullMode);
  void SetPrimitiveTopology(MTL::PrimitiveType primitiveType);
  rdcarray<BufferDescription> GetBuffers() { return m_Buffers; }
  BufferDescription GetBuffer(ResourceId id);
  rdcarray<TextureDescription> GetTextures() { return m_Textures; }
  TextureDescription GetTexture(ResourceId id);

  rdcarray<DebugMessage> GetDebugMessages() { return {}; }
  rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader);
  const ShaderReflection *GetShader(ResourceId pipeline, ResourceId shader,
                                    ShaderEntryPoint entry);
  rdcarray<rdcstr> GetDisassemblyTargets(bool withPipeline) { return {}; }
  rdcstr DisassembleShader(ResourceId pipeline, const ShaderReflection *refl, const rdcstr &target)
  {
    return "; Metal shader disassembly is not implemented.";
  }
  rdcarray<EventUsage> GetUsage(ResourceId id) { return {}; }

  void SetPipelineStates(D3D11Pipe::State *d3d11, D3D12Pipe::State *d3d12, GLPipe::State *gl,
                         VKPipe::State *vk, MetalPipe::State *metal)
  {
    m_MetalPipelineState = metal;
  }
  void SavePipelineState(uint32_t eventId);
  rdcarray<Descriptor> GetDescriptors(ResourceId descriptorStore,
                                      const rdcarray<DescriptorRange> &ranges);
  rdcarray<SamplerDescriptor> GetSamplerDescriptors(ResourceId descriptorStore,
                                                    const rdcarray<DescriptorRange> &ranges);
  rdcarray<DescriptorAccess> GetDescriptorAccess(uint32_t eventId);
  rdcarray<DescriptorLogicalLocation> GetDescriptorLocations(ResourceId descriptorStore,
                                                             const rdcarray<DescriptorRange> &ranges)
  {
    return {};
  }

  FrameRecord &WriteFrameRecord() { return m_FrameRecord; }
  FrameRecord GetFrameRecord() { return m_FrameRecord; }
  void AddEvent(uint32_t chunkIndex, uint64_t fileOffset);
  void AddAction(const ActionDescription &action);
  const APIEvent *GetEvent(uint32_t eventId) const;
  uint64_t GetNextEventOffset(uint32_t eventId, uint64_t frameSize) const;

  RDResult ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers);
  void ReplayLog(uint32_t endEventID, ReplayLogType replayType);
  SDFile *GetStructuredFile();
  rdcarray<uint32_t> GetPassEvents(uint32_t eventId) { return {eventId}; }

  void InitPostVSBuffers(uint32_t eventId) {}
  void InitPostVSBuffers(const rdcarray<uint32_t> &passEvents) {}
  MeshFormat GetPostVSBuffers(uint32_t eventId, uint32_t instID, uint32_t viewID, MeshDataStage stage)
  {
    MeshFormat ret;
    ret.status = "Post-VS data is not supported for Metal captures.";
    return ret;
  }

  void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData);
  void GetTextureData(ResourceId tex, const Subresource &sub, const GetTextureDataParams &params,
                      bytebuf &data);

  void BuildTargetShader(ShaderEncoding sourceEncoding, const bytebuf &source, const rdcstr &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId &id,
                         rdcstr &errors);
  rdcarray<ShaderEncoding> GetTargetShaderEncodings() { return {}; }
  void ReplaceResource(ResourceId from, ResourceId to);
  void RemoveReplacement(ResourceId id);
  void FreeTargetResource(ResourceId id) {}
  void ClearReplayCache() {}
  void ReloadShaderDebugInformation() {}

  rdcarray<GPUCounter> EnumerateCounters() { return {}; }
  CounterDescription DescribeCounter(GPUCounter counterID) { return {}; }
  rdcarray<CounterResult> FetchCounters(const rdcarray<GPUCounter> &counterID) { return {}; }
  void FillCBufferVariables(ResourceId pipeline, ResourceId shader, ShaderStage stage,
                            rdcstr entryPoint, uint32_t cbufSlot, rdcarray<ShaderVariable> &outvars,
                            const bytebuf &data)
  {
  }

  rdcarray<PixelModification> PixelHistory(rdcarray<EventUsage> events, ResourceId target, uint32_t x,
                                           uint32_t y, const Subresource &sub, CompType typeCast)
  {
    return {};
  }
  ShaderDebugTrace *DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid, uint32_t idx,
                                uint32_t view)
  {
    ShaderDebugTrace *ret = new ShaderDebugTrace;
    ret->stage = ShaderStage::Vertex;
    return ret;
  }
  ShaderDebugTrace *DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                               const DebugPixelInputs &inputs)
  {
    ShaderDebugTrace *ret = new ShaderDebugTrace;
    ret->stage = ShaderStage::Pixel;
    return ret;
  }
  ShaderDebugTrace *DebugThread(uint32_t eventId, const rdcfixedarray<uint32_t, 3> &groupid,
                                const rdcfixedarray<uint32_t, 3> &threadid)
  {
    ShaderDebugTrace *ret = new ShaderDebugTrace;
    ret->stage = ShaderStage::Compute;
    return ret;
  }
  ShaderDebugTrace *DebugMeshThread(uint32_t eventId, const rdcfixedarray<uint32_t, 3> &groupid,
                                    const rdcfixedarray<uint32_t, 3> &threadid)
  {
    ShaderDebugTrace *ret = new ShaderDebugTrace;
    ret->stage = ShaderStage::Mesh;
    return ret;
  }
  rdcarray<ShaderDebugState> ContinueDebug(ShaderDebugger *debugger) { return {}; }
  void FreeDebugger(ShaderDebugger *debugger) {}
  ResourceId RenderOverlay(ResourceId texid, FloatVector clearCol, DebugOverlay overlay,
                           uint32_t eventId, const rdcarray<uint32_t> &passEvents)
  {
    return ResourceId();
  }
  bool IsRenderOutput(ResourceId id);
  void FileChanged() {}
  bool NeedRemapForFetch(const ResourceFormat &format) { return false; }

  rdcarray<WindowingSystem> GetSupportedWindowSystems() { return {WindowingSystem::MacOS}; }
  AMDRGPControl *GetRGPControl() { return NULL; }
  uint64_t MakeOutputWindow(WindowingData window, bool depth);
  void DestroyOutputWindow(uint64_t id);
  bool CheckResizeOutputWindow(uint64_t id);
  void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
  void GetOutputWindowData(uint64_t id, bytebuf &retData);
  void ClearOutputWindowColor(uint64_t id, FloatVector col);
  void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil) {}
  void BindOutputWindow(uint64_t id, bool depth);
  bool IsOutputWindowVisible(uint64_t id);
  void FlipOutputWindow(uint64_t id);

  bool GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast, float *minval,
                 float *maxval);
  bool GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast, float minval,
                    float maxval, const rdcfixedarray<bool, 4> &channels,
                    rdcarray<uint32_t> &histogram);
  void PickPixel(ResourceId texture, uint32_t x, uint32_t y, const Subresource &sub,
                 CompType typeCast, float pixel[4]);

  ResourceId CreateProxyTexture(const TextureDescription &templateTex) { return ResourceId(); }
  void SetProxyTextureData(ResourceId texid, const Subresource &sub, byte *data, size_t dataSize) {}
  bool IsTextureSupported(const TextureDescription &tex) { return true; }
  ResourceId CreateProxyBuffer(const BufferDescription &templateBuf) { return ResourceId(); }
  void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize) {}

  void RenderMesh(uint32_t eventId, const rdcarray<MeshFormat> &secondaryDraws,
                  const MeshDisplay &cfg);
  bool RenderTexture(TextureDisplay cfg);
  void SetCustomShaderIncludes(const rdcarray<rdcstr> &directories) {}
  void BuildCustomShader(ShaderEncoding sourceEncoding, const bytebuf &source, const rdcstr &entry,
                         const ShaderCompileFlags &compileFlags, ShaderStage type, ResourceId &id,
                         rdcstr &errors);
  rdcarray<ShaderEncoding> GetCustomShaderEncodings() { return {}; }
  rdcarray<ShaderSourcePrefix> GetCustomShaderSourcePrefixes() { return {}; }
  ResourceId ApplyCustomShader(TextureDisplay &display) { return ResourceId(); }
  void FreeCustomShader(ResourceId id) {}
  void RenderCheckerboard(FloatVector dark, FloatVector light);
  void RenderHighlightBox(float w, float h, float scale) {}
  uint32_t PickVertex(uint32_t eventId, int32_t width, int32_t height, const MeshDisplay &cfg,
                      uint32_t x, uint32_t y)
  {
    return ~0U;
  }

private:
  struct OutputWindow
  {
    CA::MetalLayer *layer = NULL;
    MTL::Texture *texture = NULL;
    int32_t width = 0;
    int32_t height = 0;
  };

  bool InitialiseOutputResources();
  bool ResizeOutputWindow(OutputWindow &output, int32_t width, int32_t height);
  bool ReadTextureSubresource(MTL::Texture *texture, const Subresource &sub, bytebuf &data);
  bool RenderTextureInternal(MTL::Texture *source, MTL::Texture *target, TextureDisplay cfg,
                             MTL::LoadAction loadAction, MTL::ClearColor clearColor);
  void AddShaderBindings(ResourceId shader, NS::Array *arguments);

  WrappedMTLDevice *m_pDriver = NULL;
  bool m_Proxy = false;
  RDResult m_FatalError;
  DriverInformation m_DriverInfo = {};
  FrameRecord m_FrameRecord;
  rdcarray<APIEvent> m_PendingEvents;
  rdcarray<APIEvent> m_Events;
  uint32_t m_NextEventID = 1;
  uint32_t m_NextActionID = 1;

  rdcarray<ResourceDescription> m_Resources;
  std::map<ResourceId, size_t> m_ResourceIdx;
  rdcarray<BufferDescription> m_Buffers;
  rdcarray<TextureDescription> m_Textures;
  std::map<ResourceId, rdcstr> m_LibrarySources;
  std::map<ResourceId, ShaderReflection> m_Shaders;

  struct ShaderBindingUsage
  {
    bool available = false;
    rdcarray<bool> samplers;
    rdcarray<bool> readOnlyResources;
    rdcarray<bool> readWriteResources;
  };
  std::map<ResourceId, ShaderBindingUsage> m_ShaderBindingUsage;

  struct RenderPipelineInfo
  {
    ResourceId vertexFunction;
    ResourceId fragmentFunction;
    RDMTL::VertexDescriptor vertexDescriptor;
  };
  std::map<ResourceId, RenderPipelineInfo> m_RenderPipelines;
  std::map<ResourceId, RDMTL::DepthStencilDescriptor> m_DepthStencilStates;
  std::map<ResourceId, RDMTL::SamplerDescriptor> m_SamplerStates;
  MetalPipe::State m_CurrentPipelineState;
  std::map<uint32_t, MetalPipe::State> m_EventPipelineStates;
  MetalPipe::State *m_MetalPipelineState = NULL;

  MTL::CommandQueue *m_OutputQueue = NULL;
  MTL::RenderPipelineState *m_OutputPipeline = NULL;
  MTL::RenderPipelineState *m_MeshPipeline = NULL;
  std::map<uint64_t, OutputWindow> m_OutputWindows;
  uint64_t m_NextOutputWindowID = 1;
  uint64_t m_ActiveOutputWindowID = 0;
};
