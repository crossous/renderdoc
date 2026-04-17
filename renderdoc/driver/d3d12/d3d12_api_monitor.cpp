/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024-2026 Baldur Karlsson
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

#include "d3d12_api_monitor.h"

#if RENDERDOC_ENABLE_API_MONITOR

#include "core/api_monitor.h"

namespace D3D12ApiMonitor
{

static inline uint64_t ResIdToU64(ResourceId id)
{
  uint64_t val;
  memcpy(&val, &id, sizeof(uint64_t));
  return val;
}

//=============================================================================
// Device creation hook - push to event queue (safe for multi-threaded loading)
//=============================================================================

void OnCreateResource(const D3D12_RESOURCE_DESC1 &desc, const D3D12_HEAP_PROPERTIES *pHeapProps,
                      D3D12_HEAP_FLAGS HeapFlags, ResourceId resId)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  MonitorEvent evt;
  evt.type = MonitorEvent::CreateResource;
  evt.createRes.resId = ResIdToU64(resId);
  evt.createRes.dimension = (UINT)desc.Dimension;
  evt.createRes.alignment = desc.Alignment;
  evt.createRes.width = desc.Width;
  evt.createRes.height = desc.Height;
  evt.createRes.depthOrArraySize = desc.DepthOrArraySize;
  evt.createRes.mipLevels = desc.MipLevels;
  evt.createRes.format = (UINT64)desc.Format;
  evt.createRes.sampleCount = desc.SampleDesc.Count;
  evt.createRes.sampleQuality = desc.SampleDesc.Quality;
  evt.createRes.layout = (UINT64)desc.Layout;
  evt.createRes.flags = (UINT64)desc.Flags;
  evt.createRes.heapFlags = (UINT64)HeapFlags;

  if(pHeapProps)
  {
    evt.createRes.hasHeapProps = true;
    evt.createRes.heapType = (UINT64)pHeapProps->Type;
    evt.createRes.cpuPageProp = (UINT64)pHeapProps->CPUPageProperty;
    evt.createRes.memPoolPref = (UINT64)pHeapProps->MemoryPoolPreference;
  }
  else
  {
    evt.createRes.hasHeapProps = false;
    evt.createRes.heapType = 0;
    evt.createRes.cpuPageProp = 0;
    evt.createRes.memPoolPref = 0;
  }

  ApiMonitor::Inst().PushEvent(evt);
}

//=============================================================================
// Command list hooks - push events to queue (multi-threaded, no GIL)
//=============================================================================

void OnDrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation,
                     UINT StartInstanceLocation)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  MonitorEvent evt;
  evt.type = MonitorEvent::Draw;
  evt.draw.vertexCount = VertexCountPerInstance;
  evt.draw.instanceCount = InstanceCount;
  evt.draw.startVertex = StartVertexLocation;
  evt.draw.startInstance = StartInstanceLocation;
  ApiMonitor::Inst().PushEvent(evt);
}

void OnDrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount,
                            UINT StartIndexLocation, INT BaseVertexLocation,
                            UINT StartInstanceLocation)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  MonitorEvent evt;
  evt.type = MonitorEvent::DrawIndexed;
  evt.drawIndexed.indexCount = IndexCountPerInstance;
  evt.drawIndexed.instanceCount = InstanceCount;
  evt.drawIndexed.startIndex = StartIndexLocation;
  evt.drawIndexed.baseVertex = BaseVertexLocation;
  evt.drawIndexed.startInstance = StartInstanceLocation;
  ApiMonitor::Inst().PushEvent(evt);
}

void OnDispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  MonitorEvent evt;
  evt.type = MonitorEvent::Dispatch;
  evt.dispatch.x = ThreadGroupCountX;
  evt.dispatch.y = ThreadGroupCountY;
  evt.dispatch.z = ThreadGroupCountZ;
  ApiMonitor::Inst().PushEvent(evt);
}

void OnCopyTextureRegion(ResourceId dst, UINT DstSubresource, ResourceId src, UINT SrcSubresource)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  MonitorEvent evt;
  evt.type = MonitorEvent::CopyTexture;
  evt.copyTex.dst = ResIdToU64(dst);
  evt.copyTex.dstSub = DstSubresource;
  evt.copyTex.src = ResIdToU64(src);
  evt.copyTex.srcSub = SrcSubresource;
  ApiMonitor::Inst().PushEvent(evt);
}

void OnCopyBufferRegion(ResourceId dst, UINT64 DstOffset, ResourceId src, UINT64 SrcOffset,
                        UINT64 NumBytes)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  MonitorEvent evt;
  evt.type = MonitorEvent::CopyBuffer;
  evt.copyBuf.dst = ResIdToU64(dst);
  evt.copyBuf.dstOff = DstOffset;
  evt.copyBuf.src = ResIdToU64(src);
  evt.copyBuf.srcOff = SrcOffset;
  evt.copyBuf.numBytes = NumBytes;
  ApiMonitor::Inst().PushEvent(evt);
}

void OnCopyResource(ResourceId dst, ResourceId src)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  MonitorEvent evt;
  evt.type = MonitorEvent::CopyResource;
  evt.copyRes.dst = ResIdToU64(dst);
  evt.copyRes.src = ResIdToU64(src);
  ApiMonitor::Inst().PushEvent(evt);
}

void OnResourceBarrier(UINT NumBarriers, ResourceId *resourceIds)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  MonitorEvent evt;
  evt.type = MonitorEvent::Barrier;
  evt.barrier.count = NumBarriers;
  UINT n = NumBarriers > 8 ? 8 : NumBarriers;
  for(UINT i = 0; i < n; i++)
    evt.barrier.resources[i] = ResIdToU64(resourceIds[i]);
  ApiMonitor::Inst().PushEvent(evt);
}

}    // namespace D3D12ApiMonitor

#endif    // RENDERDOC_ENABLE_API_MONITOR
