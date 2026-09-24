if not pyrenderdoc.IsCaptureLoaded():
    filename = pyrenderdoc.Extensions().OpenFileName("Choose a capture", "", "*.rdc")

    pyrenderdoc.LoadCapture(filename, renderdoc.ReplayOptions(), filename, False, True)

eid = pyrenderdoc.CurEvent()

name = pyrenderdoc.GetEventBrowser().GetEventName(eid)

print(f"Currently we are at EID {eid} named: '{name}'")
if eid > 1:
    prevname = pyrenderdoc.GetEventBrowser().GetEventName(eid - 1)
    print(f"    > the previous event {eid-1} is named: '{prevname}'")

pipe = pyrenderdoc.CurPipelineState()

outputs = pipe.GetOutputTargets()

for idx, out in enumerate(outputs):
    if out.resource != renderdoc.ResourceId.Null():
        name = pyrenderdoc.GetResourceName(out.resource)
        print(f"Output {idx} is: {name}")

depth = pipe.GetDepthTarget()

name = pyrenderdoc.GetResourceName(depth.resource)
print(f"Depth is: {name}")