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

#include "metal_replay.h"
#include "maths/camera.h"
#include "maths/matrix.h"
#include "replay/dummy_driver.h"
#include "serialise/rdcfile.h"
#include "metal_buffer.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_texture.h"

MetalReplay::MetalReplay(WrappedMTLDevice *wrappedMTLDevice)
{
  m_pDriver = wrappedMTLDevice;
  m_DriverInfo.vendor = GPUVendor::Unknown;
  memset(m_DriverInfo.version, 0, sizeof(m_DriverInfo.version));
  snprintf(m_DriverInfo.version, sizeof(m_DriverInfo.version), "Apple Metal");
}

MetalReplay::~MetalReplay()
{
  for(auto &it : m_OutputWindows)
  {
    if(it.second.texture)
      it.second.texture->release();
    if(it.second.layer)
      it.second.layer->release();
  }
  m_OutputWindows.clear();

  if(m_OutputPipeline)
    m_OutputPipeline->release();
  if(m_MeshPipeline)
    m_MeshPipeline->release();
  if(m_OutputQueue)
    m_OutputQueue->release();
}

void MetalReplay::Shutdown()
{
  delete m_pDriver;
}

IReplayDriver *MetalReplay::MakeDummyDriver()
{
  return NULL;
}

APIProperties MetalReplay::GetAPIProperties()
{
  APIProperties ret;
  ret.pipelineType = GraphicsAPI::Metal;
  ret.localRenderer = GraphicsAPI::Metal;
  ret.vendor = GPUVendor::Unknown;
  ret.remoteReplay = m_Proxy;
  ret.degraded = false;
  ret.shaderDebugging = false;
  ret.pixelHistory = false;
  return ret;
}

rdcarray<GPUDevice> MetalReplay::GetAvailableGPUs()
{
  if(!m_pDriver || !Unwrap(m_pDriver))
    return {};

  GPUDevice gpu;
  gpu.vendor = GPUVendor::Unknown;
  gpu.name = Unwrap(m_pDriver)->name()->utf8String();
  gpu.apis = {GraphicsAPI::Metal};
  return {gpu};
}

ResourceDescription &MetalReplay::GetResourceDesc(ResourceId id)
{
  auto it = m_ResourceIdx.find(id);
  if(it == m_ResourceIdx.end())
  {
    m_ResourceIdx[id] = m_Resources.size();
    m_Resources.push_back(ResourceDescription());
    m_Resources.back().resourceId = id;
    return m_Resources.back();
  }

  return m_Resources[it->second];
}

void MetalReplay::AddBuffer(ResourceId id, uint64_t length)
{
  for(BufferDescription &buf : m_Buffers)
  {
    if(buf.resourceId == id)
    {
      buf.length = length;
      return;
    }
  }

  BufferDescription desc;
  desc.resourceId = id;
  desc.length = length;
  m_Buffers.push_back(desc);
}

static TextureType MakeShaderTextureType(MTL::TextureType type);

void MetalReplay::AddTexture(ResourceId id, MTL::Texture *texture, bool swapBuffer)
{
  TextureDescription desc;
  desc.resourceId = id;
  desc.dimension = texture->textureType() == MTL::TextureType3D ? 3 : 2;
  desc.type = MakeShaderTextureType(texture->textureType());
  desc.width = (uint32_t)texture->width();
  desc.height = (uint32_t)texture->height();
  desc.depth = (uint32_t)texture->depth();
  desc.mips = (uint32_t)texture->mipmapLevelCount();
  desc.cubemap = texture->textureType() == MTL::TextureTypeCube ||
                 texture->textureType() == MTL::TextureTypeCubeArray;
  desc.arraysize = (uint32_t)texture->arrayLength() * (desc.cubemap ? 6U : 1U);
  desc.msSamp = (uint32_t)texture->sampleCount();
  desc.msQual = 0;
  desc.format = MakeResourceFormat(texture->pixelFormat());
  desc.byteSize = 0;
  for(uint32_t mip = 0; mip < desc.mips; mip++)
    desc.byteSize += GetByteSize(desc.width, desc.height, desc.depth, texture->pixelFormat(), mip) *
                     RDCMAX(1U, desc.msSamp) * RDCMAX(1U, desc.arraysize);
  desc.creationFlags = TextureCategory::NoFlags;
  if(texture->usage() & MTL::TextureUsageShaderRead)
    desc.creationFlags |= TextureCategory::ShaderRead;
  if(texture->usage() & MTL::TextureUsageShaderWrite)
    desc.creationFlags |= TextureCategory::ShaderReadWrite;
  if(texture->usage() & MTL::TextureUsageRenderTarget)
    desc.creationFlags |= TextureCategory::ColorTarget;
  if(swapBuffer)
    desc.creationFlags |= TextureCategory::SwapBuffer;

  for(TextureDescription &tex : m_Textures)
  {
    if(tex.resourceId == id)
    {
      tex = desc;
      return;
    }
  }

  m_Textures.push_back(desc);
}

BufferDescription MetalReplay::GetBuffer(ResourceId id)
{
  for(const BufferDescription &buf : m_Buffers)
    if(buf.resourceId == id)
      return buf;
  return {};
}

TextureDescription MetalReplay::GetTexture(ResourceId id)
{
  for(const TextureDescription &tex : m_Textures)
    if(tex.resourceId == id)
      return tex;
  return {};
}

void MetalReplay::AddShaderLibrary(ResourceId id, const rdcstr &source)
{
  m_LibrarySources[id] = source;
}

static ShaderStage MakeShaderStage(MTL::FunctionType type)
{
  switch(type)
  {
    case MTL::FunctionTypeVertex: return ShaderStage::Vertex;
    case MTL::FunctionTypeFragment: return ShaderStage::Fragment;
    case MTL::FunctionTypeKernel: return ShaderStage::Compute;
    case MTL::FunctionTypeVisible: return ShaderStage::Callable;
    case MTL::FunctionTypeIntersection: return ShaderStage::Intersection;
    case MTL::FunctionTypeMesh: return ShaderStage::Mesh;
    case MTL::FunctionTypeObject: return ShaderStage::Task;
  }

  return ShaderStage::Invalid;
}

static uint32_t FindEntryLine(const rdcstr &source, const rdcstr &entryPoint)
{
  int32_t offset = source.find(entryPoint);
  if(offset < 0)
    return 0;

  uint32_t line = 1;
  for(int32_t i = 0; i < offset; i++)
    if(source[(size_t)i] == '\n')
      line++;
  return line;
}

void MetalReplay::AddShader(ResourceId id, ResourceId library, MTL::Function *function,
                            const rdcstr &entryPoint)
{
  if(id == ResourceId() || function == NULL || entryPoint.empty())
    return;

  ShaderReflection &reflection = m_Shaders[id];
  reflection = ShaderReflection();
  m_ShaderBindingUsage[id] = ShaderBindingUsage();
  reflection.resourceId = id;
  reflection.entryPoint = entryPoint;
  reflection.stage = MakeShaderStage(function->functionType());
  reflection.debugInfo.entrySourceName = entryPoint;
  reflection.debugInfo.debuggable = false;
  reflection.debugInfo.debugStatus = "Metal shader debugging is not implemented.";

  auto source = m_LibrarySources.find(library);
  if(source != m_LibrarySources.end() && !source->second.empty())
  {
    reflection.encoding = ShaderEncoding::MSL;
    reflection.rawBytes.assign((const byte *)source->second.data(), source->second.size());
    reflection.debugInfo.encoding = ShaderEncoding::MSL;
    reflection.debugInfo.files.push_back({"captured.metal", source->second});
    reflection.debugInfo.editBaseFile = 0;
    reflection.debugInfo.entryLocation.fileIndex = 0;
    reflection.debugInfo.entryLocation.lineStart = FindEntryLine(source->second, entryPoint);
    reflection.debugInfo.entryLocation.lineEnd = reflection.debugInfo.entryLocation.lineStart;
  }
}

static TextureType MakeShaderTextureType(MTL::TextureType type)
{
  switch(type)
  {
    case MTL::TextureType1D: return TextureType::Texture1D;
    case MTL::TextureType1DArray: return TextureType::Texture1DArray;
    case MTL::TextureType2D: return TextureType::Texture2D;
    case MTL::TextureType2DArray: return TextureType::Texture2DArray;
    case MTL::TextureType2DMultisample: return TextureType::Texture2DMS;
    case MTL::TextureTypeCube: return TextureType::TextureCube;
    case MTL::TextureTypeCubeArray: return TextureType::TextureCubeArray;
    case MTL::TextureType3D: return TextureType::Texture3D;
    case MTL::TextureType2DMultisampleArray: return TextureType::Texture2DMSArray;
    case MTL::TextureTypeTextureBuffer: return TextureType::Buffer;
  }

  return TextureType::Unknown;
}

void MetalReplay::AddShaderBindings(ResourceId shader, NS::Array *arguments)
{
  auto shaderIt = m_Shaders.find(shader);
  if(shaderIt == m_Shaders.end() || arguments == NULL)
    return;

  ShaderReflection &reflection = shaderIt->second;
  reflection.constantBlocks.clear();
  reflection.samplers.clear();
  reflection.readOnlyResources.clear();
  reflection.readWriteResources.clear();

  ShaderBindingUsage &usage = m_ShaderBindingUsage[shader];
  usage = ShaderBindingUsage();
  usage.available = true;

  for(NS::UInteger i = 0; i < arguments->count(); i++)
  {
    MTL::Argument *argument = arguments->object<MTL::Argument>(i);
    if(argument == NULL)
      continue;

    const rdcstr name = argument->name() ? argument->name()->utf8String() : "";
    const uint32_t bind = (uint32_t)argument->index();
    const uint32_t arraySize = RDCMAX(1U, (uint32_t)argument->arrayLength());

    if(argument->type() == MTL::ArgumentTypeBuffer)
    {
      ConstantBlock block;
      block.name = name;
      block.fixedBindNumber = bind;
      block.bindArraySize = arraySize;
      block.byteSize = (uint32_t)argument->bufferDataSize();
      block.bufferBacked = true;
      reflection.constantBlocks.push_back(block);
      usage.constantBlocks.push_back(argument->active());
    }
    else if(argument->type() == MTL::ArgumentTypeSampler)
    {
      ShaderSampler sampler;
      sampler.name = name;
      sampler.fixedBindNumber = bind;
      sampler.bindArraySize = arraySize;
      reflection.samplers.push_back(sampler);
      usage.samplers.push_back(argument->active());
    }
    else if(argument->type() == MTL::ArgumentTypeTexture)
    {
      ShaderResource resource;
      resource.name = name;
      resource.fixedBindNumber = bind;
      resource.bindArraySize = arraySize;
      resource.textureType = MakeShaderTextureType(argument->textureType());
      resource.isTexture = true;
      resource.hasSampler = false;
      resource.isInputAttachment = false;
      resource.isReadOnly = argument->access() == MTL::BindingAccessReadOnly;
      resource.descriptorType = resource.isReadOnly ? DescriptorType::Image
                                                    : DescriptorType::ReadWriteImage;

      if(resource.isReadOnly)
      {
        reflection.readOnlyResources.push_back(resource);
        usage.readOnlyResources.push_back(argument->active());
      }
      else
      {
        reflection.readWriteResources.push_back(resource);
        usage.readWriteResources.push_back(argument->active());
      }
    }
  }
}

