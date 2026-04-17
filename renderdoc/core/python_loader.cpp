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

#include "python_loader.h"

#if RENDERDOC_ENABLE_API_MONITOR

#include "common/formatting.h"
#include "os/os_specific.h"

#if ENABLED(RDOC_WIN32)
#include <windows.h>
#define PYTHON_DLL_NAME "python36.dll"

// Dummy variable used to get the address of our own DLL via GetModuleHandleEx
static int s_PythonLoaderMarker = 0;
#elif ENABLED(RDOC_LINUX) || ENABLED(RDOC_APPLE)
#include <dlfcn.h>
#define PYTHON_DLL_NAME "libpython3.6m.so.1.0"
#elif ENABLED(RDOC_ANDROID)
#define PYTHON_DLL_NAME "libpython3.6m.so"
#endif

PythonLoader::~PythonLoader()
{
  Unload();
}

void *PythonLoader::GetSymbol(const char *name)
{
  if(!m_Module)
    return NULL;

#if ENABLED(RDOC_WIN32)
  return (void *)GetProcAddress((HMODULE)m_Module, name);
#elif ENABLED(RDOC_LINUX) || ENABLED(RDOC_APPLE) || ENABLED(RDOC_ANDROID)
  return dlsym(m_Module, name);
#else
  return NULL;
#endif
}

bool PythonLoader::Load(const rdcstr &pythonDllPath)
{
  if(m_Module)
    return true;

  rdcstr dllPath = pythonDllPath;

  // Try loading from specified path first, then search standard locations
#if ENABLED(RDOC_WIN32)
  if(!dllPath.empty())
  {
    m_Module = (void *)LoadLibraryA(dllPath.c_str());
  }

  if(!m_Module)
  {
    // Try in the same directory as our DLL
    char moduleDir[MAX_PATH] = {};
    HMODULE hSelf = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&s_PythonLoaderMarker, &hSelf);
    if(hSelf)
    {
      GetModuleFileNameA(hSelf, moduleDir, MAX_PATH);
      // Strip the filename to get directory
      char *lastSlash = strrchr(moduleDir, '\\');
      if(!lastSlash)
        lastSlash = strrchr(moduleDir, '/');
      if(lastSlash)
      {
        lastSlash[1] = '\0';
        rdcstr tryPath = rdcstr(moduleDir) + PYTHON_DLL_NAME;
        m_Module = (void *)LoadLibraryA(tryPath.c_str());
      }
    }
  }

  if(!m_Module)
  {
    // Try system search path
    m_Module = (void *)LoadLibraryA(PYTHON_DLL_NAME);
  }

#elif ENABLED(RDOC_LINUX) || ENABLED(RDOC_APPLE) || ENABLED(RDOC_ANDROID)
  if(!dllPath.empty())
  {
    m_Module = dlopen(dllPath.c_str(), RTLD_NOW | RTLD_GLOBAL);
  }

  if(!m_Module)
  {
    m_Module = dlopen(PYTHON_DLL_NAME, RTLD_NOW | RTLD_GLOBAL);
  }
#endif

  if(!m_Module)
  {
    m_LoadError = StringFormat::Fmt("Failed to load %s", PYTHON_DLL_NAME);
    RDCWARN("%s", m_LoadError.c_str());
    return false;
  }

  if(!ResolveAllSymbols())
  {
    Unload();
    return false;
  }

  RDCLOG("API Monitor: Successfully loaded Python from %s",
         dllPath.empty() ? PYTHON_DLL_NAME : dllPath.c_str());
  return true;
}

void PythonLoader::Unload()
{
  if(!m_Module)
    return;

#if ENABLED(RDOC_WIN32)
  FreeLibrary((HMODULE)m_Module);
#elif ENABLED(RDOC_LINUX) || ENABLED(RDOC_APPLE) || ENABLED(RDOC_ANDROID)
  dlclose(m_Module);
#endif

  m_Module = NULL;

  // Reset all function pointers
  Py_InitializeEx = NULL;
  Py_Finalize = NULL;
  Py_IsInitialized = NULL;
  // ... all others will be zero after unload since object is reusable
}

