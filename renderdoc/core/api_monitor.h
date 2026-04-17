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

#include "common/common.h"
#include "common/threading.h"
#include "core/python_loader.h"

#if RENDERDOC_ENABLE_API_MONITOR

#include <unordered_map>
#include <unordered_set>
#include "api/replay/resourceid.h"

// Event struct for D3D12 multi-threaded hooks.
// D3D12 command list hooks push these instead of acquiring GIL directly.
// DrainEvents() processes them on the Tick thread with a single GIL acquisition.
struct MonitorEvent
{
  enum Type
  {
    Draw,
    DrawIndexed,
    Dispatch,
    CopyTexture,
    CopyBuffer,
    CopyResource,
    Barrier,
    CreateResource
  };
  Type type;
  union
  {
    struct
    {
      UINT vertexCount, instanceCount, startVertex, startInstance;
    } draw;
    struct
    {
      UINT indexCount, instanceCount, startIndex;
      INT baseVertex;
      UINT startInstance;
    } drawIndexed;
    struct
    {
      UINT x, y, z;
    } dispatch;
    struct
    {
      uint64_t dst, src;
      UINT dstSub, srcSub;
    } copyTex;
    struct
    {
      uint64_t dst, src;
      UINT64 dstOff, srcOff, numBytes;
    } copyBuf;
    struct
    {
      uint64_t dst, src;
    } copyRes;
    struct
    {
      UINT count;
      uint64_t resources[8];
    } barrier;
    struct
    {
      uint64_t resId;
      UINT dimension;    // D3D12_RESOURCE_DIMENSION
      UINT64 alignment, width;
      UINT height, depthOrArraySize, mipLevels;
      UINT64 format;
      UINT sampleCount, sampleQuality;
      UINT64 layout, flags;
      bool hasHeapProps;
      UINT64 heapType, cpuPageProp, memPoolPref;
      UINT64 heapFlags;
    } createRes;
  };
};

// Callback type for building API call parameters as a Python dict.
// The function receives the PythonLoader reference and should return a new PyObject* dict
// representing the call arguments. Caller is responsible for reference.
typedef PyObject *(*BuildCallArgsFn)(PythonLoader &py, const void *paramData);

class ApiMonitor
{
public:
  static ApiMonitor &Inst();

  // Lifecycle
  void Initialise();
  void Shutdown();
  void Tick();    // Called each frame from RenderDoc::Tick(), handles deferred autoload

  // Set by driver Present after m_FrameCounter++, so log output matches overlay frame number
  void SetFrameNumber(uint32_t frame) { m_FrameNumber = frame; }

  // Fast path check - called from every hooked wrapper function.
  // Single volatile bool check, near-zero cost when disabled.
  inline bool IsActive() const { return m_Active; }

  // Python availability
  bool IsPythonAvailable() const { return m_Python.IsLoaded(); }
  rdcstr GetPythonError() const { return m_Python.GetLoadError(); }

  // Script management (called from target control thread)
  bool LoadScript(const rdcstr &scriptSource, rdcstr &errorOut);
  void UnloadScript();
  bool ReloadScript(rdcstr &errorOut);

  // Load Python dynamically. pythonDllPath can be empty to search default locations.
  bool LoadPython(const rdcstr &pythonDllPath = "");

  // Get current status string for the UI
  rdcstr GetStatus() const;

  // Hook invocation - called from D3D11/D3D12 wrapper functions.
  // apiName: e.g. "ID3D11Device::CreateTexture2D"
  // paramData: pointer to API-specific parameter struct
  // buildArgs: function that builds a PyObject* dict from paramData
  // result: ResourceId of the created resource (if applicable)
  void InvokeHook(const char *apiName, const void *paramData, BuildCallArgsFn buildArgs,
                  ResourceId result = ResourceId());

  // Simplified hook invocation with pre-built dict (for cases where building is inline)
  void InvokeHookWithDict(const char *apiName, PyObject *argsDict, ResourceId result = ResourceId());

  // Resource tracking
  void TrackResource(ResourceId id);
  void UntrackResource(ResourceId id);
  bool IsTracked(ResourceId id) const;

  // Logging - thread-safe, buffered
  void Log(const rdcstr &msg);
  rdcarray<rdcstr> DrainLogs();
  bool HasPendingLogs() const;

  // Resource dump request
  void RequestDump(ResourceId id, const rdcstr &outputPath, const rdcstr &format = "dds");

  // Event queue for D3D12 multi-threaded command list hooks.
  // These are lock-push from any thread, then drained on the Tick thread with GIL.
  void PushEvent(const MonitorEvent &evt);

  struct DumpRequest
  {
    ResourceId resourceId;
    rdcstr outputPath;
    rdcstr format;
  };