rdcarray<ShaderEntryPoint> MetalReplay::GetShaderEntryPoints(ResourceId shader)
{
  auto it = m_Shaders.find(shader);
  if(it == m_Shaders.end())
    return {};

  return {{it->second.entryPoint, it->second.stage}};
}

const ShaderReflection *MetalReplay::GetShader(ResourceId pipeline, ResourceId shader,
                                               ShaderEntryPoint entry)
{
  auto it = m_Shaders.find(shader);
  if(it == m_Shaders.end())
    return NULL;

  if(!entry.name.empty() &&
     (entry.name != it->second.entryPoint || entry.stage != it->second.stage))
    return NULL;

  return &it->second;
}

void MetalReplay::AddRenderPipeline(ResourceId id,
                                    const RDMTL::RenderPipelineDescriptor &descriptor,
                                    MTL::RenderPipelineReflection *reflection)
{
  RenderPipelineInfo &pipeline = m_RenderPipelines[id];
  pipeline.vertexFunction = GetResID(descriptor.vertexFunction);
  pipeline.fragmentFunction = GetResID(descriptor.fragmentFunction);
  pipeline.vertexDescriptor = descriptor.vertexDescriptor;
  pipeline.sampleCount = (uint32_t)descriptor.rasterSampleCount;
  pipeline.alphaToCoverageEnabled = descriptor.alphaToCoverageEnabled;
  pipeline.alphaToOneEnabled = descriptor.alphaToOneEnabled;
  pipeline.colorAttachments = descriptor.colorAttachments;

  if(reflection)
  {
    AddShaderBindings(pipeline.vertexFunction, reflection->vertexArguments());
    AddShaderBindings(pipeline.fragmentFunction, reflection->fragmentArguments());
  }
}

void MetalReplay::AddDepthStencilState(ResourceId id,
                                       const RDMTL::DepthStencilDescriptor &descriptor)
{
  m_DepthStencilStates[id] = descriptor;
}

void MetalReplay::AddSamplerState(ResourceId id, const RDMTL::SamplerDescriptor &descriptor)
{
  m_SamplerStates[id] = descriptor;
}

static ResourceFormat MakeVertexFormat(MTL::VertexFormat vertexFormat)
{
  ResourceFormat ret;
  ret.type = ResourceFormatType::Regular;

  switch(vertexFormat)
  {
    case MTL::VertexFormatFloat:
      ret.compCount = 1;
      ret.compByteWidth = 4;
      ret.compType = CompType::Float;
      break;
    case MTL::VertexFormatFloat2:
      ret.compCount = 2;
      ret.compByteWidth = 4;
      ret.compType = CompType::Float;
      break;
    case MTL::VertexFormatFloat3:
      ret.compCount = 3;
      ret.compByteWidth = 4;
      ret.compType = CompType::Float;
      break;
    case MTL::VertexFormatFloat4:
      ret.compCount = 4;
      ret.compByteWidth = 4;
      ret.compType = CompType::Float;
      break;
    default: ret = ResourceFormat(); break;
  }

  return ret;
}

static CompareFunction MakeCompareFunction(MTL::CompareFunction function)
{
  switch(function)
  {
    case MTL::CompareFunctionNever: return CompareFunction::Never;
    case MTL::CompareFunctionLess: return CompareFunction::Less;
    case MTL::CompareFunctionEqual: return CompareFunction::Equal;
    case MTL::CompareFunctionLessEqual: return CompareFunction::LessEqual;
    case MTL::CompareFunctionGreater: return CompareFunction::Greater;
    case MTL::CompareFunctionNotEqual: return CompareFunction::NotEqual;
    case MTL::CompareFunctionGreaterEqual: return CompareFunction::GreaterEqual;
    case MTL::CompareFunctionAlways: return CompareFunction::AlwaysTrue;
  }

  return CompareFunction::AlwaysTrue;
}

static StencilOperation MakeStencilOperation(MTL::StencilOperation operation)
{
  switch(operation)
  {
    case MTL::StencilOperationKeep: return StencilOperation::Keep;
    case MTL::StencilOperationZero: return StencilOperation::Zero;
    case MTL::StencilOperationReplace: return StencilOperation::Replace;
    case MTL::StencilOperationIncrementClamp: return StencilOperation::IncSat;
    case MTL::StencilOperationDecrementClamp: return StencilOperation::DecSat;
    case MTL::StencilOperationIncrementWrap: return StencilOperation::IncWrap;
    case MTL::StencilOperationDecrementWrap: return StencilOperation::DecWrap;
    case MTL::StencilOperationInvert: return StencilOperation::Invert;
  }

  return StencilOperation::Keep;
}

static StencilFace MakeStencilFace(const RDMTL::StencilDescriptor &descriptor)
{
  StencilFace ret;
  ret.function = MakeCompareFunction(descriptor.stencilCompareFunction);
  ret.failOperation = MakeStencilOperation(descriptor.stencilFailureOperation);
  ret.depthFailOperation = MakeStencilOperation(descriptor.depthFailureOperation);
  ret.passOperation = MakeStencilOperation(descriptor.depthStencilPassOperation);
  ret.compareMask = descriptor.readMask;
  ret.writeMask = descriptor.writeMask;
  return ret;
}

static Descriptor MakeRenderTargetDescriptor(MetalReplay *replay,
                                             const RDMTL::RenderPassAttachmentDescriptor &attachment)
{
  Descriptor ret;
  ret.type = DescriptorType::ReadWriteImage;
  ret.resource = attachment.texture ? GetResID(attachment.texture) : ResourceId();
  ret.firstMip = (uint8_t)attachment.level;
  ret.firstSlice = (uint16_t)attachment.slice;

  TextureDescription texture = replay->GetTexture(ret.resource);
  if(texture.resourceId != ResourceId())
  {
    ret.format = texture.format;
    ret.textureType = texture.type;
  }

  return ret;
}

static Descriptor MakeResolveTargetDescriptor(MetalReplay *replay,
                                              const RDMTL::RenderPassAttachmentDescriptor &attachment)
{
  Descriptor ret;
  ret.type = DescriptorType::ReadWriteImage;
  ret.resource = attachment.resolveTexture ? GetResID(attachment.resolveTexture) : ResourceId();
  ret.firstMip = (uint8_t)attachment.resolveLevel;
  ret.firstSlice = (uint16_t)attachment.resolveSlice;

  TextureDescription texture = replay->GetTexture(ret.resource);
  if(texture.resourceId != ResourceId())
  {
    ret.format = texture.format;
    ret.textureType = texture.type;
  }

  return ret;
}

void MetalReplay::BeginRenderPass(const RDMTL::RenderPassDescriptor &descriptor)
{
  m_CurrentPipelineState = MetalPipe::State();

  m_CurrentPipelineState.colorTargets.resize(descriptor.colorAttachments.size());
  m_CurrentPipelineState.resolveTargets.resize(descriptor.colorAttachments.size());
  for(size_t i = 0; i < descriptor.colorAttachments.size(); i++)
  {
    m_CurrentPipelineState.colorTargets[i] =
        MakeRenderTargetDescriptor(this, descriptor.colorAttachments[i]);
    m_CurrentPipelineState.resolveTargets[i] =
        MakeResolveTargetDescriptor(this, descriptor.colorAttachments[i]);
  }

  m_CurrentPipelineState.depthTarget = MakeRenderTargetDescriptor(this, descriptor.depthAttachment);
  if(m_CurrentPipelineState.depthTarget.resource == ResourceId())
    m_CurrentPipelineState.depthTarget =
        MakeRenderTargetDescriptor(this, descriptor.stencilAttachment);
}

void MetalReplay::EndRenderPass()
{
  m_CurrentPipelineState = MetalPipe::State();
}

