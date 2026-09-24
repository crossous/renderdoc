Shader Reflection
=================

RenderDoc provides access to shader reflection data through the :class:`~renderdoc.ShaderReflection`, which shows the interfaces that the shader expects to be bound as well as their format and contents. The amount of data that is available will depend heavily on the API as well as the amount of metadata kept in the shader or available as separate debug information - on some APIs information may be stripped out at which point RenderDoc will show the best data that it can.

This reflection data is presented as an abstraction to enable as much as possible for scripts to be written API-agnostically, but allow API-specific concepts to be easily addressed in most cases. We will also list how API-specific concepts map to RenderDoc's reflection.

.. note::
    We will not go into details about how resources are bound to shaders as that is covered in :doc:`descriptors_bindings`. We will only mention the details of how resources may be bound outside of those normal paths.

.. _binding-types:
.. _binding-categories:

Binding interfaces
------------------

RenderDoc maps all kinds of resource bindings into four different broad categories. In some cases there are resources which may not fit cleanly into one or the other but generally there should be no surprises if you are familiar with typical API concepts:

#. Constant blocks (:class:`~renderdoc.ConstantBlock`)

    These are bindings which are formatted as plain values, and are read-only to the shader. The most common example would be constant/uniform buffers. Depending on the API there may be multiple ways for these buffers to be bound but any buffer which is intended to be used only for small amounts of constant data will be listed as a constant block.

    Bindings which *could* be written to but are either marked read-only or are coincidentally only read from are not included here. 

#. Samplers (:class:`~renderdoc.ShaderSampler`)

    These are separate sampler-only bindings, bound to the shader via API binding mechanisms. This does not include possible combined texture-and-sampler objects which are supported on some APIs.

#. Read-only resources (:class:`~renderdoc.ShaderResource`)

    These are other types of resources which are bound explicitly as read-only to the shader. This includes textures as well as type-converted buffers (sometimes called texture buffers) and formatted or structured buffers.

    Some overlap exists here between constant blocks and buffer read-only resources. Each API has its own way of distinguishing a read-only buffer from a constant block, and that will be detailed below. Typically the distinction is that a read-only buffer is intended for reading significantly more data than a constant block and may have higher API limits such as allowing more than 64KB of data, or it may be designed to have a large array of small structures or even vectors.

#. Read-write resources (:class:`~renderdoc.ShaderResource`)

    Similarly to read-only resources above, these bindings can be either textures or buffers but can be modified and written by the shader as well as being read.

    If a resources is marked as 'write only' it will still be listed here under read-write resources as it is not a read-only resource.

.. _fixed-bind-numbers:

Common properties
-----------------

All types of bindings have a few common properties that mean the same regardless of which type of binding is being examined:

* Each binding has a ``name`` member with the name of the variable in the shader.

* For APIs that support arrays of bindings, ``bindArraySize`` gives the size of the array as declared in the shader.

* ``fixedBindSetOrSpace`` and ``fixedBindNumber`` may give an API-specific notion of a particular binding point where this binding exists. This may be absolute or relative to the type. These values are informational only and are not used for locating any bindings or resources.

D3D11
    On D3D11 ``fixedBindNumber`` gives the register number of the binding, and ``fixedBindSetOrSpace`` is ignored.

D3D12
    On D3D12 ``fixedBindSetOrSpace`` gives the ``space`` of the binding and ``fixedBindNumber`` gives the register number, which are then remapped in the root signature.

OpenGL
    On OpenGL ``fixedBindNumber`` and ``fixedBindSetOrSpace`` **are not used** because the binding for a given resource may be dynamic based on the uniform value at runtime.

Vulkan
    On Vulkan  ``fixedBindSetOrSpace`` gives the ``set`` of the binding, and ``fixedBindNumber`` gives the binding number within that descriptor set.

Constant blocks
---------------

Constant blocks are generally small fixed-size structures of data that are bound to read-only values in the API, described by :class:`~renderdoc.ConstantBlock`. This will describe the layout of the data as :class:`~renderdoc.ShaderConstant` descriptions of the variables within.

The most common case for a constant block will be a simple buffer or region of memory that is bound, but it is also possible for constant blocks to be set directly as values with no explicit memory location (:data:`~renderdoc.ConstantBlock.inlineDataBytes`), or even as compile-time constants (:data:`~renderdoc.ConstantBlock.compileConstants`). There are several flags within each :class:`~renderdoc.ConstantBlock` that describe these different types of block.

D3D11
    In D3D11 the only types of constant blocks are constant buffers, bound per-stage to the pipeline. :data:`~renderdoc.ConstantBlock.bufferBacked` will be set to ``True``.

