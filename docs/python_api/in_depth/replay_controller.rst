Replay Controller
=================

The :class:`~renderdoc.ReplayController` gives direct access to RenderDoc's analysis. Some of the information it provides is cached and provided by the higher level interface such as pipeline states and lists of actions, so does not need to be queried again.

Exclusively available through the controller is more complex or stateful data such as the current contents of buffers (:meth:`~renderdoc.ReplayController.GetBufferData`), textures (:meth:`~renderdoc.ReplayController.GetTextureData`), as well as the results of analysis steps like pixel history (:meth:`~renderdoc.ReplayController.PixelHistory`) or shader debugging (:meth:`~renderdoc.ReplayController.DebugPixel`, :meth:`~renderdoc.ReplayController.DebugThread`).

Most information the replay controller will return will be context-specific, varying across the frame depending on the :ref:`current event <currentevent>`. Information that does not vary will include things like the lists of buffers, textures, resources, or the frame information and API properties.

The :class:`~renderdoc.ReplayController` also provides access to functionality like shader editing, allowing you to compile custom shader source and receive a new shader to replace the existing one in the capture.

The python bindings for this level of interface are more 'literal' and provide not much more safety than the underlying C++ API. For this reason illegal or invalid calls can cause corruption, unexpected behaviour or even :ref:`crashes <python-crashes>`. Working at this level gives the greatest level of flexibility but does require the greatest level of responsibility.

Using the replay controller directly can cause desyncs from the UI as there is nothing to prevent you changing the internal state in ways the UI may not reflect. It is strongly recommended that for example changing the current frame event is done via the UI interfaces so the UI can remain consistent.

Obtaining a :class:`~renderdoc.ReplayController` from a UI script can be done in two ways. Firstly :meth:`~qrenderdoc.CaptureContext.GetBlockingController` can return a *blocking* replay controller, if available while a capture is open. Otherwise using :meth:`~qrenderdoc.ReplayManager.AsyncInvoke` or can provide asynchronous access. Refer to the dedicated page for more information about :ref:`RenderDoc's threading <pythreading>`.

Replay controllers must not be used after a :ref:`capture is closed <lifetimes>`.