void MetalReplay::BindRenderPipeline(ResourceId id)
{
  m_CurrentPipelineState.pipelineResourceId = id;
  m_CurrentPipelineState.vertexShader = MetalPipe::Shader();
  m_CurrentPipelineState.fragmentShader = MetalPipe::Shader();
  m_CurrentPipelineState.sampleCount = 1;
  m_CurrentPipelineState.alphaToCoverageEnabled = false;
  m_CurrentPipelineState.alphaToOneEnabled = false;
  m_CurrentPipelineState.vertexAttributes.clear();
  m_CurrentPipelineState.colorBlends.clear();
  for(MetalPipe::VertexBuffer &binding : m_CurrentPipelineState.vertexBuffers)
  {
    binding.byteStride = 0;
    binding.perInstance = false;
    binding.stepRate = 1;
  }

  auto pipeline = m_RenderPipelines.find(id);
  if(pipeline == m_RenderPipelines.end())
    return;

  auto bindShader = [this](ResourceId shader, ShaderStage stage) {
    MetalPipe::Shader ret;
    ret.resourceId = shader;
    ret.stage = stage;

    auto reflection = m_Shaders.find(shader);
    if(reflection != m_Shaders.end())
    {
      ret.reflection = &reflection->second;
      ret.entryPoint = reflection->second.entryPoint;
    }

    return ret;
  };

  m_CurrentPipelineState.vertexShader =
      bindShader(pipeline->second.vertexFunction, ShaderStage::Vertex);
  m_CurrentPipelineState.fragmentShader =
      bindShader(pipeline->second.fragmentFunction, ShaderStage::Fragment);
  m_CurrentPipelineState.sampleCount = pipeline->second.sampleCount;
  m_CurrentPipelineState.alphaToCoverageEnabled = pipeline->second.alphaToCoverageEnabled;
  m_CurrentPipelineState.alphaToOneEnabled = pipeline->second.alphaToOneEnabled;

  const RDMTL::VertexDescriptor &vertexDescriptor = pipeline->second.vertexDescriptor;
  for(size_t attributeIndex = 0; attributeIndex < vertexDescriptor.attributes.size();
      attributeIndex++)
  {
    const RDMTL::VertexAttributeDescriptor &attribute = vertexDescriptor.attributes[attributeIndex];
    if(attribute.format == MTL::VertexFormatInvalid)
      continue;

    m_CurrentPipelineState.vertexAttributes.push_back(MetalPipe::VertexAttribute());
    MetalPipe::VertexAttribute &state = m_CurrentPipelineState.vertexAttributes.back();
    state.attributeIndex = (uint32_t)attributeIndex;
    state.bufferIndex = (uint32_t)attribute.bufferIndex;
    state.byteOffset = (uint32_t)attribute.offset;
    state.format = MakeVertexFormat(attribute.format);
  }

  for(size_t slot = 0; slot < vertexDescriptor.layouts.size(); slot++)
  {
    const RDMTL::VertexBufferLayoutDescriptor &layout = vertexDescriptor.layouts[slot];
    if(layout.stride == 0)
      continue;

    m_CurrentPipelineState.vertexBuffers.resize_for_index(slot);
    MetalPipe::VertexBuffer &binding = m_CurrentPipelineState.vertexBuffers[slot];
    binding.byteStride = (uint32_t)layout.stride;
    binding.perInstance = layout.stepFunction == MTL::VertexStepFunctionPerInstance;
    binding.stepRate = (uint32_t)layout.stepRate;
  }

  for(const RDMTL::RenderPipelineColorAttachmentDescriptor &attachment :
      pipeline->second.colorAttachments)
  {
    ColorBlend blend;
    blend.enabled = attachment.blendingEnabled;
    blend.colorBlend.source = MakeBlendMultiplier(attachment.sourceRGBBlendFactor);
    blend.colorBlend.destination =
        MakeBlendMultiplier(attachment.destinationRGBBlendFactor);
    blend.colorBlend.operation = MakeBlendOp(attachment.rgbBlendOperation);
    blend.alphaBlend.source = MakeBlendMultiplier(attachment.sourceAlphaBlendFactor);
    blend.alphaBlend.destination =
        MakeBlendMultiplier(attachment.destinationAlphaBlendFactor);
    blend.alphaBlend.operation = MakeBlendOp(attachment.alphaBlendOperation);
    blend.writeMask = MakeWriteMask(attachment.writeMask);
    m_CurrentPipelineState.colorBlends.push_back(blend);
  }
}

void MetalReplay::BindDepthStencilState(ResourceId id)
{
  const uint32_t frontReference = m_CurrentPipelineState.depthStencil.frontFace.reference;
  const uint32_t backReference = m_CurrentPipelineState.depthStencil.backFace.reference;
  m_CurrentPipelineState.depthStencil = MetalPipe::DepthStencil();
  m_CurrentPipelineState.depthStencil.resourceId = id;
  m_CurrentPipelineState.depthStencil.frontFace.reference = frontReference;
  m_CurrentPipelineState.depthStencil.backFace.reference = backReference;

  auto state = m_DepthStencilStates.find(id);
  if(state != m_DepthStencilStates.end())
  {
    m_CurrentPipelineState.depthStencil.depthFunction =
        MakeCompareFunction(state->second.depthCompareFunction);
    m_CurrentPipelineState.depthStencil.depthWrites = state->second.depthWriteEnabled;
    m_CurrentPipelineState.depthStencil.stencilEnabled =
        state->second.frontFaceStencil.enabled || state->second.backFaceStencil.enabled;
    m_CurrentPipelineState.depthStencil.frontFace = MakeStencilFace(state->second.frontFaceStencil);
    m_CurrentPipelineState.depthStencil.backFace = MakeStencilFace(state->second.backFaceStencil);
    m_CurrentPipelineState.depthStencil.frontFace.reference = frontReference;
    m_CurrentPipelineState.depthStencil.backFace.reference = backReference;
  }
}

void MetalReplay::SetStencilReferenceValue(uint32_t referenceValue)
{
  SetStencilReferenceValues(referenceValue, referenceValue);
}

void MetalReplay::SetStencilReferenceValues(uint32_t frontReferenceValue,
                                            uint32_t backReferenceValue)
{
  m_CurrentPipelineState.depthStencil.frontFace.reference = frontReferenceValue;
  m_CurrentPipelineState.depthStencil.backFace.reference = backReferenceValue;
}

void MetalReplay::BindVertexBuffer(uint32_t index, ResourceId id, uint64_t offset)
{
  m_CurrentPipelineState.vertexBuffers.resize_for_index(index);
  MetalPipe::VertexBuffer &binding = m_CurrentPipelineState.vertexBuffers[index];
  binding.resourceId = id;
  binding.byteOffset = offset;

  BufferDescription buffer = GetBuffer(id);
  binding.byteSize = buffer.resourceId != ResourceId() && offset < buffer.length
                         ? buffer.length - offset
                         : 0;
}

void MetalReplay::BindFragmentBuffer(uint32_t index, ResourceId id, uint64_t offset)
{
  m_CurrentPipelineState.fragmentBuffers.resize_for_index(index);
  MetalPipe::BufferBinding &binding = m_CurrentPipelineState.fragmentBuffers[index];
  binding.resourceId = id;
  binding.byteOffset = offset;

  BufferDescription buffer = GetBuffer(id);
  binding.byteSize = buffer.resourceId != ResourceId() && offset < buffer.length
                         ? buffer.length - offset
                         : 0;
}

void MetalReplay::SetFragmentBufferOffset(uint32_t index, uint64_t offset)
{
  m_CurrentPipelineState.fragmentBuffers.resize_for_index(index);
  MetalPipe::BufferBinding &binding = m_CurrentPipelineState.fragmentBuffers[index];
  binding.byteOffset = offset;

  BufferDescription buffer = GetBuffer(binding.resourceId);
  binding.byteSize = buffer.resourceId != ResourceId() && offset < buffer.length
                         ? buffer.length - offset
                         : 0;
}

void MetalReplay::BindFragmentTexture(uint32_t index, ResourceId id)
{
  m_CurrentPipelineState.fragmentTextures.resize_for_index(index);
  m_CurrentPipelineState.fragmentTextures[index] = id;
}

void MetalReplay::BindFragmentSampler(uint32_t index, ResourceId id)
{
  m_CurrentPipelineState.fragmentSamplers.resize_for_index(index);
  m_CurrentPipelineState.fragmentSamplers[index] = id;
}

void MetalReplay::BindIndexBuffer(ResourceId id, uint64_t offset, MTL::IndexType indexType)
{
  MetalPipe::VertexBuffer &binding = m_CurrentPipelineState.indexBuffer;
  binding.resourceId = id;
  binding.byteOffset = offset;
  binding.byteStride = indexType == MTL::IndexTypeUInt16 ? 2 : 4;

  BufferDescription buffer = GetBuffer(id);
  binding.byteSize = buffer.resourceId != ResourceId() && offset < buffer.length
                         ? buffer.length - offset
                         : 0;
}

void MetalReplay::SetViewport(const MTL::Viewport &viewport)
{
  m_CurrentPipelineState.rasterizer.viewport =
      Viewport((float)viewport.originX, (float)viewport.originY, (float)viewport.width,
               (float)viewport.height, (float)viewport.znear, (float)viewport.zfar, true);
}

void MetalReplay::SetScissor(const MTL::ScissorRect &scissor)
{
  m_CurrentPipelineState.rasterizer.scissor =
      Scissor((int32_t)scissor.x, (int32_t)scissor.y, (int32_t)scissor.width,
              (int32_t)scissor.height, true);
}

void MetalReplay::SetFrontFacingWinding(MTL::Winding winding)
{
  m_CurrentPipelineState.rasterizer.frontCCW = winding == MTL::WindingCounterClockwise;
}

void MetalReplay::SetCullMode(MTL::CullMode cullMode)
{
  switch(cullMode)
  {
    case MTL::CullModeNone: m_CurrentPipelineState.rasterizer.cullMode = CullMode::NoCull; break;
    case MTL::CullModeFront: m_CurrentPipelineState.rasterizer.cullMode = CullMode::Front; break;
    case MTL::CullModeBack: m_CurrentPipelineState.rasterizer.cullMode = CullMode::Back; break;
  }
}

void MetalReplay::SetPrimitiveTopology(MTL::PrimitiveType primitiveType)
{
  switch(primitiveType)
  {
    case MTL::PrimitiveTypePoint: m_CurrentPipelineState.topology = Topology::PointList; break;
    case MTL::PrimitiveTypeLine: m_CurrentPipelineState.topology = Topology::LineList; break;
    case MTL::PrimitiveTypeLineStrip: m_CurrentPipelineState.topology = Topology::LineStrip; break;
    case MTL::PrimitiveTypeTriangle:
      m_CurrentPipelineState.topology = Topology::TriangleList;
      break;
    case MTL::PrimitiveTypeTriangleStrip:
      m_CurrentPipelineState.topology = Topology::TriangleStrip;
      break;
  }
}

void MetalReplay::SavePipelineState(uint32_t eventId)
{
  if(m_MetalPipelineState)
  {
    auto state = m_EventPipelineStates.upper_bound(eventId);
    if(state != m_EventPipelineStates.begin())
    {
      --state;
      *m_MetalPipelineState = state->second;
    }
    else
    {
      *m_MetalPipelineState = m_CurrentPipelineState;
    }
  }
}

