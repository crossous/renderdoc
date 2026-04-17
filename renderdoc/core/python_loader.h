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
#include "strings/string_utils.h"

#if RENDERDOC_ENABLE_API_MONITOR

// We use the real Python headers for correct struct layouts (PyMethodDef, PyModuleDef, etc.)
// but load all function symbols dynamically via LoadLibrary/GetProcAddress.
// The Python headers are already available in qrenderdoc/3rdparty/python/include/.
//
// We must avoid linking against python36.lib - only the header definitions are used at compile time.

// We manually define the minimal Python types we need, matching the REAL Python 3.6 ABI layout.
// This avoids needing to add Python include paths to renderdoc.vcxproj.

// Forward declarations matching Python 3.6 ABI (64-bit)
#include <stdint.h>
#include <stddef.h>

typedef intptr_t Py_ssize_t;

// PyObject_HEAD: refcount + type pointer
typedef struct _object {
  intptr_t ob_refcnt;
  struct _typeobject *ob_type;
} PyObject;

typedef struct _ts PyThreadState;

// PyVarObject_HEAD: PyObject_HEAD + ob_size
typedef struct {
  intptr_t ob_refcnt;
  struct _typeobject *ob_type;
  intptr_t ob_size;
} PyVarObject;

// Python method definition
struct PyMethodDef
{
  const char *ml_name;
  PyObject *(*ml_meth)(PyObject *, PyObject *);
  int ml_flags;
  const char *ml_doc;
};

// Minimal PyModuleDef_Base matching Python 3.6 layout
struct PyModuleDef_Base
{
  // PyObject_HEAD
  intptr_t ob_refcnt;
  struct _typeobject *ob_type;
  // module-specific
  PyObject *(*m_init)(void);
  intptr_t m_index;
  PyObject *m_copy;
};

// PyModuleDef matching Python 3.6 layout
struct PyModuleDef
{
  PyModuleDef_Base m_base;
  const char *m_name;
  const char *m_doc;
  intptr_t m_size;
  PyMethodDef *m_methods;
  void *m_slots;      // PyModuleDef_Slot*
  void *m_traverse;   // traverseproc
  void *m_clear;      // inquiry
  void *m_free;       // freefunc
};

// METH_VARARGS constant
#define PY_METH_VARARGS 0x0001

// Python C API function pointer types
#define PYTHON_FUNC(rettype, name, args) typedef rettype (*PFN_##name) args

PYTHON_FUNC(void, Py_InitializeEx, (int initsigs));
PYTHON_FUNC(void, Py_Finalize, (void));
PYTHON_FUNC(int, Py_IsInitialized, (void));
PYTHON_FUNC(void, Py_SetProgramName, (const wchar_t *name));
PYTHON_FUNC(void, Py_SetPythonHome, (const wchar_t *home));

// GIL
PYTHON_FUNC(int, PyGILState_Ensure, (void));
PYTHON_FUNC(void, PyGILState_Release, (int));
PYTHON_FUNC(PyThreadState *, PyEval_SaveThread, (void));
PYTHON_FUNC(void, PyEval_RestoreThread, (PyThreadState *));

// Execution
PYTHON_FUNC(int, PyRun_SimpleString, (const char *command));
PYTHON_FUNC(PyObject *, PyRun_String, (const char *str, int start, PyObject *globals, PyObject *locals));

// Module
PYTHON_FUNC(PyObject *, PyModule_Create2, (PyModuleDef *def, int apiver));
PYTHON_FUNC(int, PyImport_AppendInittab, (const char *name, PyObject *(*initfunc)(void)));
PYTHON_FUNC(PyObject *, PyImport_ImportModule, (const char *name));
PYTHON_FUNC(PyObject *, PyModule_GetDict, (PyObject *module));

// Object
PYTHON_FUNC(PyObject *, PyObject_CallObject, (PyObject *callable, PyObject *args));
PYTHON_FUNC(PyObject *, PyObject_CallFunction, (PyObject *callable, const char *format, ...));
PYTHON_FUNC(PyObject *, PyObject_Str, (PyObject *o));
PYTHON_FUNC(PyObject *, PyObject_GetAttrString, (PyObject *o, const char *attr_name));
PYTHON_FUNC(int, PyObject_SetAttrString, (PyObject *o, const char *attr_name, PyObject *v));
PYTHON_FUNC(int, PyCallable_Check, (PyObject *o));

