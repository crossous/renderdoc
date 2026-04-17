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

#pragma once

#include "core/api_monitor.h"

#if RENDERDOC_ENABLE_API_MONITOR

#include "driver/dx/official/d3d12.h"
#include "api/replay/resourceid.h"

namespace D3D12ApiMonitor
{

// Resource creation hook (all committed/placed/reserved resources go through CreateResource)
void OnCreateResource(const D3D12_RESOURCE_DESC1 &desc, const D3D12_HEAP_PROPERTIES *pHeapProps,
                      D3D12_HEAP_FLAGS HeapFlags, ResourceId resId);

// Command list draw/dispatch hooks
void OnDrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation,
                     UINT StartInstanceLocation);
void OnDrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount,
                            UINT StartIndexLocation, INT BaseVertexLocation,
                            UINT StartInstanceLocation);
void OnDispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ);

// Command list resource copy hooks
void OnCopyTextureRegion(ResourceId dst, UINT DstSubresource, ResourceId src, UINT SrcSubresource);
void OnCopyBufferRegion(ResourceId dst, UINT64 DstOffset, ResourceId src, UINT64 SrcOffset,
                        UINT64 NumBytes);
void OnCopyResource(ResourceId dst, ResourceId src);

// Resource barrier hook
void OnResourceBarrier(UINT NumBarriers, ResourceId *resourceIds);

}    // namespace D3D12ApiMonitor

#endif    // RENDERDOC_ENABLE_API_MONITOR
