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

static bool PixelMatches(IReplayController *renderer, ResourceId texture, uint32_t eventId,
                         uint32_t x, uint32_t y, float r, float g, float b);

static bool ValidateTextureSubresourceFixture(IReplayController *renderer, IReplayOutput *output,
                                              ResourceId colorTarget, const char *savePath)
{
  TextureDescription mipTexture;
  TextureDescription arrayTexture;
  TextureDescription cubeTexture;
  for(const TextureDescription &texture : renderer->GetTextures())
  {
    if(texture.width != 4 || texture.height != 4 || texture.depth != 1 ||
       texture.format.compByteWidth != 1 || texture.format.compCount != 4)
      continue;

    if(texture.type == TextureType::Texture2D && texture.mips == 3 && texture.arraysize == 1)
      mipTexture = texture;
    else if(texture.type == TextureType::Texture2DArray && texture.mips == 1 &&
            texture.arraysize == 3)
      arrayTexture = texture;
    else if(texture.type == TextureType::TextureCube && texture.cubemap && texture.mips == 1 &&
            texture.arraysize == 6)
      cubeTexture = texture;
  }

  // Other fixtures do not contain T09's three distinct 4x4 texture shapes.
  if(mipTexture.resourceId == ResourceId() && arrayTexture.resourceId == ResourceId() &&
     cubeTexture.resourceId == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal texture subresource fixture validation failed: %s\n", message);
    return false;
  };

  if(mipTexture.resourceId == ResourceId() || arrayTexture.resourceId == ResourceId() ||
     cubeTexture.resourceId == ResourceId() || mipTexture.byteSize != 84 ||
     arrayTexture.byteSize != 192 || cubeTexture.byteSize != 384)
    return fail("mip/array/cube texture descriptions or byte sizes do not match");

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 1 || (draws[0]->flags & ActionFlags::Indexed) || draws[0]->numIndices != 4)
    return fail("draw action does not match the four-vertex triangle strip");

  renderer->SetFrameEvent(draws[0]->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *metal = pipe.GetMetalPipelineState();
  if(metal == NULL || pipe.GetPrimitiveTopology() != Topology::TriangleStrip ||
     metal->fragmentTextures.size() != 3 ||
     metal->fragmentTextures[0] != mipTexture.resourceId ||
     metal->fragmentTextures[1] != arrayTexture.resourceId ||
     metal->fragmentTextures[2] != cubeTexture.resourceId ||
     metal->fragmentSamplers.size() != 1 || metal->fragmentSamplers[0] == ResourceId())
    return fail("Metal fragment texture/sampler slots do not match");

  const rdcarray<UsedDescriptor> resources =
      pipe.GetReadOnlyResources(ShaderStage::Fragment, true);
  if(resources.size() != 3 || resources[0].access.index != 0 ||
     resources[0].descriptor.resource != mipTexture.resourceId ||
     resources[0].descriptor.textureType != TextureType::Texture2D ||
     resources[0].descriptor.numMips != 3 || resources[0].descriptor.numSlices != 1 ||
     resources[1].access.index != 1 ||
     resources[1].descriptor.resource != arrayTexture.resourceId ||
     resources[1].descriptor.textureType != TextureType::Texture2DArray ||
     resources[1].descriptor.numMips != 1 || resources[1].descriptor.numSlices != 3 ||
     resources[2].access.index != 2 ||
     resources[2].descriptor.resource != cubeTexture.resourceId ||
     resources[2].descriptor.textureType != TextureType::TextureCube ||
     resources[2].descriptor.numMips != 1 || resources[2].descriptor.numSlices != 6)
    return fail("generic texture descriptors do not expose mip/slice/face counts");

  const ShaderReflection *reflection = pipe.GetShaderReflection(ShaderStage::Fragment);
  if(reflection == NULL || reflection->readOnlyResources.size() != 3 ||
     reflection->readOnlyResources[0].name != "mipTexture" ||
     reflection->readOnlyResources[0].textureType != TextureType::Texture2D ||
     reflection->readOnlyResources[1].name != "arrayTexture" ||
     reflection->readOnlyResources[1].textureType != TextureType::Texture2DArray ||
     reflection->readOnlyResources[2].name != "cubeTexture" ||
     reflection->readOnlyResources[2].textureType != TextureType::TextureCube)
    return fail("fragment shader reflection does not preserve texture binding types");

  static const byte colours[12][4] = {
      {255, 32, 16, 255},  {16, 224, 48, 255},  {24, 64, 255, 255},
      {240, 208, 32, 255}, {224, 48, 192, 255}, {32, 208, 224, 255},
      {255, 128, 32, 255}, {128, 32, 255, 255},  {32, 255, 128, 255},
      {255, 64, 128, 255}, {128, 255, 32, 255},  {32, 128, 255, 255},
  };

  auto displayMatches = [output](ResourceId resource, const Subresource &sub,
                                 const byte colour[4]) {
    TextureDisplay display;
    display.resourceId = resource;
    display.subresource = sub;
    display.scale = -1.0f;
    display.rangeMin = 0.0f;
    display.rangeMax = 1.0f;
    display.red = display.green = display.blue = display.alpha = true;
    display.rawOutput = true;
    output->SetTextureDisplay(display);
    output->Display();
    const bytebuf pixels = output->ReadbackOutputTexture();
    if(pixels.size() != 640 * 480 * 3)
      return false;
    const size_t centre = (size_t(240) * 640 + 320) * 3;
    return pixels[centre + 0] == colour[0] && pixels[centre + 1] == colour[1] &&
           pixels[centre + 2] == colour[2];
  };

  for(uint32_t mip = 0; mip < 3; mip++)
  {
    const uint32_t size = 4U >> mip;
    const bytebuf data = renderer->GetTextureData(mipTexture.resourceId, {mip, 0, 0});
    if(data.size() != size_t(size * size * 4))
      return fail("mip readback size does not match");
    for(size_t i = 0; i < data.size(); i += 4)
      if(memcmp(data.data() + i, colours[mip], 4) != 0)
        return fail("mip readback texels do not match");
    if(!displayMatches(mipTexture.resourceId, {mip, 0, 0}, colours[mip]))
      return fail("mip display did not render the selected level");
  }
  for(uint32_t slice = 0; slice < 3; slice++)
  {
    const bytebuf data = renderer->GetTextureData(arrayTexture.resourceId, {0, slice, 0});
    if(data.size() != 64)
      return fail("array slice readback size does not match");
    for(size_t i = 0; i < data.size(); i += 4)
      if(memcmp(data.data() + i, colours[3 + slice], 4) != 0)
        return fail("array slice readback texels do not match");
    if(!displayMatches(arrayTexture.resourceId, {0, slice, 0}, colours[3 + slice]))
      return fail("array display did not render the selected slice");
  }
  for(uint32_t face = 0; face < 6; face++)
  {
    const Subresource sub = {0, face, 0};
    const bytebuf data = renderer->GetTextureData(cubeTexture.resourceId, sub);
    const PixelValue pixel =
        renderer->PickPixel(cubeTexture.resourceId, 2, 2, sub, CompType::Typeless);
    if(data.size() != 64)
      return fail("cube face readback size does not match");
    for(size_t i = 0; i < data.size(); i += 4)
      if(memcmp(data.data() + i, colours[6 + face], 4) != 0)
        return fail("cube face readback texels do not match");
    if(!Near(pixel.floatValue[0], colours[6 + face][0] / 255.0f) ||
       !Near(pixel.floatValue[1], colours[6 + face][1] / 255.0f) ||
       !Near(pixel.floatValue[2], colours[6 + face][2] / 255.0f) ||
       !Near(pixel.floatValue[3], 1.0f))
      return fail("cube face PickPixel value does not match");
    if(!displayMatches(cubeTexture.resourceId, sub, colours[6 + face]))
      return fail("cube display did not render the selected face");
  }

  if(!renderer->GetTextureData(mipTexture.resourceId, {3, 0, 0}).empty() ||
     !renderer->GetTextureData(arrayTexture.resourceId, {0, 3, 0}).empty() ||
     !renderer->GetTextureData(cubeTexture.resourceId, {0, 6, 0}).empty())
    return fail("out-of-range mip/slice/face readback was not rejected");

  for(uint32_t band = 0; band < 12; band++)
  {
    const uint32_t x = (band * 400 + 200) / 12;
    if(!PixelMatches(renderer, colorTarget, draws[0]->eventId, x, 150,
                     colours[band][0] / 255.0f, colours[band][1] / 255.0f,
                     colours[band][2] / 255.0f))
      return fail("sampled output band does not match its subresource colour");
  }

  if(savePath != NULL)
  {
    TextureSave save;
    save.resourceId = cubeTexture.resourceId;
    save.destType = FileType::DDS;
    save.mip = -1;
    save.slice.sliceIndex = -1;
    ResultDetails result = renderer->SaveTexture(save, savePath);
    if(!result.OK())
      return fail("cube DDS save failed");
    std::ifstream file(savePath, std::ios::binary | std::ios::ate);
    if(!file || file.tellg() != std::streampos(128 + 6 * 4 * 4 * 4))
      return fail("cube DDS save did not contain all six faces");
  }

  return true;
}

