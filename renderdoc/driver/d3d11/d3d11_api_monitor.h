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

#include <d3d11.h>
#include "api/replay/resourceid.h"

// D3D11 API Monitor hook helpers.
// These functions build Python dicts from D3D11 structures and invoke the appropriate hooks.
namespace D3D11ApiMonitor
{

// Device resource creation hooks
void OnCreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc, ResourceId resId);
void OnCreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc, ResourceId resId);
void OnCreateBuffer(const D3D11_BUFFER_DESC *pDesc, ResourceId resId);
void OnCreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc, ResourceId resourceId,
                                ResourceId viewId);
void OnCreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC *pDesc, ResourceId resourceId,
                              ResourceId viewId);
void OnCreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                                 ResourceId resourceId, ResourceId viewId);

// Context draw/dispatch hooks
void OnDraw(UINT VertexCount, UINT StartVertexLocation);
void OnDrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
void OnDrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation,
                     UINT StartInstanceLocation);
void OnDrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount,
                            UINT StartIndexLocation, INT BaseVertexLocation,
                            UINT StartInstanceLocation);
void OnDispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ);

// Context resource update hooks
void OnUpdateSubresource(ResourceId dstResource, UINT DstSubresource, const D3D11_BOX *pDstBox,
                         UINT SrcRowPitch, UINT SrcDepthPitch);
void OnMap(ResourceId resource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags);
void OnCopyResource(ResourceId dstResource, ResourceId srcResource);
void OnCopySubresourceRegion(ResourceId dstResource, UINT DstSubresource, UINT DstX, UINT DstY,
                             UINT DstZ, ResourceId srcResource, UINT SrcSubresource,
                             const D3D11_BOX *pSrcBox);

// Context shader resource binding hooks
void OnPSSetShaderResources(UINT StartSlot, UINT NumViews, ResourceId *viewIds);
void OnVSSetShaderResources(UINT StartSlot, UINT NumViews, ResourceId *viewIds);
void OnCSSetShaderResources(UINT StartSlot, UINT NumViews, ResourceId *viewIds);

}    // namespace D3D11ApiMonitor

#endif    // RENDERDOC_ENABLE_API_MONITOR
