# these imports are not strictly necessary, but are convenient
import renderdoc
import qrenderdoc

# this is here to give autocomplete when editing the example
# in VS Code where it doesn't know about this global
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    pyrenderdoc = qrenderdoc.CaptureContext()

if not pyrenderdoc.IsCaptureLoaded():
    filename = pyrenderdoc.Extensions().OpenFileName("Choose a capture", "", "*.rdc")

    pyrenderdoc.LoadCapture(filename, renderdoc.ReplayOptions(), filename, False, True)

pyrenderdoc.CurPipelineState().GetVertexInputs()


vs = pyrenderdoc.CurPipelineState().GetShaderReflection(renderdoc.ShaderStage.Vertex)
ps = pyrenderdoc.CurPipelineState().GetShaderReflection(renderdoc.ShaderStage.Pixel)

if vs is None or ps is None:
    raise ValueError("Expected a draw with a VS and PS to be selected")

for vin in vs.inputSignature:
    name = vin.varName
    if name == "":
        name = vin.semanticIdxName

    print(
        f"Vertex input {name} is {str(vin.varType)} x {vin.compCount} "
        f"at register {vin.regIndex}"
    )

print()

for vout in vs.outputSignature:
    name = vout.varName
    if name == "":
        name = vout.semanticIdxName

    print(
        f"Vertex input {name} is {str(vout.varType)} x {vout.compCount} "
        f"at register {vout.regIndex}"
    )

print()

print(f"VS has {len(vs.constantBlocks)} constant blocks declared")

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

print()

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

print()

print(f"PS was compiled by {renderdoc.ToolExecutable(ps.debugInfo.compiler)}")
print(f"{str(ps.debugInfo.encoding)} was compiled to {str(ps.encoding)}")

if ps.debugInfo.debuggable:
    print("PS is debuggable!")
else:
    print(f"PS can't be debugged: {ps.debugInfo.debugStatus}")
