Example: Launching an application
=================================

In this example we will show how you can utilise the UI's :doc:`../../window/capture_attach` to configure and launch an application for capture, which could be useful as a way of automating workflows or tests.

Configuring capture
-------------------

After showing and getting the capture dialog handle, we can directly set things like the executable path, command line, or also working directory as needed. In our case we ask the user to browse to the executable. We set a filter of ``*.exe`` which is only relevant on windows, you could change this as needed. These common properties can be set directly with helper functions

.. highlight:: python
.. code:: python

    pyrenderdoc.ShowCaptureDialog()
    dialog = pyrenderdoc.GetCaptureDialog()

    exe = pyrenderdoc.Extensions().OpenFileName("Find an executable", "", "*.exe")

    dialog.SetExecutableFilename(exe)

    dialog.SetCommandLine("--cool-level very")

For setting more specific functionality, we can grab the whole set of settings. This contains everything about the configuration for launching an application including any capture options.

.. highlight:: python
.. code:: python

    settings = dialog.Settings()

    # we could also set the command line here, this is identical to SetCommandLine() above
    print(settings.commandLine)

    # reset anything the user has changed to default
    settings.options = renderdoc.CaptureOptions()

    # enable callstack capture
    settings.options.captureCallstacks = True

    dialog.SetSettings(settings)

Launching capture
-----------------

Once we have set the configuration, we double-check with the user - this is optional but good for our example. If they click yes, we will launch the capture.

.. highlight:: python
.. code:: python

    opts = [qrenderdoc.DialogButton.Yes, qrenderdoc.DialogButton.No]
    go = pyrenderdoc.Extensions().QuestionDialog("Ready to Launch?", opts, "Final Check")

    if go == qrenderdoc.DialogButton.Yes:
        conn = dialog.Launch()

Capture connection
------------------

If the program has successfully launched, we will be returned a handle to the capture connection window that is added to the UI. This contains the details of the active connection and any captures made.

.. warning::

    Capture connection windows are temporary and may close themselves if the program exits after not making any captures - at this point the connection handle we hold will no longer be valid. You should either register a callback to be invoked when the connection closes itself with :meth:`~qrenderdoc.CaptureConnection.RegisterClosedCallback`, or prevent this auto-closing with :meth:`~qrenderdoc.CaptureConnection.PreventAutoClose`.

With this connection it is possible to control the capture and opening of captures, which is outside the scope of this example. We will take a very simple extra step, having a callback after 5 seconds which prints the connected program and the active graphics APIs that have been initialised.

.. highlight:: python
.. code:: python

    def connected_cb():
        print(f"Connected to {conn.Target()} running APIs: {', '.join(conn.GetAPIs())}")

        numcaps = len(conn.GetCaptures())
        if numcaps == 0:
            print("No captures have been made!")
        else:
            print(f"{numcaps} captures have been made!")

    # wait a little bit, then call our callback to print the connection status
    pyrenderdoc.DelayedCallback(5000, connected_cb)

Example Source
--------------

This example can be found under the name "Launching an application" in the python scripting window.

.. only:: html and not htmlhelp

    :download:`Download the example script <exe_launching.py>`.

.. literalinclude:: exe_launching.py
