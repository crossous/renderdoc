Launching programs manually
===========================

If you are using the RenderDoc UI for scripting, you should use :doc:`the UI interfaces <../examples/exe_launching>` for launching executables. This integrates well with the UI and displays to the user what is happening while still being automatable.

If you are using the RenderDoc python module directly and do not have the UI present, then you can launch and capture from executables directly.

Starting a program
------------------

We will assume you know the program you want to launch and the capture options you want to provide, as detailed in the :doc:`UI example <../examples/exe_launching>`. From here you will use :func:`~renderdoc.ExecuteAndInject` to launch the program.

This function will take all the parameters that can be customised when launching an executable, including not only the executable path and working directory but also :class:`~renderdoc.EnvironmentModification` changes to environment variables, any options with :class:`~renderdoc.CaptureOptions`, and a target path for any captures to be made.

You can choose whether or not this function will be blocking - if you wait for the program to exit then control will not return until the program has exited. This is not recommended when automating as it means you will need to determine which captures were made in another way.

Typically you would not wait, and use the :class:`~renderdoc.ExecuteResult` to determine whether the program launched correctly and how to connect to it.

Connecting to a running program
-------------------------------

If the program was launched successfully, then :data:`~renderdoc.ExecuteResult.ident` tells you the identifier of the running program that can be used to connect to it. It is also possible to enumerate available identifiers on a particular hostname using :func:`~renderdoc.EnumerateRemoteTargets` which allows iterative querying of available identifiers - it will not be detailed here as you are assumed to have the ident from :func:`~renderdoc.ExecuteAndInject` above.

You can make a target control connection to a particular program by connecting to it using :func:`~renderdoc.CreateTargetControl`. This requires the hostname and identifier above, the hostname can be blank for locally launched programs. Only one target control connection can be made to a program at any one time - the client name specified when connecting can be used to disambiguate, and it is also possible to forcibly disconnect any existing connection when connected - RenderDoc assumes co-operation rather than competition for these connections between multiple users.

If the connection was made successfully a :class:`~renderdoc.TargetControl` will be returned which must be managed by python and closed using :meth:`~renderdoc.TargetControl.Shutdown` when finished with.

Target control
--------------

A target control connection allows you to both send and receive messages to the running program, to get information about its status as well as to send commands. Commands can be sent at any time using e.g. :meth:`~renderdoc.TargetControl.TriggerCapture` or :meth:`~renderdoc.TargetControl.QueueCapture`. Responses from these will be received as messages, as well as messages for other information such as new child processes or new captures being made (which may be triggered by user actions).

The target control connection uses a simple message loop to return information to the user without blocking. Calling :meth:`~renderdoc.TargetControl.ReceiveMessage` will check for a new message and return either the new message or a no-op message. The receive function internally will wait a short time if no message is pending so it is safe to call repeatedly in a loop with no extra waits. This also keeps the connection alive so you must call :meth:`~renderdoc.TargetControl.ReceiveMessage` at least once every few seconds to maintain the connection.

The message returned will have a type as specified by :class:`~renderdoc.TargetControlMessageType`, which can be switched on to examine the different data available in the message types. For example if a new capture is made then a :data:`~renderdoc.TargetControlMessageType.NewCapture` type message will be returned and the :data:`~renderdoc.TargetControlMessage.newCapture` member will be valid containing the information about the capture.

Transferring captures
---------------------

If the target control connection is local, any new captures identified will be immediately replayable using :doc:`capture_access` and :meth:`~renderdoc.CaptureFile.OpenCapture`. If the connection is remote it may be necessary to transfer the capture across the connection from the remote machine. This can be done using :meth:`~renderdoc.TargetControl.CopyCapture` and will be notified using a :data:`~renderdoc.TargetControlMessageType.CaptureCopied` message.

It is also possible to leave the capture on the remote machine and use a :class:`~renderdoc.RemoteServer` connection to replay directly on the remote machine - see :doc:`remote_replay`.