static bool PixelMatches(IReplayController *renderer, ResourceId texture, uint32_t eventId,
                         uint32_t x, uint32_t y, float r, float g, float b)
{
  renderer->SetFrameEvent(eventId, true);
  const PixelValue pixel =
      renderer->PickPixel(texture, x, y, {0, 0, 0}, CompType::Typeless);
  return Near(pixel.floatValue[0], r) && Near(pixel.floatValue[1], g) &&
         Near(pixel.floatValue[2], b) && Near(pixel.floatValue[3], 1.0f);
}

static bool PixelMatchesRGBA(IReplayController *renderer, ResourceId texture, uint32_t eventId,
                             uint32_t x, uint32_t y, float r, float g, float b, float a)
{
  renderer->SetFrameEvent(eventId, true);
  const PixelValue pixel =
      renderer->PickPixel(texture, x, y, {0, 0, 0}, CompType::Typeless);
  return Near(pixel.floatValue[0], r) && Near(pixel.floatValue[1], g) &&
         Near(pixel.floatValue[2], b) && Near(pixel.floatValue[3], a);
}

static bool ValidateDynamicUniformFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId uniformBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 512)
    {
      uniformBuffer = buffer.resourceId;
      break;
    }
  }

  // Other fixtures do not contain T04's aligned 512-byte uniform buffer.
  if(uniformBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal dynamic uniform fixture validation failed: %s\n", message);
    return false;
  };

  byte expected[512] = {};
  const float left[4] = {1.0f, 0.125f, 0.0625f, 1.0f};
  const float right[4] = {0.0625f, 0.875f, 0.1875f, 1.0f};
  memcpy(expected, left, sizeof(left));
  memcpy(expected + 256, right, sizeof(right));
  const bytebuf data = renderer->GetBufferData(uniformBuffer, 0, 0);
  if(data.size() != sizeof(expected) || memcmp(data.data(), expected, sizeof(expected)) != 0)
    return fail("uniform buffer bytes do not match offsets 0/256");

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 2)
    return fail("expected two draw actions");

  for(size_t drawIndex = 0; drawIndex < draws.size(); drawIndex++)
  {
    if((draws[drawIndex]->flags & ActionFlags::Indexed) || draws[drawIndex]->numIndices != 3 ||
       draws[drawIndex]->numInstances != 1)
      return fail("draw metadata does not match the fullscreen triangle");

    renderer->SetFrameEvent(draws[drawIndex]->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const MetalPipe::State *metal = pipe.GetMetalPipelineState();
    const uint64_t expectedOffset = drawIndex == 0 ? 0 : 256;
    const uint64_t expectedSize = 512 - expectedOffset;
    if(metal == NULL || metal->fragmentBuffers.size() != 1 ||
       metal->fragmentBuffers[0].resourceId != uniformBuffer ||
       metal->fragmentBuffers[0].byteOffset != expectedOffset ||
       metal->fragmentBuffers[0].byteSize != expectedSize)
      return fail("event fragment buffer binding/offset does not match");

    const rdcarray<UsedDescriptor> blocks =
        pipe.GetConstantBlocks(ShaderStage::Fragment, false);
    const rdcarray<UsedDescriptor> usedBlocks =
        pipe.GetConstantBlocks(ShaderStage::Fragment, true);
    if(blocks.size() != 1 || usedBlocks.size() != 1 || blocks[0].access.index != 0 ||
       blocks[0].access.type != DescriptorType::ConstantBuffer ||
       blocks[0].access.staticallyUnused || blocks[0].descriptor.resource != uniformBuffer ||
       blocks[0].descriptor.type != DescriptorType::ConstantBuffer ||
       blocks[0].descriptor.byteOffset != expectedOffset ||
       blocks[0].descriptor.byteSize != expectedSize)
      return fail("generic constant buffer descriptor does not match");

    const ShaderReflection *reflection = pipe.GetShaderReflection(ShaderStage::Fragment);
    if(reflection == NULL || reflection->constantBlocks.size() != 1 ||
       reflection->constantBlocks[0].name != "uniforms" ||
       reflection->constantBlocks[0].fixedBindNumber != 0 ||
       reflection->constantBlocks[0].bindArraySize != 1 ||
       reflection->constantBlocks[0].byteSize != 16 ||
       !reflection->constantBlocks[0].bufferBacked)
      return fail("fragment constant block reflection does not match MSL");

    const Viewport viewport = pipe.GetViewport(0);
    const float expectedX = drawIndex == 0 ? 0.0f : 200.0f;
    if(!viewport.enabled || viewport.x != expectedX || viewport.y != 0.0f ||
       viewport.width != 200.0f || viewport.height != 300.0f)
      return fail("draw viewport does not match the left/right split");
  }

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL)
    return fail("clear action is unavailable");

  const float bgR = 0.025f, bgG = 0.035f, bgB = 0.055f;
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 300, 150, bgR, bgG, bgB))
    return fail("clear image does not contain two background halves");
  if(!PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 150, left[0], left[1], left[2]) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 150, bgR, bgG, bgB))
    return fail("first draw did not update only the left half");
  if(!PixelMatches(renderer, colorTarget, draws[1]->eventId, 100, 150, left[0], left[1], left[2]) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 300, 150, right[0], right[1], right[2]))
    return fail("second draw did not preserve left and update right half");
  if(!PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 150, left[0], left[1], left[2]) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 150, bgR, bgG, bgB))
    return fail("rewinding to the first draw did not restore its image");

  return true;
}

