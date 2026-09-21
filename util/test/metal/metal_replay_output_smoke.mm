/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
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

#include <fstream>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "renderdoc/api/replay/renderdoc_replay.h"

template <>
rdcstr DoStringise(const uint32_t &value)
{
  char buffer[16] = {};
  snprintf(buffer, sizeof(buffer), "%u", value);
  return buffer;
}

#include "renderdoc/api/replay/pipestate.inl"
#include "renderdoc/driver/metal/official/metal-cpp.h"

REPLAY_PROGRAM_MARKER()

static const ActionDescription *FindAction(const rdcarray<ActionDescription> &actions,
                                           ActionFlags flags)
{
  for(const ActionDescription &action : actions)
  {
    if(action.flags & flags)
      return &action;
    if(const ActionDescription *child = FindAction(action.children, flags))
      return child;
  }
  return NULL;
}

static void FindDrawActions(const rdcarray<ActionDescription> &actions,
                            rdcarray<const ActionDescription *> &draws)
{
  for(const ActionDescription &action : actions)
  {
    if(action.flags & ActionFlags::Drawcall)
      draws.push_back(&action);
    FindDrawActions(action.children, draws);
  }
}

static bool WriteOutput(IReplayController *renderer, IReplayOutput *output, uint32_t eventId,
                        const char *path)
{
  renderer->SetFrameEvent(eventId, true);
  output->Display();
  bytebuf pixels = output->ReadbackOutputTexture();

  std::ofstream ppm(path, std::ios::binary);
  ppm << "P6\n640 480\n255\n";
  ppm.write((const char *)pixels.data(), pixels.size());
  ppm.close();
  return pixels.size() == 640 * 480 * 3;
}

static bool Near(float actual, float expected)
{
  return fabsf(actual - expected) <= 1.5f / 255.0f;
}

static bool ValidateShaders(IReplayController *renderer)
{
  bool vertexFound = false;
  bool fragmentFound = false;
  uint32_t shaderCount = 0;

  for(const ResourceDescription &resource : renderer->GetResources())
  {
    if(resource.type != ResourceType::Shader)
      continue;

    shaderCount++;
    rdcarray<ShaderEntryPoint> entries = renderer->GetShaderEntryPoints(resource.resourceId);
    if(entries.size() != 1)
      return false;

    const ShaderReflection *reflection =
        renderer->GetShader(ResourceId(), resource.resourceId, entries[0]);
    if(reflection == NULL || reflection->encoding != ShaderEncoding::MSL ||
       reflection->debugInfo.encoding != ShaderEncoding::MSL ||
       reflection->debugInfo.files.size() != 1 ||
       !reflection->debugInfo.files[0].contents.contains("#include <metal_stdlib>") ||
       !reflection->debugInfo.files[0].contents.contains("vertex VSOut vs_main") ||
       !reflection->debugInfo.files[0].contents.contains("fragment float4 fs_main"))
      return false;

    if(entries[0] == ShaderEntryPoint("vs_main", ShaderStage::Vertex))
      vertexFound = true;
    else if(entries[0] == ShaderEntryPoint("fs_main", ShaderStage::Fragment))
      fragmentFound = true;
    else
      return false;
  }

  return shaderCount == 2 && vertexFound && fragmentFound;
}

static ResourceType GetResourceType(IReplayController *renderer, ResourceId id)
{
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.resourceId == id)
      return resource.type;
  return ResourceType::Unknown;
}

static bool PipelineFailure(const char *message)
{
  fprintf(stderr, "Metal pipeline state validation failed: %s\n", message);
  return false;
}

