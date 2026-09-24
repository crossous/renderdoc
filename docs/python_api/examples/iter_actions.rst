Example: Iterating over Actions
===============================

In this example we will walk through the actions in a capture and print them out as a quick tree summary.

Actions include broadly anything that can modify memory - this includes draws and dispatches, as well as clears/copies. Although not modifying memory, marker regions and marker labels are also considered actions and form a hierarchy of nested markers.

Beginning recursion of action tree
----------------------------------

Actions are stored in a tree structure :class:`~renderdoc.ActionDescription`. Each action may have 0 or more children stored in :data:`~renderdoc.ActionDescription.children` and so we will walk this tree with a recursive function.

Each action also stores links to the :data:`~renderdoc.ActionDescription.previousAction`, :data:`~renderdoc.ActionDescription.nextAction`, and :data:`~renderdoc.ActionDescription.parent` actions but you should note that these may be ``None``. Previous and next actions are based on the linear :ref:`event ID <eventids>` and can be used for linearly walking events.

.. tip::
    The :class:`~qrenderdoc.EventBrowser` has some helpers for also fetching actions such as :meth:`~qrenderdoc.EventBrowser.GetActionForEID`.

The root of this recursion starts appropriately with the root actions obtained with :meth:`~qrenderdoc.CaptureContext.CurRootActions` - the list of actions in a capture which have no parents. We will call our recursive function with this list and it returns a list of strings to print.

.. highlight:: python
.. code:: python

    for line in format_tree(pyrenderdoc.CurRootActions()):
        print(line)

Recursing into marker regions
-----------------------------

Our function will receive a list of actions and process them. First we will define how we recurse, by checking :data:`~renderdoc.ActionDescription.flags`. These flags can be used to quickly check for the 'type' of action - and we look for :data:`~renderdoc.ActionFlags.PushMarker` or :data:`~renderdoc.ActionFlags.MultiAction`.

.. highlight:: python
.. code:: python

    from typing import List

    def format_tree(actions: List[renderdoc.ActionDescription]):
        ret = []

        for a in actions:
            ActionFlags = renderdoc.ActionFlags

            if a.flags & (ActionFlags.PushMarker | ActionFlags.MultiAction):
                ret.append(f"{a.customName}:")
                ret += ["    " + l for l in format_tree(a.children)]

        return ret

.. tip::
    The import and use of ``typing.List`` is optional, python type annotations have no semantic meaning on the code, but they are useful to inform IDEs and RenderDoc's script editor of the type you expect for arguments and improve autocomplete. Without this, type checkers will not know the type of ``a`` in the loop and will not be able to provide autocomplete of its members.

This will look at each action, and whenever we encounter a marker region print the name of the marker region and then recursively call on the children with an indent. As we return a list of lines this makes it easy for us to have one indent level per level of recursion.

Counting other actions
----------------------

This will already form a complete recursion of the tree of markers and print them out, but we can also do more as we are walking through by counting the number of some other types of actions as we go.

These can also be identified via the :class:`~renderdoc.ActionFlags` flags.

.. highlight:: python
.. code:: python

    def format_tree(actions: List[renderdoc.ActionDescription]):
        draws, dispatches, copies = 0, 0, 0
        ret = []

        for a in actions:
            ActionFlags = renderdoc.ActionFlags

            if a.flags & (ActionFlags.PushMarker | ActionFlags.MultiAction):
                ...
            # for non marker-regions, count them
            elif a.flags & ActionFlags.Drawcall:
                draws += 1
            elif a.flags & ActionFlags.Dispatch:
                dispatches += 1
            elif a.flags & (ActionFlags.Copy | ActionFlags.Clear):
                copies += 1

This allows us to check for some number of actions quickly via the flags and count them up individually.

Once we have the final counts and have finished iterating over the list of actions, we can format these counts into an extra line for returning.

.. highlight:: python
.. code:: python

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

Final Output
------------

Depending on your capture, it may look something like this, with some markers having both children and draws/dispatches, and other markers only containing either a dispatch or other markers:

.. sourcecode:: text

    Scene Render:
        Particle Update:
            7 dispatches, 4 clears/copies
            ExecuteIndirect(maxCount 1, count <1>):
                1 dispatches
            ExecuteIndirect(maxCount 1, count <1>):
                1 dispatches
            ExecuteIndirect(maxCount 1, count <1>):
                1 dispatches
        RenderLightShadows:
            34 draws, 2 clears/copies
        Z PrePass:
            Opaque:
                29 draws, 1 clears/copies
            Cutout:
                5 draws
        Generate SSAO:
            Decompress and downsample:
                2 dispatches
            Analyze depth volumes:
                5 dispatches
            Blur and upsample:
                3 dispatches
    ...

Example Source
--------------

This example can be found under the name "Iterating over Actions" in the python scripting window.

.. only:: html and not htmlhelp

    :download:`Download the example script <iter_actions.py>`.

.. literalinclude:: iter_actions.py