static bool ValidateInstancedFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId positionBuffer;
  ResourceId instanceBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 3 * 2 * sizeof(float))
      positionBuffer = buffer.resourceId;
    else if(buffer.length == 4 * 6 * sizeof(float))
      instanceBuffer = buffer.resourceId;
  }

  // Other fixtures do not contain T05's paired 24-byte/96-byte vertex buffers.
  if(positionBuffer == ResourceId() || instanceBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal instanced fixture validation failed: %s\n", message);
    return false;
  };

  static const float expectedPositions[] = {
      -0.18f, -0.18f, 0.18f, -0.18f, 0.0f, 0.22f,
  };
  static const float expectedInstances[] = {
      0.00f, 1.40f, 1.00f, 0.00f, 1.00f, 1.00f,
      -0.55f, 0.00f, 1.00f, 0.125f, 0.0625f, 1.00f,
      0.00f, 0.00f, 0.0625f, 0.875f, 0.1875f, 1.00f,
      0.55f, 0.00f, 0.09375f, 0.25f, 1.00f, 1.00f,
  };
  const bytebuf positions = renderer->GetBufferData(positionBuffer, 0, 0);
  const bytebuf instances = renderer->GetBufferData(instanceBuffer, 0, 0);
  if(positions.size() != sizeof(expectedPositions) ||
     memcmp(positions.data(), expectedPositions, sizeof(expectedPositions)) != 0)
    return fail("position buffer bytes do not match");
  if(instances.size() != sizeof(expectedInstances) ||
     memcmp(instances.data(), expectedInstances, sizeof(expectedInstances)) != 0)
    return fail("instance buffer bytes do not match");

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 1)
    return fail("expected one draw action");

  const ActionDescription *draw = draws[0];
  if((draw->flags & ActionFlags::Indexed) || !(draw->flags & ActionFlags::Instanced) ||
     draw->numIndices != 3 || draw->numInstances != 3 || draw->vertexOffset != 0 ||
     draw->instanceOffset != 1)
    return fail("instanced action metadata does not match");

  renderer->SetFrameEvent(draw->eventId, true);
  const PipeState &pipe = renderer->GetPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  if(state == NULL || state->vertexAttributes.size() != 3 || state->vertexBuffers.size() != 2)
    return fail("Metal vertex input state shape does not match");

  if(state->vertexAttributes[0].attributeIndex != 0 ||
     state->vertexAttributes[0].bufferIndex != 0 ||
     state->vertexAttributes[0].byteOffset != 0 ||
     state->vertexAttributes[0].format.compType != CompType::Float ||
     state->vertexAttributes[0].format.compByteWidth != 4 ||
     state->vertexAttributes[0].format.compCount != 2 ||
     state->vertexAttributes[1].attributeIndex != 1 ||
     state->vertexAttributes[1].bufferIndex != 1 ||
     state->vertexAttributes[1].byteOffset != 0 ||
     state->vertexAttributes[1].format.compType != CompType::Float ||
     state->vertexAttributes[1].format.compByteWidth != 4 ||
     state->vertexAttributes[1].format.compCount != 2 ||
     state->vertexAttributes[2].attributeIndex != 2 ||
     state->vertexAttributes[2].bufferIndex != 1 ||
     state->vertexAttributes[2].byteOffset != 8 ||
     state->vertexAttributes[2].format.compType != CompType::Float ||
     state->vertexAttributes[2].format.compByteWidth != 4 ||
     state->vertexAttributes[2].format.compCount != 4)
    return fail("Metal attributes do not match the two-buffer descriptor");

  if(state->vertexBuffers[0].resourceId != positionBuffer ||
     state->vertexBuffers[0].byteOffset != 0 || state->vertexBuffers[0].byteSize != 24 ||
     state->vertexBuffers[0].byteStride != 8 || state->vertexBuffers[0].perInstance ||
     state->vertexBuffers[0].stepRate != 1 ||
     state->vertexBuffers[1].resourceId != instanceBuffer ||
     state->vertexBuffers[1].byteOffset != 0 || state->vertexBuffers[1].byteSize != 96 ||
     state->vertexBuffers[1].byteStride != 24 || !state->vertexBuffers[1].perInstance ||
     state->vertexBuffers[1].stepRate != 1)
    return fail("Metal vertex buffer ranges/step modes do not match");

  const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
  const rdcarray<BoundVBuffer> vertexBuffers = pipe.GetVBuffers();
  if(inputs.size() != 3 || vertexBuffers.size() != 2 || inputs[0].name != "attr0" ||
     inputs[0].vertexBuffer != 0 || inputs[0].byteOffset != 0 || inputs[0].perInstance ||
     inputs[0].instanceRate != 1 || inputs[1].name != "attr1" ||
     inputs[1].vertexBuffer != 1 || inputs[1].byteOffset != 0 || !inputs[1].perInstance ||
     inputs[1].instanceRate != 1 || inputs[2].name != "attr2" ||
     inputs[2].vertexBuffer != 1 || inputs[2].byteOffset != 8 || !inputs[2].perInstance ||
     inputs[2].instanceRate != 1 || vertexBuffers[0].resourceId != positionBuffer ||
     vertexBuffers[1].resourceId != instanceBuffer)
    return fail("generic vertex inputs do not preserve per-instance mapping");

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL)
    return fail("clear action is unavailable");

  const float bgR = 0.025f, bgG = 0.035f, bgB = 0.055f;
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 90, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 200, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 310, 150, bgR, bgG, bgB))
    return fail("clear image does not contain the expected background");
  if(!PixelMatches(renderer, colorTarget, draw->eventId, 90, 150, 1.0f, 0.125f, 0.0625f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 200, 150, 0.0625f, 0.875f, 0.1875f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 310, 150, 0.09375f, 0.25f, 1.0f) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 10, 10, bgR, bgG, bgB))
    return fail("instanced replay image does not contain the three expected colours");
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 200, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draw->eventId, 200, 150, 0.0625f, 0.875f, 0.1875f))
    return fail("clear/draw event seek did not restore the instanced image");

  return true;
}

