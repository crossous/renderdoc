
.. _pythreading:

Threading in RenderDoc's UI
===========================

RenderDoc runs with two main threads: The UI thread created by the operating system and where UI interactions are processed, and a replay thread where most replay work happens.

UI extension python code runs on the UI thread itself for direct access to widgets and other UI panels.

Most replay work does not take a long time but it can still be noticeable enough that it would cause UI stalls if it were not run asynchronously on a thread. This also allows for occasional long-running tasks that may take multiple seconds to happen without the UI becoming completely unresponsive.

Replay thread
-------------

From python the replay thread is handled in :class:`~qrenderdoc.ReplayManager`. While a capture is open, the replay manager provides access to the replay thread via callbacks that can be invoked either with :meth:`~qrenderdoc.ReplayManager.AsyncInvoke` or :meth:`~qrenderdoc.ReplayManager.BlockInvoke`. Both functions are identical and queue processing of a callback which receives the :class:`~renderdoc.ReplayController` for use. The asynchronous version will not wait for the callback to happen whereas the blocking version will stall the caller until the callback has been called and returned. For this reason blocking invokes should be used very sparingly from the UI thread.

It is recommended that most work that can be done on the replay thread is moved there via callbacks, as long-running work on the UI thread can cause unpleasant stalls or hangs.

For convenience when working with simple scripts, you can obtain a blocking version of :class:`~renderdoc.ReplayController` via :meth:`~qrenderdoc.CaptureContext.GetBlockingController` which will automatically blocking invoke onto the correct thread for each call through its API.

Python script thread
--------------------

When running scripts in the RenderDoc UI directly in the :doc:`../../window/python_scripting` window the script executes in a special thread to prevent long-running scripts from freezing the UI, which automatically blocks the UI thread when accessing any UI elements.

The python thread allows the use of :meth:`~qrenderdoc.CaptureContext.GetBlockingController` as noted above without causing UI stalls and for simple scripts is convenient.

.. warning::
    If using Qt directly via PySide you should ensure that you run code directly on the UI thread (:meth:`~qrenderdoc.CaptureContext.InvokeOntoUIThread`) as Qt is not always thread-safe.