bool PythonLoader::ResolveAllSymbols()
{
#define RESOLVE(name)                                                          \
  name = (PFN_##name)GetSymbol(#name);                                         \
  if(!name)                                                                    \
  {                                                                            \
    m_LoadError = StringFormat::Fmt("Failed to resolve symbol: %s", #name);    \
    RDCERR("%s", m_LoadError.c_str());                                         \
    return false;                                                              \
  }

#define RESOLVE_DATA(type, name)                                               \
  name = (type)GetSymbol(#name);                                               \
  if(!name)                                                                    \
  {                                                                            \
    m_LoadError = StringFormat::Fmt("Failed to resolve data symbol: %s", #name); \
    RDCERR("%s", m_LoadError.c_str());                                         \
    return false;                                                              \
  }

  // Lifecycle
  RESOLVE(Py_InitializeEx);
  RESOLVE(Py_Finalize);
  RESOLVE(Py_IsInitialized);
  RESOLVE(Py_SetProgramName);
  RESOLVE(Py_SetPythonHome);

  // GIL
  RESOLVE(PyGILState_Ensure);
  RESOLVE(PyGILState_Release);
  RESOLVE(PyEval_SaveThread);
  RESOLVE(PyEval_RestoreThread);

  // Execution
  RESOLVE(PyRun_SimpleString);
  RESOLVE(PyRun_String);

  // Module
  RESOLVE(PyModule_Create2);
  RESOLVE(PyImport_AppendInittab);
  RESOLVE(PyImport_ImportModule);
  RESOLVE(PyModule_GetDict);

  // Object
  RESOLVE(PyObject_CallObject);
  RESOLVE(PyObject_CallFunction);
  RESOLVE(PyObject_Str);
  RESOLVE(PyObject_GetAttrString);
  RESOLVE(PyObject_SetAttrString);
  RESOLVE(PyCallable_Check);

  // Refcounting
  RESOLVE(Py_IncRef);
  RESOLVE(Py_DecRef);

  // Dict
  RESOLVE(PyDict_New);
  RESOLVE(PyDict_SetItemString);
  RESOLVE(PyDict_GetItemString);
  RESOLVE(PyDict_Contains);
  RESOLVE(PyDict_Size);

  // List
  RESOLVE(PyList_New);
  RESOLVE(PyList_Append);
  RESOLVE(PyList_GetItem);
  RESOLVE(PyList_Size);

  // Tuple
  RESOLVE(PyTuple_New);
  RESOLVE(PyTuple_SetItem);
  RESOLVE(PyTuple_GetItem);

  // Long
  RESOLVE(PyLong_FromLong);
  RESOLVE(PyLong_FromUnsignedLong);
  RESOLVE(PyLong_FromLongLong);
  RESOLVE(PyLong_FromUnsignedLongLong);
  RESOLVE(PyLong_AsLong);
  RESOLVE(PyLong_AsUnsignedLongLong);

  // Float
  RESOLVE(PyFloat_FromDouble);
  RESOLVE(PyFloat_AsDouble);

  // String
  RESOLVE(PyUnicode_FromString);
  RESOLVE(PyUnicode_AsUTF8String);
  RESOLVE(PyBytes_AsString);

  // Error handling
  RESOLVE(PyErr_Print);
  RESOLVE(PyErr_Clear);
  RESOLVE(PyErr_Occurred);
  RESOLVE(PyErr_SetString);
  RESOLVE(PyErr_Fetch);

  // Data symbols (global variables exported from python dll)
  // These are struct instances, not pointers-to-pointers. We get the address of the struct.
  Py_None_Ptr = (PyObject *)GetSymbol("_Py_NoneStruct");
  if(!Py_None_Ptr)
  {
    m_LoadError = "Failed to resolve _Py_NoneStruct";
    return false;
  }

  Py_True_Ptr = (PyObject *)GetSymbol("_Py_TrueStruct");
  if(!Py_True_Ptr)
  {
    m_LoadError = "Failed to resolve _Py_TrueStruct";
    return false;
  }

  Py_False_Ptr = (PyObject *)GetSymbol("_Py_FalseStruct");
  if(!Py_False_Ptr)
  {
    m_LoadError = "Failed to resolve _Py_FalseStruct";
    return false;
  }

  PyExc_RuntimeError_Ptr = (PyObject **)GetSymbol("PyExc_RuntimeError");
  PyExc_TypeError_Ptr = (PyObject **)GetSymbol("PyExc_TypeError");
  PyExc_ValueError_Ptr = (PyObject **)GetSymbol("PyExc_ValueError");

#undef RESOLVE

  return true;
}

#endif    // RENDERDOC_ENABLE_API_MONITOR