static bool ValidatePipelineState(IReplayController *renderer, ResourceId colorTarget,
                                  uint32_t clearEvent, uint32_t drawEvent)
{
  renderer->SetFrameEvent(clearEvent, true);
  const PipeState &clear = renderer->GetPipelineState();
  if(!clear.IsCaptureMetal())
    return PipelineFailure("clear event is not reported as Metal");

  rdcarray<Descriptor> clearTargets = clear.GetOutputTargets();
  if(clearTargets.empty())
    return PipelineFailure("clear event has no color target");
  if(clearTargets[0].resource != colorTarget)
    return PipelineFailure("clear event color target does not match the swapbuffer");

  renderer->SetFrameEvent(drawEvent, true);
  const PipeState &draw = renderer->GetPipelineState();
  const ResourceId pipeline = draw.GetGraphicsPipelineObject();
  const ResourceId vertexShader = draw.GetShader(ShaderStage::Vertex);
  const ResourceId fragmentShader = draw.GetShader(ShaderStage::Fragment);
  const rdcarray<BoundVBuffer> vertexBuffers = draw.GetVBuffers();
  const rdcarray<Descriptor> colorTargets = draw.GetOutputTargets();

  if(!draw.IsCaptureMetal())
    return PipelineFailure("draw event is not reported as Metal");
  if(pipeline == ResourceId() ||
     GetResourceType(renderer, pipeline) != ResourceType::PipelineState)
    return PipelineFailure("draw event has no valid render pipeline");
  if(vertexShader == ResourceId() || fragmentShader == ResourceId())
    return PipelineFailure("draw event is missing a shader");
  if(draw.GetShaderEntryPoint(ShaderStage::Vertex) != "vs_main" ||
     draw.GetShaderEntryPoint(ShaderStage::Fragment) != "fs_main")
    return PipelineFailure("draw event shader entry points do not match");
  if(draw.GetShaderReflection(ShaderStage::Vertex) == NULL ||
     draw.GetShaderReflection(ShaderStage::Fragment) == NULL)
    return PipelineFailure("draw event is missing shader reflection");
  if(draw.GetPrimitiveTopology() != Topology::TriangleList)
    return PipelineFailure("draw event topology is not TriangleList");
  if(vertexBuffers.size() != 1)
    return PipelineFailure("draw event does not have exactly one vertex buffer");
  if(GetResourceType(renderer, vertexBuffers[0].resourceId) != ResourceType::Buffer)
    return PipelineFailure("draw event vertex buffer is not a buffer resource");
  if(vertexBuffers[0].byteOffset != 0 || vertexBuffers[0].byteSize != 96)
    return PipelineFailure("draw event vertex buffer range does not match");
  if(colorTargets.empty() || colorTargets[0].resource != colorTarget)
    return PipelineFailure("draw event color target does not match the swapbuffer");

  return true;
}

static bool ValidateTextureData(IReplayController *renderer, ResourceId texture,
                                const TextureDescription &desc, uint32_t eventId,
                                bool expectBackgroundAtCentre)
{
  renderer->SetFrameEvent(eventId, true);

  const Subresource sub = {0, 0, 0};
  bytebuf data = renderer->GetTextureData(texture, sub);
  if(desc.width != 400 || desc.height != 300 || data.size() != size_t(400 * 300 * 4))
    return false;

  const size_t centre = (size_t(desc.height / 2) * desc.width + desc.width / 2) * 4;
  const bool centreIsBackground = data[centre + 0] == 0x1a && data[centre + 1] == 0x14 &&
                                  data[centre + 2] == 0x14 && data[centre + 3] == 0xff;
  if(centreIsBackground != expectBackgroundAtCentre)
    return false;

  PixelValue background = renderer->PickPixel(texture, 10, 10, sub, CompType::Typeless);
  if(!Near(background.floatValue[0], 0.08f) || !Near(background.floatValue[1], 0.08f) ||
     !Near(background.floatValue[2], 0.10f) || !Near(background.floatValue[3], 1.0f))
    return false;

  PixelValue picked =
      renderer->PickPixel(texture, desc.width / 2, desc.height / 2, sub, CompType::Typeless);
  const bool pickedIsBackground = Near(picked.floatValue[0], 0.08f) &&
                                  Near(picked.floatValue[1], 0.08f) &&
                                  Near(picked.floatValue[2], 0.10f) &&
                                  Near(picked.floatValue[3], 1.0f);
  return pickedIsBackground == expectBackgroundAtCentre;
}

