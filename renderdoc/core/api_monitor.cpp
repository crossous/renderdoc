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

#include "api_monitor.h"

#if RENDERDOC_ENABLE_API_MONITOR

#include "common/formatting.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

static inline uint64_t ResIdToU64(ResourceId id)
{
  uint64_t val;
  memcpy(&val, &id, sizeof(uint64_t));
  return val;
}

// Forward declaration - defined in api_monitor_pymodule.cpp
extern PyObject *PyInit_rdcmonitor(void);

ApiMonitor &ApiMonitor::Inst()
{
  static ApiMonitor inst;
  return inst;
}

ApiMonitor::~ApiMonitor()
{
  Shutdown();
}

void ApiMonitor::Initialise()
{
  RDCLOG("API Monitor: Initialising");
  m_Status = "Idle - Python not loaded";
}

void ApiMonitor::Shutdown()
{
  m_Active = false;
  ShutdownPython();
}

void ApiMonitor::Tick()
{
  // On first few frames, check for a pending script file written by the UI.
  // This is the mechanism for Global Hook mode (no LiveCapture/TargetControl connection).
  if(!m_PendingScriptChecked)
  {
    m_TickCount++;
    if(m_TickCount >= 10)
    {
      m_PendingScriptChecked = true;
      TryLoadPendingScript();
    }
  }

  // Drain queued D3D12 events each frame
  if(m_Active)
    DrainEvents();
}

bool ApiMonitor::LoadPython(const rdcstr &pythonDllPath)
{
  if(m_Python.IsLoaded())
    return true;

  if(!m_Python.Load(pythonDllPath))
  {
    m_Status = StringFormat::Fmt("Error: %s", m_Python.GetLoadError().c_str());
    return false;
  }

  m_Status = "Python loaded - no script";
  return true;
}

bool ApiMonitor::InitialisePython()
{
  if(m_PythonInitialised)
    return true;

  if(!m_Python.IsLoaded())
    return false;

  SCOPED_LOCK(m_PythonLock);

  // Register our built-in module before Py_Initialize
  m_Python.PyImport_AppendInittab("_rdcmonitor", &PyInit_rdcmonitor);

  // Initialize Python without signal handlers (important for injected DLL)
  m_Python.Py_InitializeEx(0);

  if(!m_Python.Py_IsInitialized())
  {
    m_Status = "Error: Python failed to initialize";
    RDCERR("API Monitor: Py_InitializeEx failed");
    return false;
  }

  // Import our module
  m_MonitorModule = m_Python.PyImport_ImportModule("_rdcmonitor");
  if(!m_MonitorModule)
  {
    HandlePythonError("importing _rdcmonitor");
    m_Python.Py_Finalize();
    return false;
  }

  // Link the hook dictionary from the module (created in PyInit_rdcmonitor)
  extern PyObject *g_ApiMonitorHookDict;
  m_HookDict = g_ApiMonitorHookDict;
  if(!m_HookDict)
    m_HookDict = m_Python.PyDict_New();

  // Create global namespace for script execution
  PyObject *mainModule = m_Python.PyImport_ImportModule("__main__");
  m_GlobalDict = m_Python.PyModule_GetDict(mainModule);
  m_Python.Py_IncRef(m_GlobalDict);

  // Make _rdcmonitor available as 'monitor' in the global namespace
  m_Python.PyDict_SetItemString(m_GlobalDict, "monitor", m_MonitorModule);

  // Also import it so "import _rdcmonitor" works
  m_Python.PyDict_SetItemString(m_GlobalDict, "_rdcmonitor", m_MonitorModule);

  // Release the GIL so other threads can use it
  // (we'll re-acquire it when we need to run Python code)
  m_SavedThreadState = m_Python.PyEval_SaveThread();

  m_PythonInitialised = true;
  RDCLOG("API Monitor: Python initialized successfully");
  return true;
}

void ApiMonitor::ShutdownPython()
{
  if(!m_PythonInitialised)
    return;

  SCOPED_LOCK(m_PythonLock);

  m_Active = false;

  if(m_Python.IsLoaded() && m_Python.Py_IsInitialized())
  {
    // Re-acquire the GIL before cleanup
    m_Python.PyEval_RestoreThread(m_SavedThreadState);
    m_SavedThreadState = NULL;

    if(m_HookDict)
    {
      m_Python.Py_DecRef(m_HookDict);
      m_HookDict = NULL;
    }
    if(m_GlobalDict)
    {
      m_Python.Py_DecRef(m_GlobalDict);
      m_GlobalDict = NULL;
    }
    m_MonitorModule = NULL;

    m_Python.Py_Finalize();
  }

  m_PythonInitialised = false;
  m_Status = "Python loaded - no script";
  RDCLOG("API Monitor: Python shut down");
}

