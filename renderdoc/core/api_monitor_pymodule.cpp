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

// This file implements the _rdcmonitor Python C extension module.
// It is registered as a built-in module via PyImport_AppendInittab before Py_Initialize.
//
// Python API exposed:
//   monitor.on(api_name, callback)     - Register a hook for an API call
//   monitor.off(api_name)              - Unregister a hook
//   monitor.log(message)               - Log a message (sent to UI)
//   monitor.track(resource_id)         - Track a resource by ID
//   monitor.untrack(resource_id)       - Stop tracking a resource
//   monitor.is_tracked(resource_id)    - Check if a resource is tracked
//   monitor.dump(resource_id, path)    - Request a GPU resource dump to file
//   monitor.format_name(format_int)    - Get human-readable name for DXGI_FORMAT

#include "api_monitor.h"

#if RENDERDOC_ENABLE_API_MONITOR

#include "common/formatting.h"
#include "core/core.h"
#include "strings/string_utils.h"

// DXGI format names for format_name()
static const char *GetDXGIFormatName(int fmt)
{
  switch(fmt)
  {
    case 0: return "UNKNOWN";
    case 1: return "R32G32B32A32_TYPELESS";
    case 2: return "R32G32B32A32_FLOAT";
    case 3: return "R32G32B32A32_UINT";
    case 4: return "R32G32B32A32_SINT";
    case 5: return "R32G32B32_TYPELESS";
    case 6: return "R32G32B32_FLOAT";
    case 7: return "R32G32B32_UINT";
    case 8: return "R32G32B32_SINT";
    case 9: return "R16G16B16A16_TYPELESS";
    case 10: return "R16G16B16A16_FLOAT";
    case 11: return "R16G16B16A16_UNORM";
    case 12: return "R16G16B16A16_UINT";
    case 13: return "R16G16B16A16_SNORM";
    case 14: return "R16G16B16A16_SINT";
    case 15: return "R32G32_TYPELESS";
    case 16: return "R32G32_FLOAT";
    case 17: return "R32G32_UINT";
    case 18: return "R32G32_SINT";
    case 24: return "R10G10B10A2_TYPELESS";
    case 25: return "R10G10B10A2_UNORM";
    case 26: return "R10G10B10A2_UINT";
    case 27: return "R11G11B10_FLOAT";
    case 28: return "R8G8B8A8_TYPELESS";
    case 29: return "R8G8B8A8_UNORM";
    case 30: return "R8G8B8A8_UNORM_SRGB";
    case 31: return "R8G8B8A8_UINT";
    case 32: return "R8G8B8A8_SNORM";
    case 33: return "R8G8B8A8_SINT";
    case 34: return "R16G16_TYPELESS";
    case 35: return "R16G16_FLOAT";
    case 36: return "R16G16_UNORM";
    case 37: return "R16G16_UINT";
    case 38: return "R16G16_SNORM";
    case 39: return "R16G16_SINT";
    case 40: return "R32_TYPELESS";
    case 41: return "D32_FLOAT";
    case 42: return "R32_FLOAT";
    case 43: return "R32_UINT";
    case 44: return "R32_SINT";
    case 45: return "R24G8_TYPELESS";
    case 46: return "D24_UNORM_S8_UINT";
    case 47: return "R24_UNORM_X8_TYPELESS";
    case 49: return "R8G8_TYPELESS";
    case 50: return "R8G8_UNORM";
    case 51: return "R8G8_UINT";
    case 52: return "R8G8_SNORM";
    case 53: return "R8G8_SINT";
    case 54: return "R16_TYPELESS";
    case 55: return "R16_FLOAT";
    case 56: return "D16_UNORM";
    case 57: return "R16_UNORM";
    case 58: return "R16_UINT";
    case 59: return "R16_SNORM";
    case 60: return "R16_SINT";
    case 61: return "R8_TYPELESS";
    case 62: return "R8_UNORM";
    case 63: return "R8_UINT";
    case 64: return "R8_SNORM";
    case 65: return "R8_SINT";
    case 66: return "A8_UNORM";
    case 67: return "R1_UNORM";
    case 70: return "R9G9B9E5_SHAREDEXP";
    case 71: return "BC1_TYPELESS";
    case 72: return "BC1_UNORM";
    case 73: return "BC1_UNORM_SRGB";
    case 74: return "BC2_TYPELESS";
    case 75: return "BC2_UNORM";
    case 76: return "BC2_UNORM_SRGB";
    case 77: return "BC3_TYPELESS";
    case 78: return "BC3_UNORM";
    case 79: return "BC3_UNORM_SRGB";
    case 80: return "BC4_TYPELESS";
    case 81: return "BC4_UNORM";
    case 82: return "BC4_SNORM";
    case 83: return "BC5_TYPELESS";
    case 84: return "BC5_UNORM";
    case 85: return "BC5_SNORM";
    case 86: return "B5G6R5_UNORM";
    case 87: return "B5G5R5A1_UNORM";
    case 88: return "B8G8R8A8_UNORM";
    case 89: return "B8G8R8X8_UNORM";
    case 90: return "R10G10B10_XR_BIAS_A2_UNORM";
    case 91: return "B8G8R8A8_TYPELESS";
    case 92: return "B8G8R8A8_UNORM_SRGB";
    case 93: return "B8G8R8X8_TYPELESS";
    case 94: return "B8G8R8X8_UNORM_SRGB";
    case 95: return "BC6H_TYPELESS";
    case 96: return "BC6H_UF16";
    case 97: return "BC6H_SF16";
    case 98: return "BC7_TYPELESS";
    case 99: return "BC7_UNORM";
    case 100: return "BC7_UNORM_SRGB";
    default: return "UNKNOWN";
  }
}

