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

pipe = pyrenderdoc.CurPipelineState()

depth = pipe.GetDepthTarget().resource
ib = pipe.GetIBuffer().resourceId

if depth == renderdoc.ResourceId() or ib == renderdoc.ResourceId():
    raise RuntimeError(
        "Can't run example!\n"
        "Current event doesn't use both index buffer and depth target"
    )

eid = pyrenderdoc.CurEvent()

controller = pyrenderdoc.GetBlockingController()

for name, id in [("Depth Target", depth), ("Index Buffer", ib)]:
    usagelist = controller.GetUsage(id)

    cur_usage = next(u for u in usagelist if u.eventId == eid).usage

    prev_usages = [u for u in usagelist if u.eventId < eid and u.usage != cur_usage]
    later_usages = [u for u in usagelist if u.eventId > eid and u.usage != cur_usage]

    if len(prev_usages) == 0:
        print(f"{name} {pyrenderdoc.GetResourceName(id)} was never used before {eid}!")
    else:
        print(
            f"{name} {pyrenderdoc.GetResourceName(id)} was used as "
            f"{str(prev_usages[-1].usage)} at {str(prev_usages[-1].eventId)}."
        )

    if len(later_usages) == 0:
        print(f"{name} {pyrenderdoc.GetResourceName(id)} is never used after {eid}!")
    else:
        print(
            f"{name} {pyrenderdoc.GetResourceName(id)} will be used as "
            f"{str(later_usages[0].usage)} at {str(later_usages[0].eventId)}."
        )