static bool ValidateMRTBlendFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId vertexBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 6 * 10 * sizeof(float))
    {
      vertexBuffer = buffer.resourceId;
      break;
    }
  }

  // Other fixtures do not contain T06's 240-byte interleaved vertex buffer.
  if(vertexBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal MRT/blend fixture validation failed: %s\n", message);
    return false;
  };

  TextureDescription firstTarget;
  TextureDescription secondTarget;
  for(const TextureDescription &texture : renderer->GetTextures())
  {
    if(texture.resourceId == colorTarget)
      firstTarget = texture;
    else if(!(texture.creationFlags & TextureCategory::SwapBuffer) && texture.width == 400 &&
            texture.height == 300 && texture.depth == 1 && texture.mips == 1 &&
            texture.arraysize == 1 && texture.format.type == ResourceFormatType::Regular &&
            texture.format.compType == CompType::UNorm && texture.format.compByteWidth == 1 &&
            texture.format.compCount == 4 && !texture.format.BGRAOrder())
      secondTarget = texture;
  }

  if(firstTarget.resourceId == ResourceId() || secondTarget.resourceId == ResourceId() ||
     !firstTarget.format.BGRAOrder())
    return fail("BGRA8 swapbuffer/RGBA8 secondary target pair is unavailable");

  static const float expectedVertices[] = {
      -1.0f, -1.0f, 1.0f, 0.125f, 0.0625f, 0.5f, 0.09375f, 0.25f, 1.0f, 0.75f,
      3.0f,  -1.0f, 1.0f, 0.125f, 0.0625f, 0.5f, 0.09375f, 0.25f, 1.0f, 0.75f,
      -1.0f, 3.0f,  1.0f, 0.125f, 0.0625f, 0.5f, 0.09375f, 0.25f, 1.0f, 0.75f,
      -0.45f, -0.45f, 0.0625f, 0.875f, 0.1875f, 0.25f, 0.9375f, 0.8125f, 0.125f, 0.25f,
      0.45f,  -0.45f, 0.0625f, 0.875f, 0.1875f, 0.25f, 0.9375f, 0.8125f, 0.125f, 0.25f,
      0.0f,   0.55f, 0.0625f, 0.875f, 0.1875f, 0.25f, 0.9375f, 0.8125f, 0.125f, 0.25f,
  };
  const bytebuf vertexData = renderer->GetBufferData(vertexBuffer, 0, 0);
  if(vertexData.size() != sizeof(expectedVertices) ||
     memcmp(vertexData.data(), expectedVertices, sizeof(expectedVertices)) != 0)
    return fail("interleaved position/dual-colour vertex bytes do not match");

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 2)
    return fail("expected two draw actions");

  for(size_t drawIndex = 0; drawIndex < draws.size(); drawIndex++)
  {
    const ActionDescription *draw = draws[drawIndex];
    if((draw->flags & (ActionFlags::Indexed | ActionFlags::Instanced)) ||
       draw->numIndices != 3 || draw->numInstances != 1 ||
       draw->vertexOffset != drawIndex * 3 || draw->outputs[0] != colorTarget ||
       draw->outputs[1] != secondTarget.resourceId)
      return fail("draw metadata/output slots do not match");

    renderer->SetFrameEvent(draw->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const MetalPipe::State *state = pipe.GetMetalPipelineState();
    const rdcarray<Descriptor> targets = pipe.GetOutputTargets();
    const rdcarray<ColorBlend> blends = pipe.GetColorBlends();
    const rdcarray<VertexInputAttribute> inputs = pipe.GetVertexInputs();
    const rdcarray<BoundVBuffer> buffers = pipe.GetVBuffers();
    if(state == NULL || targets.size() != 2 || targets[0].resource != colorTarget ||
       targets[1].resource != secondTarget.resourceId || targets[0].format.BGRAOrder() == false ||
       targets[1].format.BGRAOrder() || state->colorBlends.size() != 2 || blends.size() != 2)
      return fail("two output targets/generic blend array do not match");

    if(!blends[0].enabled || blends[0].colorBlend.source != BlendMultiplier::SrcAlpha ||
       blends[0].colorBlend.destination != BlendMultiplier::InvSrcAlpha ||
       blends[0].colorBlend.operation != BlendOperation::Add ||
       blends[0].alphaBlend.source != BlendMultiplier::One ||
       blends[0].alphaBlend.destination != BlendMultiplier::Zero ||
       blends[0].alphaBlend.operation != BlendOperation::Add || blends[0].writeMask != 0xf ||
       blends[1].enabled || blends[1].writeMask != 0x7)
      return fail("per-attachment blend factors/operations/write masks do not match");

    if(inputs.size() != 3 || buffers.size() != 1 || inputs[0].vertexBuffer != 0 ||
       inputs[0].byteOffset != 0 || inputs[0].format.compCount != 2 ||
       inputs[1].vertexBuffer != 0 || inputs[1].byteOffset != 8 ||
       inputs[1].format.compCount != 4 || inputs[2].vertexBuffer != 0 ||
       inputs[2].byteOffset != 24 || inputs[2].format.compCount != 4 ||
       buffers[0].resourceId != vertexBuffer || buffers[0].byteOffset != 0 ||
       buffers[0].byteSize != sizeof(expectedVertices) || buffers[0].byteStride != 40)
      return fail("Float2/Float4/Float4 vertex input does not match");
  }

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL || clear->outputs[0] != colorTarget ||
     clear->outputs[1] != secondTarget.resourceId)
    return fail("clear action does not expose both output slots");

  const uint32_t outsideX = 20, outsideY = 20, centreX = 200, centreY = 150;
  if(!PixelMatchesRGBA(renderer, colorTarget, clear->eventId, outsideX, outsideY, 0.10f, 0.20f,
                       0.30f, 1.0f) ||
     !PixelMatchesRGBA(renderer, secondTarget.resourceId, clear->eventId, outsideX, outsideY,
                       0.02f, 0.04f, 0.06f, 1.0f))
    return fail("clear image does not preserve both attachment clear colours");

  if(!PixelMatchesRGBA(renderer, colorTarget, draws[0]->eventId, centreX, centreY, 0.55f,
                       0.1625f, 0.18125f, 0.5f) ||
     !PixelMatchesRGBA(renderer, secondTarget.resourceId, draws[0]->eventId, centreX, centreY,
                       0.09375f, 0.25f, 1.0f, 1.0f))
    return fail("first draw output does not match blend/write-mask result");

  if(!PixelMatchesRGBA(renderer, colorTarget, draws[1]->eventId, outsideX, outsideY, 0.55f,
                       0.1625f, 0.18125f, 0.5f) ||
     !PixelMatchesRGBA(renderer, colorTarget, draws[1]->eventId, centreX, centreY, 0.428125f,
                       0.340625f, 0.1828125f, 0.25f) ||
     !PixelMatchesRGBA(renderer, secondTarget.resourceId, draws[1]->eventId, outsideX, outsideY,
                       0.09375f, 0.25f, 1.0f, 1.0f) ||
     !PixelMatchesRGBA(renderer, secondTarget.resourceId, draws[1]->eventId, centreX, centreY,
                       0.9375f, 0.8125f, 0.125f, 1.0f))
    return fail("second draw output does not preserve outside/overlap MRT results");

  if(!PixelMatchesRGBA(renderer, colorTarget, draws[0]->eventId, centreX, centreY, 0.55f,
                       0.1625f, 0.18125f, 0.5f) ||
     !PixelMatchesRGBA(renderer, secondTarget.resourceId, draws[0]->eventId, centreX, centreY,
                       0.09375f, 0.25f, 1.0f, 1.0f))
    return fail("rewinding to the first draw did not restore both attachments");

  return true;
}