void MetalReplay::SetActionOutputs(ActionDescription &action) const
{
  const size_t count = RDCMIN(action.outputs.size(), m_CurrentPipelineState.colorTargets.size());
  for(size_t i = 0; i < count; i++)
  {
    if(i < m_CurrentPipelineState.resolveTargets.size() &&
       m_CurrentPipelineState.resolveTargets[i].resource != ResourceId())
      action.outputs[i] = m_CurrentPipelineState.resolveTargets[i].resource;
    else
      action.outputs[i] = m_CurrentPipelineState.colorTargets[i].resource;
  }
  action.depthOut = m_CurrentPipelineState.depthTarget.resource;
}

rdcarray<Descriptor> MetalReplay::GetDescriptors(ResourceId descriptorStore,
                                                 const rdcarray<DescriptorRange> &ranges)
{
  static const uint32_t SamplerOffset = 0x100;
  static const uint32_t BufferOffset = 0x200;
  if(descriptorStore != GetResID(m_pDriver) || m_MetalPipelineState == NULL)
    return {};

  size_t count = 0;
  for(const DescriptorRange &range : ranges)
    count += range.count;
  rdcarray<Descriptor> ret;
  ret.resize(count);

  size_t dst = 0;
  for(const DescriptorRange &range : ranges)
  {
    for(uint32_t i = 0; i < range.count; i++, dst++)
    {
      const uint32_t offset = range.offset + i * range.descriptorSize;
      Descriptor &descriptor = ret[dst];
      if(range.type == DescriptorType::Image && offset < SamplerOffset &&
         offset < m_MetalPipelineState->fragmentTextures.size())
      {
        descriptor.type = DescriptorType::Image;
        descriptor.resource = m_MetalPipelineState->fragmentTextures[offset];
        TextureDescription texture = GetTexture(descriptor.resource);
        if(texture.resourceId != ResourceId())
        {
          descriptor.format = texture.format;
          descriptor.textureType = texture.type;
          descriptor.numMips = (uint8_t)texture.mips;
          descriptor.numSlices = (uint16_t)texture.arraysize;
        }
      }
      else if(range.type == DescriptorType::ConstantBuffer && offset >= BufferOffset)
      {
        const uint32_t slot = offset - BufferOffset;
        if(slot < m_MetalPipelineState->fragmentBuffers.size())
        {
          const MetalPipe::BufferBinding &binding =
              m_MetalPipelineState->fragmentBuffers[slot];
          descriptor.type = DescriptorType::ConstantBuffer;
          descriptor.resource = binding.resourceId;
          descriptor.byteOffset = binding.byteOffset;
          descriptor.byteSize = binding.byteSize;
        }
      }
    }
  }
  return ret;
}

static AddressMode MakeAddressMode(MTL::SamplerAddressMode mode)
{
  switch(mode)
  {
    case MTL::SamplerAddressModeClampToEdge: return AddressMode::ClampEdge;
    case MTL::SamplerAddressModeMirrorClampToEdge: return AddressMode::MirrorOnce;
    case MTL::SamplerAddressModeRepeat: return AddressMode::Wrap;
    case MTL::SamplerAddressModeMirrorRepeat: return AddressMode::Mirror;
    case MTL::SamplerAddressModeClampToZero:
    case MTL::SamplerAddressModeClampToBorderColor: return AddressMode::ClampBorder;
  }
  return AddressMode::Wrap;
}

static FilterMode MakeFilterMode(MTL::SamplerMinMagFilter filter)
{
  return filter == MTL::SamplerMinMagFilterLinear ? FilterMode::Linear : FilterMode::Point;
}

static FilterMode MakeFilterMode(MTL::SamplerMipFilter filter)
{
  if(filter == MTL::SamplerMipFilterLinear)
    return FilterMode::Linear;
  if(filter == MTL::SamplerMipFilterNearest)
    return FilterMode::Point;
  return FilterMode::NoFilter;
}

rdcarray<SamplerDescriptor> MetalReplay::GetSamplerDescriptors(ResourceId descriptorStore,
                                                               const rdcarray<DescriptorRange> &ranges)
{
  static const uint32_t SamplerOffset = 0x100;
  if(descriptorStore != GetResID(m_pDriver) || m_MetalPipelineState == NULL)
    return {};

  size_t count = 0;
  for(const DescriptorRange &range : ranges)
    count += range.count;
  rdcarray<SamplerDescriptor> ret;
  ret.resize(count);

  size_t dst = 0;
  for(const DescriptorRange &range : ranges)
  {
    for(uint32_t i = 0; i < range.count; i++, dst++)
    {
      const uint32_t offset = range.offset + i * range.descriptorSize;
      if(range.type != DescriptorType::Sampler || offset < SamplerOffset)
        continue;

      const uint32_t slot = offset - SamplerOffset;
      if(slot >= m_MetalPipelineState->fragmentSamplers.size())
        continue;

      const ResourceId samplerId = m_MetalPipelineState->fragmentSamplers[slot];
      auto captured = m_SamplerStates.find(samplerId);
      if(captured == m_SamplerStates.end())
        continue;

      const RDMTL::SamplerDescriptor &source = captured->second;
      SamplerDescriptor &sampler = ret[dst];
      sampler.type = DescriptorType::Sampler;
      sampler.object = samplerId;
      sampler.addressU = MakeAddressMode(source.sAddressMode);
      sampler.addressV = MakeAddressMode(source.tAddressMode);
      sampler.addressW = MakeAddressMode(source.rAddressMode);
      sampler.compareFunction = MakeCompareFunction(source.compareFunction);
      sampler.filter.minify = MakeFilterMode(source.minFilter);
      sampler.filter.magnify = MakeFilterMode(source.magFilter);
      sampler.filter.mip = MakeFilterMode(source.mipFilter);
      sampler.filter.filter = source.compareFunction == MTL::CompareFunctionNever
                                  ? FilterFunction::Normal
                                  : FilterFunction::Comparison;
      sampler.maxAnisotropy = (float)source.maxAnisotropy;
      sampler.minLOD = source.lodMinClamp;
      sampler.maxLOD = source.lodMaxClamp;
      sampler.unnormalized = !source.normalizedCoordinates;
      if(source.borderColor == MTL::SamplerBorderColorOpaqueBlack)
        sampler.borderColorValue.floatValue[3] = 1.0f;
      else if(source.borderColor == MTL::SamplerBorderColorOpaqueWhite)
      {
        for(float &component : sampler.borderColorValue.floatValue)
          component = 1.0f;
      }
    }
  }
  return ret;
}

rdcarray<DescriptorAccess> MetalReplay::GetDescriptorAccess(uint32_t eventId)
{
  static const uint32_t SamplerOffset = 0x100;
  static const uint32_t BufferOffset = 0x200;
  const MetalPipe::State *state = m_MetalPipelineState;
  if(state == NULL)
    return {};

  rdcarray<DescriptorAccess> ret;
  const ResourceId store = GetResID(m_pDriver);
  const ResourceId fragmentShader = state->fragmentShader.resourceId;
  auto reflectionIt = m_Shaders.find(fragmentShader);
  auto usageIt = m_ShaderBindingUsage.find(fragmentShader);
  const bool hasReflection = reflectionIt != m_Shaders.end() &&
                             usageIt != m_ShaderBindingUsage.end() &&
                             usageIt->second.available;
  for(size_t slot = 0; slot < state->fragmentBuffers.size(); slot++)
  {
    if(state->fragmentBuffers[slot].resourceId == ResourceId())
      continue;
    DescriptorAccess access;
    access.stage = ShaderStage::Fragment;
    access.type = DescriptorType::ConstantBuffer;
    access.index = DescriptorAccess::NoShaderBinding;
    if(hasReflection)
    {
      const rdcarray<ConstantBlock> &blocks = reflectionIt->second.constantBlocks;
      for(size_t bind = 0; bind < blocks.size(); bind++)
      {
        if(blocks[bind].fixedBindNumber == slot)
        {
          access.index = (uint16_t)bind;
          access.staticallyUnused = bind >= usageIt->second.constantBlocks.size() ||
                                    !usageIt->second.constantBlocks[bind];
          break;
        }
      }
      if(access.index == DescriptorAccess::NoShaderBinding)
        access.staticallyUnused = true;
    }
    else
    {
      access.index = (uint16_t)slot;
    }
    access.descriptorStore = store;
    access.byteOffset = BufferOffset + (uint32_t)slot;
    access.byteSize = 1;
    ret.push_back(access);
  }
  for(size_t slot = 0; slot < state->fragmentTextures.size(); slot++)
  {
    if(state->fragmentTextures[slot] == ResourceId())
      continue;
    DescriptorAccess access;
    access.stage = ShaderStage::Fragment;
    access.type = DescriptorType::Image;
    access.index = DescriptorAccess::NoShaderBinding;
    if(hasReflection)
    {
      const rdcarray<ShaderResource> &resources = reflectionIt->second.readOnlyResources;
      for(size_t bind = 0; bind < resources.size(); bind++)
      {
        if(resources[bind].fixedBindNumber == slot)
        {
          access.index = (uint16_t)bind;
          access.staticallyUnused = bind >= usageIt->second.readOnlyResources.size() ||
                                    !usageIt->second.readOnlyResources[bind];
          break;
        }
      }
      if(access.index == DescriptorAccess::NoShaderBinding)
        access.staticallyUnused = true;
    }
    else
    {
      access.index = (uint16_t)slot;
    }
    access.descriptorStore = store;
    access.byteOffset = (uint32_t)slot;
    access.byteSize = 1;
    ret.push_back(access);
  }
  for(size_t slot = 0; slot < state->fragmentSamplers.size(); slot++)
  {
    if(state->fragmentSamplers[slot] == ResourceId())
      continue;
    DescriptorAccess access;
    access.stage = ShaderStage::Fragment;
    access.type = DescriptorType::Sampler;
    access.index = DescriptorAccess::NoShaderBinding;
    if(hasReflection)
    {
      const rdcarray<ShaderSampler> &samplers = reflectionIt->second.samplers;
      for(size_t bind = 0; bind < samplers.size(); bind++)
      {
        if(samplers[bind].fixedBindNumber == slot)
        {
          access.index = (uint16_t)bind;
          access.staticallyUnused = bind >= usageIt->second.samplers.size() ||
                                    !usageIt->second.samplers[bind];
          break;
        }
      }
      if(access.index == DescriptorAccess::NoShaderBinding)
        access.staticallyUnused = true;
    }
    else
    {
      access.index = (uint16_t)slot;
    }
    access.descriptorStore = store;
    access.byteOffset = SamplerOffset + (uint32_t)slot;
    access.byteSize = 1;
    ret.push_back(access);
  }
  return ret;
}