// Reference counting
PYTHON_FUNC(void, Py_IncRef, (PyObject *o));
PYTHON_FUNC(void, Py_DecRef, (PyObject *o));

// Dict
PYTHON_FUNC(PyObject *, PyDict_New, (void));
PYTHON_FUNC(int, PyDict_SetItemString, (PyObject *dp, const char *key, PyObject *item));
PYTHON_FUNC(PyObject *, PyDict_GetItemString, (PyObject *dp, const char *key));
PYTHON_FUNC(int, PyDict_Contains, (PyObject *dp, PyObject *key));
PYTHON_FUNC(Py_ssize_t, PyDict_Size, (PyObject *dp));

// List
PYTHON_FUNC(PyObject *, PyList_New, (Py_ssize_t len));
PYTHON_FUNC(int, PyList_Append, (PyObject *list, PyObject *item));
PYTHON_FUNC(PyObject *, PyList_GetItem, (PyObject *list, Py_ssize_t index));
PYTHON_FUNC(Py_ssize_t, PyList_Size, (PyObject *list));

// Tuple
PYTHON_FUNC(PyObject *, PyTuple_New, (Py_ssize_t len));
PYTHON_FUNC(int, PyTuple_SetItem, (PyObject *p, Py_ssize_t pos, PyObject *o));
PYTHON_FUNC(PyObject *, PyTuple_GetItem, (PyObject *p, Py_ssize_t pos));

// Long
PYTHON_FUNC(PyObject *, PyLong_FromLong, (long v));
PYTHON_FUNC(PyObject *, PyLong_FromUnsignedLong, (unsigned long v));
PYTHON_FUNC(PyObject *, PyLong_FromLongLong, (long long v));
PYTHON_FUNC(PyObject *, PyLong_FromUnsignedLongLong, (unsigned long long v));
PYTHON_FUNC(long, PyLong_AsLong, (PyObject *obj));
PYTHON_FUNC(unsigned long long, PyLong_AsUnsignedLongLong, (PyObject *obj));

// Float
PYTHON_FUNC(PyObject *, PyFloat_FromDouble, (double v));
PYTHON_FUNC(double, PyFloat_AsDouble, (PyObject *obj));

// Unicode/String
PYTHON_FUNC(PyObject *, PyUnicode_FromString, (const char *u));
PYTHON_FUNC(PyObject *, PyUnicode_AsUTF8String, (PyObject *unicode));
PYTHON_FUNC(const char *, PyBytes_AsString, (PyObject *o));

// Bool / None
PYTHON_FUNC(int, PyBool_Check_Func, (PyObject *o));

// Error handling
PYTHON_FUNC(void, PyErr_Print, (void));
PYTHON_FUNC(void, PyErr_Clear, (void));
PYTHON_FUNC(PyObject *, PyErr_Occurred, (void));
PYTHON_FUNC(void, PyErr_SetString, (PyObject *type, const char *message));
PYTHON_FUNC(void, PyErr_Fetch, (PyObject **ptype, PyObject **pvalue, PyObject **ptraceback));

// Special objects (retrieved at init time)
// Py_None, Py_True, Py_False are global variables in python36.dll

#undef PYTHON_FUNC

class PythonLoader
{
public:
  PythonLoader() = default;
  ~PythonLoader();

  // Try to load python36.dll from the given path, or search standard locations
  bool Load(const rdcstr &pythonDllPath = "");
  bool IsLoaded() const { return m_Module != NULL; }
  void Unload();

  rdcstr GetLoadError() const { return m_LoadError; }

  // Python C API function pointers - only valid after Load() returns true
  PFN_Py_InitializeEx Py_InitializeEx = NULL;
  PFN_Py_Finalize Py_Finalize = NULL;
  PFN_Py_IsInitialized Py_IsInitialized = NULL;
  PFN_Py_SetProgramName Py_SetProgramName = NULL;
  PFN_Py_SetPythonHome Py_SetPythonHome = NULL;