bool ApiMonitor::LoadScript(const rdcstr &scriptSource, rdcstr &errorOut)
{
  // Set up log file if not already set
  if(m_LogFilePath.empty())
  {
    rdcstr selfDir = GetSelfDirectory();
    if(!selfDir.empty())
      m_LogFilePath = selfDir + "rdcmonitor_log.txt";
    else
      m_LogFilePath = "rdcmonitor_log.txt";

    // Clear previous log file
    FILE *logf = fopen(m_LogFilePath.c_str(), "w");
    if(logf)
      fclose(logf);
  }

  // Ensure Python is loaded and initialized
  if(!m_Python.IsLoaded())
  {
    errorOut = "Python DLL not loaded. Call LoadPython() first.";
    return false;
  }

  if(!m_PythonInitialised && !InitialisePython())
  {
    errorOut = "Failed to initialize Python: " + m_Status;
    return false;
  }

  SCOPED_LOCK(m_PythonLock);

  // Acquire GIL
  int gilState = m_Python.PyGILState_Ensure();

  // Clear existing hooks
  m_Python.Py_DecRef(m_HookDict);
  m_HookDict = m_Python.PyDict_New();

  // Clear tracked resources
  {
    SCOPED_WRITELOCK(m_TrackLock);
    m_TrackedResources.clear();
  }

  // Execute the script in our global namespace
  PyObject *result =
      m_Python.PyRun_String(scriptSource.c_str(), PythonLoader::FileInput, m_GlobalDict, m_GlobalDict);

  if(!result)
  {
    // Script had an error
    PyObject *exType = NULL, *exValue = NULL, *exTraceback = NULL;
    m_Python.PyErr_Fetch(&exType, &exValue, &exTraceback);

    if(exValue)
    {
      errorOut = PyObjectToString(exValue);
    }
    else
    {
      errorOut = "Unknown Python error";
    }

    if(exType)
      m_Python.Py_DecRef(exType);
    if(exValue)
      m_Python.Py_DecRef(exValue);
    if(exTraceback)
      m_Python.Py_DecRef(exTraceback);

    m_Status = StringFormat::Fmt("Script error: %s", errorOut.c_str());
    m_Python.PyGILState_Release(gilState);
    return false;
  }

  m_Python.Py_DecRef(result);
  m_LastScript = scriptSource;

  // Check if any hooks were registered
  Py_ssize_t hookCount = m_Python.PyDict_Size(m_HookDict);
  m_Active = (hookCount > 0);

  if(m_Active)
  {
    m_Status = StringFormat::Fmt("Active - %d hook(s) registered", (int)hookCount);
    Log(StringFormat::Fmt("API Monitor: Script loaded, %d hook(s) active", (int)hookCount));
  }
  else
  {
    m_Status = "Script loaded - no hooks registered (call monitor.on() to register hooks)";
  }

  m_Python.PyGILState_Release(gilState);
  RDCLOG("API Monitor: Script loaded, %d hooks registered", (int)hookCount);
  return true;
}

void ApiMonitor::UnloadScript()
{
  SCOPED_LOCK(m_PythonLock);

  m_Active = false;

  if(m_PythonInitialised)
  {
    int gilState = m_Python.PyGILState_Ensure();

    if(m_HookDict)
    {
      m_Python.Py_DecRef(m_HookDict);
      m_HookDict = m_Python.PyDict_New();
    }

    m_Python.PyGILState_Release(gilState);
  }

  {
    SCOPED_WRITELOCK(m_TrackLock);
    m_TrackedResources.clear();
  }

  m_LastScript.clear();
  m_Status = "Python loaded - no script";
  Log("API Monitor: Script unloaded");
}

bool ApiMonitor::ReloadScript(rdcstr &errorOut)
{
  if(m_LastScript.empty())
  {
    errorOut = "No script previously loaded";
    return false;
  }
  return LoadScript(m_LastScript, errorOut);
}

