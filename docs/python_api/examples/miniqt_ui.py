# these imports are not strictly necessary, but are convenient
import renderdoc
import qrenderdoc

# this is here to give autocomplete when editing the example
# in VS Code where it doesn't know about this global
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    pyrenderdoc = qrenderdoc.CaptureContext()

mqt = pyrenderdoc.Extensions().GetMiniQtHelper()

# helper to make for shorter example code
def add_widget(parent, child, text=""):
    mqt.AddWidget(parent, child)
    if text != "":
        mqt.SetWidgetText(child, text)
    return child

# create a new floating window for our example
top = mqt.CreateToplevelWidget("Example!")
pyrenderdoc.AddDockWindow(top, qrenderdoc.DockReference.NewFloatingArea, None)

# add a group of interactive widgets
group = add_widget(top, mqt.CreateGroupBox(True), "Interactive Widgets")
layout = add_widget(group, mqt.CreateHorizontalContainer())

# callback for when the button is pressed that randomises the
# progress bar and counts its presses
count = 0
def update_button(ctx=None, wid=None, text=None):
    global count
    mqt.SetWidgetText(butt, f"{count} button presses")
    count += 1

    import random
    mqt.SetProgressBarValue(prog, random.randint(0, 100))

# when the checkbox is toggled, update the label
def update_checkbox(ctx=None, wid=None, text=None):
    checked = "checked" if mqt.IsWidgetChecked(check) else "unchecked"
    mqt.SetWidgetText(output_label, f"checkbox is {checked}")

# create three widgets in this group
butt = add_widget(layout, mqt.CreateButton(update_button))
check = add_widget(layout, mqt.CreateCheckbox(update_checkbox))
output_label = add_widget(layout, mqt.CreateLabel(), "checkbox is ????")

# create a second group of read only widgets
group = add_widget(top, mqt.CreateGroupBox(True), "Display Widgets")

lab = add_widget(group, mqt.CreateLabel(), "A label with a funky font")
mqt.SetWidgetFont(lab, "Comic Sans MS", 15, False, True)

prog = add_widget(group, mqt.CreateProgressBar(True))
mqt.SetProgressBarRange(prog, 0, 100)

readonly_text = add_widget(
    group, mqt.CreateTextBox(True), "This text box is read only!"
)
mqt.SetWidgetEnabled(readonly_text, False)

# initialise the button text to start with
update_button()
