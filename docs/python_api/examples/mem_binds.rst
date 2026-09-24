Example: Memory bindings
========================

For Vulkan and D3D12 that support explicit memory binding of texture and buffer resources to memory, RenderDoc exposes this information to python. This example shows how to query that and some simple processing we can do with it

Memory information
------------------

In each of :class:`~renderdoc.TextureDescription` and :class:`~renderdoc.BufferDescription` there are two members - :data:`~renderdoc.TextureDescription.memory` and :data:`~renderdoc.TextureDescription.memoryOffset` which give the memory object being bound to, as well as the offset in that object.

For APIs where this binding does not happen explicitly, both members will be unset. This can also happen on Vulkan if the resource was created but never bound to memory, or on D3D12 if the resource was created as a committed resource with no separate memory object.

We will iterate over the list of textures (:meth:`~qrenderdoc.CaptureContext.GetTextures`) and buffers (:meth:`~qrenderdoc.CaptureContext.GetBuffers`) and store each memory range into a dictionary indexed by the memory object being bound to.

Finally we use a simple O(n\ :sup:`2`) check for any overlaps of resources, printing each one as we find it.

Sample Output
-------------

.. sourcecode:: text

    In memory Memory 123 overlap:
        05100000 - 06c00000: [Texture] PostProcessScratch1
        05904000 - 05a84000: [Buffer]  dynamic particles
    In memory Memory 123 overlap:
        02300000 - 055c8000: [Buffer]  ScratchMemory1
        02fe2000 - 05904000: [Buffer]  ScratchMemory2
    In memory Memory 123 overlap:
        055c8000 - 08890000: [Buffer]  ScratchMemory3
        05904000 - 05a84000: [Buffer]  DebugUIVertices

Example Source
--------------

This example can be found under the name "Memory bindings" in the python scripting window.

.. only:: html and not htmlhelp

    :download:`Download the example script <mem_binds.py>`.

.. literalinclude:: mem_binds.py

