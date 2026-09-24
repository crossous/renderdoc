Descriptors and Bindings
========================

When querying bindings either for a given shader stage or binding type, generally you can use the high-level :doc:`pipeline helper <../examples/pipe_state>` :class:`~renderdoc.PipeState` which lets you query the details of logical bindings without needing to worry about the API details or how those bindings are set. This can be checked against the :doc:`shader_refl` to see what resources are bound where (see also :doc:`../examples/shader_refl`).

This removes a significant amount of API-specific complexity or variance in bindings and lets you directly see e.g. which particular texture was bound to a given shader parameter. All you need to understand is that a 'descriptor' is the umbrella term for how a given resource or set of data is configured and provided to a shader by the CPU-side API.

.. warning::
    By its nature this abstraction is complex and doesn't map directly to any one API's concepts.

    For the large majority of use cases you *very likely* do not need something more complex than the :class:`~renderdoc.PipeState` helper.
    
    You should only delve into the details below if this high-level helper does not provide the information you need like specific binding points, or you want API-specific information not represented in the information from that helper.

Access to descriptors or fixed resource bindings is an area of graphics APIs that varies significantly between each API. To avoid implementing a large amount of access code duplicated per-API with each API's quirks, RenderDoc builds the high-level helper on top of a more detailed and more direct abstraction.

Allowances are provided for looking up API-specific information or interpreting the bindings with an API-specific lens, if the code knows which API it is being used with and how it wants to interpret that data.

.. _descriptor-abstraction:

Overview
--------

The descriptor abstraction is designed around a modern API structure, with mappings for older APIs that don't fit this natively.

Descriptors for different types of resources are created possibly with different sizes. These descriptors are written into memory in objects called descriptor stores, and finally those descriptor stores are made available to shaders and accessed from declared shader bindings.

This concept maps more closely in some APIs (like Vulkan descriptor sets or D3D12 descriptor heaps) but most APIs do not match this exactly in all cases. Where necessary this abstraction is effectively emulated - for example on D3D11 or OpenGL there are fixed binding slots, so a virtual descriptor store is created with a fixed size representing emulated storage for the available fixed binding slots.

Descriptor Access
-----------------

The RenderDoc replay can be queried for which descriptors were accessed via :meth:`~renderdoc.ReplayController.GetDescriptorAccess`. This returns a number of :class:`~renderdoc.DescriptorAccess` mappings.

Each access indicates a single use of a descriptor by a shader binding:

Descriptor
    * :data:`~renderdoc.DescriptorAccess.descriptorStore` = ``Descriptor Store XYZ``
    * :data:`~renderdoc.DescriptorAccess.byteOffset` = ``0x1000``
    * :data:`~renderdoc.DescriptorAccess.byteSize` = ``64``

Binding
    * :data:`~renderdoc.DescriptorAccess.type` = :data:`~renderdoc.DescriptorType.Image`
    * :data:`~renderdoc.DescriptorAccess.index` = ``3``
    * (if the binding is arrayed) :data:`~renderdoc.DescriptorAccess.arrayElement` = ``500``

.. note::
    Depending on the API and usage pattern this may either reflect a dynamically determined access from the shader at runtime, or it may be a statically declared access.

These :class:`~renderdoc.DescriptorAccess` mappings provide a relation of what happened at the current event. "This binding" was accessed and it read from "that descriptor".

Descriptor Contents
-------------------

To find information about the descriptor, you can query the contents of a descriptor via :meth:`~renderdoc.ReplayController.GetDescriptors` and :meth:`~renderdoc.ReplayController.GetSamplerDescriptors`. It is encouraged to call these functions in batch rather than individually per descriptor.

It is safe to call :meth:`~renderdoc.ReplayController.GetDescriptors` on a sampler descriptor and vice versa as long as it is a valid descriptor, a default-initialised structure will be returned if there is a mismatch in descriptor type. On some APIs descriptors can contain both a resource and a sampler in which case both functions will return valid appropriate data.