void MetalReplay::AddEvent(uint32_t chunkIndex, uint64_t fileOffset)
{
  APIEvent event;
  event.eventId = m_NextEventID++;
  event.chunkIndex = chunkIndex;
  event.fileOffset = fileOffset;
  m_PendingEvents.push_back(event);
  m_Events.resize_for_index(event.eventId);
  m_Events[event.eventId] = event;
}

void MetalReplay::AddAction(const ActionDescription &in)
{
  ActionDescription action = in;
  action.eventId = m_PendingEvents.empty() ? m_NextEventID++ : m_PendingEvents.back().eventId;
  action.actionId = m_NextActionID++;
  action.events.swap(m_PendingEvents);
  m_EventPipelineStates[action.eventId] = m_CurrentPipelineState;
  m_FrameRecord.actionList.push_back(action);
}

RDResult MetalReplay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  if(!rdc)
    return ResultCode::Succeeded;

  m_FrameRecord = {};
  m_PendingEvents.clear();
  m_Events.clear();
  m_EventPipelineStates.clear();
  m_NextEventID = 1;
  m_NextActionID = 1;

  RDResult ret = m_pDriver->ReadLogInitialisation(rdc, storeStructuredBuffers);
  if(ret != ResultCode::Succeeded)
    m_FatalError = ret;

  return ret;
}

void MetalReplay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  if(replayType != eReplay_OnlyDraw)
    m_CurrentPipelineState = MetalPipe::State();

  RDResult result = m_pDriver->ReplayLog(endEventID, replayType);
  if(result != ResultCode::Succeeded)
    m_FatalError = result;
}

const APIEvent *MetalReplay::GetEvent(uint32_t eventId) const
{
  if(m_Events.size() <= 1)
    return NULL;

  size_t index = RDCMIN((size_t)eventId, m_Events.size() - 1);
  while(index > 0 && m_Events[index].eventId == 0)
    index--;
  return index > 0 ? &m_Events[index] : NULL;
}

uint64_t MetalReplay::GetNextEventOffset(uint32_t eventId, uint64_t frameSize) const
{
  for(size_t i = (size_t)eventId + 1; i < m_Events.size(); i++)
  {
    if(m_Events[i].eventId != 0)
      return m_Events[i].fileOffset;
  }
  return frameSize;
}

SDFile *MetalReplay::GetStructuredFile()
{
  return m_pDriver->GetStructuredFile();
}

void MetalReplay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData)
{
  retData.clear();
  WrappedMTLObject *resource = m_pDriver->GetResourceManager()->GetResource(buff, true);
  WrappedMTLBuffer *buffer = (WrappedMTLBuffer *)resource;
  MTL::Buffer *real = buffer ? Unwrap(buffer) : NULL;
  if(!real || offset >= real->length() || real->storageMode() == MTL::StorageModePrivate)
    return;

  uint64_t available = real->length() - offset;
  if(len == 0 || len > available)
    len = available;
  retData.assign((byte *)real->contents() + offset, len);
}

void MetalReplay::GetTextureData(ResourceId tex, const Subresource &sub,
                                 const GetTextureDataParams &params, bytebuf &data)
{
  data.clear();

  if(params.remap != RemapTexture::NoRemap || params.resolve)
  {
    RDCERR("Metal texture readback does not yet support remapping or multisample resolve");
    return;
  }

  if(GetTexture(tex).resourceId == ResourceId())
  {
    RDCERR("Metal texture readback requested for non-texture resource %s", ToStr(tex).c_str());
    return;
  }

  WrappedMTLObject *resource = m_pDriver->GetResourceManager()->GetResource(tex, true);
  WrappedMTLTexture *texture = (WrappedMTLTexture *)resource;
  MTL::Texture *real = texture ? Unwrap(texture) : NULL;
  ReadTextureSubresource(real, sub, data);
}

void MetalReplay::BuildTargetShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                    const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId &id, rdcstr &errors)
{
  id = ResourceId();
  errors = "Metal target shader compilation is not implemented.";
}

void MetalReplay::ReplaceResource(ResourceId from, ResourceId to)
{
  m_pDriver->GetResourceManager()->ReplaceResource(from, to);
}

void MetalReplay::RemoveReplacement(ResourceId id)
{
  m_pDriver->GetResourceManager()->RemoveReplacement(id);
}

bool MetalReplay::IsRenderOutput(ResourceId id)
{
  return id != ResourceId() && id == m_pDriver->GetLastPresentedImage();
}

uint64_t MetalReplay::MakeOutputWindow(WindowingData window, bool depth)
{
  if(window.system != WindowingSystem::MacOS || window.macOS.layer == NULL)
  {
    RDCERR("Metal replay requires a macOS CAMetalLayer output window");
    return 0;
  }

  CA::MetalLayer *layer = (CA::MetalLayer *)window.macOS.layer;
  layer->retain();
  layer->setDevice(Unwrap(m_pDriver));
  layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  layer->setFramebufferOnly(true);

  CGRect bounds = layer->bounds();
  CGFloat scale = layer->contentsScale();
  int32_t width = (int32_t)(bounds.size.width * scale);
  int32_t height = (int32_t)(bounds.size.height * scale);
  if(width <= 0 || height <= 0)
  {
    CGSize drawableSize = layer->drawableSize();
    width = (int32_t)drawableSize.width;
    height = (int32_t)drawableSize.height;
  }
  else
  {
    layer->setDrawableSize(CGSizeMake(width, height));
  }

  OutputWindow output;
  output.layer = layer;
  if(!ResizeOutputWindow(output, RDCMAX(1, width), RDCMAX(1, height)))
  {
    layer->release();
    return 0;
  }

  const uint64_t id = m_NextOutputWindowID++;
  m_OutputWindows[id] = output;
  return id;
}

void MetalReplay::DestroyOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(it == m_OutputWindows.end())
    return;

  if(m_ActiveOutputWindowID == id)
    m_ActiveOutputWindowID = 0;
  if(it->second.texture)
    it->second.texture->release();
  if(it->second.layer)
    it->second.layer->release();
  m_OutputWindows.erase(it);
}

bool MetalReplay::CheckResizeOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(it == m_OutputWindows.end() || it->second.layer == NULL)
    return false;

  CGRect bounds = it->second.layer->bounds();
  CGFloat scale = it->second.layer->contentsScale();
  int32_t width = (int32_t)(bounds.size.width * scale);
  int32_t height = (int32_t)(bounds.size.height * scale);
  if(width <= 0 || height <= 0)
  {
    CGSize drawableSize = it->second.layer->drawableSize();
    width = (int32_t)drawableSize.width;
    height = (int32_t)drawableSize.height;
  }
  else
  {
    it->second.layer->setDrawableSize(CGSizeMake(width, height));
  }

  width = RDCMAX(1, width);
  height = RDCMAX(1, height);
  if(width == it->second.width && height == it->second.height)
    return false;

  return ResizeOutputWindow(it->second, width, height);
}

void MetalReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  auto it = m_OutputWindows.find(id);
  if(it == m_OutputWindows.end())
  {
    w = h = 0;
    return;
  }

  w = it->second.width;
  h = it->second.height;
}

void MetalReplay::GetOutputWindowData(uint64_t id, bytebuf &retData)
{
  retData.clear();

  auto it = m_OutputWindows.find(id);
  if(it == m_OutputWindows.end() || it->second.texture == NULL || !InitialiseOutputResources())
    return;

  MTL::Device *device = Unwrap(m_pDriver);
  const uint64_t tightRowPitch = uint64_t(it->second.width) * 4;
  const uint64_t alignment = RDCMAX<uint64_t>(
      1, device->minimumLinearTextureAlignmentForPixelFormat(MTL::PixelFormatBGRA8Unorm));
  const uint64_t rowPitch = AlignUp(tightRowPitch, alignment);
  const uint64_t bufferSize = rowPitch * uint64_t(it->second.height);
  MTL::Buffer *readback = device->newBuffer((NS::UInteger)bufferSize, MTL::ResourceStorageModeShared);
  if(readback == NULL)
  {
    RDCERR("Failed to allocate Metal replay output readback buffer");
    return;
  }

  MTL::CommandBuffer *commandBuffer = m_OutputQueue->commandBuffer();
  MTL::BlitCommandEncoder *blit = commandBuffer->blitCommandEncoder();
  blit->copyFromTexture(
      it->second.texture, 0, 0, MTL::Origin::Make(0, 0, 0),
      MTL::Size::Make((NS::UInteger)it->second.width, (NS::UInteger)it->second.height, 1), readback,
      0, (NS::UInteger)rowPitch, (NS::UInteger)bufferSize);
  blit->endEncoding();
  commandBuffer->commit();
  commandBuffer->waitUntilCompleted();

  retData.resize(size_t(it->second.width) * size_t(it->second.height) * 3);
  const byte *srcBase = (const byte *)readback->contents();
  for(int32_t y = 0; y < it->second.height; ++y)
  {
    const byte *src = srcBase + uint64_t(y) * rowPitch;
    byte *dst = retData.data() + size_t(y) * size_t(it->second.width) * 3;
    for(int32_t x = 0; x < it->second.width; ++x)
    {
      dst[x * 3 + 0] = src[x * 4 + 2];
      dst[x * 3 + 1] = src[x * 4 + 1];
      dst[x * 3 + 2] = src[x * 4 + 0];
    }
  }

  readback->release();
}

