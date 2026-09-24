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

# annotate the function parameter so that autocomplete
# understands the type
from typing import List


# recursively walk the actions and their children,
# looking at marker regions. Returns a list of lines
# so we can more easily indent when recursing
def format_tree(actions: List[renderdoc.ActionDescription]):
    draws, dispatches, copies = 0, 0, 0
    ret = []

    for a in actions:
        # take the flags type for brevity
        ActionFlags = renderdoc.ActionFlags

        if a.flags & (ActionFlags.PushMarker | ActionFlags.MultiAction):
            # markers store their name in the action's customName
            # field so include that and then indent all the lines
            # from recursing into the action's children
            ret.append(f"{a.customName}:")
            ret += ["    " + l for l in format_tree(a.children)]
        # for non marker-regions, count them
        elif a.flags & ActionFlags.Drawcall:
            draws += 1
        elif a.flags & ActionFlags.Dispatch:
            dispatches += 1
        elif a.flags & (ActionFlags.Copy | ActionFlags.Clear):
            copies += 1

    # make a final line if we found anything else in this region
    line = ""
    if draws > 0:
        line += f", {draws} draws"
    if dispatches > 0:
        line += f", {dispatches} dispatches"
    if copies > 0:
        line += f", {copies} clears/copies"

    # trim the starting ", "
    if line != "":
        ret.insert(0, line[2:])

    return ret


# the root of the recursion starts with actions at the
# root level of the capture
for line in format_tree(pyrenderdoc.CurRootActions()):
    print(line)