rdcstr ApiMonitor::GetStatus() const
{
  return m_Status;
}

void ApiMonitor::InvokeHook(const char *apiName, const void *paramData, BuildCallArgsFn buildArgs,
                            ResourceId result)
{
  if(!m_Active || !m_PythonInitialised)
    return;

  SCOPED_LOCK(m_PythonLock);

  int gilState = m_Python.PyGILState_Ensure();

  // Look up the callback for this API name
  PyObject *callback = m_Python.PyDict_GetItemString(m_HookDict, apiName);
  if(!callback || !m_Python.PyCallable_Check(callback))
  {
    m_Python.PyGILState_Release(gilState);
    return;
  }

  // Build the call info dict
  PyObject *callDict = m_Python.PyDict_New();

  // Set apiName
  PyObject *pyApiName = m_Python.PyUnicode_FromString(apiName);
  m_Python.PyDict_SetItemString(callDict, "apiName", pyApiName);
  m_Python.Py_DecRef(pyApiName);

  // Build and set args
  if(buildArgs && paramData)
  {
    PyObject *argsDict = buildArgs(m_Python, paramData);
    if(argsDict)
    {
      m_Python.PyDict_SetItemString(callDict, "args", argsDict);
      m_Python.Py_DecRef(argsDict);
    }
  }

  // Set result ResourceId
  if(result != ResourceId())
  {
    PyObject *pyResult = m_Python.PyLong_FromUnsignedLongLong(IsTracked(result) ? 1 : 0);
    // Store the actual resource ID as an integer for tracking
    PyObject *pyResId = m_Python.PyLong_FromUnsignedLongLong(ResIdToU64(result));
    m_Python.PyDict_SetItemString(callDict, "result", pyResId);
    m_Python.Py_DecRef(pyResult);
    m_Python.Py_DecRef(pyResId);
  }

  // Call the callback with the call dict as argument
  PyObject *argsTuple = m_Python.PyTuple_New(1);
  m_Python.PyTuple_SetItem(argsTuple, 0, callDict);    // steals reference to callDict

  PyObject *ret = m_Python.PyObject_CallObject(callback, argsTuple);

  if(!ret)
  {
    HandlePythonError(apiName);
  }
  else
  {
    m_Python.Py_DecRef(ret);
  }

  m_Python.Py_DecRef(argsTuple);

  m_Python.PyGILState_Release(gilState);
}

void ApiMonitor::InvokeHookWithDict(const char *apiName, PyObject *argsDict, ResourceId result)
{
  if(!m_Active || !m_PythonInitialised)
    return;

  SCOPED_LOCK(m_PythonLock);

  int gilState = m_Python.PyGILState_Ensure();

  PyObject *callback = m_Python.PyDict_GetItemString(m_HookDict, apiName);
  if(!callback || !m_Python.PyCallable_Check(callback))
  {
    m_Python.PyGILState_Release(gilState);
    return;
  }

  PyObject *callDict = m_Python.PyDict_New();

  PyObject *pyApiName = m_Python.PyUnicode_FromString(apiName);
  m_Python.PyDict_SetItemString(callDict, "apiName", pyApiName);
  m_Python.Py_DecRef(pyApiName);

  if(argsDict)
  {
    m_Python.PyDict_SetItemString(callDict, "args", argsDict);
  }

  if(result != ResourceId())
  {
    PyObject *pyResId = m_Python.PyLong_FromUnsignedLongLong(ResIdToU64(result));
    m_Python.PyDict_SetItemString(callDict, "result", pyResId);
    m_Python.Py_DecRef(pyResId);
  }

  PyObject *argsTuple = m_Python.PyTuple_New(1);
  m_Python.PyTuple_SetItem(argsTuple, 0, callDict);

  PyObject *ret = m_Python.PyObject_CallObject(callback, argsTuple);

  if(!ret)
    HandlePythonError(apiName);
  else
    m_Python.Py_DecRef(ret);

  m_Python.Py_DecRef(argsTuple);
  m_Python.PyGILState_Release(gilState);
}

void ApiMonitor::PushEvent(const MonitorEvent &evt)
{
  SCOPED_LOCK(m_EventLock);
  if((int)m_EventQueue.size() < MAX_QUEUED_EVENTS)
    m_EventQueue.push_back(evt);
}