For this query you need to know the :doc:`descriptor store <resourceids>`, and the offset and size within it referred to. When querying a range with a :data:`~renderdoc.DescriptorRange.count` greater than 1, the size becomes the stride.

.. warning::
    Due to the emulation/virtualisation mentioned above where this does not map directly onto a real concept on all APIs, you should not make any assumptions about what valid descriptor sizes and offsets are. Although in some cases these may literally be bytes in user-visible memory, this is not guaranteed.

The descriptor data returned (:class:`~renderdoc.Descriptor`) will have optional fields as it will depend on the particular type of the resource as well as what functionality the API provides. Common information would be not only the :doc:`bound resource <resourceids>` (:data:`~renderdoc.Descriptor.resource`) but also a byte offset (:data:`~renderdoc.Descriptor.byteOffset`) or stride (:data:`~renderdoc.Descriptor.elementByteSize`) of buffer data, or a format cast (:data:`~renderdoc.Descriptor.format`) or mip level (:data:`~renderdoc.Descriptor.firstMip`) of a texture.

.. note::
    You can query a descriptor at any time, however without outside knowledge of the valid offset and size of a descriptor within a descriptor store you might not find valid data.

For the common case of looking up the currently accessed descriptors you can use :meth:`~renderdoc.PipeState.GetAllUsedDescriptors`, which provides both resource descriptor and sampler descriptors per accessed descriptors all together.

Shader Bindings
---------------

Finally to correlate to the shader binding in the :doc:`shader reflection <shader_refl>`, each descriptor access gives information about which binding performed the access.

The shader binding is identified by a shader stage, a descriptor type, an index, and an array element. The descriptor type can be :ref:`categorised <binding-types>` as sampler, constant block, read-only or read-write resource according to helper function :func:`~renderdoc.CategoryForDescriptorType` or per-type by :func:`~renderdoc.IsConstantBlockDescriptor` etc.

For example if a texture "DiffuseTexture" was element ``[2]`` in the shader reflection's :data:`~renderdoc.ShaderReflection.readOnlyResources` list, the descriptor access would contain something like:

.. highlight:: python
.. code:: python

    access.stage == renderdoc.ShaderStage.Pixel
    access.descriptorType == renderdoc.DescriptorType.Image
    access.index == 2
    access.arrayElement == 0

.. note:: 
    On some APIs, it is possible for descriptor accesses to go directly to a descriptor store from shader code with no binding or reflection information declared at all. In this case the :data:`~renderdoc.DescriptorAccess.index` member will be set to :data:`~renderdoc.DescriptorAccess.NoShaderBinding`.

Location and binding information
--------------------------------

With the above process you can determine which bindings are used, which descriptors they reference, and the contents of those descriptors. However on most APIs there is additional API-specific binding or location information associated either with a binding or a descriptor which can be helpful to display or filter by.

In the shader reflection, each binding contains two additional values: :ref:`fixedBindSetOrSpace <fixed-bind-numbers>` and :ref:`fixedBindNumber <fixed-bind-numbers>`. These values are entirely arbitrary and they serve no purpose within RenderDoc's general APIs for accessing descriptors, as their interpretation is API-specific. On some APIs these values may not be set at all. They are provided for informational purposes, if you want to look up resources in a way only relevant for a particular graphics API.

Similarly, descriptors in a descriptor store may have locations associated. In the same way that you can query descriptor contents with :meth:`~renderdoc.ReplayController.GetDescriptors` you can query locations with :meth:`~renderdoc.ReplayController.GetDescriptorLocations` which returns a list of :class:`~renderdoc.DescriptorLogicalLocation`.

Again this information is API-specific and is not used for any lookups or processing, only for user display or API-specific details.

The logical location contains a ``fixedBindNumber`` value, which depending on the API may match the binding in a shader reflection resource but is not guaranteed to. It also contains a mask of shader stages which can legally access it, the category of shader binding it may contain (if known), and a string which can be used for user display of this particular descriptor.

