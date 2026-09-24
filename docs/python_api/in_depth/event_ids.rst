.. _eventids:

Event IDs
=========

The events within a capture are all assigned Event IDs or EIDs for short. These are simple integers and the first real event in a capture is given EID 1. EID 0 represents the point just before the first event happens.

Actions like draws, dispatches and copies are also events and so they are assigned EIDs as normal.

Event IDs *typically* correspond 1-to-1 with function calls made by the application but this is not guaranteed. With function calls like multi-draw or indirect execution it may be that a single CPU-side function call turns into multiple events on the GPU and so there will be multiple Event IDs assigned.

Event IDs are normally contiguous and ascending starting from 1, however there is an exception to this. When a capture has no marker regions in it and you have the option enabled to add fake marker regions, these will be given higher EIDs so you may find a marker region with EID 100 with children 5-10.

For how event IDs are used to browse the frame and control RenderDoc's replay, see also information about :ref:`the current event <currentevent>`.

.. _actions:

Actions
-------

Actions are typically what are used to browse the frame. Actions include any event which will execute shader code such as a draw or dispatch, but also includes anything that can modify memory or have visible side-effects like copies and clears. Although debug markers do not modify anything and have no semantic impact they are considered actions so that they can form the hierarchy that organises the actions in a capture.

RenderDoc organises event information around actions, as the list of actions in a capture can be returned via :meth:`~qrenderdoc.CaptureContext.CurRootActions` or :meth:`~renderdoc.ReplayController.GetRootActions`. These actions are those immediately at the root of the capture but each action can have children - commonly marker regions but also multi-draw calls.

Actions are represented as a :class:`~renderdoc.ActionDescription` which contains a number of optional properties depending on the type of the action. Some properties are unified between different variations for ease, so e.g. :data:`~renderdoc.ActionDescription.numIndices` represents both the number of rendered indices for an indexed draw as well as the number of rendered vertices for a non-indexed draw.

For historical reasons in RenderDoc each action contains a list of events in :data:`~renderdoc.ActionDescription.events`. Each action has the events leading up to it - e.g. for a draw it will have any state-setting that happened between the previous action and the current one.

.. _apiparams:

API parameters
--------------

The :class:`~renderdoc.APIEvent` does not contain any details about the parameters to the call itself or even its name, it is a lightweight representation. For getting this information you can cross-reference to the structured data representation which contains an iterable record of the parameters and their contents.

This can be looked up by cross-referencing from :data:`~renderdoc.APIEvent.chunkIndex` into the list of chunks in :class:`~renderdoc.SDFile` obtained from :meth:`~qrenderdoc.CaptureContext.GetStructuredFile` or :meth:`~renderdoc.ReplayController.GetStructuredFile`.

For more information see :doc:`the page on Structured Data <structured_data>`.