// Helper: set int on dict
static inline void EvtSetUInt(PythonLoader &py, PyObject *dict, const char *key,
                              unsigned long long val)
{
  PyObject *pyVal = py.PyLong_FromUnsignedLongLong(val);
  py.PyDict_SetItemString(dict, key, pyVal);
  py.Py_DecRef(pyVal);
}

static inline void EvtSetInt(PythonLoader &py, PyObject *dict, const char *key, long long val)
{
  PyObject *pyVal = py.PyLong_FromLongLong(val);
  py.PyDict_SetItemString(dict, key, pyVal);
  py.Py_DecRef(pyVal);
}

void ApiMonitor::DrainEvents()
{
  // Swap the queue out under lock, then process without holding the lock
  rdcarray<MonitorEvent> events;
  {
    SCOPED_LOCK(m_EventLock);
    events.swap(m_EventQueue);
  }

  if(events.empty())
    return;

  SCOPED_LOCK(m_PythonLock);
  int gilState = m_Python.PyGILState_Ensure();

  for(const MonitorEvent &evt : events)
  {
    if(evt.type == MonitorEvent::CreateResource)
    {
      // CreateResource: build desc dict and fire generic + dimension-specific hooks
      PyObject *argsDict = m_Python.PyDict_New();

      PyObject *descDict = m_Python.PyDict_New();
      const char *dimNames[] = {"UNKNOWN", "BUFFER", "TEXTURE1D", "TEXTURE2D", "TEXTURE3D"};
      const char *dimName = evt.createRes.dimension <= 4 ? dimNames[evt.createRes.dimension] : "UNKNOWN";
      {
        PyObject *v = m_Python.PyUnicode_FromString(dimName);
        m_Python.PyDict_SetItemString(descDict, "Dimension", v);
        m_Python.Py_DecRef(v);
      }
      EvtSetUInt(m_Python, descDict, "Alignment", evt.createRes.alignment);
      EvtSetUInt(m_Python, descDict, "Width", evt.createRes.width);
      EvtSetUInt(m_Python, descDict, "Height", evt.createRes.height);
      EvtSetUInt(m_Python, descDict, "DepthOrArraySize", evt.createRes.depthOrArraySize);
      EvtSetUInt(m_Python, descDict, "MipLevels", evt.createRes.mipLevels);
      EvtSetUInt(m_Python, descDict, "Format", evt.createRes.format);
      EvtSetUInt(m_Python, descDict, "SampleCount", evt.createRes.sampleCount);
      EvtSetUInt(m_Python, descDict, "SampleQuality", evt.createRes.sampleQuality);
      EvtSetUInt(m_Python, descDict, "Layout", evt.createRes.layout);
      EvtSetUInt(m_Python, descDict, "Flags", evt.createRes.flags);
      m_Python.PyDict_SetItemString(argsDict, "pResourceDesc", descDict);
      m_Python.Py_DecRef(descDict);

      if(evt.createRes.hasHeapProps)
      {
        PyObject *heapDict = m_Python.PyDict_New();
        EvtSetUInt(m_Python, heapDict, "Type", evt.createRes.heapType);
        EvtSetUInt(m_Python, heapDict, "CPUPageProperty", evt.createRes.cpuPageProp);
        EvtSetUInt(m_Python, heapDict, "MemoryPoolPreference", evt.createRes.memPoolPref);
        m_Python.PyDict_SetItemString(argsDict, "pHeapProperties", heapDict);
        m_Python.Py_DecRef(heapDict);
      }

      EvtSetUInt(m_Python, argsDict, "HeapFlags", evt.createRes.heapFlags);

      // Helper lambda to invoke a hook with argsDict + result
      auto invokeCreate = [&](const char *name) {
        PyObject *callback = m_Python.PyDict_GetItemString(m_HookDict, name);
        if(!callback || !m_Python.PyCallable_Check(callback))
          return;

        PyObject *callDict = m_Python.PyDict_New();
        PyObject *pyName = m_Python.PyUnicode_FromString(name);
        m_Python.PyDict_SetItemString(callDict, "apiName", pyName);
        m_Python.Py_DecRef(pyName);
        m_Python.PyDict_SetItemString(callDict, "args", argsDict);

        PyObject *pyResId = m_Python.PyLong_FromUnsignedLongLong(evt.createRes.resId);
        m_Python.PyDict_SetItemString(callDict, "result", pyResId);
        m_Python.Py_DecRef(pyResId);

        PyObject *argsTuple = m_Python.PyTuple_New(1);
        m_Python.PyTuple_SetItem(argsTuple, 0, callDict);

        PyObject *ret = m_Python.PyObject_CallObject(callback, argsTuple);
        if(!ret)
          HandlePythonError(name);
        else
          m_Python.Py_DecRef(ret);
        m_Python.Py_DecRef(argsTuple);
      };

      invokeCreate("ID3D12Device::CreateResource");
      if(evt.createRes.dimension == 3)    // TEXTURE2D
        invokeCreate("ID3D12Device::CreateTexture2D");
      else if(evt.createRes.dimension == 1)    // BUFFER
        invokeCreate("ID3D12Device::CreateBuffer");
      else if(evt.createRes.dimension == 4)    // TEXTURE3D
        invokeCreate("ID3D12Device::CreateTexture3D");

      m_Python.Py_DecRef(argsDict);
      continue;
    }

    // Non-CreateResource events
    const char *apiName = NULL;
    PyObject *argsDict = m_Python.PyDict_New();

    switch(evt.type)
    {
      case MonitorEvent::Draw:
        apiName = "ID3D12GraphicsCommandList::DrawInstanced";
        EvtSetUInt(m_Python, argsDict, "VertexCountPerInstance", evt.draw.vertexCount);
        EvtSetUInt(m_Python, argsDict, "InstanceCount", evt.draw.instanceCount);
        EvtSetUInt(m_Python, argsDict, "StartVertexLocation", evt.draw.startVertex);
        EvtSetUInt(m_Python, argsDict, "StartInstanceLocation", evt.draw.startInstance);
        break;
      case MonitorEvent::DrawIndexed:
        apiName = "ID3D12GraphicsCommandList::DrawIndexedInstanced";
        EvtSetUInt(m_Python, argsDict, "IndexCountPerInstance", evt.drawIndexed.indexCount);
        EvtSetUInt(m_Python, argsDict, "InstanceCount", evt.drawIndexed.instanceCount);
        EvtSetUInt(m_Python, argsDict, "StartIndexLocation", evt.drawIndexed.startIndex);
        EvtSetInt(m_Python, argsDict, "BaseVertexLocation", evt.drawIndexed.baseVertex);
        EvtSetUInt(m_Python, argsDict, "StartInstanceLocation", evt.drawIndexed.startInstance);
        break;
      case MonitorEvent::Dispatch:
        apiName = "ID3D12GraphicsCommandList::Dispatch";
        EvtSetUInt(m_Python, argsDict, "ThreadGroupCountX", evt.dispatch.x);
        EvtSetUInt(m_Python, argsDict, "ThreadGroupCountY", evt.dispatch.y);
        EvtSetUInt(m_Python, argsDict, "ThreadGroupCountZ", evt.dispatch.z);
        break;
      case MonitorEvent::CopyTexture:
        apiName = "ID3D12GraphicsCommandList::CopyTextureRegion";
        EvtSetUInt(m_Python, argsDict, "pDst", evt.copyTex.dst);
        EvtSetUInt(m_Python, argsDict, "DstSubresource", evt.copyTex.dstSub);
        EvtSetUInt(m_Python, argsDict, "pSrc", evt.copyTex.src);
        EvtSetUInt(m_Python, argsDict, "SrcSubresource", evt.copyTex.srcSub);
        break;
      case MonitorEvent::CopyBuffer:
        apiName = "ID3D12GraphicsCommandList::CopyBufferRegion";
        EvtSetUInt(m_Python, argsDict, "pDst", evt.copyBuf.dst);
        EvtSetUInt(m_Python, argsDict, "DstOffset", evt.copyBuf.dstOff);
        EvtSetUInt(m_Python, argsDict, "pSrc", evt.copyBuf.src);
        EvtSetUInt(m_Python, argsDict, "SrcOffset", evt.copyBuf.srcOff);
        EvtSetUInt(m_Python, argsDict, "NumBytes", evt.copyBuf.numBytes);
        break;
      case MonitorEvent::CopyResource:
        apiName = "ID3D12GraphicsCommandList::CopyResource";
        EvtSetUInt(m_Python, argsDict, "pDst", evt.copyRes.dst);
        EvtSetUInt(m_Python, argsDict, "pSrc", evt.copyRes.src);
        break;
      case MonitorEvent::Barrier:
      {
        apiName = "ID3D12GraphicsCommandList::ResourceBarrier";
        EvtSetUInt(m_Python, argsDict, "NumBarriers", evt.barrier.count);
        PyObject *resList = m_Python.PyList_New(0);
        UINT n = evt.barrier.count > 8 ? 8 : evt.barrier.count;
        for(UINT i = 0; i < n; i++)
        {
          PyObject *id = m_Python.PyLong_FromUnsignedLongLong(evt.barrier.resources[i]);
          m_Python.PyList_Append(resList, id);
          m_Python.Py_DecRef(id);
        }
        m_Python.PyDict_SetItemString(argsDict, "resources", resList);
        m_Python.Py_DecRef(resList);
        break;
      }
      default: break;
    }

    if(!apiName)
    {
      m_Python.Py_DecRef(argsDict);
      continue;
    }

    // Look up callback
    PyObject *callback = m_Python.PyDict_GetItemString(m_HookDict, apiName);
    if(callback && m_Python.PyCallable_Check(callback))
    {
      PyObject *callDict = m_Python.PyDict_New();

      PyObject *pyApiName = m_Python.PyUnicode_FromString(apiName);
      m_Python.PyDict_SetItemString(callDict, "apiName", pyApiName);
      m_Python.Py_DecRef(pyApiName);

      m_Python.PyDict_SetItemString(callDict, "args", argsDict);

      PyObject *argsTuple = m_Python.PyTuple_New(1);
      m_Python.PyTuple_SetItem(argsTuple, 0, callDict);

      PyObject *ret = m_Python.PyObject_CallObject(callback, argsTuple);
      if(!ret)
        HandlePythonError(apiName);
      else
        m_Python.Py_DecRef(ret);

      m_Python.Py_DecRef(argsTuple);
    }

    m_Python.Py_DecRef(argsDict);
  }

  m_Python.PyGILState_Release(gilState);
}