bool MetalReplay::ReadTextureSubresource(MTL::Texture *texture, const Subresource &sub,
                                         bytebuf &data)
{
  data.clear();

  if(texture == NULL)
  {
    RDCERR("Cannot read back a NULL Metal texture");
    return false;
  }

  const MTL::TextureType textureType = texture->textureType();
  const bool supportedType = textureType == MTL::TextureType2D ||
                             textureType == MTL::TextureType2DArray ||
                             textureType == MTL::TextureTypeCube ||
                             textureType == MTL::TextureTypeCubeArray;
  const uint32_t sliceCount =
      (uint32_t)texture->arrayLength() *
      ((textureType == MTL::TextureTypeCube || textureType == MTL::TextureTypeCubeArray) ? 6U : 1U);
  if(!supportedType || texture->sampleCount() != 1 || sub.sample != 0 || sub.slice >= sliceCount)
  {
    RDCERR("Metal texture readback does not support type %s, sample %u, or slice %u/%u",
           ToStr(textureType).c_str(), sub.sample, sub.slice, sliceCount);
    return false;
  }

  if(sub.mip >= texture->mipmapLevelCount())
  {
    RDCERR("Metal texture readback mip %u is out of range for a texture with %llu mips", sub.mip,
           (unsigned long long)texture->mipmapLevelCount());
    return false;
  }

  switch(texture->pixelFormat())
  {
    case MTL::PixelFormatRGBA8Unorm:
    case MTL::PixelFormatRGBA8Unorm_sRGB:
    case MTL::PixelFormatBGRA8Unorm:
    case MTL::PixelFormatBGRA8Unorm_sRGB: break;
    default:
      RDCERR("Metal texture readback does not yet support format %s",
             ToStr(texture->pixelFormat()).c_str());
      return false;
  }

  if(!InitialiseOutputResources())
    return false;

  MTL::Device *device = Unwrap(m_pDriver);
  const uint64_t width = RDCMAX(1ULL, uint64_t(texture->width()) >> sub.mip);
  const uint64_t height = RDCMAX(1ULL, uint64_t(texture->height()) >> sub.mip);
  const uint64_t tightRowPitch = width * 4;
  const uint64_t alignment = RDCMAX<uint64_t>(
      1, device->minimumLinearTextureAlignmentForPixelFormat(texture->pixelFormat()));
  const uint64_t rowPitch = AlignUp(tightRowPitch, alignment);
  const uint64_t bufferSize = rowPitch * height;

  MTL::Buffer *readback =
      device->newBuffer((NS::UInteger)bufferSize, MTL::ResourceStorageModeShared);
  if(readback == NULL)
  {
    RDCERR("Failed to allocate %llu bytes for Metal texture readback",
           (unsigned long long)bufferSize);
    return false;
  }

  MTL::CommandBuffer *commandBuffer = m_OutputQueue->commandBuffer();
  MTL::BlitCommandEncoder *blit = commandBuffer ? commandBuffer->blitCommandEncoder() : NULL;
  if(commandBuffer == NULL || blit == NULL)
  {
    RDCERR("Failed to create Metal texture readback command buffer");
    readback->release();
    return false;
  }

  blit->copyFromTexture(texture, sub.slice, sub.mip, MTL::Origin::Make(0, 0, 0),
                        MTL::Size::Make((NS::UInteger)width, (NS::UInteger)height, 1), readback, 0,
                        (NS::UInteger)rowPitch, (NS::UInteger)bufferSize);
  blit->endEncoding();
  commandBuffer->commit();
  commandBuffer->waitUntilCompleted();

  if(commandBuffer->status() == MTL::CommandBufferStatusError)
  {
    NS::Error *error = commandBuffer->error();
    RDCERR("Metal texture readback failed: %s",
           error ? error->localizedDescription()->utf8String() : "unknown error");
    readback->release();
    return false;
  }

  data.resize(size_t(tightRowPitch * height));
  const byte *src = (const byte *)readback->contents();
  for(uint64_t y = 0; y < height; y++)
    memcpy(data.data() + size_t(y * tightRowPitch), src + y * rowPitch, size_t(tightRowPitch));

  readback->release();
  return true;
}

void MetalReplay::ClearOutputWindowColor(uint64_t id, FloatVector col)
{
  auto it = m_OutputWindows.find(id);
  if(it == m_OutputWindows.end() || it->second.texture == NULL || !InitialiseOutputResources())
    return;

  MTL::CommandBuffer *commandBuffer = m_OutputQueue->commandBuffer();
  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor *colour = pass->colorAttachments()->object(0);
  colour->setTexture(it->second.texture);
  colour->setLoadAction(MTL::LoadActionClear);
  colour->setStoreAction(MTL::StoreActionStore);
  colour->setClearColor(MTL::ClearColor::Make(col.x, col.y, col.z, col.w));

  MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
  encoder->endEncoding();
  commandBuffer->commit();
  commandBuffer->waitUntilCompleted();
}

void MetalReplay::BindOutputWindow(uint64_t id, bool depth)
{
  m_ActiveOutputWindowID = m_OutputWindows.find(id) != m_OutputWindows.end() ? id : 0;
}

bool MetalReplay::IsOutputWindowVisible(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  return it != m_OutputWindows.end() && it->second.layer != NULL && it->second.width > 0 &&
         it->second.height > 0;
}

void MetalReplay::FlipOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(it == m_OutputWindows.end() || it->second.layer == NULL || it->second.texture == NULL)
    return;

  CA::MetalDrawable *drawable = it->second.layer->nextDrawable();
  if(drawable == NULL)
    return;

  TextureDisplay display;
  display.scale = -1.0f;
  display.red = display.green = display.blue = display.alpha = true;
  display.rangeMin = 0.0f;
  display.rangeMax = 1.0f;
  display.rawOutput = true;

  if(!RenderTextureInternal(it->second.texture, drawable->texture(), display, MTL::LoadActionClear,
                            MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0)))
    return;

  MTL::CommandBuffer *present = m_OutputQueue->commandBuffer();
  present->presentDrawable(drawable);
  present->commit();
  present->waitUntilCompleted();
}

bool MetalReplay::InitialiseOutputResources()
{
  if(m_OutputPipeline && m_MeshPipeline && m_OutputQueue)
    return true;

  MTL::Device *device = Unwrap(m_pDriver);
  if(device == NULL)
    return false;

  if(m_OutputQueue == NULL)
    m_OutputQueue = device->newCommandQueue();
  if(m_OutputQueue == NULL)
  {
    RDCERR("Failed to create Metal replay output command queue");
    return false;
  }

  static const char *source = R"EOSHADER(
#include <metal_stdlib>
using namespace metal;

struct DisplayParams
{
  float2 outputSize;
  float2 textureSize;
  float2 offset;
  float scale;
  uint mip;
  uint channelMask;
  uint flipY;
  uint rawOutput;
  float rangeMin;
  float inverseRange;
  uint slice;
  uint textureType;
};

struct VSOut
{
  float4 position [[position]];
  float2 uv;
};

vertex VSOut rdoc_display_vs(uint vertexID [[vertex_id]], constant DisplayParams &params [[buffer(0)]])
{
  const float2 corners[4] = {float2(0.0, 0.0), float2(0.0, 1.0),
                             float2(1.0, 0.0), float2(1.0, 1.0)};
  float2 corner = corners[vertexID];
  float2 pixel = params.offset + corner * params.textureSize * params.scale;

  VSOut output;
  output.position = float4(pixel.x * 2.0 / params.outputSize.x - 1.0,
                           1.0 - pixel.y * 2.0 / params.outputSize.y, 0.0, 1.0);
  output.uv = corner;
  if(params.flipY != 0)
    output.uv.y = 1.0 - output.uv.y;
  return output;
}

fragment float4 rdoc_display_fs(VSOut input [[stage_in]], texture2d<float> source2d [[texture(0)]],
                                texture2d_array<float> sourceArray [[texture(1)]],
                                texturecube<float> sourceCube [[texture(2)]],
                                constant DisplayParams &params [[buffer(0)]])
{
  constexpr sampler nearestSampler(coord::normalized, address::clamp_to_edge, filter::nearest,
                                   mip_filter::nearest);
  float4 value;
  if(params.textureType == 1)
  {
    value = sourceArray.sample(nearestSampler, input.uv, params.slice, level(params.mip));
  }
  else if(params.textureType == 2)
  {
    const float3 directions[6] = {
        float3(1.0, 0.0, 0.0), float3(-1.0, 0.0, 0.0),
        float3(0.0, 1.0, 0.0), float3(0.0, -1.0, 0.0),
        float3(0.0, 0.0, 1.0), float3(0.0, 0.0, -1.0),
    };
    value = sourceCube.sample(nearestSampler, directions[params.slice % 6], level(params.mip));
  }
  else
  {
    value = source2d.sample(nearestSampler, input.uv, level(params.mip));
  }
  if(params.rawOutput == 0)
    value = saturate((value - params.rangeMin) * params.inverseRange);

  bool r = (params.channelMask & 1) != 0;
  bool g = (params.channelMask & 2) != 0;
  bool b = (params.channelMask & 4) != 0;
  bool a = (params.channelMask & 8) != 0;
  uint rgbCount = uint(r) + uint(g) + uint(b);
  if(rgbCount == 0 && a)
    return float4(value.aaa, 1.0);
  if(rgbCount == 1 && !a)
  {
    float selected = r ? value.r : (g ? value.g : value.b);
    return float4(selected, selected, selected, 1.0);
  }

  value.rgb *= float3(r, g, b);
  value.a = a ? value.a : 1.0;
  return value;
}

struct MeshParams
{
  float4x4 mvp;
  uint stride;
  uint componentCount;
  uint2 padding;
  float4 color;
};

vertex VSOut rdoc_mesh_vs(device const uchar *vertices [[buffer(0)]],
                          constant MeshParams &params [[buffer(1)]],
                          uint vertexID [[vertex_id]])
{
  device const float *position =
      reinterpret_cast<device const float *>(vertices + ulong(vertexID) * params.stride);
  float4 value = float4(position[0], position[1], 0.0, 1.0);
  if(params.componentCount >= 3)
    value.z = position[2];
  if(params.componentCount >= 4)
    value.w = position[3];

  VSOut output;
  output.position = params.mvp * value;
  output.uv = float2(0.0);
  return output;
}

fragment float4 rdoc_mesh_fs(VSOut input [[stage_in]],
                             constant MeshParams &params [[buffer(1)]])
{
  return params.color;
}
)EOSHADER";

  NS::Error *error = NULL;
  MTL::Library *library =
      device->newLibrary(NS::String::string(source, NS::UTF8StringEncoding), NULL, &error);
  if(library == NULL)
  {
    RDCERR("Failed to compile Metal replay output shaders: %s",
           error ? error->localizedDescription()->utf8String() : "unknown error");
    return false;
  }

  MTL::Function *vertex =
      library->newFunction(NS::String::string("rdoc_display_vs", NS::UTF8StringEncoding));
  MTL::Function *fragment =
      library->newFunction(NS::String::string("rdoc_display_fs", NS::UTF8StringEncoding));
  MTL::RenderPipelineDescriptor *descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
  descriptor->setVertexFunction(vertex);
  descriptor->setFragmentFunction(fragment);
  MTL::RenderPipelineColorAttachmentDescriptor *colour = descriptor->colorAttachments()->object(0);
  colour->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  colour->setBlendingEnabled(true);
  colour->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
  colour->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
  colour->setRgbBlendOperation(MTL::BlendOperationAdd);
  colour->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
  colour->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
  colour->setAlphaBlendOperation(MTL::BlendOperationAdd);

  if(m_OutputPipeline == NULL)
    m_OutputPipeline = device->newRenderPipelineState(descriptor, &error);
  descriptor->release();

  MTL::Function *meshVertex =
      library->newFunction(NS::String::string("rdoc_mesh_vs", NS::UTF8StringEncoding));
  MTL::Function *meshFragment =
      library->newFunction(NS::String::string("rdoc_mesh_fs", NS::UTF8StringEncoding));
  MTL::RenderPipelineDescriptor *meshDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
  meshDescriptor->setVertexFunction(meshVertex);
  meshDescriptor->setFragmentFunction(meshFragment);
  meshDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  if(m_MeshPipeline == NULL)
    m_MeshPipeline = device->newRenderPipelineState(meshDescriptor, &error);
  meshDescriptor->release();
  if(meshFragment)
    meshFragment->release();
  if(meshVertex)
    meshVertex->release();
  if(fragment)
    fragment->release();
  if(vertex)
    vertex->release();
  library->release();

  if(m_OutputPipeline == NULL)
  {
    RDCERR("Failed to create Metal replay output pipeline: %s",
           error ? error->localizedDescription()->utf8String() : "unknown error");
    return false;
  }
  if(m_MeshPipeline == NULL)
  {
    RDCERR("Failed to create Metal replay mesh pipeline: %s",
           error ? error->localizedDescription()->utf8String() : "unknown error");
    return false;
  }

  return true;
}

