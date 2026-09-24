.. _currentevent:

Current Frame Event
===================

Aside from information that is immutable or provided across the whole capture, all things in RenderDoc reflect the state snapshotted at a single virtual point - immediately following the execution on the GPU of a single event.

The current event is the :ref:`event ID <eventids>` where this point sits. All things that follow including resource contents like buffers and textures, as well as pipeline state and anything else will be frozen exactly at that point.

Selected vs Current event
-------------------------

When selecting a marker region that contains many events, there are two distinct concepts:

* The 'selected' event ID is the actual marker region itself, the root event which contains other events and will typically have a lower event ID than its children.
* The current event ID, or often referred to just as the event ID, is the effective event where the snapshotted state is taken from. When selecting a marker region this effective event is immediately after all child events have happened - so selecting a region surrounding a pass of many draws will show the results of all rendering within that region as if you had selected the very final child event.