  PFN_PyGILState_Ensure PyGILState_Ensure = NULL;
  PFN_PyGILState_Release PyGILState_Release = NULL;
  PFN_PyEval_SaveThread PyEval_SaveThread = NULL;
  PFN_PyEval_RestoreThread PyEval_RestoreThread = NULL;

  PFN_PyRun_SimpleString PyRun_SimpleString = NULL;
  PFN_PyRun_String PyRun_String = NULL;

  PFN_PyModule_Create2 PyModule_Create2 = NULL;
  PFN_PyImport_AppendInittab PyImport_AppendInittab = NULL;
  PFN_PyImport_ImportModule PyImport_ImportModule = NULL;
  PFN_PyModule_GetDict PyModule_GetDict = NULL;

  PFN_PyObject_CallObject PyObject_CallObject = NULL;
  PFN_PyObject_CallFunction PyObject_CallFunction = NULL;
  PFN_PyObject_Str PyObject_Str = NULL;
  PFN_PyObject_GetAttrString PyObject_GetAttrString = NULL;
  PFN_PyObject_SetAttrString PyObject_SetAttrString = NULL;
  PFN_PyCallable_Check PyCallable_Check = NULL;

  PFN_Py_IncRef Py_IncRef = NULL;
  PFN_Py_DecRef Py_DecRef = NULL;

  PFN_PyDict_New PyDict_New = NULL;
  PFN_PyDict_SetItemString PyDict_SetItemString = NULL;
  PFN_PyDict_GetItemString PyDict_GetItemString = NULL;
  PFN_PyDict_Contains PyDict_Contains = NULL;
  PFN_PyDict_Size PyDict_Size = NULL;

  PFN_PyList_New PyList_New = NULL;
  PFN_PyList_Append PyList_Append = NULL;
  PFN_PyList_GetItem PyList_GetItem = NULL;
  PFN_PyList_Size PyList_Size = NULL;

  PFN_PyTuple_New PyTuple_New = NULL;
  PFN_PyTuple_SetItem PyTuple_SetItem = NULL;
  PFN_PyTuple_GetItem PyTuple_GetItem = NULL;

  PFN_PyLong_FromLong PyLong_FromLong = NULL;
  PFN_PyLong_FromUnsignedLong PyLong_FromUnsignedLong = NULL;
  PFN_PyLong_FromLongLong PyLong_FromLongLong = NULL;
  PFN_PyLong_FromUnsignedLongLong PyLong_FromUnsignedLongLong = NULL;
  PFN_PyLong_AsLong PyLong_AsLong = NULL;
  PFN_PyLong_AsUnsignedLongLong PyLong_AsUnsignedLongLong = NULL;

  PFN_PyFloat_FromDouble PyFloat_FromDouble = NULL;
  PFN_PyFloat_AsDouble PyFloat_AsDouble = NULL;

  PFN_PyUnicode_FromString PyUnicode_FromString = NULL;
  PFN_PyUnicode_AsUTF8String PyUnicode_AsUTF8String = NULL;
  PFN_PyBytes_AsString PyBytes_AsString = NULL;

  PFN_PyErr_Print PyErr_Print = NULL;
  PFN_PyErr_Clear PyErr_Clear = NULL;
  PFN_PyErr_Occurred PyErr_Occurred = NULL;
  PFN_PyErr_SetString PyErr_SetString = NULL;
  PFN_PyErr_Fetch PyErr_Fetch = NULL;

  // Special objects (resolved from dll data symbols)
  PyObject *Py_None_Ptr = NULL;
  PyObject *Py_True_Ptr = NULL;
  PyObject *Py_False_Ptr = NULL;
  PyObject **PyExc_RuntimeError_Ptr = NULL;
  PyObject **PyExc_TypeError_Ptr = NULL;
  PyObject **PyExc_ValueError_Ptr = NULL;

  // Convenience accessors
  PyObject *GetNone() const { return Py_None_Ptr; }
  PyObject *GetTrue() const { return Py_True_Ptr; }
  PyObject *GetFalse() const { return Py_False_Ptr; }

  // Py_file_input constant (value is 257 in Python 3.6)
  static const int FileInput = 257;

private:
  void *m_Module = NULL;
  rdcstr m_LoadError;

  void *GetSymbol(const char *name);
  bool ResolveAllSymbols();
};

#endif    // RENDERDOC_ENABLE_API_MONITOR
