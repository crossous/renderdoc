Example: Resource Usage
=======================

When loading a capture, RenderDoc stores a limited amount of information about the global use of resources across the whole frame. This can then be queried so that you can know what events a texture is used in without having to select every event and check the current pipeline state bindings.

In this example we will show how this can be used to track the usage of a buffer and a texture relative to the :ref:`currentevent`.

Selecting the resources
-----------------------

First we will choose which resources we want to track the usage for. To keep things simple we will look at fixed bindings that are likely to be commonly used at a normal draw - the index buffer (:meth:`~renderdoc.PipeState.GetIBuffer`) and the depth target (:meth:`~renderdoc.PipeState.GetDepthTarget`). If one or both of these are unbound we will throw an error to avoid needing to error-check later on.

We also need to obtain the :class:`~renderdoc.ReplayController` to query the usage information. As in other examples for simplicity we use :meth:`~qrenderdoc.CaptureContext.GetBlockingController` to obtain a blocking version of the :class:`~renderdoc.ReplayController`. Although this does block, we expect usage queries to be fast so it has minimal impact but it is worth noting that this could be done on a different thread to be truly asynchronous - see :ref:`pythreading`.

.. highlight:: python
.. code:: python

    pipe = pyrenderdoc.CurPipelineState()

    depth = pipe.GetDepthTarget().resource
    ib = pipe.GetIBuffer().resourceId

    if depth == renderdoc.ResourceId() or ib == renderdoc.ResourceId():
        raise RuntimeError(
            "Can't run example!\n"
            "Current event doesn't use both index buffer and depth target"
        )

    eid = pyrenderdoc.CurEvent()

    controller = pyrenderdoc.GetBlockingController()

Querying usage list
-------------------

We will loop over both resources since the querying for usage is agnostic and we will not be looking for anything resource-specific but just looking at the list of usage entries. When calling :meth:`~renderdoc.ReplayController.GetUsage` you pass the :ref:`Resource ID <resourceids>` of the resource and it will return a list of :class:`~renderdoc.EventUsage` in order of ascending :ref:`event ID <eventids>` and giving the :class:`~renderdoc.ResourceUsage` at each event where the resource is used.

If there is only one entry and it is at :ref:`event ID <eventids>` 0 with usage :data:`~renderdoc.ResourceUsage.Unused` then this resource type was not tracked during loading and no data is available. If the list is empty, that means the resource was never used - in our case this is impossible as we know it was used at least at the current event so we look up the :class:`~renderdoc.ResourceUsage` for the current event by filtering the list.

.. highlight:: python
.. code:: python

    for name, id in [("Depth Target", depth), ("Index Buffer", ib)]:
        usagelist = controller.GetUsage(id)

        cur_usage = next(u for u in usagelist if u.eventId == eid).usage

Finding adjacent usage
----------------------

We will now look before and after the current event for the next usage entry which has a *different* :class:`~renderdoc.ResourceUsage`. There will be one usage entry per event so it is quite likely to find series of several events in the same pass where the resource is used in the same way.

.. highlight:: python
.. code:: python

    prev_usages = [u for u in usagelist if u.eventId < eid and u.usage != cur_usage]
    later_usages = [u for u in usagelist if u.eventId > eid and u.usage != cur_usage]

Because we don't know which event is currently selected and where else the resource is used, either of these lists may be empty. If they are empty we will print a message indicating so, otherwise we will print the last previous usage, or the first later usage.

.. highlight:: python
.. code:: python

    if len(prev_usages) == 0:
        print(f"{name} {pyrenderdoc.GetResourceName(id)} was never used before {eid}!")
    else:
        print(
            f"{name} {pyrenderdoc.GetResourceName(id)} was used as "
            f"{str(prev_usages[-1].usage)} at {str(prev_usages[-1].eventId)}."
        )

Sample Output
-------------

.. sourcecode:: text

    Depth Target GBufferDepth was used as ResourceUsage.Clear at 1347.
    Depth Target GBufferDepth will be used as ResourceUsage.Barrier at 1516.
    Index Buffer MeshIndices was used as ResourceUsage.CS_RWResource at 1390.
    Index Buffer MeshIndices will be used as ResourceUsage.CS_RWResource at 2065.

Example Source
--------------

This example can be found under the name "Resource Usage" in the python scripting window.

.. only:: html and not htmlhelp

    :download:`Download the example script <resource_usage.py>`.

.. literalinclude:: resource_usage.py