  rdcarray<DumpRequest> DrainDumpRequests();

  // Access to PythonLoader for driver-specific code that builds Python objects
  PythonLoader &GetPython() { return m_Python; }

  // RAII helper to acquire/release GIL. Must be held while calling ANY Python C API.
  // Only used by DrainEvents (Tick thread) and OnCreateResource (device thread).
  // D3D12 command list hooks use PushEvent() instead — no GIL needed.
  struct ScopedGIL
  {
    ScopedGIL(ApiMonitor &mon) : m_Mon(mon)
    {
      if(!mon.m_PythonInitialised)
      {
        m_Valid = false;
        return;
      }
      m_GILState = mon.m_Python.PyGILState_Ensure();
    }
    ~ScopedGIL()
    {
      if(m_Valid && m_Mon.m_PythonInitialised)
        m_Mon.m_Python.PyGILState_Release(m_GILState);
    }
    bool IsValid() const { return m_Valid; }

  private:
    ApiMonitor &m_Mon;
    int m_GILState = 0;
    bool m_Valid = true;
  };

private:
  ApiMonitor() = default;
  ~ApiMonitor();

  PythonLoader m_Python;
  volatile bool m_Active = false;
  bool m_PythonInitialised = false;
  bool m_PendingScriptChecked = false;    // true after we've checked for the temp script file
  int m_TickCount = 0;
  volatile uint32_t m_FrameNumber = 0;    // Set by driver Present, used for log prefix
  PyThreadState *m_SavedThreadState = NULL;

  Threading::CriticalSection m_PythonLock;

  // The last loaded script source (for reload)
  rdcstr m_LastScript;

  // Python objects - only valid when m_PythonInitialised == true
  PyObject *m_HookDict = NULL;       // dict: apiName -> callback function
  PyObject *m_GlobalDict = NULL;     // global namespace for script execution
  PyObject *m_MonitorModule = NULL;  // the _rdcmonitor module object

  // Tracked resources (C++ side for fast lookup without Python)
  mutable Threading::RWLock m_TrackLock;
  std::unordered_set<uint64_t> m_TrackedResources;

  // Log buffer
  mutable Threading::CriticalSection m_LogLock;
  rdcarray<rdcstr> m_LogBuffer;
  rdcstr m_LogFilePath;    // When set, logs are also written to this file

  // Dump request queue
  Threading::CriticalSection m_DumpLock;
  rdcarray<DumpRequest> m_DumpRequests;

  // Status
  rdcstr m_Status;

  // Event queue for D3D12 command list hooks (PushEvent / DrainEvents)
  Threading::CriticalSection m_EventLock;
  rdcarray<MonitorEvent> m_EventQueue;
  static const int MAX_QUEUED_EVENTS = 100000;

  // Internal helpers
  bool InitialisePython();
  void ShutdownPython();
  void TryLoadPendingScript();    // Check %TEMP%\rdcmonitor_pending.py, load if exists, then delete
  void DrainEvents();    // Process queued events under GIL
  rdcstr GetSelfDirectory();
  rdcstr PyObjectToString(PyObject *obj);
  void HandlePythonError(const char *context);
};

// Helper macro for hook insertion in wrapper functions.
// Usage:
//   API_MONITOR_HOOK("ID3D11Device::CreateTexture2D", &paramStruct, BuildFn, resourceId);
#define API_MONITOR_HOOK(apiName, paramData, buildFn, result)      \
  do                                                                \
  {                                                                 \
    if(ApiMonitor::Inst().IsActive())                                \
    {                                                               \
      ApiMonitor::Inst().InvokeHook(apiName, paramData, buildFn, result); \
    }                                                               \
  } while(0)

#define API_MONITOR_HOOK_NORET(apiName, paramData, buildFn)        \
  do                                                                \
  {                                                                 \
    if(ApiMonitor::Inst().IsActive())                                \
    {                                                               \
      ApiMonitor::Inst().InvokeHook(apiName, paramData, buildFn);   \
    }                                                               \
  } while(0)

#else    // !RENDERDOC_ENABLE_API_MONITOR

// Stub class when feature is disabled
class ApiMonitor
{
public:
  static ApiMonitor &Inst()
  {
    static ApiMonitor inst;
    return inst;
  }
  inline bool IsActive() const { return false; }
  void Initialise() {}
  void Shutdown() {}
};

#define API_MONITOR_HOOK(apiName, paramData, buildFn, result) \
  do                                                           \
  {                                                            \
  } while(0)

#define API_MONITOR_HOOK_NORET(apiName, paramData, buildFn) \
  do                                                         \
  {                                                          \
  } while(0)

#endif    // RENDERDOC_ENABLE_API_MONITOR