D3D12
    In D3D12 the only types of constant blocks are constant buffers. These may be bound either via root constants, root descriptors, through a root table. :data:`~renderdoc.ConstantBlock.bufferBacked` will be ``True`` on all of the constant blocks. The shader itself does not specify if a constant buffer will be set from a real buffer or root constants, so the reflection does not give this information.
    
    Constant buffers accessed via ``ResourceDescriptorHeap`` will not be listed in the shader reflection - as of the time of writing DXC does not emit any reflection data for such resources and so they can't be described.

OpenGL
    In OpenGL a constant block could either be a uniform buffer, or it could be a special virtual block which contains all 'bare' uniforms. Uniform buffers are bound to the pipeline through one of the available uniform binding points and 'bare' uniforms are set via ``glUniform*`` entry points on the program object itself.

    The special virtual block for 'bare' uniforms will have :data:`~renderdoc.ConstantBlock.bufferBacked` set to ``False`` to distinguish it, all other bindings will have it as ``True``. :data:`~renderdoc.ConstantBlock.inlineDataBytes` will be ``False`` because the storage is opaque and not actually backed by addressable bytes.

Vulkan
    Vulkan has several different types of constant block.
    
    A uniform buffer is backed by normal memory and so will present as a constant block with no flags other than :data:`~renderdoc.ConstantBlock.bufferBacked` set to ``True``.

    The push constants region will have :data:`~renderdoc.ConstantBlock.bufferBacked` set to ``False`` and :data:`~renderdoc.ConstantBlock.inlineDataBytes` will be ``True``.

    Specialization constants will be represented by a constant block with :data:`~renderdoc.ConstantBlock.bufferBacked` set to ``False`` and :data:`~renderdoc.ConstantBlock.compileConstants` set to ``True``. :data:`~renderdoc.ConstantBlock.inlineDataBytes` will also be ``True``.

Samplers
--------

There is no significant variation between APIs for samplers so these map quite directly to the concept of a sampler object in a shader. The exception is that OpenGL does not have the concept of true separate samplers in shaders - only as an API convenience for setting sampler state. The samplers array on OpenGL will always be empty.

On D3D12 samplers accessed via ``SamplerDescriptorHeap`` will not be listed in the shader reflection - as of the time of writing DXC does not emit any reflection data for such resources and so they can't be described.

.. note::
    Although APIs have concepts of samplers that are bound vs. defined as immutable or static, this is done outside the shader and so is not listed here.

Read-only resources
-------------------

Because the :class:`~renderdoc.ShaderResource` structure is shared for both read-only and read-write resources, :data:`~renderdoc.ShaderResource.isReadOnly` will be set accordingly to be able to differentiate.

:data:`~renderdoc.ShaderResource.descriptorType` can be used to differentiate different types of descriptors within a single binding, which should generally map 1:1 to different API binding types.

For texture-type bindings, :data:`~renderdoc.ShaderResource.textureType` gives the type of texture being accessed - e.g. :data:`~renderdoc.TextureType.Texture2D` or :data:`~renderdoc.TextureType.Texture3D` etc. The type given in :data:`~renderdoc.ShaderResource.variableType` will give information about the component type and number expected of the texture.

For buffer-type bindings, :data:`~renderdoc.ShaderResource.variableType` gives the information about what the inner variable type is of the buffer.

D3D11 & D3D12
    On D3D read-only resources map directly to shader resource views (SRVs). All types of SRV bindings are represented in read-only resources, and for D3D12, acceleration structures are *not* considered texture resources.
    
    On D3D a ``StructuredBuffer`` maps to :data:`~renderdoc.DescriptorType.Buffer` and a ``Buffer`` maps to :data:`~renderdoc.DescriptorType.TypedBuffer` as the latter allows for format conversion and lists at most one vector type as its element type.

    For buffer resources the :data:`~renderdoc.ShaderResource.variableType` is the structure in an array of structures layout buffer.

OpenGL
    On OpenGL read-only resources are textures, including buffer textures which will be listed as :data:`~renderdoc.DescriptorType.TypedBuffer`.

    For buffer resources the :data:`~renderdoc.ShaderResource.variableType` may have a trailing child of unbounded size, indicating that the rest of the buffer is an array of that type.

Vulkan
    On Vulkan read-only resources are sampled images, combined image/samplers, input attachments, texel buffers, and acceleration structures. Acceleration structures are *not* considered texture resources.

    For input attachments, :data:`~renderdoc.ShaderResource.isInputAttachment` will be ``True``.

    For combined image/samplers :data:`~renderdoc.ShaderResource.hasSampler` will be ``True``.

    For buffer resources the :data:`~renderdoc.ShaderResource.variableType` may have a trailing child of unbounded size, indicating that the rest of the buffer is an array of that type.

