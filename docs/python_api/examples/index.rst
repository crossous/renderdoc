Python Examples
===============

These examples show small snippets of different commonly used areas of RenderDoc and how to access them from the UI. The source for each example is available in the :guilabel:`Examples` section of the python scripting panel.

There is a common preamble in the source code used to help external IDEs know about the pre-provided module imports and global variable, which is explained in a :ref:`FAQ entry <example_preamble>`.

The examples will also usually check to see if a capture is open, and prompt for one if not. This is the same in each example and is not explained each time. This could be hardcoded, use some other logic to open a capture, or raise an exception if a capture isn't already open.

.. highlight:: python
.. code:: python

    if not pyrenderdoc.IsCaptureLoaded():
        filename = pyrenderdoc.Extensions().OpenFileName("Choose a capture", "", "*.rdc")

        pyrenderdoc.LoadCapture(filename, renderdoc.ReplayOptions(), filename, False, True)

----------------

.. toctree::
    :maxdepth: 1

    show_buffer
    show_texture
    iter_actions
    pipe_state
    shader_refl
    resource_usage
    mem_binds
    history_debug
    mesh_output
    advanced_buffers
    exe_launching
    event_filter