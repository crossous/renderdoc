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

#include "d3d11_api_monitor.h"

#if RENDERDOC_ENABLE_API_MONITOR

#include "core/api_monitor.h"

namespace D3D11ApiMonitor
{

// Helper to set an int field on a dict
static inline void SetInt(PythonLoader &py, PyObject *dict, const char *key, long long val)
{
  PyObject *pyVal = py.PyLong_FromLongLong(val);
  py.PyDict_SetItemString(dict, key, pyVal);
  py.Py_DecRef(pyVal);
}

// Helper to set an unsigned int field on a dict
static inline void SetUInt(PythonLoader &py, PyObject *dict, const char *key, unsigned long long val)
{
  PyObject *pyVal = py.PyLong_FromUnsignedLongLong(val);
  py.PyDict_SetItemString(dict, key, pyVal);
  py.Py_DecRef(pyVal);
}

// Helper to set a string field on a dict
static inline void SetStr(PythonLoader &py, PyObject *dict, const char *key, const char *val)
{
  PyObject *pyVal = py.PyUnicode_FromString(val);
  py.PyDict_SetItemString(dict, key, pyVal);
  py.Py_DecRef(pyVal);
}

// Helper to convert ResourceId to uint64_t (ResourceId::id is private)
static inline uint64_t ResIdToU64(ResourceId id)
{
  uint64_t val;
  memcpy(&val, &id, sizeof(uint64_t));
  return val;
}

// Build a Python dict from D3D11_TEXTURE2D_DESC
static PyObject *BuildTexture2DDesc(PythonLoader &py, const D3D11_TEXTURE2D_DESC *pDesc)
{
  PyObject *dict = py.PyDict_New();
  SetUInt(py, dict, "Width", pDesc->Width);
  SetUInt(py, dict, "Height", pDesc->Height);
  SetUInt(py, dict, "MipLevels", pDesc->MipLevels);
  SetUInt(py, dict, "ArraySize", pDesc->ArraySize);
  SetUInt(py, dict, "Format", (unsigned long long)pDesc->Format);
  SetUInt(py, dict, "SampleCount", pDesc->SampleDesc.Count);
  SetUInt(py, dict, "SampleQuality", pDesc->SampleDesc.Quality);
  SetUInt(py, dict, "Usage", (unsigned long long)pDesc->Usage);
  SetUInt(py, dict, "BindFlags", pDesc->BindFlags);
  SetUInt(py, dict, "CPUAccessFlags", pDesc->CPUAccessFlags);
  SetUInt(py, dict, "MiscFlags", pDesc->MiscFlags);
  return dict;
}

// Build a Python dict from D3D11_TEXTURE3D_DESC
static PyObject *BuildTexture3DDesc(PythonLoader &py, const D3D11_TEXTURE3D_DESC *pDesc)
{
  PyObject *dict = py.PyDict_New();
  SetUInt(py, dict, "Width", pDesc->Width);
  SetUInt(py, dict, "Height", pDesc->Height);
  SetUInt(py, dict, "Depth", pDesc->Depth);
  SetUInt(py, dict, "MipLevels", pDesc->MipLevels);
  SetUInt(py, dict, "Format", (unsigned long long)pDesc->Format);
  SetUInt(py, dict, "Usage", (unsigned long long)pDesc->Usage);
  SetUInt(py, dict, "BindFlags", pDesc->BindFlags);
  SetUInt(py, dict, "CPUAccessFlags", pDesc->CPUAccessFlags);
  SetUInt(py, dict, "MiscFlags", pDesc->MiscFlags);
  return dict;
}

// Build a Python dict from D3D11_BUFFER_DESC
static PyObject *BuildBufferDesc(PythonLoader &py, const D3D11_BUFFER_DESC *pDesc)
{
  PyObject *dict = py.PyDict_New();
  SetUInt(py, dict, "ByteWidth", pDesc->ByteWidth);
  SetUInt(py, dict, "Usage", (unsigned long long)pDesc->Usage);
  SetUInt(py, dict, "BindFlags", pDesc->BindFlags);
  SetUInt(py, dict, "CPUAccessFlags", pDesc->CPUAccessFlags);
  SetUInt(py, dict, "MiscFlags", pDesc->MiscFlags);
  SetUInt(py, dict, "StructureByteStride", pDesc->StructureByteStride);
  return dict;
}

//=============================================================================
// Device creation hooks
//=============================================================================

void OnCreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc, ResourceId resId)
{
  if(!ApiMonitor::Inst().IsActive() || !pDesc)
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  PyObject *descDict = BuildTexture2DDesc(py, pDesc);
  py.PyDict_SetItemString(argsDict, "pDesc", descDict);
  py.Py_DecRef(descDict);

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11Device::CreateTexture2D", argsDict, resId);
  py.Py_DecRef(argsDict);
}

void OnCreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc, ResourceId resId)
{
  if(!ApiMonitor::Inst().IsActive() || !pDesc)
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  PyObject *descDict = BuildTexture3DDesc(py, pDesc);
  py.PyDict_SetItemString(argsDict, "pDesc", descDict);
  py.Py_DecRef(descDict);

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11Device::CreateTexture3D", argsDict, resId);
  py.Py_DecRef(argsDict);
}

void OnCreateBuffer(const D3D11_BUFFER_DESC *pDesc, ResourceId resId)
{
  if(!ApiMonitor::Inst().IsActive() || !pDesc)
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  PyObject *descDict = BuildBufferDesc(py, pDesc);
  py.PyDict_SetItemString(argsDict, "pDesc", descDict);
  py.Py_DecRef(descDict);

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11Device::CreateBuffer", argsDict, resId);
  py.Py_DecRef(argsDict);
}

void OnCreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                ResourceId resourceId, ResourceId viewId)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "pResource", ResIdToU64(resourceId));
  if(pDesc)
  {
    SetUInt(py, argsDict, "Format", (unsigned long long)pDesc->Format);
    SetUInt(py, argsDict, "ViewDimension", (unsigned long long)pDesc->ViewDimension);
  }

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11Device::CreateShaderResourceView", argsDict, viewId);
  py.Py_DecRef(argsDict);
}

void OnCreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC *pDesc, ResourceId resourceId,
                              ResourceId viewId)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "pResource", ResIdToU64(resourceId));
  if(pDesc)
  {
    SetUInt(py, argsDict, "Format", (unsigned long long)pDesc->Format);
    SetUInt(py, argsDict, "ViewDimension", (unsigned long long)pDesc->ViewDimension);
  }

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11Device::CreateRenderTargetView", argsDict, viewId);
  py.Py_DecRef(argsDict);
}

void OnCreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                                 ResourceId resourceId, ResourceId viewId)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "pResource", ResIdToU64(resourceId));
  if(pDesc)
  {
    SetUInt(py, argsDict, "Format", (unsigned long long)pDesc->Format);
    SetUInt(py, argsDict, "ViewDimension", (unsigned long long)pDesc->ViewDimension);
  }

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11Device::CreateUnorderedAccessView", argsDict, viewId);
  py.Py_DecRef(argsDict);
}

//=============================================================================
// Context draw/dispatch hooks
//=============================================================================

void OnDraw(UINT VertexCount, UINT StartVertexLocation)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "VertexCount", VertexCount);
  SetUInt(py, argsDict, "StartVertexLocation", StartVertexLocation);

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11DeviceContext::Draw", argsDict);
  py.Py_DecRef(argsDict);
}

void OnDrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "IndexCount", IndexCount);
  SetUInt(py, argsDict, "StartIndexLocation", StartIndexLocation);
  SetInt(py, argsDict, "BaseVertexLocation", BaseVertexLocation);

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11DeviceContext::DrawIndexed", argsDict);
  py.Py_DecRef(argsDict);
}

void OnDrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation,
                     UINT StartInstanceLocation)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "VertexCountPerInstance", VertexCountPerInstance);
  SetUInt(py, argsDict, "InstanceCount", InstanceCount);
  SetUInt(py, argsDict, "StartVertexLocation", StartVertexLocation);
  SetUInt(py, argsDict, "StartInstanceLocation", StartInstanceLocation);

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11DeviceContext::DrawInstanced", argsDict);
  py.Py_DecRef(argsDict);
}

void OnDrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount,
                            UINT StartIndexLocation, INT BaseVertexLocation,
                            UINT StartInstanceLocation)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "IndexCountPerInstance", IndexCountPerInstance);
  SetUInt(py, argsDict, "InstanceCount", InstanceCount);
  SetUInt(py, argsDict, "StartIndexLocation", StartIndexLocation);
  SetInt(py, argsDict, "BaseVertexLocation", BaseVertexLocation);
  SetUInt(py, argsDict, "StartInstanceLocation", StartInstanceLocation);

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11DeviceContext::DrawIndexedInstanced", argsDict);
  py.Py_DecRef(argsDict);
}

void OnDispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "ThreadGroupCountX", ThreadGroupCountX);
  SetUInt(py, argsDict, "ThreadGroupCountY", ThreadGroupCountY);
  SetUInt(py, argsDict, "ThreadGroupCountZ", ThreadGroupCountZ);

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11DeviceContext::Dispatch", argsDict);
  py.Py_DecRef(argsDict);
}

//=============================================================================
// Context resource update hooks
//=============================================================================

