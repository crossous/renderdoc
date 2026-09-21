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

#include <mach/mach.h>
#include <cstdio>
#include <cstdlib>
#include "renderdoc/api/replay/renderdoc_replay.h"
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

static uint64_t ResidentBytes()
{
  mach_task_basic_info_data_t info = {};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  if(task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) != KERN_SUCCESS)
    return 0;
  return info.resident_size;
}

static bool ValidateUnsupportedInterfaces(IReplayController *renderer, ResourceId texture,
                                          uint32_t drawEvent)
{
  renderer->SetFrameEvent(drawEvent, true);

  const Subresource sub = {0, 0, 0};
  const rdcfixedarray<bool, 4> channels = {true, true, true, true};
  if(!renderer->GetHistogram(texture, sub, CompType::Typeless, 0.0f, 1.0f, channels).empty())
    return false;

  if(!renderer->PixelHistory(texture, 0, 0, sub, CompType::Typeless).empty())
    return false;

  MeshFormat postVS = renderer->GetPostVSData(0, 0, MeshDataStage::VSOut);
  if(postVS.vertexResourceId != ResourceId() || postVS.indexResourceId != ResourceId())
    return false;

  ShaderDebugTrace *trace = renderer->DebugVertex(0, 0, 0, 0);
  if(trace == NULL || trace->debugger != NULL || trace->stage != ShaderStage::Vertex)
    return false;
  renderer->FreeTrace(trace);

  trace = renderer->DebugPixel(0, 0, DebugPixelInputs());
  if(trace == NULL || trace->debugger != NULL || trace->stage != ShaderStage::Pixel)
    return false;
  renderer->FreeTrace(trace);

  const rdcfixedarray<uint32_t, 3> thread = {0, 0, 0};
  trace = renderer->DebugThread(thread, thread);
  if(trace == NULL || trace->debugger != NULL || trace->stage != ShaderStage::Compute)
    return false;
  renderer->FreeTrace(trace);

  trace = renderer->DebugMeshThread(thread, thread);
  if(trace == NULL || trace->debugger != NULL || trace->stage != ShaderStage::Mesh)
    return false;
  renderer->FreeTrace(trace);

  bytebuf source;
  source.push_back('x');
  rdcpair<ResourceId, rdcstr> shader = renderer->BuildTargetShader(
      "main", ShaderEncoding::MSL, source, ShaderCompileFlags(), ShaderStage::Vertex);
  if(shader.first != ResourceId() || shader.second.empty())
    return false;

  shader = renderer->BuildCustomShader("main", ShaderEncoding::MSL, source, ShaderCompileFlags(),
                                       ShaderStage::Vertex);
  return shader.first == ResourceId() && !shader.second.empty();
}

static bool OpenValidateClose(const char *path, bool expectDraw, bool validateUnsupported)
{
  NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
  ICaptureFile *file = RENDERDOC_OpenCaptureFile();
  ResultDetails result = file->OpenFile(path, "rdc", NULL);
  if(!result.OK())
  {
    file->Shutdown();
    pool->release();
    return false;
  }

  IReplayController *renderer = NULL;
  rdctie(result, renderer) = file->OpenCapture(ReplayOptions(), NULL);
  file->Shutdown();
  if(!result.OK() || renderer == NULL)
  {
    pool->release();
    return false;
  }

  const APIProperties props = renderer->GetAPIProperties();
  const ActionDescription *draw = FindAction(renderer->GetRootActions(), ActionFlags::Drawcall);
  ResourceId swapBuffer;
  for(const TextureDescription &texture : renderer->GetTextures())
  {
    if(texture.creationFlags & TextureCategory::SwapBuffer)
    {
      swapBuffer = texture.resourceId;
      break;
    }
  }

  bool success = props.pipelineType == GraphicsAPI::Metal && props.localRenderer == GraphicsAPI::Metal &&
                 !renderer->GetRootActions().empty() && swapBuffer != ResourceId() &&
                 ((draw != NULL) == expectDraw);

  if(success && draw)
  {
    renderer->SetFrameEvent(draw->eventId, true);
    bytebuf pixels = renderer->GetTextureData(swapBuffer, {0, 0, 0});
    success = !pixels.empty();
    if(success && validateUnsupported)
      success = ValidateUnsupportedInterfaces(renderer, swapBuffer, draw->eventId);
  }

  renderer->Shutdown();
  pool->release();
  return success;
}

int main(int argc, char **argv)
{
  if(argc < 4)
    return 2;

  const int iterations = atoi(argv[argc - 1]);
  if(iterations < 10)
    return 3;

  NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();
  GlobalEnvironment env;
  env.enumerateGPUs = false;
  rdcarray<rdcstr> args;
  args.push_back(argv[0]);
  RENDERDOC_InitialiseReplay(env, args);

  uint64_t baseline = 0;
  bool success = true;
  for(int i = 0; i < iterations && success; i++)
  {
    for(int capture = 1; capture < argc - 1 && success; capture++)
      success = OpenValidateClose(argv[capture], capture != 1, i == 0 && capture == 2);
    if(i == 1)
      baseline = ResidentBytes();
  }

  const uint64_t finalResident = ResidentBytes();
  RENDERDOC_ShutdownReplay();
  pool->release();

  const uint64_t growth = finalResident > baseline ? finalResident - baseline : 0;
  const uint64_t maximumGrowth = 64ULL * 1024ULL * 1024ULL;
  if(!success || baseline == 0 || growth > maximumGrowth)
  {
    fprintf(stderr, "Metal replay lifecycle smoke failed: success=%d, resident growth=%llu bytes\n",
            success ? 1 : 0, growth);
    return 4;
  }

  printf("Metal replay lifecycle smoke passed: %d captures x %d iterations, resident growth %llu "
         "bytes\n",
         argc - 2, iterations, growth);
  return 0;
}