bool MetalReplay::ResizeOutputWindow(OutputWindow &output, int32_t width, int32_t height)
{
  MTL::Device *device = Unwrap(m_pDriver);
  if(device == NULL || width <= 0 || height <= 0)
    return false;

  MTL::TextureDescriptor *descriptor = MTL::TextureDescriptor::texture2DDescriptor(
      MTL::PixelFormatBGRA8Unorm, (NS::UInteger)width, (NS::UInteger)height, false);
  descriptor->setStorageMode(MTL::StorageModePrivate);
  descriptor->setUsage(
      (MTL::TextureUsage)(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead));
  MTL::Texture *texture = device->newTexture(descriptor);
  if(texture == NULL)
  {
    RDCERR("Failed to create %dx%d Metal replay output texture", width, height);
    return false;
  }

  if(output.texture)
    output.texture->release();
  output.texture = texture;
  output.width = width;
  output.height = height;
  return true;
}

bool MetalReplay::RenderTextureInternal(MTL::Texture *source, MTL::Texture *target,
                                        TextureDisplay cfg, MTL::LoadAction loadAction,
                                        MTL::ClearColor clearColor)
{
  if(source == NULL || target == NULL || !InitialiseOutputResources())
    return false;

  const MTL::TextureType textureType = source->textureType();
  const bool supportedType = textureType == MTL::TextureType2D ||
                             textureType == MTL::TextureType2DArray ||
                             textureType == MTL::TextureTypeCube;
  const uint32_t sliceCount = textureType == MTL::TextureTypeCube
                                  ? 6U
                                  : (uint32_t)source->arrayLength();
  if(!supportedType || source->sampleCount() != 1 ||
     cfg.subresource.slice >= sliceCount ||
     cfg.subresource.mip >= source->mipmapLevelCount())
  {
    RDCERR("Metal texture display does not support type %s or subresource mip %u slice %u",
           ToStr(textureType).c_str(), cfg.subresource.mip, cfg.subresource.slice);
    return false;
  }

  struct DisplayParams
  {
    float outputSize[2];
    float textureSize[2];
    float offset[2];
    float scale;
    uint32_t mip;
    uint32_t channelMask;
    uint32_t flipY;
    uint32_t rawOutput;
    float rangeMin;
    float inverseRange;
    uint32_t slice;
    uint32_t textureType;
  } params = {};

  const uint32_t mip = cfg.subresource.mip;
  const float textureWidth = (float)RDCMAX(1ULL, source->width() >> mip);
  const float textureHeight = (float)RDCMAX(1ULL, source->height() >> mip);
  params.outputSize[0] = (float)target->width();
  params.outputSize[1] = (float)target->height();
  params.textureSize[0] = textureWidth;
  params.textureSize[1] = textureHeight;
  params.scale = cfg.scale;
  params.offset[0] = cfg.xOffset;
  params.offset[1] = cfg.yOffset;
  if(params.scale <= 0.0f)
  {
    params.scale = RDCMIN(params.outputSize[0] / textureWidth, params.outputSize[1] / textureHeight);
    params.offset[0] = (params.outputSize[0] - textureWidth * params.scale) * 0.5f;
    params.offset[1] = (params.outputSize[1] - textureHeight * params.scale) * 0.5f;
  }
  params.mip = mip;
  params.channelMask =
      (cfg.red ? 1U : 0U) | (cfg.green ? 2U : 0U) | (cfg.blue ? 4U : 0U) | (cfg.alpha ? 8U : 0U);
  params.flipY = cfg.flipY ? 1U : 0U;
  params.rawOutput = cfg.rawOutput ? 1U : 0U;
  params.rangeMin = cfg.rangeMin;
  params.inverseRange = cfg.rangeMax > cfg.rangeMin ? 1.0f / (cfg.rangeMax - cfg.rangeMin) : 1.0f;
  params.slice = cfg.subresource.slice;
  params.textureType = textureType == MTL::TextureType2DArray ? 1U
                       : textureType == MTL::TextureTypeCube   ? 2U
                                                              : 0U;

  MTL::CommandBuffer *commandBuffer = m_OutputQueue->commandBuffer();
  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor *colour = pass->colorAttachments()->object(0);
  colour->setTexture(target);
  colour->setLoadAction(loadAction);
  colour->setStoreAction(MTL::StoreActionStore);
  colour->setClearColor(clearColor);

  MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
  encoder->setRenderPipelineState(m_OutputPipeline);
  encoder->setVertexBytes(&params, sizeof(params), 0);
  encoder->setFragmentBytes(&params, sizeof(params), 0);
  encoder->setFragmentTexture(source, params.textureType);
  encoder->drawPrimitives(MTL::PrimitiveTypeTriangleStrip, NS::UInteger(0), NS::UInteger(4));
  encoder->endEncoding();
  commandBuffer->commit();
  commandBuffer->waitUntilCompleted();
  return true;
}

bool MetalReplay::RenderTexture(TextureDisplay cfg)
{
  auto output = m_OutputWindows.find(m_ActiveOutputWindowID);
  if(output == m_OutputWindows.end() || output->second.texture == NULL)
    return false;

  TextureDescription desc = GetTexture(cfg.resourceId);
  if(desc.resourceId == ResourceId())
    return false;

  WrappedMTLObject *resource = m_pDriver->GetResourceManager()->GetResource(cfg.resourceId, true);
  WrappedMTLTexture *wrappedTexture = (WrappedMTLTexture *)resource;
  MTL::Texture *texture = wrappedTexture ? Unwrap(wrappedTexture) : NULL;
  return RenderTextureInternal(texture, output->second.texture, cfg, MTL::LoadActionLoad,
                               MTL::ClearColor::Make(0.0, 0.0, 0.0, 0.0));
}

