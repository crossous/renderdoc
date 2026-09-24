.. _lifetimes:

Object Lifetimes
================

The python API exposed by RenderDoc is a fairly thin auto-generated wrapper around the underlying C++ API. This means that a significant amount of functionality is exposed 'for free', but it also means that the API does not always perfectly match the expected python semantics. This can have unexpected behaviour particularly around how long these objects are valid for.

Here we will talk about the lifetime management of objects and what things to look out for, with particular note of some common areas where things do not behave as they normally would for python objects.

We also mention when objects should be treated as read-only, which is not a typical python concept. Normally in python objects are either copied or references are freely mutable, but in some cases with the RenderDoc python API you should avoid modifying python objects which are owned by C++.

Plain structures
----------------

With exceptions listed below, most plain data structures can be treated normally by python and have natural python reference counting. Modifying the properties of these structures will not affect underlying data stored in C++ and they can be held naturally in python variables to be destroyed when they are no longer accessible.

Lists of such structures also behave as common python lists, with each structure a reference inside the list.

These structures can also be created in python just like normal objects, with no special handling.

RenderDoc-owned objects
-----------------------

Any structure with a lifetime exclusively managed by RenderDoc such as :class:`~renderdoc.ReplayController` or :class:`~renderdoc.CaptureFile` can't be created or destroyed directly in python. A handle to these is **only** valid for as long as the underlying object exists, and it is possible for the underlying object to be destroyed while a handle still exists in python.

In this case, it is invalid to access the handle after the object is destroyed and this will very likely lead to crashes.

Typically these objects are either only available while a given capture is open, or must be explicitly destroyed with a ``Shutdown`` method, but this will be context-dependent so care should be taken.

Actions
-------

The :class:`~renderdoc.ActionDescription` objects returned by queries for the current set of actions in a capture contain members :data:`~renderdoc.ActionDescription.parent`, :data:`~renderdoc.ActionDescription.previousAction`, :data:`~renderdoc.ActionDescription.nextAction`, which contain references to neighbouring actions.

These members are internally represented by C++ pointers and so do not refer to the same copied objects that may be returned to and owned by python. Their properties will be the same, but care must be taken to **not** modify through these references as this will affect the internal C++ structures. They should be treated as read-only (which is not representable in python except via deep copy).

This also means that although any :class:`~renderdoc.ActionDescription` objects stored by python will remain valid indefinitely, these members will no longer be valid after the capture is closed.

Structured data
---------------

Structured data as returned by :class:`~renderdoc.SDFile` possibly takes up a huge amount of memory for storage. For this reason, it is not copied and instead the C++-owned object is returned to python directly.

This means that you should treat the object as well as all chunks and buffers within as read-only and ensure that it is not used beyond the scope of where the capture is open.

Shader Reflection
-----------------

:class:`~renderdoc.ShaderReflection` objects can be quite numerous in some captures, as well as potentially including original shader source that takes up a large amount of memory. This means they are not feasible to store as copies and instead these reflection objects are returned to python directly.

This means that you should treat these objects as read-only and ensure that they are not used beyond the scope of where the capture is open.

Widgets
-------

When using :class:`~qrenderdoc.MiniQtHelper` it is possible to access Qt widgets from python. These handles are directly into the Qt objects themselves and so must respect the lifetime rules of Qt which are not reference counted but are owned from parents to children. Qt widgets sit in a hierarchy from the top level window down through each widget contained within. When a Qt widget is destroyed, it also destroys all children.

All widget handles returned via RenderDoc's APIs are *not* owned by python, and must be destroyed implicitly as above or explicitly with :class:`~qrenderdoc.MiniQtHelper.DestroyWidget`.

.. note::
    If using PySide and creating widgets through its interfaces, you should refer to PySide's documentation for ownership, as that will differ.

This also means that it is possible to keep a reference to a widget in python even after it has been destroyed. These handles are no longer valid once a widget is destroyed and must not be used from python or passed into any other API functions. When creating a top-level widget with :meth:`~qrenderdoc.MiniQtHelper.CreateToplevelWidget` you can provide a callback to be called when the widget is closed, so that you can be aware that any children are no longer valid.

Some panels are considered 'temporary' when attached to the UI - for example viewing a constant buffer with :meth:`~qrenderdoc.CaptureContext.ViewConstantBuffer` will return a :class:`~qrenderdoc.BufferViewer` that views the given constant buffer but when a capture is closed all constant buffers will be removed as they are no longer referenced. You should take care not to access these handles after the capture is closed as they will refer to deleted objects.

Shader traces
-------------

When debugging a shader via the RenderDoc API, a :class:`~renderdoc.ShaderDebugTrace` is returned with information about the trace as well as the debug engine.

This trace's lifetime must be explicitly managed, and destroyed with :meth:`~renderdoc.ReplayController.FreeTrace`. After calling that function to destroy it, the trace and all members must not be accessed.