static bool ValidateTextureSave(IReplayController *renderer, ResourceId texture,
                                uint32_t eventId, const char *path)
{
  renderer->SetFrameEvent(eventId, true);

  TextureSave save;
  save.resourceId = texture;
  save.destType = FileType::DDS;
  save.mip = 0;
  save.slice.sliceIndex = 0;
  ResultDetails result = renderer->SaveTexture(save, path);
  if(!result.OK())
    return false;

  std::ifstream file(path, std::ios::binary | std::ios::ate);
  return file && file.tellg() > 128;
}

static bool ValidateTexturedFixture(IReplayController *renderer)
{
  TextureDescription sampledTexture;
  for(const TextureDescription &texture : renderer->GetTextures())
  {
    if(!(texture.creationFlags & TextureCategory::SwapBuffer) && texture.width == 4 &&
       texture.height == 4 && texture.depth == 1 && texture.mips == 1 && texture.arraysize == 1)
    {
      sampledTexture = texture;
      break;
    }
  }

  // Other fixtures do not contain T03's 4x4 texture.
  if(sampledTexture.resourceId == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal textured fixture validation failed: %s\n", message);
    return false;
  };

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 1 || (draws[0]->flags & ActionFlags::Indexed) || draws[0]->numIndices != 4 ||
     draws[0]->numInstances != 1)
    return fail("draw action does not match the four-vertex triangle strip");

  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  if(pipe.GetPrimitiveTopology() != Topology::TriangleStrip)
    return fail("pipeline topology is not TriangleStrip");

  const MetalPipe::State *metal = pipe.GetMetalPipelineState();
  if(metal == NULL || metal->fragmentTextures.size() != 2 ||
     metal->fragmentTextures[0] != sampledTexture.resourceId ||
     metal->fragmentTextures[1] != sampledTexture.resourceId ||
     metal->fragmentSamplers.size() != 2 || metal->fragmentSamplers[0] == ResourceId() ||
     metal->fragmentSamplers[1] != metal->fragmentSamplers[0])
    return fail("Metal fragment texture/sampler slots do not match");

  const rdcarray<UsedDescriptor> resources =
      pipe.GetReadOnlyResources(ShaderStage::Fragment, false);
  const rdcarray<UsedDescriptor> samplers = pipe.GetSamplers(ShaderStage::Fragment, false);
  const rdcarray<UsedDescriptor> usedResources =
      pipe.GetReadOnlyResources(ShaderStage::Fragment, true);
  const rdcarray<UsedDescriptor> usedSamplers = pipe.GetSamplers(ShaderStage::Fragment, true);
  if(resources.size() != 2 || resources[0].access.index != 0 ||
     resources[0].access.type != DescriptorType::Image ||
     resources[0].access.staticallyUnused ||
     resources[0].descriptor.resource != sampledTexture.resourceId ||
     resources[0].descriptor.type != DescriptorType::Image ||
     resources[0].descriptor.textureType != TextureType::Texture2D ||
     resources[1].access.index != DescriptorAccess::NoShaderBinding ||
     !resources[1].access.staticallyUnused ||
     resources[1].descriptor.resource != sampledTexture.resourceId || usedResources.size() != 1)
    return fail("generic fragment texture used/unused descriptors do not match slots 0/1");
  if(samplers.size() != 2 || samplers[0].access.index != 0 ||
     samplers[0].access.type != DescriptorType::Sampler ||
     samplers[0].access.staticallyUnused ||
     samplers[0].sampler.object != metal->fragmentSamplers[0] ||
     samplers[0].sampler.type != DescriptorType::Sampler ||
     samplers[0].sampler.filter.minify != FilterMode::Point ||
     samplers[0].sampler.filter.magnify != FilterMode::Point ||
     samplers[0].sampler.filter.mip != FilterMode::NoFilter ||
     samplers[0].sampler.addressU != AddressMode::ClampEdge ||
     samplers[0].sampler.addressV != AddressMode::ClampEdge ||
     samplers[0].sampler.addressW != AddressMode::ClampEdge ||
     samplers[1].access.index != DescriptorAccess::NoShaderBinding ||
     !samplers[1].access.staticallyUnused ||
     samplers[1].sampler.object != metal->fragmentSamplers[1] || usedSamplers.size() != 1)
    return fail("generic fragment sampler used/unused descriptors do not match slots 0/1");

  const ShaderReflection *fragmentReflection = pipe.GetShaderReflection(ShaderStage::Fragment);
  if(fragmentReflection == NULL || fragmentReflection->readOnlyResources.size() != 1 ||
     fragmentReflection->samplers.size() != 1 ||
     fragmentReflection->readOnlyResources[0].name != "colourTexture" ||
     fragmentReflection->readOnlyResources[0].fixedBindNumber != 0 ||
     fragmentReflection->readOnlyResources[0].descriptorType != DescriptorType::Image ||
     fragmentReflection->readOnlyResources[0].textureType != TextureType::Texture2D ||
     !fragmentReflection->readOnlyResources[0].isTexture ||
     !fragmentReflection->readOnlyResources[0].isReadOnly ||
     fragmentReflection->samplers[0].name != "colourSampler" ||
     fragmentReflection->samplers[0].fixedBindNumber != 0)
    return fail("fragment shader resource binding reflection does not match MSL arguments");

  const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
  const rdcarray<BoundVBuffer> vertexBuffers = pipe.GetVBuffers();
  if(inputs.size() != 2 || vertexBuffers.size() != 1 || inputs[0].byteOffset != 0 ||
     inputs[0].format.compType != CompType::Float || inputs[0].format.compByteWidth != 4 ||
     inputs[0].format.compCount != 2 || inputs[1].byteOffset != 8 ||
     inputs[1].format.compType != CompType::Float || inputs[1].format.compByteWidth != 4 ||
     inputs[1].format.compCount != 2 || vertexBuffers[0].byteOffset != 0 ||
     vertexBuffers[0].byteSize != 64 || vertexBuffers[0].byteStride != 16)
    return fail("Float2 position/UV vertex input does not match");

  static const byte expected[] = {
      255, 32, 16, 255, 255, 32, 16, 255, 16, 224, 48, 255, 16, 224, 48, 255,
      255, 32, 16, 255, 255, 32, 16, 255, 16, 224, 48, 255, 16, 224, 48, 255,
      24, 64, 255, 255, 24, 64, 255, 255, 240, 208, 32, 255, 240, 208, 32, 255,
      24, 64, 255, 255, 24, 64, 255, 255, 240, 208, 32, 255, 240, 208, 32, 255,
  };
  const Subresource sub = {0, 0, 0};
  const bytebuf data = renderer->GetTextureData(sampledTexture.resourceId, sub);
  if(data.size() != sizeof(expected) || memcmp(data.data(), expected, sizeof(expected)) != 0)
    return fail("captured RGBA8 texels do not match the upload");

  const PixelValue red =
      renderer->PickPixel(sampledTexture.resourceId, 0, 0, sub, CompType::Typeless);
  const PixelValue yellow =
      renderer->PickPixel(sampledTexture.resourceId, 3, 3, sub, CompType::Typeless);
  if(!Near(red.floatValue[0], 1.0f) || !Near(red.floatValue[1], 32.0f / 255.0f) ||
     !Near(red.floatValue[2], 16.0f / 255.0f) || !Near(red.floatValue[3], 1.0f) ||
     !Near(yellow.floatValue[0], 240.0f / 255.0f) ||
     !Near(yellow.floatValue[1], 208.0f / 255.0f) ||
     !Near(yellow.floatValue[2], 32.0f / 255.0f) || !Near(yellow.floatValue[3], 1.0f))
    return fail("PickPixel did not preserve RGBA channel order");

  size_t samplerCount = 0;
  for(const ResourceDescription &resource : renderer->GetResources())
    if(resource.type == ResourceType::Sampler)
      samplerCount++;
  if(samplerCount != 1)
    return fail("capture does not expose exactly one sampler resource");

  return true;
}

