
.. _resourceids:

Resource IDs
============

Within RenderDoc all resources are referenced internally by a unique ID. This ID should be considered arbitrary and unordered but will never overlap so will always uniquely identify an object. The python class for this ID is :class:`~renderdoc.ResourceId`. IDs can't be constructed directly by python scripts so you must find the ID in some reference point - from a list of resources by name, or from a pipeline binding point.

Resource IDs are only unique within a capture, and there is no correlation between the IDs used in one capture and another.

Every API object like a texture, buffer, or shader will have its own ID. When looking up a resource or cross referencing from an API call or pipeline binding you will often find that a resource is referenced only by its ID.

Anywhere that having ``NULL`` or no resource is a valid concept you may find a "Null" Resource ID. This can be compared either to a default-constructed resource ID e.g. ``renderdoc.ResourceId()`` or explicitly via the helper :meth:`~renderdoc.ResourceId.Null`.

.. highlight:: python
.. code:: python

    pipe = pyrenderdoc.CurPipelineState()

    buffer_id = pipe.GetIBuffer().resourceId

    if buffer_id == renderdoc.ResourceId():
        print("No index buffer is bound")
    else:
        buffer_info = pyrenderdoc.GetBuffer(buffer_id)
        print(f"Index buffer is {buffer_info.length} bytes in size")