Read-write resources
--------------------

Because the :class:`~renderdoc.ShaderResource` structure is shared for both read-only and read-write resources, :data:`~renderdoc.ShaderResource.isReadOnly` will be set accordingly to be able to differentiate.

Most members have the same meaning as above in read-only resources, and so are not documented again here.

D3D11 & D3D12
    On D3D read-write resources map to unordered resource views (UAVs).

OpenGL
    On OpenGL read-write resources are SSBOs, load/store images, and atomic counter buffers.

    Atomic counter buffers are treated as a read-write buffer with a single unsigned integer member. The variable name will be ``atomic_uint``.

Vulkan
    On Vulkan read-write resources are storage images and storage buffers.

Debug information
-----------------

Debug information for the shader is stored in :data:`~renderdoc.ShaderReflection.debugInfo`, a structure of type :data:`~renderdoc.ShaderDebugInfo`.

All APIs can provide extra shader debug information when compiling shaders, though depending on the API and compilation pipeline this may have to be explicitly enabled or it may be stripped out by default. On some APIs debug information can be separated out into an offline file so that the bytes passed to the graphics API don't contain the debug information directly but do contain an identifier of how to find it.

How to configure this compilation and set up RenderDoc to locate this debug information is documented in :ref:`how_shader_debug_info` but it is useful to note that :data:`~renderdoc.ShaderDebugInfo.debugInfoLoadingLog` contains a log of debug info loading which can be useful for diagnosing issues.

If not all the debug information is present, RenderDoc will fill out as much of it as is possible from what is available, other fields may be left blank or have less information than usual - similar to the more direct reflection information above. :data:`~renderdoc.ShaderDebugInfo.sourceDebugInformation` is a flag which indicates that RenderDoc has received enough information that source-level debugging is available, which generally means all information has been found.

Source code
-----------

The source files for the shader are stored in :data:`~renderdoc.ShaderDebugInfo.files`. If the shader debug information contains a preprocessor-output file with only a single source file using ``#line`` directives to refer to other files, the files list will contain both the original preprocessor-output file as well as virtually split apart files with the partial lines as referenced by those ``#line`` directives. This allows shader debugging to refer to the original lines in original files even if not all of those files are present.

The files have both a filename and string contents, but the filename may vary depending on the particular compiler and how it generates debug information - it is not known whether it will be an absolute path, truncated path, relative path (with ``../../`` elements) or just a filename. It is also not known if the filenames will be case sensitive or not. These conventions all come from the shader compiler that produced the debug information in the first place.

The entry point is the function that begins the execution for a particular shader - a shader reflection object corresponds to one shader, and so it may share source code with other shaders with other entry points. The location of the entry point will be given in :data:`~renderdoc.ShaderDebugInfo.entryLocation` if available in the debug information but depending on the debug information not all members may be present.

On some APIs, the entry point may be renamed between what it is in the source code and what is exposed to the API - if this happens :data:`~renderdoc.ShaderDebugInfo.entrySourceName` will contain the name of the entry point in the source code itself, and all other uses of the entry point name will refer to the API-facing name.

Compiler & Binary information
-----------------------------

Shaders can be compiled to different encodings, and in D3D12 for example multiple shader encodings are accepted - both DXBC shaders produced by ``fxc`` and DXIL shaders produced by ``dxc``. These are considered separate encodings by RenderDoc even if they share a container format as they are mostly distinct.

The shader encoding of the shader binary itself is given by :data:`~renderdoc.ShaderReflection.encoding`, and the encoding of the shader source (if different and known) is given by :data:`~renderdoc.ShaderDebugInfo.encoding`.

The compiler used, if corresponding to a known shader tool, will be given by :data:`~renderdoc.ShaderDebugInfo.compiler`. If an unknown compiler is used, this field will not be set to unknown. The compilation flags used will be given in :data:`~renderdoc.ShaderDebugInfo.compileFlags` - this is a series of key-value pairs with some special known keys:

* ``@cmdline`` will be set to a string containing the command line parameters for the compiler.
* ``@spirver`` will be available for SPIR-V shaders containing a target SPIR-V version for recompiling, e.g. ``spirv1.3``.

Other flags may be available depending on the shader compiler, API, and shader encoding.

Debugging
---------

If you are :doc:`debugging shaders <../examples/history_debug>` you will need to check if a shader supports debugging. This can be determined using :data:`~renderdoc.ShaderDebugInfo.debuggable` which is a single flag indicating if this shader can be debugged or not.

If the shader can't be debugged it is likely due to an unsupported feature or capability in the shader, and information can this be found in a string :data:`~renderdoc.ShaderDebugInfo.debugStatus`.