void OnUpdateSubresource(ResourceId dstResource, UINT DstSubresource, const D3D11_BOX *pDstBox,
                         UINT SrcRowPitch, UINT SrcDepthPitch)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "pDstResource", ResIdToU64(dstResource));
  SetUInt(py, argsDict, "DstSubresource", DstSubresource);
  SetUInt(py, argsDict, "SrcRowPitch", SrcRowPitch);
  SetUInt(py, argsDict, "SrcDepthPitch", SrcDepthPitch);

  if(pDstBox)
  {
    PyObject *boxDict = py.PyDict_New();
    SetUInt(py, boxDict, "left", pDstBox->left);
    SetUInt(py, boxDict, "top", pDstBox->top);
    SetUInt(py, boxDict, "front", pDstBox->front);
    SetUInt(py, boxDict, "right", pDstBox->right);
    SetUInt(py, boxDict, "bottom", pDstBox->bottom);
    SetUInt(py, boxDict, "back", pDstBox->back);
    py.PyDict_SetItemString(argsDict, "pDstBox", boxDict);
    py.Py_DecRef(boxDict);
  }

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11DeviceContext::UpdateSubresource", argsDict);
  py.Py_DecRef(argsDict);
}

void OnMap(ResourceId resource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "pResource", ResIdToU64(resource));
  SetUInt(py, argsDict, "Subresource", Subresource);
  SetUInt(py, argsDict, "MapType", (unsigned long long)MapType);
  SetUInt(py, argsDict, "MapFlags", MapFlags);

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11DeviceContext::Map", argsDict);
  py.Py_DecRef(argsDict);
}

void OnCopyResource(ResourceId dstResource, ResourceId srcResource)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "pDstResource", ResIdToU64(dstResource));
  SetUInt(py, argsDict, "pSrcResource", ResIdToU64(srcResource));

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11DeviceContext::CopyResource", argsDict);
  py.Py_DecRef(argsDict);
}

void OnCopySubresourceRegion(ResourceId dstResource, UINT DstSubresource, UINT DstX, UINT DstY,
                             UINT DstZ, ResourceId srcResource, UINT SrcSubresource,
                             const D3D11_BOX *pSrcBox)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "pDstResource", ResIdToU64(dstResource));
  SetUInt(py, argsDict, "DstSubresource", DstSubresource);
  SetUInt(py, argsDict, "DstX", DstX);
  SetUInt(py, argsDict, "DstY", DstY);
  SetUInt(py, argsDict, "DstZ", DstZ);
  SetUInt(py, argsDict, "pSrcResource", ResIdToU64(srcResource));
  SetUInt(py, argsDict, "SrcSubresource", SrcSubresource);

  if(pSrcBox)
  {
    PyObject *boxDict = py.PyDict_New();
    SetUInt(py, boxDict, "left", pSrcBox->left);
    SetUInt(py, boxDict, "top", pSrcBox->top);
    SetUInt(py, boxDict, "front", pSrcBox->front);
    SetUInt(py, boxDict, "right", pSrcBox->right);
    SetUInt(py, boxDict, "bottom", pSrcBox->bottom);
    SetUInt(py, boxDict, "back", pSrcBox->back);
    py.PyDict_SetItemString(argsDict, "pSrcBox", boxDict);
    py.Py_DecRef(boxDict);
  }

  ApiMonitor::Inst().InvokeHookWithDict("ID3D11DeviceContext::CopySubresourceRegion", argsDict);
  py.Py_DecRef(argsDict);
}

//=============================================================================
// Context shader resource binding hooks
//=============================================================================

static void OnSetShaderResources(const char *apiName, UINT StartSlot, UINT NumViews,
                                 ResourceId *viewIds)
{
  if(!ApiMonitor::Inst().IsActive())
    return;

  ApiMonitor::ScopedGIL gil(ApiMonitor::Inst());
  if(!gil.IsValid())
    return;
  PythonLoader &py = ApiMonitor::Inst().GetPython();
  PyObject *argsDict = py.PyDict_New();

  SetUInt(py, argsDict, "StartSlot", StartSlot);
  SetUInt(py, argsDict, "NumViews", NumViews);

  PyObject *viewList = py.PyList_New(0);
  for(UINT i = 0; i < NumViews; i++)
  {
    PyObject *id = py.PyLong_FromUnsignedLongLong(ResIdToU64(viewIds[i]));
    py.PyList_Append(viewList, id);
    py.Py_DecRef(id);
  }
  py.PyDict_SetItemString(argsDict, "views", viewList);
  py.Py_DecRef(viewList);

  ApiMonitor::Inst().InvokeHookWithDict(apiName, argsDict);
  py.Py_DecRef(argsDict);
}

void OnPSSetShaderResources(UINT StartSlot, UINT NumViews, ResourceId *viewIds)
{
  OnSetShaderResources("ID3D11DeviceContext::PSSetShaderResources", StartSlot, NumViews, viewIds);
}

void OnVSSetShaderResources(UINT StartSlot, UINT NumViews, ResourceId *viewIds)
{
  OnSetShaderResources("ID3D11DeviceContext::VSSetShaderResources", StartSlot, NumViews, viewIds);
}

void OnCSSetShaderResources(UINT StartSlot, UINT NumViews, ResourceId *viewIds)
{
  OnSetShaderResources("ID3D11DeviceContext::CSSetShaderResources", StartSlot, NumViews, viewIds);
}

}    // namespace D3D11ApiMonitor

#endif    // RENDERDOC_ENABLE_API_MONITOR
