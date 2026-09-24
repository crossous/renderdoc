Example: Advanced Buffers
=========================

RenderDoc has a helper in :meth:`~renderdoc.ReplayController.GetCBufferVariableContents` that will both fetch buffer data as well as interpret it into a convenient :class:`~renderdoc.ShaderVariable` representation for analysis.

When dealing with more complex cases it is always possible to fetch the raw bytes inside a buffer and do all the work of analysis yourself, but it may be helpful to be able to reuse RenderDoc's built-in ability to process buffers into more structured representations.

This example relies on UI-provided helpers in :class:`qrenderdoc.BufferInterpreter`, and so when running outside of the UI much of this would instead need to be done by hand.

.. warning::
    This example is quite involved and dives into complex topics like GPU-side pointers and manual processing of buffers into data. For simpler cases where you want to obtain a value in a constant block you should check if the :meth:`~renderdoc.ReplayController.GetCBufferVariableContents` helper function will give you what you need directly.

Expected environment
--------------------

Although most of the examples are written fairly generically to be able to run on most captures, by the nature of this example we are trying to handle a very complex case and so if you don't have a capture to hand that fits what it expects it may not run all the way through.

This example will run completely when loaded with a vulkan capture that uses GPU-pointers inside push constants, as that is a case where very complex buffer interpretation is likely needed. In other cases it may be fairly trivial but it can still be useful to see what the code is attempting to do, and perhaps adapt it for your captures.

.. tip::
    Running `Sascha Willems' bufferdeviceaddress <https://github.com/SaschaWillems/Vulkan>`_ sample will show this off, although the buffer data will be interpreted using a different format.

Selecting a starting point
--------------------------

As noted above, we are assuming more than usual about the capture. First we check that a capture is loaded and an action is selected. We look for either a mesh shader dispatch or a normal draw so that we can look at either the mesh or vertex shader:

.. highlight:: python
.. code:: python

    action = pyrenderdoc.CurAction()

    if action.flags & renderdoc.ActionFlags.MeshDispatch:
        stage = renderdoc.ShaderStage.Mesh
    elif action.flags & renderdoc.ActionFlags.Drawcall:
        stage = renderdoc.ShaderStage.Vertex

We now look at the shader reflection and find a suitable constant block. We prefer looking for push constants in vulkan, by selecting buffers with :data:`~renderdoc.ConstantBlock.bufferBacked` and :data:`~renderdoc.ConstantBlock.compileConstants` both being ``False``. If we can't find such a buffer, we will select the smallest constant block otherwise.

.. highlight:: python
.. code:: python

    push_buffers = [
        ic
        for ic in enumerate(refl.constantBlocks)
        if ic[1].bufferBacked == False and ic[1].compileConstants == False
    ]

    if len(push_buffers) == 0:
        push_buffers = list(
            sorted(enumerate(refl.constantBlocks), key=lambda ic: ic[1].byteSize)
        )

    idx, cb = push_buffers[0]

From here we will use :meth:`~renderdoc.ReplayController.GetCBufferVariableContents` to fetch the contents of the push constant buffer. We could do this by hand using the techniques shown below, but this demonstrates the convenience if that complexity is not needed. We will iterate over each variable and print out its type.

.. highlight:: python
.. code:: python

    descriptor = pipe.GetConstantBlock(stage, idx, 0).descriptor

    vars = controller.GetCBufferVariableContents(
        pipe.GetComputePipelineObject(),
        pipe.GetShader(stage),
        stage,
        refl.entryPoint,
        idx,
        descriptor.resource,
        descriptor.byteOffset,
        descriptor.byteSize,
    )

    print(f"== Variables in {cb.name} at {cb.fixedBindSetOrSpace},{cb.fixedBindNumber}")
    for v in vars:
        print(f"{v.name} is {str(v.type)}")

.. warning::
    As in other examples, we are using a blocking :class:`~renderdoc.ReplayController` for simplicity, but for more complex cases you may consider ensuring this call happens on the :ref:`replay thread <pythreading>`!

Introspecting pointers
----------------------

So far this has not used any advanced methods for decoding buffers and shows how to fetch the contents of a constant buffer directly.

We will now look for any variables that are typed as GPU pointers, and indirect the address given to read the data behind the pointer. Pointer typed variables in RenderDoc provide the type information as best as is given by the reflection data in the shader, but we will also override this with a custom format as sometimes the pointer type declared in the shader's reflection is incomplete and doesn't contain the best information.

Pointer addresses
-----------------

Whenever we encounter a GPU pointer typed variable (:data:`~renderdoc.VarType.GPUPointer`) we will fetch its pointer value and print whether it is NULL, or the resource location it points to.