static bool T02PixelIsBackground(const bytebuf &data, uint32_t width, uint32_t x, uint32_t y)
{
  const size_t offset = (size_t(y) * width + x) * 4;
  return offset + 3 < data.size() && data[offset + 0] == 0x0e && data[offset + 1] == 0x09 &&
         data[offset + 2] == 0x06 && data[offset + 3] == 0xff;
}

static bool ValidateIndexedEventImage(IReplayController *renderer, ResourceId texture,
                                      const TextureDescription &desc, uint32_t eventId,
                                      bool expectLeftBackground, bool expectRightBackground)
{
  renderer->SetFrameEvent(eventId, true);
  bytebuf data = renderer->GetTextureData(texture, {0, 0, 0});
  if(desc.width != 400 || desc.height != 300 || data.size() != size_t(400 * 300 * 4))
    return false;

  const bool leftBackground = T02PixelIsBackground(data, desc.width, 100, 150);
  const bool rightBackground = T02PixelIsBackground(data, desc.width, 300, 150);
  return leftBackground == expectLeftBackground && rightBackground == expectRightBackground;
}

static bool ValidateIndexedFixture(IReplayController *renderer, ResourceId colorTarget,
                                   const TextureDescription &desc)
{
  auto fail = [](const char *message) {
    fprintf(stderr, "Metal indexed fixture validation failed: %s\n", message);
    return false;
  };

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.empty() || !(draws[0]->flags & ActionFlags::Indexed))
    return true;

  if(draws.size() != 2)
    return fail("expected two draw actions");

  for(size_t drawIndex = 0; drawIndex < draws.size(); drawIndex++)
  {
    const ActionDescription *draw = draws[drawIndex];
    if(!(draw->flags & ActionFlags::Indexed) || draw->numIndices != 36 ||
       draw->numInstances != 1 || draw->indexOffset != 0)
      return fail("indexed action metadata does not match");

    renderer->SetFrameEvent(draw->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    if(pipe.GetPrimitiveTopology() != Topology::TriangleList ||
       pipe.GetDepthTarget().resource == ResourceId())
    {
      fprintf(stderr, "T02 state: topology=%u depthPresent=%d\n",
              (uint32_t)pipe.GetPrimitiveTopology(),
              pipe.GetDepthTarget().resource != ResourceId() ? 1 : 0);
      return fail("indexed action pipeline topology/depth target does not match");
    }

    const MetalPipe::State *state = pipe.GetMetalPipelineState();
    if(state == NULL)
      return fail("Metal pipeline state is unavailable");
    if(state->vertexAttributes.size() != 2 || state->vertexBuffers.size() != 1)
      return fail("vertex descriptor shape does not match");
    if(state->vertexAttributes[0].attributeIndex != 0 ||
       state->vertexAttributes[0].bufferIndex != 0 ||
       state->vertexAttributes[0].byteOffset != 0 ||
       state->vertexAttributes[0].format.type != ResourceFormatType::Regular ||
       state->vertexAttributes[0].format.compType != CompType::Float ||
       state->vertexAttributes[0].format.compByteWidth != 4 ||
       state->vertexAttributes[0].format.compCount != 3 ||
       state->vertexAttributes[1].attributeIndex != 1 ||
       state->vertexAttributes[1].bufferIndex != 0 ||
       state->vertexAttributes[1].byteOffset != 12 ||
       state->vertexAttributes[1].format.compType != CompType::Float ||
       state->vertexAttributes[1].format.compByteWidth != 4 ||
       state->vertexAttributes[1].format.compCount != 4)
      return fail("vertex attributes do not match Float3/Float4 interleaved input");

    const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
    if(inputs.size() != 2 || inputs[0].name != "attr0" || inputs[0].vertexBuffer != 0 ||
       inputs[0].byteOffset != 0 || inputs[0].perInstance || inputs[0].instanceRate != 1 ||
       inputs[0].format.compType != CompType::Float || inputs[0].format.compByteWidth != 4 ||
       inputs[0].format.compCount != 3 || !inputs[0].used || inputs[0].genericEnabled ||
       inputs[1].name != "attr1" || inputs[1].vertexBuffer != 0 ||
       inputs[1].byteOffset != 12 || inputs[1].perInstance || inputs[1].instanceRate != 1 ||
       inputs[1].format.compType != CompType::Float || inputs[1].format.compByteWidth != 4 ||
       inputs[1].format.compCount != 4 || !inputs[1].used || inputs[1].genericEnabled)
      return fail("generic vertex inputs do not match the Metal descriptor");
    if(state->vertexBuffers[0].resourceId == ResourceId() ||
       state->vertexBuffers[0].byteOffset != 0 || state->vertexBuffers[0].byteSize != 224 ||
       state->vertexBuffers[0].byteStride != 28 || state->vertexBuffers[0].perInstance ||
       state->vertexBuffers[0].stepRate != 1)
      return fail("vertex buffer layout does not match");

    const BoundVBuffer indexBuffer = pipe.GetIBuffer();
    const uint32_t expectedIndexStride = drawIndex == 0 ? 2 : 4;
    const uint64_t expectedIndexSize = drawIndex == 0 ? 72 : 144;
    if(indexBuffer.resourceId == ResourceId() || indexBuffer.byteOffset != 0 ||
       indexBuffer.byteStride != expectedIndexStride || indexBuffer.byteSize != expectedIndexSize)
      return fail("indexed draw buffer binding does not match");

    const DepthTestState depth = pipe.GetDepthTestState();
    if(state->depthStencil.resourceId == ResourceId() || !depth.depthEnable ||
       !depth.depthWrites || depth.depthFunction != CompareFunction::Less)
      return fail("depth state does not match less/write");

    const RasterState raster = pipe.GetRasterState();
    const Viewport viewport = pipe.GetViewport(0);
    const Scissor scissor = pipe.GetScissor(0);
    const float expectedX = drawIndex == 0 ? 0.0f : 200.0f;
    if(raster.cullMode != CullMode::Back || !raster.frontCCW || !viewport.enabled ||
       viewport.x != expectedX || viewport.y != 0.0f || viewport.width != 200.0f ||
       viewport.height != 300.0f || viewport.minDepth != 0.0f || viewport.maxDepth != 1.0f ||
       !scissor.enabled || scissor.x != (int32_t)expectedX || scissor.y != 0 ||
       scissor.width != 200 || scissor.height != 300)
    {
      fprintf(stderr,
              "T02 raster[%zu]: cull=%u ccw=%d vp=%d %.1f %.1f %.1f %.1f %.1f %.1f "
              "sc=%d %d %d %d %d\n",
              drawIndex, (uint32_t)raster.cullMode, raster.frontCCW ? 1 : 0,
              viewport.enabled ? 1 : 0, viewport.x, viewport.y, viewport.width, viewport.height,
              viewport.minDepth, viewport.maxDepth, scissor.enabled ? 1 : 0, scissor.x, scissor.y,
              scissor.width, scissor.height);
      return fail("viewport/scissor/cull/front-face state does not match");
    }
  }

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL)
    return fail("clear action is unavailable");
  if(!ValidateIndexedEventImage(renderer, colorTarget, desc, clear->eventId, true, true))
    return fail("clear event image does not contain two background halves");
  if(!ValidateIndexedEventImage(renderer, colorTarget, desc, draws[0]->eventId, false, true))
    return fail("UInt16 draw event did not update only the left half");
  if(!ValidateIndexedEventImage(renderer, colorTarget, desc, draws[1]->eventId, false, false))
    return fail("UInt32 draw event did not preserve left and update right half");
  if(!ValidateIndexedEventImage(renderer, colorTarget, desc, draws[0]->eventId, false, true))
    return fail("rewinding to the UInt16 draw did not restore its event image");

  static const uint16_t expected16[] = {
      0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 1, 5, 6, 1, 6, 2,
      4, 0, 3, 4, 3, 7, 3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
  };
  static constexpr size_t indexCount = sizeof(expected16) / sizeof(expected16[0]);
  uint32_t expected32[indexCount] = {};
  for(size_t i = 0; i < indexCount; i++)
    expected32[i] = expected16[i];

  bool found16 = false;
  bool found32 = false;
  bool foundVertices = false;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    bytebuf data = renderer->GetBufferData(buffer.resourceId, 0, 0);
    if(buffer.length == sizeof(expected16))
      found16 = data.size() == sizeof(expected16) &&
                memcmp(data.data(), expected16, sizeof(expected16)) == 0;
    else if(buffer.length == sizeof(expected32))
      found32 = data.size() == sizeof(expected32) &&
                memcmp(data.data(), expected32, sizeof(expected32)) == 0;
    else if(buffer.length == 8 * (3 + 4) * sizeof(float))
      foundVertices = data.size() == buffer.length;
  }

  if(!found16)
    return fail("UInt16 index bytes do not match");
  if(!found32)
    return fail("UInt32 index bytes do not match");
  if(!foundVertices)
    return fail("vertex buffer bytes are unavailable");
  return true;
}

