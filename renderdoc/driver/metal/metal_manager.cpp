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

#include "metal_manager.h"
#include "metal_blit_command_encoder.h"
#include "metal_buffer.h"
#include "metal_command_buffer.h"
#include "metal_command_queue.h"
#include "metal_device.h"
#include "metal_depth_stencil_state.h"
#include "metal_function.h"
#include "metal_library.h"
#include "metal_render_command_encoder.h"
#include "metal_render_pipeline_state.h"
#include "metal_sampler_state.h"
#include "metal_texture.h"

bool MetalResourceManager::ResourceTypeRelease(WrappedResourceType res)
{
  if(res == NULL)
    return true;

  void *real = res->m_Real;
  const bool ownsReal = res->m_OwnsReal;

  if(real && res->m_ObjcBridge)
  {
    // The real object owns the embedded bridge through this association. Removing it invokes the
    // bridge's dealloc path, which unregisters and deletes the correctly typed C++ wrapper.
    objc_setAssociatedObject((id)real, res->m_ObjcBridge, NULL, OBJC_ASSOCIATION_ASSIGN);
  }
  else
  {
    switch(res->m_Type)
    {
      case eResBuffer: ReleaseWrappedResource((WrappedMTLBuffer *)res); break;
      case eResCommandBuffer: ReleaseWrappedResource((WrappedMTLCommandBuffer *)res); break;
      case eResCommandQueue: ReleaseWrappedResource((WrappedMTLCommandQueue *)res); break;
      case eResDepthStencilState:
        ReleaseWrappedResource((WrappedMTLDepthStencilState *)res);
        break;
      case eResLibrary: ReleaseWrappedResource((WrappedMTLLibrary *)res); break;
      case eResFunction: ReleaseWrappedResource((WrappedMTLFunction *)res); break;
      case eResRenderPipelineState:
        ReleaseWrappedResource((WrappedMTLRenderPipelineState *)res);
        break;
      case eResTexture: ReleaseWrappedResource((WrappedMTLTexture *)res); break;
      case eResRenderCommandEncoder:
        ReleaseWrappedResource((WrappedMTLRenderCommandEncoder *)res);
        break;
      case eResBlitCommandEncoder:
        ReleaseWrappedResource((WrappedMTLBlitCommandEncoder *)res);
        break;
      case eResSamplerState: ReleaseWrappedResource((WrappedMTLSamplerState *)res); break;
      case eResDevice:
      case eResUnknown:
      case eResMax:
        RDCERR("Unexpected Metal resource type %u during replay shutdown", (uint32_t)res->m_Type);
        return false;
    }
  }

  if(real && ownsReal)
    ((NS::Object *)real)->release();

  return true;
}

bool MetalResourceManager::Prepare_InitialState(WrappedMTLObject *res)
{
  return m_Device->Prepare_InitialState(res);
}

uint64_t MetalResourceManager::GetSize_InitialState(ResourceId id, const MetalInitialContents &initial)
{
  return m_Device->GetSize_InitialState(id, initial);
}

bool MetalResourceManager::Serialise_InitialState(WriteSerialiser &ser, ResourceId id,
                                                  MetalResourceRecord *record,
                                                  const MetalInitialContents *initial)
{
  return m_Device->Serialise_InitialState(ser, id, record, initial);
}

void MetalResourceManager::Create_InitialState(ResourceId id, WrappedMTLObject *live, bool hasData)
{
  return m_Device->Create_InitialState(id, live, hasData);
}

void MetalResourceManager::Apply_InitialState(WrappedMTLObject *live, MetalInitialContents &initial)
{
  return m_Device->Apply_InitialState(live, initial);
}