static bool ValidateDepthStencilFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId vertexBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 15 * 7 * sizeof(float))
    {
      vertexBuffer = buffer.resourceId;
      break;
    }
  }

  // Other fixtures do not contain T07's 420-byte position/colour vertex buffer.
  if(vertexBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal depth/stencil fixture validation failed: %s\n", message);
    return false;
  };

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 5)
    return fail("expected five draw actions");

  static const uint32_t expectedVertices[] = {3, 3, 3, 3, 3};
  static const uint32_t expectedOffsets[] = {0, 3, 6, 9, 12};
  for(size_t i = 0; i < draws.size(); i++)
  {
    if((draws[i]->flags & (ActionFlags::Indexed | ActionFlags::Instanced)) ||
       draws[i]->numIndices != expectedVertices[i] ||
       draws[i]->vertexOffset != expectedOffsets[i] || draws[i]->depthOut == ResourceId())
      return fail("draw metadata/depth output does not match");

    renderer->SetFrameEvent(draws[i]->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const MetalPipe::State *state = pipe.GetMetalPipelineState();
    const DepthTestState depth = pipe.GetDepthTestState();
    const rdcpair<StencilFace, StencilFace> faces = pipe.GetStencilFaces();
    if(state == NULL || state->depthStencil.resourceId == ResourceId() ||
       pipe.GetDepthTarget().resource != draws[i]->depthOut || !pipe.IsStencilTestEnabled())
      return fail("combined depth/stencil target or state is unavailable");

    if(i < 2)
    {
      if(depth.depthFunction != CompareFunction::AlwaysTrue || depth.depthWrites ||
         faces.first.reference != (i == 0 ? 5U : 9U) ||
         faces.second.reference != (i == 0 ? 5U : 9U) ||
         faces.first.function != CompareFunction::AlwaysTrue ||
         faces.first.failOperation != StencilOperation::Zero ||
         faces.first.depthFailOperation != StencilOperation::IncSat ||
         faces.first.passOperation != StencilOperation::Replace || faces.first.compareMask != 0x3f ||
         faces.first.writeMask != 0xff ||
         faces.second.function != CompareFunction::AlwaysTrue ||
         faces.second.failOperation != StencilOperation::Invert ||
         faces.second.depthFailOperation != StencilOperation::DecWrap ||
         faces.second.passOperation != StencilOperation::Replace ||
         faces.second.compareMask != 0x7f || faces.second.writeMask != 0xff)
        return fail("stencil-write front/back state does not match");
    }
    else
    {
      const uint32_t expectedReference = i == 3 ? 9 : 5;
      if(depth.depthFunction != CompareFunction::Less || !depth.depthWrites ||
         faces.first.reference != expectedReference ||
         faces.second.reference != expectedReference ||
         faces.first.function != CompareFunction::Equal ||
         faces.second.function != CompareFunction::Equal || faces.first.compareMask != 0xff ||
         faces.second.compareMask != 0xff || faces.first.writeMask != 0 ||
         faces.second.writeMask != 0 ||
         faces.first.depthFailOperation != StencilOperation::IncSat ||
         faces.second.depthFailOperation != StencilOperation::DecSat)
        return fail("depth/stencil-test state or dynamic reference does not match");
    }
  }

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL || !(clear->flags & ActionFlags::ClearDepthStencil) ||
     clear->depthOut != draws[0]->depthOut)
    return fail("clear action does not expose the combined depth/stencil target");

  const float bgR = 0.025f, bgG = 0.035f, bgB = 0.055f;
  if(!PixelMatches(renderer, colorTarget, clear->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, clear->eventId, 300, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[0]->eventId, 300, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, colorTarget, draws[1]->eventId, 300, 150, bgR, bgG, bgB))
    return fail("clear/stencil-write events changed the color target");
  if(!PixelMatches(renderer, colorTarget, draws[2]->eventId, 100, 150, 0.0625f, 0.875f,
                   0.1875f) ||
     !PixelMatches(renderer, colorTarget, draws[2]->eventId, 300, 150, bgR, bgG, bgB))
    return fail("ref=5 draw did not update only the left mask");
  if(!PixelMatches(renderer, colorTarget, draws[3]->eventId, 100, 150, 0.0625f, 0.875f,
                   0.1875f) ||
     !PixelMatches(renderer, colorTarget, draws[3]->eventId, 300, 150, 0.09375f, 0.25f, 1.0f))
    return fail("ref=9 draw did not preserve left and update right mask");
  if(!PixelMatches(renderer, colorTarget, draws[4]->eventId, 100, 150, 0.0625f, 0.875f,
                   0.1875f) ||
     !PixelMatches(renderer, colorTarget, draws[4]->eventId, 300, 150, 0.09375f, 0.25f, 1.0f))
    return fail("far red draw did not fail depth and preserve both masks");
  if(!PixelMatches(renderer, colorTarget, draws[2]->eventId, 100, 150, 0.0625f, 0.875f,
                   0.1875f) ||
     !PixelMatches(renderer, colorTarget, draws[2]->eventId, 300, 150, bgR, bgG, bgB))
    return fail("rewinding to the first color draw did not restore its image");

  return true;
}

