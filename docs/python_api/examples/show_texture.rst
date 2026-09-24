Example: Show and save a texture
================================

This example demonstrates how to enumerate textures, show one in the texture viewer, and save the texture to disk.

Fetching Texture Metadata
-------------------------

First we iterate through the list of textures (:meth:`~qrenderdoc.CaptureContext.GetTextures`) and print their dimensions as we go. We keep track of which texture has the largest area.

.. highlight:: python
.. code:: python

    highestArea = 0
    largest = None
    for tex in pyrenderdoc.GetTextures():
        name = pyrenderdoc.GetResourceName(tex.resourceId)
        print(f"{name} is {tex.width} x {tex.height}")
        area = tex.width * tex.height
        if area > highestArea:
            highestArea = area
            largest = tex

Opening in Texture Viewer
-------------------------

Once we've found the largest texture, we print its information again as a summary and then show (:meth:`~qrenderdoc.CaptureContext.ShowTextureViewer`) and ask the :class:`~qrenderdoc.TextureViewer` to display it as a new locked tab (:meth:`~qrenderdoc.TextureViewer.ViewTexture`).

.. highlight:: python
.. code:: python

    if largest is not None:
        name = pyrenderdoc.GetResourceName(largest.resourceId)
        print(f"\n+++ Largest texture is {name}")

        # open largest texture (by area) in texture viewer, and focus
        pyrenderdoc.ShowTextureViewer()
        pyrenderdoc.GetTextureViewer().ViewTexture(largest.resourceId,
                                                renderdoc.CompType.Typeless,
                                                True)

.. figure:: ../../imgs/Screenshots/CurrentVsLockedTab.png

	An example locked tab that has been opened from the python script.

To go further we will now save this texture to disk in a couple of different formats.

Saving Texture to Disk
----------------------

We will need to obtain the :class:`~renderdoc.ReplayController` which controls RenderDoc's underlying analysis.

.. tip::
    Although not shown in this example, with the texture ID you can use :meth:`~renderdoc.ReplayController.GetTextureData` to fetch the raw bytes for a given subresource in a texture, for arbitrary processing.

For convenience we will fetch a blocking version (:meth:`~qrenderdoc.CaptureContext.GetBlockingController`) that stalls the python script and executes the given command. If this code ran in a UI extension that could cause the UI to become unresponsive while the texture is processed and written to disk so this work could be done on a thread instead - see :ref:`pythreading`.

.. highlight:: python
.. code:: python

    controller = pyrenderdoc.GetBlockingController()

Next so that we know where to save the file, we prompt the user to browse to a filename (:meth:`qrenderdoc.ExtensionManager.SaveFileName`). We'll replace the extension so trim off any ``.jpg`` we get.

.. highlight:: python
.. code:: python

    filename = pyrenderdoc.Extensions().SaveFileName(
        "Choose where to save JPG/PNG/DDS texture files", "", "*.jpg"
    )

    filename = filename.replace(".jpg", "")

Saving textures to disk can require a few different configuration options, which is contained in the :class:`~renderdoc.TextureSave` configuration structure.

Not all textures map cleanly to normal texture formats and some textures may have multiple mips or array slices. To start with we will specify that when writing a texture format without an alpha channel RenderDoc should blend to a checkerboard pattern (:data:`~renderdoc.AlphaMapping.BlendToCheckerboard`). We also choose to save mip 0 if there are multiple mips, and if there are multiple slices save only slice 0. Other options are possible to e.g. lay out all slices in a grid atlas.

.. highlight:: python
.. code:: python

    texsave = renderdoc.TextureSave()
    texsave.resourceId = largest.resourceId

    # Blend alpha to a checkerboard pattern for formats without alpha support
    texsave.alpha = renderdoc.AlphaMapping.BlendToCheckerboard

    # Most formats can only display a single image per file, so we select the
    # first mip and first slice
    texsave.mip = 0
    texsave.slice.sliceIndex = 0

With that done we can save the texture in both :data:`~renderdoc.FileType.JPG` and :data:`~renderdoc.FileType.PNG` formats with a call to :meth:`~renderdoc.ReplayController.SaveTexture`.

.. highlight:: python
.. code:: python

    texsave.destType = renderdoc.FileType.JPG
    controller.SaveTexture(texsave, filename + ".jpg")

    # For formats with an alpha channel, preserve it
    texsave.alpha = renderdoc.AlphaMapping.Preserve

    texsave.destType = renderdoc.FileType.PNG
    controller.SaveTexture(texsave, filename + ".png")

Finally we will save to :data:`~renderdoc.FileType.DDS`, and in this case we now have a texture format that can support mips and array slices. We'll change the configuration to ensure that all mips and all array slices are written to the same file.

.. code:: python
    
    # DDS textures can save multiple mips and array slices, so instead
    # of the default behaviour of saving mip 0 and slice 0, we set -1
    # which saves *all* mips and slices
    texsave.mip = -1
    texsave.slice.sliceIndex = -1

    texsave.destType = renderdoc.FileType.DDS
    controller.SaveTexture(texsave, filename + ".dds")

Example Source
--------------

This example can be found under the name "Show and save a texture" in the python scripting window.

.. only:: html and not htmlhelp

    :download:`Download the example script <show_texture.py>`.

.. literalinclude:: show_texture.py
