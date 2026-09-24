# these imports are not strictly necessary, but are convenient
import renderdoc
import qrenderdoc

# this is here to give autocomplete when editing the example
# in VS Code where it doesn't know about this global
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    pyrenderdoc = qrenderdoc.CaptureContext()

if not pyrenderdoc.IsCaptureLoaded():
    raise RuntimeError("Run example with capture open and a vertex draw selected")

import struct
from typing import List, cast

# verify the current drawcall
pipe = pyrenderdoc.CurPipelineState()

avoid_stages = [
    renderdoc.ShaderStage.Mesh,
    renderdoc.ShaderStage.Geometry,
    renderdoc.ShaderStage.Hull,
]

if any([pipe.GetShader(x) != renderdoc.ResourceId() for x in avoid_stages]):
    raise RuntimeError("Can't run example on this draw")

refl = pipe.GetShaderReflection(renderdoc.ShaderStage.Vertex)

if refl is None:
    raise RuntimeError("Can't run example on this draw")

controller = pyrenderdoc.GetBlockingController()

meshdata = controller.GetPostVSData(0, 0, renderdoc.MeshDataStage.VSOut)

print(f"Mesh data contains {meshdata.numIndices} indices in {str(meshdata.topology)}")
if meshdata.indexResourceId != renderdoc.ResourceId():
    print("         (indexed)")
else:
    print("         (non-indexed)")
if meshdata.unproject:
    far = f"{meshdata.farPlane:.2f}"
    if meshdata.farPlane > 3.0e+38:
        far = "inf"
    print(f"         Rasterized data: {meshdata.nearPlane:.2f}-{far}")

# default to just indices, but if this does use an index buffer then fetch that data
idxs = [i for i in range(meshdata.numIndices)]
if meshdata.indexResourceId != renderdoc.ResourceId():
    bufdata = controller.GetBufferData(
        meshdata.indexResourceId, meshdata.indexByteOffset, meshdata.indexByteSize
    )

    # pick the appropriate format character for 1-byte, 2-byte, or 4-byte indices
    #              01234
    struct_type = " BH I"[meshdata.indexByteStride]

    # use struct.unpack to interpret the bytes as a series of integers
    idxs = cast(
        List[int], struct.unpack_from(f"={meshdata.numIndices}{struct_type}", bufdata)
    )


def fmt_vec(vec):
    return ", ".join([f"{x:.3f}" for x in vec])


# print the first 4 triangles
for tri in range(min(4, meshdata.numIndices // 3)):
    tri_idxs = idxs[tri * 3 : tri * 3 + 3]

    print()
    print(f"Triangle {tri}:")

    for idx in tri_idxs:
        print(f"  [{idx}]:")

        # we expect baseVertex to be 0 - this does NOT come from the
        # original draw, but we still include it
        idx += meshdata.baseVertex

        offset = meshdata.vertexByteOffset + meshdata.vertexByteStride * idx

        # it would definitely be better to cache this data and look it up locally,
        # but we do this to demonstrate how GetBufferData can be used
        vert_data = controller.GetBufferData(
            meshdata.vertexResourceId, offset, meshdata.vertexByteStride
        )

        # this could again be cached outside the per-vertex loop

        offset = 0

        # RenderDoc always outputs the position at the beginning of the vertex
        # data, so that the mesh data can be re-used for rendering without needing
        # any offsets.
        posidx = [
            o.systemValue == renderdoc.ShaderBuiltin.Position
            for o in refl.outputSignature
        ].index(True)
        if posidx >= 0:
            pos = refl.outputSignature[posidx]

            # simple case, we assume float output and don't have to worry about alignment
            posdata = fmt_vec(struct.unpack_from(f"={pos.compCount}f", vert_data))

            print(f"    <pos>: {posdata}")

            offset += pos.compCount * 4

        for output in refl.outputSignature:
            # position was handled above, so skip it here
            if output.systemValue == renderdoc.ShaderBuiltin.Position:
                continue

            # some APIs align postvs data, to traditional 'wide' alignment:
            # elements rounded up to 4 bytes and 3-wide vectors aligned to 4-wide
            # in all other cases data is tightly packed
            if pipe.HasAlignedPostVSData(renderdoc.MeshDataStage.VSOut):
                align = max(4, renderdoc.VarTypeByteSize(output.varType))
                if output.compCount == 3:
                    align *= 4
                else:
                    align *= output.compCount

                if offset % align != 0:
                    offset = align - (offset % align)

            data_offs = offset

            offset += output.compCount * renderdoc.VarTypeByteSize(output.varType)

            # for simplicity we don't handle many different variable types here, only
            # simple ones. You can use the varType and more complex struct formats to
            # decode other types of data
            fmtchar = ""
            if output.varType == renderdoc.VarType.Float:
                fmtchar = "f"
            elif output.varType == renderdoc.VarType.UInt:
                fmtchar = "I"
            elif output.varType == renderdoc.VarType.SInt:
                fmtchar = "i"
            if fmtchar != "":
                fmt = f"={output.compCount}{fmtchar}"
                data = fmt_vec(struct.unpack_from(fmt, vert_data, data_offs))
            else:
                data = "<non-decoded data>"

            name = output.varName
            if name == "":
                name = output.semanticIdxName

            print(f"    {name}: {data}")