// Helper to get Python string from a PyObject arg
static rdcstr PyArgToString(PythonLoader &py, PyObject *arg)
{
  if(!arg)
    return "";
  PyObject *str = py.PyObject_Str(arg);
  if(!str)
    return "";
  PyObject *utf8 = py.PyUnicode_AsUTF8String(str);
  py.Py_DecRef(str);
  if(!utf8)
    return "";
  const char *cstr = py.PyBytes_AsString(utf8);
  rdcstr result = cstr ? cstr : "";
  py.Py_DecRef(utf8);
  return result;
}

//=============================================================================
// Python module method implementations
//=============================================================================

// monitor.on(api_name: str, callback: callable) -> None
static PyObject *rdcmonitor_on(PyObject *self, PyObject *args)
{
  PythonLoader &py = ApiMonitor::Inst().GetPython();

  if(!args || py.PyTuple_GetItem(args, 0) == NULL || py.PyTuple_GetItem(args, 1) == NULL)
  {
    py.PyErr_SetString(*py.PyExc_TypeError_Ptr, "monitor.on() requires (api_name, callback)");
    return NULL;
  }

  PyObject *pyApiName = py.PyTuple_GetItem(args, 0);
  PyObject *pyCallback = py.PyTuple_GetItem(args, 1);

  rdcstr apiName = PyArgToString(py, pyApiName);
  if(apiName.empty())
  {
    py.PyErr_SetString(*py.PyExc_TypeError_Ptr, "api_name must be a non-empty string");
    return NULL;
  }

  if(!py.PyCallable_Check(pyCallback))
  {
    py.PyErr_SetString(*py.PyExc_TypeError_Ptr, "callback must be callable");
    return NULL;
  }

  // Store the callback in our hook dict (ApiMonitor manages it)
  // Get the hook dict via the ApiMonitor
  // We need to access m_HookDict - but it's private. Instead, we set it through the module attribute.
  PyObject *hookDict = py.PyObject_GetAttrString(self, "_hooks");
  if(!hookDict)
  {
    py.PyErr_Clear();
    hookDict = py.PyDict_New();
    py.PyObject_SetAttrString(self, "_hooks", hookDict);
  }

  py.Py_IncRef(pyCallback);
  py.PyDict_SetItemString(hookDict, apiName.c_str(), pyCallback);
  py.Py_DecRef(hookDict);

  // Also register in the C++ side ApiMonitor hook dict
  // Access through a global that api_monitor.cpp sets up
  extern PyObject *g_ApiMonitorHookDict;
  if(g_ApiMonitorHookDict)
  {
    py.PyDict_SetItemString(g_ApiMonitorHookDict, apiName.c_str(), pyCallback);
  }

  py.Py_IncRef(py.GetNone());
  return py.GetNone();
}

