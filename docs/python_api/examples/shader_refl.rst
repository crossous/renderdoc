Example: Shader Reflection
==========================

From the :doc:`Pipeline state <pipe_state>` you can obtain the currently bound shader reflection for a given stage - or if you have the ID and entry point for a particular shader you can query it directly with :meth:`~renderdoc.ReplayController.GetShader`.

In this example we will examine what information is available via the :doc:`shader reflection <../in_depth/shader_refl>`.

Input & Output signatures
-------------------------

First we will fetch both the vertex and pixel shaders (:meth:`~renderdoc.PipeState.GetShaderReflection`). We assume these are present, and throw an error if they aren't - e.g. because a non-draw is selected or a draw that doesn't use both shaders.

.. highlight:: python
.. code:: python

    vs = pyrenderdoc.CurPipelineState().GetShaderReflection(renderdoc.ShaderStage.Vertex)
    ps = pyrenderdoc.CurPipelineState().GetShaderReflection(renderdoc.ShaderStage.Pixel)

    if vs is None or ps is None:
        raise ValueError("Expected a draw with a VS and PS to be selected")

From here we will look at the input and output signatures (:data:`~renderdoc.ShaderReflection.inputSignature` and :data:`~renderdoc.ShaderReflection.outputSignature`) for the vertex shader. The input signature will contain any special :class:`~renderdoc.ShaderBuiltin` elements such as (:data:`~renderdoc.ShaderBuiltin.VertexIndex`) or (:data:`~renderdoc.ShaderBuiltin.InstanceIndex`), as well as any fixed function vertex inputs declared. Similarly the output values will typically contain both special values such as position, as well as user-defined values to be interpolated and passed through to the pixel shader.

.. warning::
    RenderDoc identifies special inputs like this using :class:`~renderdoc.ShaderBuiltin` but *does not* dictate an interpretation. In some cases the meaning of these may vary by API - for example being either always 0-indexed or offset by draw parameters like ``firstVertex`` or ``vertexOffset``.

The reflection for input and output signature values may vary between APIs, and so two possible name are used. If available we use the variable name (:data:`~renderdoc.SigParameter.varName`) which is the most likely to be relevant. If there is no reflected variable name such as on D3D APIs, we instead use the semantic name combined with any semantic index (:data:`~renderdoc.SigParameter.semanticIdxName`).

We can query their type (:data:`~renderdoc.SigParameter.varType`) and vector component count (:data:`~renderdoc.SigParameter.compCount`). The configuration of where data for fixed function vertex inputs are sourced from can be queried via :class:`~renderdoc.PipeState.GetVertexInputs`, indexed by :data:`~renderdoc.SigParameter.regIndex`. For vertex outputs this information can be used to decode mesh output data (see :doc:`mesh_output`) as it is laid out according to the output signature.

.. highlight:: python
.. code:: python

    for vin in vs.inputSignature:
        name = vin.varName
        if name == "":
            name = vin.semanticIdxName

        print(
            f"Vertex input {name} is {str(vin.varType)} x {vin.compCount} "
            f"at register {vin.regIndex}"
        )

    for vout in vs.outputSignature:
        name = vout.varName
        if name == "":
            name = vout.semanticIdxName

        print(
            f"Vertex input {name} is {str(vout.varType)} x {vout.compCount} "
            f"at register {vout.regIndex}"
        )

Constant Block Bindings
-----------------------

The reflection information contains information about each type of binding a shader can have - in RenderDoc these are categorised into constant blocks, read-only resources, read-write resources and samplers. This may slightly vary from how each API treats bindings - see :doc:`../in_depth/shader_refl`.

For constant blocks, we can find out properties like the name (:data:`~renderdoc.ConstantBlock.name`) of the constant block (if available), its byte size (:data:`~renderdoc.ConstantBlock.byteSize`), and which bind point it is bound to. This bind point is API specific and is not used elsewhere in RenderDoc but can be convenient for user display and interpreted in an API-specific manner. For more information see :doc:`../in_depth/descriptors_bindings`.

We can also inspect the reflection if available to see the names and types of the shader variables declared in this constant block (:data:`~renderdoc.ConstantBlock.variables`). This is a recursive listing of structures, arrays, and basic values like scalars, vectors, and matrices. For example we print out the first variable and its type.

.. tip::
    If you want to decode and find the contents of variables in a constant block it may be helpful to use the :meth:`~renderdoc.ReplayController.GetCBufferVariableContents` function which will handle the details of interpreting these variables into a given structure with all values available in-line.

    This also handles the case where the constant block is not sourced from a buffer and its contents are not available directly.
    
.. highlight:: python
.. code:: python

    if len(vs.constantBlocks) > 0:
        cb = vs.constantBlocks[0]

        print(
            f"  First is named {cb.name} "
            f"at {cb.fixedBindSetOrSpace}:{cb.fixedBindNumber}"
        )
        if cb.compileConstants:
            print("    (compile-time constants)")
        elif not cb.bufferBacked:
            print("    (runtime non-buffer temp data)")
        else:
            print(f"    (from a buffer, expected {cb.byteSize} bytes)")

        print(f"  containing {len(cb.variables)} variables")

        if len(cb.variables) > 0:
            var = cb.variables[0]
            print(f"  the first is named {var.name} at offset {var.byteOffset}")
            print(
                f"  type {str(var.type.baseType)} "
                f"dimension {var.type.rows}x{var.type.columns}"
            )