static bool ValidateMeshPreview(IReplayController *renderer, WindowingData window)
{
  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 2 || !(draws[0]->flags & ActionFlags::Indexed))
    return true;

  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
  const rdcarray<BoundVBuffer> vertexBuffers = pipe.GetVBuffers();
  const BoundVBuffer indexBuffer = pipe.GetIBuffer();
  if(inputs.empty() || inputs[0].vertexBuffer < 0 ||
     size_t(inputs[0].vertexBuffer) >= vertexBuffers.size())
    return false;

  const BoundVBuffer &vertexBuffer = vertexBuffers[inputs[0].vertexBuffer];
  MeshDisplay mesh;
  mesh.type = MeshDataStage::VSIn;
  mesh.wireframeDraw = true;
  mesh.position.vertexResourceId = vertexBuffer.resourceId;
  mesh.position.vertexByteOffset = vertexBuffer.byteOffset + inputs[0].byteOffset;
  mesh.position.vertexByteStride = vertexBuffer.byteStride;
  mesh.position.vertexByteSize = vertexBuffer.byteSize;
  mesh.position.indexResourceId = indexBuffer.resourceId;
  mesh.position.indexByteOffset = indexBuffer.byteOffset;
  mesh.position.indexByteStride = indexBuffer.byteStride;
  mesh.position.indexByteSize = indexBuffer.byteSize;
  mesh.position.format = inputs[0].format;
  mesh.position.topology = pipe.GetPrimitiveTopology();
  mesh.position.numIndices = draws[0]->numIndices;
  mesh.position.baseVertex = draws[0]->baseVertex;

  IReplayOutput *output = renderer->CreateOutput(window, ReplayOutputType::Mesh);
  if(output == NULL)
    return false;
  output->SetMeshDisplay(mesh);
  output->Display();
  const bytebuf pixels = output->ReadbackOutputTexture();
  output->Shutdown();

  if(pixels.size() != size_t(640 * 480 * 3))
    return false;

  size_t changedPixels = 0;
  for(size_t i = 3; i + 2 < pixels.size(); i += 3)
  {
    if(pixels[i + 0] != pixels[0] || pixels[i + 1] != pixels[1] ||
       pixels[i + 2] != pixels[2])
      changedPixels++;
  }

  if(changedPixels < 100)
  {
    fprintf(stderr, "Metal mesh preview validation failed: only %zu pixels changed\n",
            changedPixels);
    return false;
  }
  return true;
}