// monitor.off(api_name: str) -> None
static PyObject *rdcmonitor_off(PyObject *self, PyObject *args)
{
  PythonLoader &py = ApiMonitor::Inst().GetPython();

  PyObject *pyApiName = py.PyTuple_GetItem(args, 0);
  rdcstr apiName = PyArgToString(py, pyApiName);

  extern PyObject *g_ApiMonitorHookDict;
  if(g_ApiMonitorHookDict)
  {
    // Set to None effectively removes the hook
    py.PyDict_SetItemString(g_ApiMonitorHookDict, apiName.c_str(), py.GetNone());
  }

  py.Py_IncRef(py.GetNone());
  return py.GetNone();
}

// monitor.log(message: str) -> None
static PyObject *rdcmonitor_log(PyObject *self, PyObject *args)
{
  PythonLoader &py = ApiMonitor::Inst().GetPython();

  PyObject *pyMsg = py.PyTuple_GetItem(args, 0);
  rdcstr msg = PyArgToString(py, pyMsg);

  ApiMonitor::Inst().Log(msg);

  py.Py_IncRef(py.GetNone());
  return py.GetNone();
}

// monitor.track(resource_id: int) -> None
static PyObject *rdcmonitor_track(PyObject *self, PyObject *args)
{
  PythonLoader &py = ApiMonitor::Inst().GetPython();

  PyObject *pyResId = py.PyTuple_GetItem(args, 0);
  uint64_t resId = py.PyLong_AsUnsignedLongLong(pyResId);

  ResourceId id;
  // ResourceId is a wrapper around uint64_t
  memcpy(&id, &resId, sizeof(uint64_t));

  ApiMonitor::Inst().TrackResource(id);

  py.Py_IncRef(py.GetNone());
  return py.GetNone();
}

// monitor.untrack(resource_id: int) -> None
static PyObject *rdcmonitor_untrack(PyObject *self, PyObject *args)
{
  PythonLoader &py = ApiMonitor::Inst().GetPython();

  PyObject *pyResId = py.PyTuple_GetItem(args, 0);
  uint64_t resId = py.PyLong_AsUnsignedLongLong(pyResId);

  ResourceId id;
  memcpy(&id, &resId, sizeof(uint64_t));

  ApiMonitor::Inst().UntrackResource(id);

  py.Py_IncRef(py.GetNone());
  return py.GetNone();
}

// monitor.is_tracked(resource_id: int) -> bool
static PyObject *rdcmonitor_is_tracked(PyObject *self, PyObject *args)
{
  PythonLoader &py = ApiMonitor::Inst().GetPython();

  PyObject *pyResId = py.PyTuple_GetItem(args, 0);
  uint64_t resId = py.PyLong_AsUnsignedLongLong(pyResId);

  ResourceId id;
  memcpy(&id, &resId, sizeof(uint64_t));

  bool tracked = ApiMonitor::Inst().IsTracked(id);

  py.Py_IncRef(tracked ? py.GetTrue() : py.GetFalse());
  return tracked ? py.GetTrue() : py.GetFalse();
}

// monitor.dump(resource_id: int, output_path: str, format: str = "dds") -> None
static PyObject *rdcmonitor_dump(PyObject *self, PyObject *args)
{
  PythonLoader &py = ApiMonitor::Inst().GetPython();

  PyObject *pyResId = py.PyTuple_GetItem(args, 0);
  PyObject *pyPath = py.PyTuple_GetItem(args, 1);

  if(!pyResId || !pyPath)
  {
    py.PyErr_SetString(*py.PyExc_TypeError_Ptr,
                       "monitor.dump() requires (resource_id, output_path)");
    return NULL;
  }

  uint64_t resId = py.PyLong_AsUnsignedLongLong(pyResId);
  rdcstr path = PyArgToString(py, pyPath);

  // Determine format from extension or third arg
  rdcstr format = "dds";
  if(path.contains(".png"))
    format = "png";
  else if(path.contains(".bmp"))
    format = "bmp";

  // Check for optional third argument
  PyObject *pyFmt = py.PyTuple_GetItem(args, 2);
  if(pyFmt && pyFmt != py.GetNone())
  {
    format = PyArgToString(py, pyFmt);
  }

  ResourceId id;
  memcpy(&id, &resId, sizeof(uint64_t));

  ApiMonitor::Inst().RequestDump(id, path, format);
  ApiMonitor::Inst().Log(
      StringFormat::Fmt("Dump requested: resource %llu -> %s", resId, path.c_str()));

  py.Py_IncRef(py.GetNone());
  return py.GetNone();
}

