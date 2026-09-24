Example: Pipeline State
=======================

The pipeline state abstraction RenderDoc provides gives simple API-agnostic access to the most common states and bindings at a given event. In this example we will see a couple of the things that can be queried to give ideas of how this can be applied in your own scripts.

Output bindings
---------------

To begin with we will fetch the pipeline state (:meth:`~qrenderdoc.CaptureContext.CurPipelineState`) at the :ref:`currentevent` and declare a helper function for looking up names to reduce verbosity of later code.

We'll then print out the output targets (:meth:`~renderdoc.PipeState.GetOutputTargets`) and depth target (:meth:`~renderdoc.PipeState.GetDepthTarget`) for the current draw. This may be empty depending on the current state at the event you run this script. The exact number of entries returned may also depend on API-specific details and how targets are bound, so we skip any 'unbound' targets.

.. highlight:: python
.. code:: python

    pipe = pyrenderdoc.CurPipelineState()

    get_name = lambda id: pyrenderdoc.GetResourceName(id)

    outs = pipe.GetOutputTargets()
    for i, out in enumerate(outs):
        id = out.resource
        if id != renderdoc.ResourceId():
            print(f"Out {i}: {get_name(id)}")

    id = pipe.GetDepthTarget().resource
    print(f"Depth: {get_name(id)}")
    
Pipeline objects and shaders
----------------------------

We can also query for the shaders and pipeline that are bound here. Again depending on the API you are using there may not be such a thing as a pipeline object, but shaders will be present regardless of whether PSOs are used or not.

.. highlight:: python
.. code:: python

    id = pipe.GetGraphicsPipelineObject()
    print(f"Pipeline: {get_name(id)}")

    id = pipe.GetShader(renderdoc.ShaderStage.Vertex)
    print(f"VS: {get_name(id)}")
    id = pipe.GetShader(renderdoc.ShaderStage.Pixel)
    print(f"PS: {get_name(id)}")

.. note::
    Although OpenGL has a concept of a 'pipeline' as well as programs and shaders, this is not considered to be a true pipeline state object (PSO) and so will not be listed here. Only the OpenGL-specific pipeline state in RenderDoc will show these bindings, which are largely opaque.

This can also be a useful point to query the current shader reflection (see :doc:`shader_refl`) via :meth:`~renderdoc.PipeState.GetShaderReflection` and dig in deeper to the declared bindings and shader information.

Shader Binding Helpers
----------------------

Accessing shader bindings can be quite involved, as this is an area where APIs can differ quite significantly and RenderDoc's abstraction must be more complex to allow easier access. First we will look at the highest level helper, which is very abstracted but will cover many common and simple uses.

The pipeline state abstraction offers several queries for obtaining different types of resource bindings by shader stage.

As outlined in :doc:`../in_depth/shader_refl` (as well as in :doc:`in more detailed write-ups <../in_depth/descriptors_bindings>`) RenderDoc classifies bindings into four broad categories - constant blocks, samplers, read-only resources and read-write resources.

Here we will query for the constant blocks bound to the vertex shader, and print out the buffer that is bound. Using shader reflection (see :doc:`shader_refl`) we could also use the known stage + index to look up in the reflection information how this buffer binding is used.

.. highlight:: python
.. code:: python

    cbs = pipe.GetConstantBlocks(renderdoc.ShaderStage.Vertex)

    for cb in cbs:
        print(
            f"{str(cb.access.stage)} CB[{cb.access.index}]: {get_name(cb.descriptor.resource)}"
        )

Within the :data:`~renderdoc.Descriptor` RenderDoc also lists more information, for constant blocks this may be a relative byte offset (:data:`~renderdoc.Descriptor.byteOffset`) where the binding starts, for texture access this could include the mips accessible (:data:`~renderdoc.Descriptor.firstMip`), format-cast (:data:`~renderdoc.Descriptor.format`), or component swizzle (:data:`~renderdoc.Descriptor.swizzle`).

Direct descriptor information
-----------------------------

In most cases looking up bindings via the helper above will be sufficient for knowing which resources are accessed, but it is possible to get more unfiltered information.

To do this we will query for a list of all descriptors (:meth:`~renderdoc.PipeState.GetAllUsedDescriptors`), and ask for only those which are actually known to be used. On some APIs it is possible to also query for descriptors which are bound but known to be unused - e.g. because the shader does not use them. Generally this distinction is only present for non-bindless style APIs where the set of possible bindings is a relatively small and fixed set.

.. highlight:: python
.. code:: python

    descs = pipe.GetAllUsedDescriptors(True)

Next we will examine this list on two different axes - printing resources of all types used by a given shader stage, and printing all descriptors of a given type used by all shader stage.

We also print out the descriptor store and offset where this came from, which can be used for more detailed analysis if desired. Bear in mind that this requires understanding how RenderDoc structures its :doc:`abstraction <../in_depth/descriptors_bindings>`.

.. highlight:: python
.. code:: python

    for stage in renderdoc.ShaderStage:
        stage_descs = [d for d in descs if d.access.stage == stage]
        if stage_descs == []:
            continue

        print(f"** {str(stage)} descriptors:")
        for d in stage_descs:
            desc_str = f"{str(d.access.type)} - "
            if (
                d.sampler.object != renderdoc.ResourceId()
                and d.descriptor.resource != renderdoc.ResourceId()
            ):
                desc_str += (
                    f"{get_name(d.descriptor.resource)} + {get_name(d.sampler.object)}"
                )
            elif d.sampler.object != renderdoc.ResourceId():
                desc_str += f"{get_name(d.sampler.object)}"
            else:
                desc_str += f"{get_name(d.descriptor.resource)}"
            print(desc_str)

            print(
                f"  in {get_name(d.access.descriptorStore)} at offset {d.access.byteOffset}"
            )

Sample Output
-------------

.. sourcecode:: text

    -------------------------
            Outputs          
    -------------------------
    Out 0: Swapchain Image 127
    Depth: 2D Depth Attachment 148

    -------------------------
        Pipeline/Shaders    
    -------------------------
    Pipeline: Graphics Pipeline 112
    VS: Shader Module 109
    PS: Shader Module 110

    -------------------------
    Constant Blocks (VS)  
    -------------------------
    ShaderStage.Vertex CB[0]: Buffer 100

    -------------------------
    Descriptors by Stage   
    -------------------------
    ** ShaderStage.Vertex descriptors:
    DescriptorType.ConstantBuffer - Buffer 100
    in Descriptor Set 118 at offset 0
    ** ShaderStage.Pixel descriptors:
    DescriptorType.ImageSampler - 2D Image 95 + Sampler 98
    in Descriptor Set 118 at offset 1

    -------------------------
    Descriptors by Type    
    -------------------------
    ** DescriptorType.ConstantBuffer descriptors:
    ShaderStage.Vertex - Buffer 100
    in Descriptor Set 118 at offset 0
    ** DescriptorType.ImageSampler descriptors:
    ShaderStage.Pixel - 2D Image 95 + Sampler 98
    in Descriptor Set 118 at offset 1


API-specific pipelines
----------------------

Although not shown in this example, it is also possible to query the pipeline state for each API. This will naturally only be available if the capture open is actually using that API.

Fetching data through the API-specific pipeline structure may be necessary if you are doing something that is very API specific and needs precise details, or if you want to examine data which is not common and shared by all APIs and is only present on some.

Example Source
--------------

This example can be found under the name "Pipeline State" in the python scripting window.

.. only:: html and not htmlhelp

    :download:`Download the example script <pipe_state.py>`.

.. literalinclude:: pipe_state.py