void ApiMonitor::TrackResource(ResourceId id)
{
  SCOPED_WRITELOCK(m_TrackLock);
  m_TrackedResources.insert(ResIdToU64(id));
}

void ApiMonitor::UntrackResource(ResourceId id)
{
  SCOPED_WRITELOCK(m_TrackLock);
  m_TrackedResources.erase(ResIdToU64(id));
}

bool ApiMonitor::IsTracked(ResourceId id) const
{
  SCOPED_READLOCK(m_TrackLock);
  return m_TrackedResources.find(ResIdToU64(id)) != m_TrackedResources.end();
}

void ApiMonitor::Log(const rdcstr &msg)
{
  rdcstr prefixed = StringFormat::Fmt("[F:%u] %s", m_FrameNumber, msg.c_str());

  SCOPED_LOCK(m_LogLock);
  m_LogBuffer.push_back(prefixed);

  // Also write to log file for debugging (when UI panel is not connected)
  if(!m_LogFilePath.empty())
  {
    FILE *f = fopen(m_LogFilePath.c_str(), "a");
    if(f)
    {
      rdcstr line = prefixed + "\n";
      fwrite(line.c_str(), 1, line.size(), f);
      fclose(f);
    }
  }

  // Also output to RenderDoc debug log
  RDCLOG("ApiMonitor: %s", prefixed.c_str());
}

