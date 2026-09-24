# these imports are not strictly necessary, but are convenient
import renderdoc
import qrenderdoc

# this is here to give autocomplete when editing the example
# in VS Code where it doesn't know about this global
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    pyrenderdoc = qrenderdoc.CaptureContext()

if not pyrenderdoc.IsCaptureLoaded():
    raise RuntimeError("Run example with capture open")

controller = pyrenderdoc.GetBlockingController()

# grab the current action and pick an appropriate stage for it
action = pyrenderdoc.CurAction()

if action is None:
    raise RuntimeError("Run example with a draw action selected")

if action.flags & renderdoc.ActionFlags.MeshDispatch:
    stage = renderdoc.ShaderStage.Mesh
elif action.flags & renderdoc.ActionFlags.Drawcall:
    stage = renderdoc.ShaderStage.Vertex
else:
    raise RuntimeError("Run example with a geometry draw selected")

pipe = pyrenderdoc.CurPipelineState()
refl = pipe.GetShaderReflection(stage)

# find the push data, if there is some - not all APIs have this concept
push_buffers = [
    ic
    for ic in enumerate(refl.constantBlocks)
    if ic[1].bufferBacked == False and ic[1].compileConstants == False
]

# if we didn't get anything, sort by the smallest declared constant buffer
if len(push_buffers) == 0:
    push_buffers = list(
        sorted(enumerate(refl.constantBlocks), key=lambda ic: ic[1].byteSize)
    )

if len(push_buffers) == 0:
    raise RuntimeError(f"{stage} shader has no appropriate constant buffer")

# get the index in the constant block interface and the ConstantBlock itself
idx, cb = push_buffers[0]

# get the descriptor for this constant block
descriptor = pipe.GetConstantBlock(stage, idx, 0).descriptor

# use the controller helper to handle all the reflection lookup,
# type interpretation, and data fetching for us
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

# iterate over all the root-level variables and print their type
print(f"== Variables in {cb.name} at {cb.fixedBindSetOrSpace},{cb.fixedBindNumber}")
for v in vars:
    print(f"{v.name} is {str(v.type)}")

    # for pointer variables, dive inside and see the data they're pointing to
    if v.type == renderdoc.VarType.GPUPointer:
        ptr = v.GetPointer()

        if ptr.pointer == 0:
            print("  NULL pointer")
        else:
            # we will need to interpret these by hand, so use the BufferInterpreter
            # helper
            interp = qrenderdoc.BufferInterpreter

            # first look up which buffer this pointer is pointing to using the helper
            # which will enumerate through all known buffers and find the first match
            buf, offs = interp.LookupPointer(ptr.pointer)
            name = pyrenderdoc.GetResourceName(buf)
            print(f"  Pointer to {name}+{offs}")

            pointerConst = renderdoc.ShaderConstant()

            # get the type as declared by the shader - ptr will have a type ID that
            # indexes into the reflection structure containing a list of type
            # descriptions
            pointerConst.name = "Pointer"
            pointerConst.type = interp.GetPointerValType(ptr)
            print(
                f"  Declared type {pointerConst.type.name} with {len(pointerConst.type.members)} members"
            )

            # in some shading langauges, pointers may be declared as a basic type
            # and may be re-interpreted inside the shader with a type that does not
            # appear in reflection. To mimic this, we will parse a structure string
            # ourselves and use that interpreted type.
            parse_result = interp.Parse("""
                #pack(C)

                struct Reinterpret
                {
                    float3 xyz;
                    float multiplier;
                };

                Reinterpret val;
            """)

            # in this example there should be no errors expected
            if len(parse_result.errors) != 0:
                raise RuntimeError("Got parsing errors!")

            # use our re-interpreted type instead of the declared type
            pointerConst = parse_result.structure

            print(
                f"  Interpreting as type {pointerConst.type.name} with {len(pointerConst.type.members)} members"
            )

            # manually fetch some raw byte data through the pointer ourselves
            data = controller.GetBufferData(buf, offs, 128)

            # now interpret the raw bytes
            ptr_vars = interp.GetShaderVariables(pointerConst, data, 3)

            # print out each of the instances of the struct and its members
            for i, pv in enumerate(ptr_vars):
                print(f"    [{i}] {pv.name}:")

                for member in pv.members:
                    val = ", ".join(
                        [f"{x:.2}" for x in member.value.f32v[0 : member.columns]]
                    )
                    print(f"      .{member.name}: {val}")

    print("")