// monitor.format_name(format_int: int) -> str
static PyObject *rdcmonitor_format_name(PyObject *self, PyObject *args)
{
  PythonLoader &py = ApiMonitor::Inst().GetPython();

  PyObject *pyFmt = py.PyTuple_GetItem(args, 0);
  int fmt = (int)py.PyLong_AsLong(pyFmt);

  const char *name = GetDXGIFormatName(fmt);
  return py.PyUnicode_FromString(name);
}

// monitor.trigger_capture(num_frames=1)
// Triggers RenderDoc to capture the next N frames.
static PyObject *rdcmonitor_trigger_capture(PyObject *self, PyObject *args)
{
  PythonLoader &py = ApiMonitor::Inst().GetPython();

  uint32_t numFrames = 1;
  PyObject *pyNum = py.PyTuple_GetItem(args, 0);
  if(pyNum)
  {
    numFrames = (uint32_t)py.PyLong_AsLong(pyNum);
    if(numFrames == 0)
      numFrames = 1;
  }
  // Clear any Python error from PyTuple_GetItem if no arg was provided
  py.PyErr_Clear();

  RenderDoc::Inst().TriggerCapture(numFrames);
  ApiMonitor::Inst().Log(StringFormat::Fmt("Capture triggered (%u frame(s))", numFrames));

  py.Py_IncRef(py.GetNone());
  return py.GetNone();
}

//=============================================================================
// Module definition
//=============================================================================

static PyMethodDef rdcmonitor_methods[] = {
    {"on", rdcmonitor_on, PY_METH_VARARGS, "Register a hook for an API call"},
    {"off", rdcmonitor_off, PY_METH_VARARGS, "Unregister a hook for an API call"},
    {"log", rdcmonitor_log, PY_METH_VARARGS, "Log a message to the UI"},
    {"track", rdcmonitor_track, PY_METH_VARARGS, "Track a resource by ID"},
    {"untrack", rdcmonitor_untrack, PY_METH_VARARGS, "Stop tracking a resource"},
    {"is_tracked", rdcmonitor_is_tracked, PY_METH_VARARGS, "Check if a resource is tracked"},
    {"dump", rdcmonitor_dump, PY_METH_VARARGS, "Dump a GPU resource to a file"},
    {"format_name", rdcmonitor_format_name, PY_METH_VARARGS, "Get human-readable DXGI format name"},
    {"trigger_capture", rdcmonitor_trigger_capture, PY_METH_VARARGS,
     "Trigger RenderDoc to capture the next N frames"},
    {NULL, NULL, 0, NULL}};

// Global hook dict reference - set by ApiMonitor::InitialisePython()
PyObject *g_ApiMonitorHookDict = NULL;

static PyModuleDef rdcmonitor_module = {
    {0, NULL, NULL, 0, NULL},    // m_base (PyModuleDef_Base zeroed)
    "_rdcmonitor",               // m_name
    "RenderDoc API Monitor module for real-time graphics API hooking",    // m_doc
    -1,                          // m_size (per-interpreter state, -1 = global)
    rdcmonitor_methods,          // m_methods
    NULL,                        // m_slots
    NULL,                        // m_traverse
    NULL,                        // m_clear
    NULL,                        // m_free
};

PyObject *PyInit_rdcmonitor(void)
{
  PythonLoader &py = ApiMonitor::Inst().GetPython();

  // Module API version for Python 3.6 is 1013
  PyObject *module = py.PyModule_Create2(&rdcmonitor_module, 1013);

  // Store the hook dict reference so module methods can access it
  g_ApiMonitorHookDict = ApiMonitor::Inst().GetPython().PyDict_New();
  // Note: ApiMonitor::InitialisePython() will replace m_HookDict with this

  return module;
}

#endif    // RENDERDOC_ENABLE_API_MONITOR