rdcarray<rdcstr> ApiMonitor::DrainLogs()
{
  SCOPED_LOCK(m_LogLock);
  rdcarray<rdcstr> logs;
  logs.swap(m_LogBuffer);
  return logs;
}

bool ApiMonitor::HasPendingLogs() const
{
  SCOPED_LOCK(m_LogLock);
  return !m_LogBuffer.empty();
}

void ApiMonitor::RequestDump(ResourceId id, const rdcstr &outputPath, const rdcstr &format)
{
  SCOPED_LOCK(m_DumpLock);
  m_DumpRequests.push_back({id, outputPath, format});
}

rdcarray<ApiMonitor::DumpRequest> ApiMonitor::DrainDumpRequests()
{
  SCOPED_LOCK(m_DumpLock);
  rdcarray<DumpRequest> reqs;
  reqs.swap(m_DumpRequests);
  return reqs;
}

rdcstr ApiMonitor::PyObjectToString(PyObject *obj)
{
  if(!obj)
    return "(null)";

  PyObject *str = m_Python.PyObject_Str(obj);
  if(!str)
    return "(str() failed)";

  PyObject *utf8 = m_Python.PyUnicode_AsUTF8String(str);
  m_Python.Py_DecRef(str);

  if(!utf8)
    return "(encoding failed)";

  const char *cstr = m_Python.PyBytes_AsString(utf8);
  rdcstr result = cstr ? cstr : "(null bytes)";
  m_Python.Py_DecRef(utf8);
  return result;
}