static bool ValidateMSAAResolveFixture(IReplayController *renderer, ResourceId colorTarget)
{
  ResourceId vertexBuffer;
  for(const BufferDescription &buffer : renderer->GetBuffers())
  {
    if(buffer.length == 9 * 6 * sizeof(float))
    {
      vertexBuffer = buffer.resourceId;
      break;
    }
  }

  // Other fixtures do not contain T08's 216-byte position/colour vertex buffer.
  if(vertexBuffer == ResourceId())
    return true;

  auto fail = [](const char *message) {
    fprintf(stderr, "Metal MSAA resolve fixture validation failed: %s\n", message);
    return false;
  };

  TextureDescription multisampleTarget;
  TextureDescription resolveTarget;
  for(const TextureDescription &texture : renderer->GetTextures())
  {
    if(texture.width != 400 || texture.height != 300)
      continue;

    if(texture.type == TextureType::Texture2DMS && texture.msSamp == 4 &&
       texture.byteSize == 400 * 300 * 4 * 4)
      multisampleTarget = texture;
    if((texture.creationFlags & TextureCategory::SwapBuffer) && texture.msSamp == 1)
      resolveTarget = texture;
  }
  if(multisampleTarget.resourceId == ResourceId() || resolveTarget.resourceId != colorTarget)
    return fail("4x multisample or single-sample resolve texture is unavailable");

  rdcarray<const ActionDescription *> draws;
  FindDrawActions(renderer->GetRootActions(), draws);
  if(draws.size() != 3)
    return fail("expected three draw actions");

  static const uint32_t expectedOffsets[] = {0, 3, 6};
  for(size_t i = 0; i < draws.size(); i++)
  {
    if((draws[i]->flags & (ActionFlags::Indexed | ActionFlags::Instanced)) ||
       draws[i]->numIndices != 3 || draws[i]->vertexOffset != expectedOffsets[i] ||
       draws[i]->outputs[0] != resolveTarget.resourceId)
      return fail("draw metadata or resolved action output does not match");

    renderer->SetFrameEvent(draws[i]->eventId, true);
    const PipeState &pipe = renderer->GetPipelineState();
    const MetalPipe::State *state = pipe.GetMetalPipelineState();
    const rdcarray<Descriptor> colorTargets = pipe.GetOutputTargets();
    if(state == NULL || state->sampleCount != 4 || !state->alphaToCoverageEnabled ||
       state->alphaToOneEnabled || colorTargets.size() != 1 ||
       colorTargets[0].resource != multisampleTarget.resourceId ||
       colorTargets[0].textureType != TextureType::Texture2DMS ||
       state->resolveTargets.size() != 1 ||
       state->resolveTargets[0].resource != resolveTarget.resourceId ||
       state->resolveTargets[0].textureType != TextureType::Texture2D)
      return fail("pipeline multisample attachment/resolve state does not match");

    const rdcarray<BoundVBuffer> vertexBuffers = pipe.GetVBuffers();
    if(vertexBuffers.size() != 1 || vertexBuffers[0].resourceId != vertexBuffer ||
       vertexBuffers[0].byteOffset != 0 || vertexBuffers[0].byteSize != 216 ||
       vertexBuffers[0].byteStride != 24)
      return fail("vertex buffer range or stride does not match");
  }

  const ActionDescription *clear = FindAction(renderer->GetRootActions(), ActionFlags::Clear);
  if(clear == NULL || clear->outputs[0] != resolveTarget.resourceId)
    return fail("clear action does not expose the resolve target");

  const float bgR = 0.03f, bgG = 0.04f, bgB = 0.06f;
  if(!PixelMatches(renderer, resolveTarget.resourceId, clear->eventId, 100, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, resolveTarget.resourceId, clear->eventId, 200, 150, bgR, bgG, bgB) ||
     !PixelMatches(renderer, resolveTarget.resourceId, clear->eventId, 300, 150, bgR, bgG, bgB))
    return fail("resolved clear image does not contain the expected background");
  if(!PixelMatches(renderer, resolveTarget.resourceId, draws[0]->eventId, 100, 150, 1.0f,
                   0.125f, 0.0625f) ||
     !PixelMatches(renderer, resolveTarget.resourceId, draws[0]->eventId, 300, 150, bgR, bgG,
                   bgB))
    return fail("first draw did not resolve only the left triangle");
  if(!PixelMatches(renderer, resolveTarget.resourceId, draws[1]->eventId, 100, 150, 1.0f,
                   0.125f, 0.0625f) ||
     !PixelMatches(renderer, resolveTarget.resourceId, draws[1]->eventId, 300, 150, 0.09375f,
                   0.25f, 1.0f))
    return fail("second draw did not resolve both side triangles");
  if(!PixelMatches(renderer, resolveTarget.resourceId, draws[2]->eventId, 200, 150, 0.0625f,
                   0.875f, 0.1875f))
    return fail("third draw did not resolve the centre overlay");
  if(!PixelMatches(renderer, resolveTarget.resourceId, draws[0]->eventId, 100, 150, 1.0f,
                   0.125f, 0.0625f) ||
     !PixelMatches(renderer, resolveTarget.resourceId, draws[0]->eventId, 300, 150, bgR, bgG,
                   bgB))
    return fail("rewinding to the first draw did not restore its resolve image");

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
  if(draws.empty())
    return true;

  const bool indexedFixture = draws.size() == 2 && (draws[0]->flags & ActionFlags::Indexed);
  const bool instancedFixture =
      draws.size() == 1 && (draws[0]->flags & ActionFlags::Instanced) &&
      draws[0]->numInstances == 3 && draws[0]->instanceOffset == 1;
  if(!indexedFixture && !instancedFixture)
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
  mesh.curInstance = instancedFixture ? 1 : 0;
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
  if(argc != 3 && argc != 4 && argc != 6)
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
  if(argc == 3 || argc == 4)
  {
    success = ValidateIndexedFixture(renderer, display.resourceId, swapBuffer) &&
              ValidateDynamicUniformFixture(renderer, display.resourceId) &&
              ValidateInstancedFixture(renderer, display.resourceId) &&
              ValidateMRTBlendFixture(renderer, display.resourceId) &&
              ValidateDepthStencilFixture(renderer, display.resourceId) &&
              ValidateMSAAResolveFixture(renderer, display.resourceId) &&
              ValidateTexturedFixture(renderer) &&
              ValidateTextureSubresourceFixture(renderer, output, display.resourceId,
                                                argc == 4 ? argv[3] : NULL) &&
              ValidateMeshPreview(renderer, window);
    if(success)
    {
      output->SetTextureDisplay(display);
      success = WriteOutput(renderer, output, 10000000, argv[2]);
    }
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