Texture & Sampler Bindings
--------------------------

Similarly to the constant blocks above, you can query which read-only resources (including read-only textures) and sampler bindings a shader has.

Most useful data is stored in the descriptor itself not in shader reflection, but in the binding you can the expected resource type (:data:`~renderdoc.ShaderResource.textureType`) and format (:data:`~renderdoc.ShaderResource.variableType`) which APIs typically require to match the descriptor.

For some APIs you may find a resource type that is a combined image and sampler. In this case you will not see a sampler binding at all, and the texture itself will have marked that it contains an embedded/combined sampler (:data:`~renderdoc.ShaderResource.hasSampler`).
    
.. highlight:: python
.. code:: python

    print(f"PS has {len(ps.readOnlyResources)} R/O resources")

    if len(ps.readOnlyResources) > 0:
        res = ps.readOnlyResources[0]

        print(
            f"  First is named {res.name} "
            f"at {res.fixedBindSetOrSpace}:{res.fixedBindNumber}"
        )

        print(f"  declared as {str(res.textureType)} of {res.variableType.baseType}")

        if res.hasSampler:
            print(f"  ++ has attached sampler")

    print(f"PS has {len(ps.samplers)} samplers")

    if len(ps.samplers) > 0:
        samp = ps.samplers[0]

        print(
            f"  First is named {samp.name} "
            f"at {samp.fixedBindSetOrSpace}:{samp.fixedBindNumber}"
        )

Debug Information
-----------------

The shader reflection can also contain optional debug information (:class:`~renderdoc.ShaderDebugInfo`) if the shader was compiled with it. Much of this information will not be available if it was stripped or never generated by the compiler, so care should be taken not to assume things will be set.

In our example we try to print out some metadata about what language the shader was compiled (:data:`~renderdoc.ShaderDebugInfo.encoding`) from and with which tool (:data:`~renderdoc.ShaderDebugInfo.compiler`), as well as whether or not it can be debugged (:data:`~renderdoc.ShaderDebugInfo.debuggable`). More debug information is available here including the source code itself (:data:`~renderdoc.ShaderDebugInfo.files`).

.. highlight:: python
.. code:: python

    print(f"PS was compiled by {renderdoc.ToolExecutable(ps.debugInfo.compiler)}")
    print(f"{str(ps.debugInfo.encoding)} was compiled to {str(ps.encoding)}")

    if ps.debugInfo.debuggable:
        print("PS is debuggable!")
    else:
        print(f"PS can't be debugged: {ps.debugInfo.debugStatus}")

Shader Disassembly
------------------

Shader disassembly is not directly available in the shader reflection because there are multiple possible disassembly formats and because generating and storing the disassembly costs enough to be done by default.

Fetching disassembly is done via :meth:`~renderdoc.ReplayController.DisassembleShader`, which requires the shader reflection object. Some disassembly formats will also expect the pipeline ID if the API uses pipeline objects - omitting this is possible, but disassembly may fail or may produce slightly different results depending on the circumstances. This will return a string either containing the disassembly in the requested format, or an error if the disassembly process failed.

If an empty string is passed as the disassembly format, RenderDoc will use its default disassembly. This varies depending on API but is what is considered the most readable form of the direct shader representation - DXBC, DXIL or SPIR-V depending on the API.

Other formats available can be enumerated via :meth:`~renderdoc.ReplayController.GetDisassemblyTargets` which returns a list of strings, each string being a name of a disassembly target.

.. warning::
    :meth:`~renderdoc.ReplayController.GetDisassemblyTargets` takes a parameter ``withPipeline`` which if set to ``True`` will include disassembly formats that *require* a pipeline object. It is still true that other disassembly formats may not produce 100% accurate results if the pipeline object is available but omitted.

Sample Output
-------------

.. sourcecode:: text

    Vertex input gl_VertexIndex is VarType.SInt x 1 at register 0

    Vertex input gl_Position is VarType.Float x 4 at register 0
    Vertex input texcoord is VarType.Float x 4 at register 0
    Vertex input frag_pos is VarType.Float x 3 at register 1

    VS has 1 constant blocks declared
    First is named ubuf at 0:0
        (from a buffer, expected 1216 bytes)
    containing 3 variables
    the first is named MVP at offset 0
    type VarType.Float dimension 4x4

    PS has 1 R/O resources
    First is named tex at 0:1
    declared as TextureType.Texture2D of 0
    ++ has attached sampler
    PS has 0 samplers

    PS was compiled by glslangValidator
    ShaderEncoding.GLSL was compiled to ShaderEncoding.SPIRV
    PS is debuggable!

Example Source
--------------

This example can be found under the name "Shader Reflection" in the python scripting window.

.. only:: html and not htmlhelp

    :download:`Download the example script <shader_refl.py>`.

.. literalinclude:: shader_refl.py