void ApiMonitor::HandlePythonError(const char *context)
{
  PyObject *exType = NULL, *exValue = NULL, *exTraceback = NULL;
  m_Python.PyErr_Fetch(&exType, &exValue, &exTraceback);

  rdcstr errMsg;
  if(exValue)
    errMsg = PyObjectToString(exValue);
  else
    errMsg = "Unknown error";

  rdcstr fullMsg = StringFormat::Fmt("API Monitor error in %s: %s", context, errMsg.c_str());
  RDCWARN("%s", fullMsg.c_str());
  Log(fullMsg);

  if(exType)
    m_Python.Py_DecRef(exType);
  if(exValue)
    m_Python.Py_DecRef(exValue);
  if(exTraceback)
    m_Python.Py_DecRef(exTraceback);
}

rdcstr ApiMonitor::GetSelfDirectory()
{
#if ENABLED(RDOC_WIN32)
  char moduleDir[MAX_PATH] = {};
  HMODULE hSelf = NULL;
  static int s_marker = 0;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     (LPCSTR)&s_marker, &hSelf);
  if(hSelf)
  {
    GetModuleFileNameA(hSelf, moduleDir, MAX_PATH);
    char *lastSlash = strrchr(moduleDir, '\\');
    if(!lastSlash)
      lastSlash = strrchr(moduleDir, '/');
    if(lastSlash)
      lastSlash[1] = '\0';
    return rdcstr(moduleDir);
  }
#endif
  return "";
}

void ApiMonitor::TryLoadPendingScript()
{
  // Check for a pending script at the well-known temp path.
  // The UI writes this file before launching / enabling global hook.
  // We load it, then DELETE it so it won't be loaded again on next run.
  rdcstr tempDir;
#if ENABLED(RDOC_WIN32)
  char tempPath[MAX_PATH + 1] = {};
  GetTempPathA(MAX_PATH, tempPath);
  tempDir = rdcstr(tempPath);
#else
  tempDir = "/tmp/";
#endif

  rdcstr scriptPath = tempDir + "rdcmonitor_pending.py";

  if(!FileIO::exists(scriptPath))
    return;

  RDCLOG("API Monitor: Found pending script at %s", scriptPath.c_str());

  // Find python36.dll next to our DLL
  rdcstr selfDir = GetSelfDirectory();
  rdcstr pythonDllPath = selfDir.empty() ? rdcstr() : (selfDir + "python36.dll");

  if(!LoadPython(pythonDllPath))
  {
    RDCWARN("API Monitor: Pending script load failed - couldn't load Python: %s", m_Status.c_str());
    // Delete the file even on failure so we don't retry every launch
    FileIO::Delete(scriptPath);
    return;
  }

  // Read the script
  FILE *f = fopen(scriptPath.c_str(), "rb");
  if(!f)
  {
    RDCWARN("API Monitor: Couldn't open pending script %s", scriptPath.c_str());
    FileIO::Delete(scriptPath);
    return;
  }

  fseek(f, 0, SEEK_END);
  long fileSize = ftell(f);
  fseek(f, 0, SEEK_SET);

  rdcstr scriptSource;
  scriptSource.resize((size_t)fileSize);
  fread(&scriptSource[0], 1, (size_t)fileSize, f);
  fclose(f);

  // Delete the pending file BEFORE executing (so crash won't leave it dangling)
  FileIO::Delete(scriptPath);

  rdcstr errorOut;
  if(LoadScript(scriptSource, errorOut))
  {
    RDCLOG("API Monitor: Pending script loaded successfully");
  }
  else
  {
    RDCERR("API Monitor: Pending script error: %s", errorOut.c_str());
  }
}

#endif    // RENDERDOC_ENABLE_API_MONITOR