API-specific information
------------------------

This section provides information about API-specific details and how they are surfaced. This may change in future but generally is expected to be stable.

D3D11
^^^^^

Descriptor access is determined at load time based on shader reflection, all resources are assumed to be used and skipping due to control flow is not considered. The shader reflection ``fixedBindNumber`` gives the register number for each resource with RenderDoc's descriptor types corresponding naturally to ``cX``, ``tX``, ``uX`` and ``sX`` registers.

A single fake descriptor storage object is used for all current bindings, with the descriptor offset identifying the binding.

The descriptor location information gives the stage and category based on the binding, and the string name is an encoded ``t0`` or ``b5`` register declaration corresponding to the HLSL declarations.

This means it is possible to iterate over all descriptors in a store without any access, and identify them according to the D3D11 binding spots. However if you do this note that although UAVs have a descriptor per stage for ease of access, in D3D11's binding model all non-compute stages share the same bindings so these will be duplicated for every stage.

OpenGL
^^^^^^

Descriptor access is determined per-event based on a combination between shader reflection and querying current uniform values. Resources which are declared but known to be unused will be marked with :data:`~renderdoc.DescriptorAccess.staticallyUnused` being set to ``True``. The shader reflection ``fixedBindNumber`` will be set to 0 as the binding number is not necessarily fixed and could vary per-event via uniform.

A single fake descriptor storage object is used for all current bindings, with the descriptor offset identifying the binding.

The descriptor location information gives the stage and category based on the binding, and the string name will be a type and index something akin to ``Tex2D 3`` or ``SSBO 5``.

This means it is possible to iterate over all descriptors in a store without any access, and identify them according to the name given. The descriptor contents will also reflect this as unbound textures will still have the correct texture type when queried for their contents.

D3D12
^^^^^

Descriptor access is combined from access to single bindings being determined statically from reflection, and arrayed/bindless or direct-heap SM6.6 access being fetched at runtime per event. The shader reflection ``fixedBindNumber`` and ``fixedBindSetOrSpace`` gives the register number and register space for each resource.

RenderDoc does not directly provide root signature mappings, but the unrolled root signature is available in the D3D12 pipeline state member :data:`~renderdoc.D3D12State.rootSignature`.

SM6.6 direct-heap access will be identified with a descriptor access with :data:`~renderdoc.DescriptorAccess.index` set to :data:`~renderdoc.DescriptorAccess.NoShaderBinding`.

Descriptor storage is primarily in descriptor heap objects, however root constants, root descriptors, and static samplers will be stored in virtualised storage elsewhere. The exact objects used as storage of these descriptor for querying should not be relied upon. Similarly the descriptor size in all cases is RenderDoc-defined and will not necessarily match the descriptor size used in D3D12 during capture.

Descriptor locations have their index in the heap listed as the ``fixedBindNumber`` and the string name is the SM6.6 indexed ``ResourceDescriptorHeap[]`` or ``SamplerDescriptorHeap[]``. As descriptors are implicitly untyped and fully visible, there is no type or shader stage information in a descriptor's location.

Vulkan
^^^^^^

Descriptor access is combined from access to single bindings being determined statically from reflection, and arrayed/bindless access being fetched at runtime per event. The shader reflection ``fixedBindNumber`` and ``fixedBindSetOrSpace`` gives the binding number and set number for each resource.

Descriptor storage is primarily in descriptor set objects, however push constants, specialisation constants, and immutable samplers will be stored elsewhere. The exact objects used as 'virtual' storage of these descriptor for querying should not be relied upon.

Descriptor locations have their index listed the as binding number within the set, and the string name will be the ``bind[arrayIndex]`` flattened value with arrays unrolled contiguously. The type will only reflect the most recently written descriptor data and may be undefined for unwritten descriptors even if only one type is valid, and the visible shader mask will be determined by the descriptor set layout visibility flags.