void MetalReplay::RenderMesh(uint32_t eventId, const rdcarray<MeshFormat> &secondaryDraws,
                             const MeshDisplay &cfg)
{
  auto output = m_OutputWindows.find(m_ActiveOutputWindowID);
  if(output == m_OutputWindows.end() || output->second.texture == NULL ||
     !InitialiseOutputResources())
    return;

  const MeshFormat &position = cfg.position;
  if(cfg.type != MeshDataStage::VSIn || position.vertexResourceId == ResourceId() ||
     position.numIndices == 0)
    return;
  if(position.format.type != ResourceFormatType::Regular ||
     position.format.compType != CompType::Float || position.format.compByteWidth != 4 ||
     position.format.compCount < 2 || position.format.compCount > 4)
  {
    RDCERR("Metal mesh preview currently supports only Float2/Float3/Float4 VS input positions");
    return;
  }

  MTL::PrimitiveType primitive = MTL::PrimitiveTypeTriangle;
  switch(position.topology)
  {
    case Topology::PointList: primitive = MTL::PrimitiveTypePoint; break;
    case Topology::LineList: primitive = MTL::PrimitiveTypeLine; break;
    case Topology::LineStrip: primitive = MTL::PrimitiveTypeLineStrip; break;
    case Topology::TriangleList: primitive = MTL::PrimitiveTypeTriangle; break;
    case Topology::TriangleStrip: primitive = MTL::PrimitiveTypeTriangleStrip; break;
    default:
      RDCERR("Metal mesh preview does not support topology %s", ToStr(position.topology).c_str());
      return;
  }

  if(GetBuffer(position.vertexResourceId).resourceId == ResourceId())
    return;
  WrappedMTLObject *vertexResource =
      m_pDriver->GetResourceManager()->GetResource(position.vertexResourceId, true);
  MTL::Buffer *vertexBuffer = vertexResource ? Unwrap((WrappedMTLBuffer *)vertexResource) : NULL;
  if(vertexBuffer == NULL || position.vertexByteOffset >= vertexBuffer->length())
    return;

  MTL::Buffer *indexBuffer = NULL;
  if(position.indexResourceId != ResourceId())
  {
    if(GetBuffer(position.indexResourceId).resourceId == ResourceId())
      return;
    WrappedMTLObject *indexResource =
        m_pDriver->GetResourceManager()->GetResource(position.indexResourceId, true);
    indexBuffer = indexResource ? Unwrap((WrappedMTLBuffer *)indexResource) : NULL;
    if(indexBuffer == NULL || position.indexByteOffset >= indexBuffer->length() ||
       (position.indexByteStride != 2 && position.indexByteStride != 4))
      return;
  }

  Matrix4f mvp = Matrix4f::Identity();
  if(cfg.cam)
  {
    Camera *camera = (Camera *)cfg.cam;
    const float aspect = output->second.height > 0
                             ? float(output->second.width) / float(output->second.height)
                             : 1.0f;
    mvp = Matrix4f::Perspective(cfg.fov, camera->GetNear(), camera->GetFar(), aspect)
              .Mul(camera->GetMatrix());
    if(!position.unproject)
      mvp = mvp.Mul(Matrix4f(cfg.axisMapping));
  }

  struct MeshParams
  {
    float mvp[16];
    uint32_t stride;
    uint32_t componentCount;
    uint32_t padding[2];
    float color[4];
  } params = {};
  memcpy(params.mvp, mvp.Data(), sizeof(params.mvp));
  params.stride = position.vertexByteStride ? position.vertexByteStride
                                             : position.format.compByteWidth *
                                                   position.format.compCount;
  params.componentCount = position.format.compCount;
  params.color[0] = position.meshColor.x;
  params.color[1] = position.meshColor.y;
  params.color[2] = position.meshColor.z;
  params.color[3] = position.meshColor.w > 0.0f ? position.meshColor.w : 1.0f;
  if(params.color[0] == 0.0f && params.color[1] == 0.0f && params.color[2] == 0.0f)
  {
    params.color[0] = 0.95f;
    params.color[1] = 0.75f;
    params.color[2] = 0.15f;
  }

  MTL::CommandBuffer *commandBuffer = m_OutputQueue->commandBuffer();
  MTL::RenderPassDescriptor *pass = MTL::RenderPassDescriptor::renderPassDescriptor();
  MTL::RenderPassColorAttachmentDescriptor *colour = pass->colorAttachments()->object(0);
  colour->setTexture(output->second.texture);
  colour->setLoadAction(MTL::LoadActionLoad);
  colour->setStoreAction(MTL::StoreActionStore);

  MTL::RenderCommandEncoder *encoder = commandBuffer->renderCommandEncoder(pass);
  encoder->setRenderPipelineState(m_MeshPipeline);
  encoder->setViewport(MTL::Viewport{0.0, 0.0, (double)output->second.width,
                                     (double)output->second.height, 0.0, 1.0});
  encoder->setCullMode(MTL::CullModeNone);
  if(cfg.wireframeDraw &&
     (primitive == MTL::PrimitiveTypeTriangle || primitive == MTL::PrimitiveTypeTriangleStrip))
    encoder->setTriangleFillMode(MTL::TriangleFillModeLines);
  encoder->setVertexBuffer(vertexBuffer, (NS::UInteger)position.vertexByteOffset, 0);
  encoder->setVertexBytes(&params, sizeof(params), 1);
  encoder->setFragmentBytes(&params, sizeof(params), 1);

  if(indexBuffer)
  {
    const MTL::IndexType indexType = position.indexByteStride == 2 ? MTL::IndexTypeUInt16
                                                                   : MTL::IndexTypeUInt32;
    encoder->drawIndexedPrimitives(primitive, position.numIndices, indexType, indexBuffer,
                                   (NS::UInteger)position.indexByteOffset, 1,
                                   (NS::Integer)position.baseVertex, 0);
  }
  else
  {
    encoder->drawPrimitives(primitive, NS::UInteger(0), NS::UInteger(position.numIndices));
  }

  encoder->endEncoding();
  commandBuffer->commit();
  commandBuffer->waitUntilCompleted();
}

void MetalReplay::RenderCheckerboard(FloatVector dark, FloatVector light)
{
  // The first output implementation uses a solid dark tile until the checkerboard shader is
  // added. Clearing here is important: pixel-context and empty thumbnail windows otherwise expose
  // undefined private-texture contents.
  ClearOutputWindowColor(m_ActiveOutputWindowID, dark);
}

bool MetalReplay::GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast,
                            float *minval, float *maxval)
{
  *minval = 0.0f;
  *maxval = 1.0f;
  return false;
}

bool MetalReplay::GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast,
                               float minval, float maxval, const rdcfixedarray<bool, 4> &channels,
                               rdcarray<uint32_t> &histogram)
{
  histogram.clear();
  return false;
}

void MetalReplay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, const Subresource &sub,
                            CompType typeCast, float pixel[4])
{
  pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0.0f;

  if(GetTexture(texture).resourceId == ResourceId())
  {
    RDCERR("Metal pixel picking requested for non-texture resource %s", ToStr(texture).c_str());
    return;
  }

  WrappedMTLObject *resource = m_pDriver->GetResourceManager()->GetResource(texture, true);
  WrappedMTLTexture *wrappedTexture = (WrappedMTLTexture *)resource;
  MTL::Texture *real = wrappedTexture ? Unwrap(wrappedTexture) : NULL;
  if(real == NULL || sub.mip >= real->mipmapLevelCount())
    return;

  const uint32_t width = RDCMAX(1U, (uint32_t)real->width() >> sub.mip);
  const uint32_t height = RDCMAX(1U, (uint32_t)real->height() >> sub.mip);
  if(x >= width || y >= height)
    return;

  bytebuf data;
  if(!ReadTextureSubresource(real, sub, data))
    return;

  const byte *value = data.data() + (size_t(y) * width + x) * 4;
  switch(real->pixelFormat())
  {
    case MTL::PixelFormatRGBA8Unorm:
    case MTL::PixelFormatRGBA8Unorm_sRGB:
      pixel[0] = float(value[0]) / 255.0f;
      pixel[1] = float(value[1]) / 255.0f;
      pixel[2] = float(value[2]) / 255.0f;
      pixel[3] = float(value[3]) / 255.0f;
      break;
    case MTL::PixelFormatBGRA8Unorm:
    case MTL::PixelFormatBGRA8Unorm_sRGB:
      pixel[0] = float(value[2]) / 255.0f;
      pixel[1] = float(value[1]) / 255.0f;
      pixel[2] = float(value[0]) / 255.0f;
      pixel[3] = float(value[3]) / 255.0f;
      break;
    default:
      RDCERR("Metal pixel picking does not yet support format %s",
             ToStr(real->pixelFormat()).c_str());
      break;
  }
}

void MetalReplay::BuildCustomShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                    const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                    ShaderStage type, ResourceId &id, rdcstr &errors)
{
  id = ResourceId();
  errors = "Metal custom shaders are not implemented.";
}

RDResult Metal_CreateReplayDevice(RDCFile *rdc, const ReplayOptions &opts, IReplayDriver **driver)
{
  if(!driver)
    return ResultCode::InvalidParameter;

  *driver = NULL;

  MetalInitParams initParams;
  uint64_t version = MetalInitParams::CurrentVersion;

  if(rdc)
  {
    int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);
    if(sectionIdx < 0)
      RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

    version = rdc->GetSectionProperties(sectionIdx).version;
    if(version != MetalInitParams::CurrentVersion)
      RETURN_ERROR_RESULT(ResultCode::APIIncompatibleVersion,
                          "Metal capture version %llu is not supported; expected %llu", version,
                          MetalInitParams::CurrentVersion);

    StreamReader *reader = rdc->ReadSection(sectionIdx);
    ReadSerialiser ser(reader, Ownership::Stream);
    ser.SetVersion(version);

    SystemChunk chunk = ser.ReadChunk<SystemChunk>();
    if(chunk != SystemChunk::DriverInit)
      RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "Expected a DriverInit chunk, instead got %u",
                          chunk);

    SERIALISE_ELEMENT(initParams);
    if(ser.IsErrored())
      return ser.GetError();
  }

  MTL::Device *realDevice = MTL::CreateSystemDefaultDevice();
  if(!realDevice)
    RETURN_ERROR_RESULT(ResultCode::APIInitFailed, "Failed to create the default Metal device");

  ResourceId deviceId = initParams.DeviceID;
  if(deviceId == ResourceId())
    deviceId = ResourceIDGen::GetNewUniqueID();

  WrappedMTLDevice *wrappedDevice = new WrappedMTLDevice(realDevice, deviceId);
  wrappedDevice->SetReplayVersion(version);
  MetalReplay *replay = wrappedDevice->GetReplay();
  replay->SetProxy(rdc == NULL);
  *driver = replay;
  return ResultCode::Succeeded;
}

static DriverRegistration MetalDriverRegistration(RDCDriver::Metal, &Metal_CreateReplayDevice);

RDResult Metal_ProcessStructured(RDCFile *rdc, SDFile &output)
{
  WrappedMTLDevice device(NULL, ResourceIDGen::GetNewUniqueID());

  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);
  if(sectionIdx < 0)
    RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

  device.SetStructuredExport(rdc->GetSectionProperties(sectionIdx).version);
  RDResult status = device.ReadLogInitialisation(rdc, true);

  if(status == ResultCode::Succeeded)
    device.GetStructuredFile()->Swap(output);

  return status;
}

static StructuredProcessRegistration MetalProcessRegistration(RDCDriver::Metal,
                                                              &Metal_ProcessStructured);
