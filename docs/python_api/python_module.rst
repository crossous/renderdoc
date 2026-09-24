.. _pymodule:

Using Python Module Manually
============================

This documentation has introduced python scripting from within the embedded python runtime in the RenderDoc UI. Demonstrating how it can be used to customise and extend the UI in personalised ways.

It is also possible to load the RenderDoc module from within a standalone python interpreter as a normal python module, though this does come with caveats and limitations. This allows the most amount of control over functionality and can integrate most easily with custom automation.

The only interface available in the python interpreter is the ``renderdoc`` module, for obvious reasons the ``qrenderdoc`` module that provides access to UI functionality is not available standalone outside of the UI itself.

.. warning::
    Using the python module directly is an advanced use case and is **not** necessary for writing scripts or extensions to customise the UI!

Building the module
-------------------

RenderDoc by default does not ship with a python module that can be loaded into the ``python`` interpreter. This is because the python bindings are specific to a particular major-and-minor version of python, as well as the general difficulty with distributing binary python modules.

It is however possible to build the module as long as you know the exact python version you will be using, and this page details how to do that and the limitations of this setup.

The first step is to ensure that you have a local version of `RenderDoc's source <https://github.com/baldurk/renderdoc>`_ and can build it successfully. The necessary `dependencies <https://github.com/baldurk/renderdoc/blob/v1.x/docs/CONTRIBUTING/Dependencies.md>`_ and `instructions <https://github.com/baldurk/renderdoc/blob/v1.x/docs/CONTRIBUTING/Compiling.md>`_ are listed on github but e.g. on windows all that is needed is Visual Studio 2015+.

Once you have built RenderDoc, by default you will have a version of the python module already. Depending on your platform it will be generated in a different place - on Windows to prevent filename collisions it is in a ``pymodules`` subfolder under the relevant platform and build type, on linux it will be output to the ``lib`` folder as ``renderdoc.so``.

This module will be built against the default python interpreter - on linux this will depend on the version available in your system which may already be the one you want, on windows this will be the version of python bundled in the source code which is Python 3.6. The module can *only* be safely loaded in this version of python and no other. To use with a different version of python, you must rebuild RenderDoc against that version of python.

.. _custom-py-ver:

Targeting a different python version
------------------------------------

Forcing the build to use a different python version varies depending on where you are building it.

Windows
^^^^^^^

On windows the python configuration is set in the visual studio project. Each of the projects ``qrenderdoc``, ``pyrenderdoc_module`` and ``qrenderdoc_module`` have a tab in their properties labelled :guilabel:`Python Configuration` which has a single entry pointing to the location of a python interpreter.

Almost everything that RenderDoc needs is present in a normal python installation - the header files in ``Include/``, link library in ``libs/`` and python interpreter DLL. However on windows RenderDoc also expects the standard library to be available as a compiled zip bundle ``python3.xx.zip`` similar to the 'embeddable package' that is provided by python downloads, and default installations only have the standard library as loose files in ``Lib/``.

For convenience the RenderDoc source checkout has a python script which will compile this zip. Running ``util/make_python_lib_zip.py`` from the interpreter you wish to use will automatically compile its library from the ``Lib/`` folder into a ``python3.xx.zip`` next to ``python.exe``. If you wish to control which library folder to compile and where to put the zip, you can pass these as arguments to the ``make_python_lib_zip.py`` script, but note that the visual studio build will expect to find the library zip next to the DLL.

Once this zip has been created or otherwise obtained, rebuilding the visual studio solution should use the new python version for both the embedded runtime in the RenderDoc UI and build the python module against it. You can see it successful with a message like so::

    Built against python from C:\Python314

If there is a problem with the specified location you may instead see a message indicating the problem, and RenderDoc will fall back to using the bundled python 3.6::

    ** Could not use python version C:\Python314 due to missing requirements.
    ** Check for C:\Python314\include\Python.h, C:\Python314\pythonMAJMIN.zip, and either C:\Python314\pythonMAJMIN.lib or C:\Python314\libs\pythonMAJMIN.lib

Linux
^^^^^

On Linux if you are using cmake version 3.12 or newer you can specify ``-DFORCE_PY_VERSION=3.xx`` on the command line to force the build to require a specific version. You may need to install a different package for development files, or use additional cmake configuration options to ensure the correct locations are found. Consult your distribution's documentation or the cmake documentation for ``FindPython3`` for more information.

If successful, the cmake configuration will print which python version is in use before build, and the module will be loadable in that version of python.

Loading the module
------------------

You can use whichever python mechanism you prefer to ensure that the RenderDoc module is in python's search path. This could mean modifying ``sys.path`` or specifying the python path another way at startup. This will ensure the RenderDoc module can be found by python.

The module itself also has a dependency on RenderDoc's core library as itself it is only a thin wrapper. The core library must be available for loading when the python module is imported. This again will depend on your platform.

Windows
^^^^^^^

On windows by default the module will expect RenderDoc's core library to be in ``PATH``, but on python 3.8 and above, there is an extra step necessary on windows only. You must call ``os.add_dll_directory`` with the path where ``renderdoc.dll`` can be found.

Provided ``renderdoc.dll`` is available in the ``PATH`` and if necessary ``os.add_dll_directory`` is called, the module should load correctly. If the DLL can't be found you will see an error like this::

    ImportError: DLL load failed while importing renderdoc: The specified module could not be found.

Linux
^^^^^

On linux the python module is built with a ``RUNPATH`` that will include its own location, meaning that by default nothing else is required since the RenderDoc library ``librenderdoc.so`` is built in the same directory. If this is moved or changed, you should use ``LD_LIBRARY_PATH`` as appropriate to ensure the library can still be loaded. If you do not you will see an error like this::

    ImportError: librenderdoc.so: cannot open shared object file: No such file or directory

Using RenderDoc's API directly
------------------------------

If you are using RenderDoc's API directly there are some things you will need to take care of which normally the UI would handle for you.

The RenderDoc replay API must be initialised using :func:`~renderdoc.InitialiseReplay` once before any other API function is called, and at the end of the process you must call :func:`~renderdoc.ShutdownReplay`. It is not valid to call any API function before initialisation or after shutdown, and you can't re-initialise after shutting down. These functions are called by the UI normally and so must only be used when writing scripts that use the module directly.

For a complex example of how to use the RenderDoc python module directly you can look at the automatic testing scripts in the `RenderDoc repository <https://github.com/baldurk/renderdoc/tree/v1.x/util/test>`_. These scripts perform automatic capture, replay and analysis for self-testing of RenderDoc and are written entirely in python.