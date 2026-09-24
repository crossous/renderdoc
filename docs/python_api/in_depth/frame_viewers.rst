Frame Viewers
=============

A common desire for a UI extension is to receive callbacks when certain actions happen in order to process the capture and current event. This is used internally for most panels to update their state to reflect newly selected events and react to captures opening and closing.

.. note::

    This interface is not specific to UI extensions but extreme care should be taken about adding capture viewers from scripts as the lifetime is harder to manage. If a capture viewer is added it will continue to receive events until it is removed, which can be harder to do if the object is not still accessible.

An interface (:class:`~qrenderdoc.CaptureViewer`) is provided which can be inherited from and implemented in python to receive these events, as shown below:

.. highlight:: python
.. code:: python

    import qrenderdoc as qrd

    class Viewer(qrd.CaptureViewer):
        def OnCaptureLoaded(self):
            print("A new capture was loaded")
            
        def OnCaptureClosed(self):
            print("The capture was closed")

        def OnSelectedEventChanged(self, eventId):
            print(f"The selected event is {eventId}")

        def OnEventChanged(self, eventId):
            print(f"The current event is {eventId}")

    view = Viewer()
    def register(version, ctx):
        ctx.AddCaptureViewer(view)

    def unregister():
        pyrenderdoc.RemoveCaptureViewer(view)

.. warning::
    If you implement an `__init__` function to construct your object, it is important that you explicitly call the parent class constructor with `super().__init__()` otherwise the interface will be left partially initialised and will likely crash when passing the object to C++.

Within a UI extension this will add a new viewer when the extension is initialised. Note that it is important to remove the viewer when the extension is unregistered - when reloading an extension if this isn't done the old viewers will remain alive and will continue to get callbacks.

The object will receive callbacks both when a capture is loaded (via ``OnCaptureLoaded``) and closed (via ``OnCaptureClosed``). These are both called *while the capture is open*, so immediately inside ``OnCaptureLoaded`` it is safe to call replay functions and in ``OnCaptureClosed`` queries will still include the capture status. It is not recommended that you perform any replay calls during capture closing as they may not all be safe and there is no guaranteed that all :ref:`asynchronous replay invokes <pythreading>` will be processed.

The ``OnEventChanged`` and ``OnSelectedEventChanged`` are called when an event is selected. The difference between them comes down to what the :ref:`effective event <currentevent>` is upon selecting a marker or other region with many children.

.. note::

    Because of this difference it is possible for the selected event to change *without* the event changing, because a user could first select the root of a marker region, and then the last event within it. In this case the selected event would change even though the effective event does not and there would only be a call to ``OnSelectedEventChanged`` and no corresponding call to ``OnEventChanged``.

If a capture is already loaded when a viewer is first added, ``OnCaptureLoaded``, ``OnEventChanged``, and ``OnSelectedEventChanged`` will immediately be called, so there is no need to manually account for whether a capture is open or not.
