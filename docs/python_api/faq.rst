Python FAQ
==========

This page details some commonly asked questions about Python.

.. _python-crashes:

What can I do if I hit a crash while running my python script?
--------------------------------------------------------------

RenderDoc's python bindings are generally quite thin wrappers over the C++ APIs, which means the benefit of low overhead and powerful exposed functionality. The drawback is that it is quite possible to cause crashes, corruption or misbehaviour by passing invalid data to the APIs.

Aside from type errors you could encounter crashes due to passing semantically invalid data such as a function that expects the :ref:`Resource ID <resourceids>` of a shader, but you pass the ID of a texture. Generally speaking you should not expect robust error-checking from the python API.

It is also important to bear in mind that python :ref:`lifetime management <lifetimes>` with refcounting may not match the C++ object lifetime. Holding onto python objects that refer to deleted C++ objects could cause crashes as well. Commonly this could happen when a capture is closed and cached information is cleaned up whether or not python still holds handles to it.

When you encounter a crash, generally this is your responsibility to debug in your script, unless this can be reproduced purely using the UI. If you are certain you have encountered a RenderDoc bug and your script is correct you can report this, but you would need strong and clear evidence that it is not a bug in your script.

Is there a more useful object preview in the REPL/print?
--------------------------------------------------------

If you are browsing the API using the REPL you may return temporary objects at times and find that the preview for them is rather unhelpful:

.. sourcecode:: text

    <Swig Object of type 'FooBar *' at 0x000001234ABCD000>

This is due to the way the RenderDoc python bindings are generated and with how python creates strings for arbitrary objects. To get a more useful preview of an object particularly if it is a struct with properties you can use :func:`~renderdoc.DumpObject`.

Why can't I see the new UI panel I created?
-------------------------------------------

To avoid unnecessary UI churn and flickering, when you create a new UI panel such as a :class:`~qrenderdoc.BufferViewer` or :class:`~qrenderdoc.ShaderViewer` particularly ones that are not singleton and so will not already be open, the panel is created but not shown in the UI yet.

You need to call :meth:`~qrenderdoc.CaptureContext.AddDockWindow` to add the UI into the UI hierarchy somewhere.

Can I run python scripts from the command line?
-----------------------------------------------

For some workflows it may be desirable to run scripts from the command line, the UI offers two ways to do

Passing ``--py path/to/script.py`` on the command line when running the RenderDoc UI runs the script early in initialisation before the UI has been created or shown and can be used for headless execution or processing. Unlike in most cases, calling ``sys.exit()`` in one of these scripts will cause the RenderDoc process to exit.

Passing ``--ui-py path/to/script.py`` on the command line will wait until the RenderDoc UI has been shown, then show the python scripting window and load & run the specified script file as a new tab.

What version compatibility guarantees does python API provide?
--------------------------------------------------------------

Currently the python API is not considered locked, and so each version of RenderDoc may cause incompatible changes to the python API. As the python API is a wrapper, this will only happen when something is renamed or removed, or if the meaning of a member changes. New members in a struct or new methods in a class will not affect any existing python structs and this represents most of the change in the API.

The release notes for each RenderDoc version includes a section on any breaking python changes, with information on how to address your scripts. Generally it is recommended that you target a recent or latest version of RenderDoc and it is not expected that scripts will try to handle multiple RenderDoc versions.

Can I get more access to the UI for customisation?
--------------------------------------------------

Currently the entire underlying :doc:`renderdoc <renderdoc/index>` module automatically exposes all functionality possible as the same API is both wrapped for python and used in C++ by the UI.

The same is not true for the :doc:`qrenderdoc <qrenderdoc/index>` module which exposes the UI windows and functionality. This interface is more conservatively written to avoid exposing huge amounts of unused functionality which may then have significant churn and API-breaking changes.

If you have something in the UI you would like to customise or interact with, this will be considered if you file a feature request. Generally as long as it would not be unreasonably difficult or constraining on the C++ implementation most things can be exposed if there is a use for them but this will be done by request more than proactively.

Can I get callbacks when captures are loaded/closed or events are selected?
---------------------------------------------------------------------------

Yes you can! RenderDoc offers a :doc:`in_depth/frame_viewers` API where you can register an object with a given interface, and it will receive callbacks for these cases. This will allow you to have something reactive or that updates when the user browses the frame.

