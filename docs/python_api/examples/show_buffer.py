import struct

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

mybuf = renderdoc.ResourceId.Null()

for buf in pyrenderdoc.GetBuffers():
    print(f"buf {buf.resourceId} is {pyrenderdoc.GetResourceName(buf.resourceId)}")

    mybuf = buf.resourceId

    # here put your actual selection criteria - i.e. look for a particular name
    if "Vertex" in pyrenderdoc.GetResourceName(buf.resourceId):
        break

print(f"selected {pyrenderdoc.GetResourceName(mybuf)}")

formatter = """
float3 pos;
half norms[6];
uint flags;
"""

if mybuf != renderdoc.ResourceId.Null():
    # Open a new buffer viewer for this buffer, with the given format
    bufview = pyrenderdoc.ViewBuffer(0, 0, mybuf, formatter)

    # Show the buffer viewer on the main tool area
    pyrenderdoc.AddDockWindow(
        bufview.Widget(), qrenderdoc.DockReference.MainToolArea, None
    )

    # Get access to a controller to get the buffer data.
    # We use the blocking controller for simplicity, but a better option
    # might be to invoke onto the replay thread with
    # pyrenderdoc.Replay().AsyncInvoke()
    controller = pyrenderdoc.GetBlockingController()

    data_bytes = controller.GetBufferData(mybuf, 0, 8)

    data_decoded = struct.unpack_from("8B", data_bytes)

    print(f"The first 8 bytes of the buffer are: {data_decoded}")