Fetching the resource from an address can be done by hand - enumerating buffers using :meth:`~renderdoc.ReplayController.GetBuffers` or :meth:`~qrenderdoc.CaptureContext.GetBuffers` and checking the address and size given in :class:`~renderdoc.BufferDescription` to correlate the address. However we can use the helper :class:`~qrenderdoc.BufferInterpreter.LookupPointer` to do that for us, it returns a pair of :class:`~renderdoc.ResourceId` and an ``int`` giving the buffer and the offset into it.

.. highlight:: python
.. code:: python

    if v.type == renderdoc.VarType.GPUPointer:
        ptr = v.GetPointer()

        if ptr.pointer == 0:
            print("  NULL pointer")
        else:
            interp = qrenderdoc.BufferInterpreter

            buf, offs = interp.LookupPointer(ptr.pointer)
            name = pyrenderdoc.GetResourceName(buf)
            print(f"  Pointer to {name}+{offs}")

Based on the reflection information, or the pointer value here, we are given a type for the shader. There is a helper :meth:`~qrenderdoc.BufferInterpreter.GetPointerValType` which we can use to fetch the type description conveniently. This could be used to interpret the contents of the buffer at the pointed-to location.

.. highlight:: python
.. code:: python

    pointerConst = renderdoc.ShaderConstant()
    pointerConst.name = "Pointer"
    pointerConst.type = interp.GetPointerValType(ptr)
    print(
        f"  Declared type {pointerConst.type.name} with {len(pointerConst.type.members)} members"
    )

Custom buffer formats
---------------------

Using the same buffer format specifications as used in the UI - see :ref:`how_buffer_format` - we can describe a custom format which may be useful if the reflected type is opaque/lacking, or if we are tracking buffer data in a way that does not have clear shader reflection information.

Calling :meth:`~qrenderdoc.BufferInterpreter.Parse` lets us parse a string and retrieve the structure back. This parsing can produce errors, describe GPU packing rules based on the format. In our case the format is relatively simple so we will just take the described structure for interpreting the buffer data.

.. highlight:: python
.. code:: python

    parse_result = interp.Parse("""
        #pack(C)

        struct Reinterpret
        {
            float3 xyz;
            float multiplier;
        };

        Reinterpret val;
    """)

    pointerConst = parse_result.structure
    print(
        f"  Interpreting as type {pointerConst.type.name} with {len(pointerConst.type.members)} members"
    )

Interpreting data to structure
------------------------------

Using the type description we have, and the raw bytes from :meth:`~renderdoc.ReplayController.GetBufferData`, we could hand-process the data as we have all of the offset and type information necessary to do so. With a complex type structure though this would be infeasible, so instead we can use :meth:`~qrenderdoc.BufferInterpreter.GetShaderVariables` to help.

This function will take a given :class:`~renderdoc.ShaderConstant` and ``bytes`` data and interpret it to as many :class:`~renderdoc.ShaderVariable` as possible. You can specify a maximum number of variables (if you know you have a lot of data and only need a certain number), otherwise it will read as many as will fit into the buffer data.

.. highlight:: python
.. code:: python

    data = controller.GetBufferData(buf, offs, 128)

    ptr_vars = interp.GetShaderVariables(pointerConst, data, 3)

Once we have the variables that the pointer was referring to we can print these out with their contents, as with the original shader variables we obtained from :meth:`~renderdoc.ReplayController.GetCBufferVariableContents`.

This technique can be generalised, as the type information can come from anywhere and similarly the byte data can come from anywhere. You could use this for more complex indirect lookups, e.g. fetching the values of one buffer based on the index found in a different buffer.

Sample Output
-------------

.. sourcecode:: text

    == Variables in push_data at 0,0
    optionalData is VarType.GPUPointer
      NULL pointer

    vertexData is VarType.GPUPointer
      Pointer to VertexDataPool+123456
      Declared type opaqueStruct with 1 members
      Interpreting as type Reinterpret with 3 members
        [0] val:
          .xyz: (0, 1, 1)
          .offset: (265456,)
          .flags: (4294967295,)
        [1] val:
          .xyz: (0, 0, 0)
          .offset: (0,)
          .flags: (0,)
        [2] val:
          .xyz: (150, 1, 1)
          .offset: (265476,)
          .flags: (4294967295,)

    drawIndex is VarType.UInt

Example Source
--------------

This example can be found under the name "Advanced Buffers" in the python scripting window.

.. only:: html and not htmlhelp

    :download:`Download the example script <advanced_buffers.py>`.

.. literalinclude:: advanced_buffers.py