Can I access these APIs in C++?
-------------------------------

The short answer is yes, but it is not recommended.

The python API gets the advantage of being tolerant to ABI-breaking changes when a structure is reorganised or a member is added, as python scripts will only break when there is a source-breaking change. If you use these APIs in C++ you will be subject to all ABI changes.

For this reason using the APIs in C++ is possible but not documented or supported. You should consider very carefully if you are thinking about this whether it would be better to use the python interfaces.

Can I freely modify and use data I obtain from the APIs?
--------------------------------------------------------

As mentioned above and :ref:`elsewhere <lifetimes>`, RenderDoc's python bindings are fairly directly linked to the underlying C++ structures. In many cases the C++ provides data that is a read-only reference - which has no direct equivalent in python.

In the majority of cases, lists and objects returned to python are instead copied - these are then owned by python and can be freely modified at will. There are some exceptions for cases where copying by value is prohibitive and so references are returned:

* For :class:`~renderdoc.ShaderReflection` objects these are stored as references.
* When :class:`~renderdoc.ActionDescription` objects are obtained for a capture, these refer to previous/next neighbours and children by reference.
* The :class:`~renderdoc.SDFile` for a capture stores all children as reference, including the :class:`~renderdoc.SDObject` and buffers.

You **should not** modify any of these objects, as this could lead to problems or even crashes as internal data is corrupted.

Can I use Python scripting together with Android?
-------------------------------------------------

In theory RenderDoc's scripting works transparently regardless of where the replay is running, when used in the UI.

However Android is an unstable, unreliable, and often broken platform. As a result the use of python scripting with Android captures is not considered officially supported. It is possible that you can use scripting when running on Android but this should be taken with care.

Why doesn't VS Code apply breakpoints or catch exceptions properly?
-------------------------------------------------------------------

VS Code's python debugging requires some particular setup as in :doc:`ide_integration` and may only partially function if something is not configured as needed.

If you find that script files open in a new tab even if the file is already open, and breakpoints aren't applied, you may have "path mappings" configured in your :file:`launch.json` when attaching the debugger. VS Code creates these by default when it adds remote debugging as an option, but RenderDoc does not. These mappings are intended for debugging across different machines but it causes VS Code to get confused when debugging on the same machine with the same path. You should delete these, and restart RenderDoc before trying to attach again.

If exceptions are not being caught by VS Code, make sure you have the ``User Uncaught Exceptions`` setting under ``Breakpoints`` enabled, as RenderDoc itself catches otherwise-uncaught exceptions when running python code to improve UI stability and so VS Code's unhandled exception handler will not usually catch them.

RenderDoc only listens on one fixed port for the python debugger, so if you have multiple instances of RenderDoc's UI open only the first one to launch will be able to debug python code.

.. _example_preamble:

Why do the examples have a preamble for ``pyrenderdoc``?
--------------------------------------------------------

The :doc:`examples <examples/index>` all contain a preamble in their source which is used as hints for external IDEs about the pre-provided modules & global variable. Including this effectively does nothing when the script runs and could be omitted, but means autocomplete and type checking works correctly.

.. highlight:: python
.. code:: python

    # these imports are not strictly necessary, but are convenient
    import renderdoc
    import qrenderdoc

    # this is here to give autocomplete when editing the example
    # in VS Code where it doesn't know about this global
    from typing import TYPE_CHECKING

    if TYPE_CHECKING:
        pyrenderdoc = qrenderdoc.CaptureContext()

When opening one of the example sources in an IDE like VS Code, it will have no way of knowing that the ``renderdoc`` and ``qrenderdoc`` modules are already imported when running any script in the RenderDoc UI. Importing these again costs very little and helps with autocomplete.

The ``pyrenderdoc`` global is also pre-provided in python script environments in the RenderDoc UI. To hint this we use a python feature where a single constant in the ``typing`` module called ``TYPE_CHECKING`` is only set to ``True`` when in a type checker like in an IDE, and is ``False`` when actually executing. This allows us to 'initialise' ``pyrenderdoc`` with the correct :class:`~qrenderdoc.CaptureContext` type. Note that this type can not be created from python so this statement would fail if actually executed.