int main(int argc, char **argv)
{
  if(argc != 3 && argc != 6)
    return 2;

  GlobalEnvironment env;
  env.enumerateGPUs = false;
  rdcarray<rdcstr> args;
  args.push_back(argv[0]);
  RENDERDOC_InitialiseReplay(env, args);

  ICaptureFile *file = RENDERDOC_OpenCaptureFile();
  ResultDetails result = file->OpenFile(argv[1], "rdc", NULL);
  if(!result.OK())
    return 3;

  IReplayController *renderer = NULL;
  rdctie(result, renderer) = file->OpenCapture(ReplayOptions(), NULL);
  file->Shutdown();
  if(!result.OK() || renderer == NULL)
    return 4;

  TextureDisplay display;
  display.subresource = {0, 0, 0};
  display.scale = -1.0f;
  display.rangeMin = 0.0f;
  display.rangeMax = 1.0f;
  display.red = display.green = display.blue = true;
  display.alpha = false;

  TextureDescription swapBuffer;

  for(const TextureDescription &desc : renderer->GetTextures())
  {
    if(desc.creationFlags & TextureCategory::SwapBuffer)
    {
      display.resourceId = desc.resourceId;
      swapBuffer = desc;
      break;
    }
  }

  if(display.resourceId == ResourceId())
  {
    renderer->Shutdown();
    return 5;
  }

  NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
  CA::MetalLayer *layer = CA::MetalLayer::layer();
  layer->setDrawableSize(CGSizeMake(640, 480));

  WindowingData window = {};
  window.system = WindowingSystem::MacOS;
  window.macOS.layer = layer;

  IReplayOutput *output = renderer->CreateOutput(window, ReplayOutputType::Texture);
  if(output == NULL)
  {
    pool->release();
    renderer->Shutdown();
    return 6;
  }

  output->SetTextureDisplay(display);

  bool success = false;
  if(argc == 3)
  {
    success = ValidateIndexedFixture(renderer, display.resourceId, swapBuffer) &&
              ValidateTexturedFixture(renderer) &&
              ValidateMeshPreview(renderer, window) &&
              WriteOutput(renderer, output, 10000000, argv[2]);
  }
  else
  {
    const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
    const ActionDescription *draw = FindAction(renderer->GetRootActions(), ActionFlags::Drawcall);
    if(!clear || !draw)
    {
      output->Shutdown();
      renderer->Shutdown();
      pool->release();
      RENDERDOC_ShutdownReplay();
      return 8;
    }

    success = ValidateShaders(renderer) &&
              ValidatePipelineState(renderer, swapBuffer.resourceId, clear->eventId,
                                    draw->eventId) &&
              ValidateTextureData(renderer, display.resourceId, swapBuffer, clear->eventId, true) &&
              ValidateTextureData(renderer, display.resourceId, swapBuffer, draw->eventId, false) &&
              ValidateTextureSave(renderer, display.resourceId, draw->eventId, argv[5]);
    for(int i = 0; i < 10 && success; i++)
    {
      success = WriteOutput(renderer, output, clear->eventId, argv[2]) &&
                WriteOutput(renderer, output, draw->eventId, argv[3]) &&
                WriteOutput(renderer, output, clear->eventId, argv[4]);
    }
  }

  output->Shutdown();
  renderer->Shutdown();
  pool->release();
  RENDERDOC_ShutdownReplay();
  return success ? 0 : 7;
}
