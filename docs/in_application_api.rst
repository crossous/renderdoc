In-application API
==================

Reference for RenderDoc in-application API version 1.7.0. This API is not necessary to use RenderDoc by default, but if you would like more control or custom triggering of captures this API can provide the mechanism to do so.

It is recommended to use the latest API header - but be aware that if you request a newer API version than that supported by the RenderDoc you are using it will be unavailable. All RenderDoc builds supporting this API ship the header ``renderdoc_app.h`` in their root directory.

This page describes the RenderDoc API exposed to applications being captured, both in overall organisation as well as a specific reference on each function.

Accessing API
-------------

To begin using the API you need to fetch the :cpp:func:`RENDERDOC_GetAPI` function which is the only directly-exported function. You should never link against RenderDoc's library as it's intended to be injected at runtime by the RenderDoc UI, instead you will load the library at runtime. The header does not declare :cpp:func:`RENDERDOC_GetAPI`, it declares a function pointer typedef ``pRENDERDOC_GetAPI`` that you can use.

The recommended way to access the RenderDoc API is to passively check if the module is loaded, and use the API if it is. This lets you continue to use RenderDoc entirely as normal, launching your program through the UI, but you can access additional functionality to e.g. trigger captures at custom times. When your program is launched on its own it will see that the RenderDoc module is not present and safely fall back to not using RenderDoc's API.

To do this you'll use your platforms dynamic library functions to see if the library is open already - e.g. ``GetModuleHandle`` on Windows, or ``dlopen`` with the ``RTLD_NOW | RTLD_NOLOAD`` flags if available on \*nix systems. On most platforms you can just search for the module name - ``renderdoc.dll`` on Windows, or ``librenderdoc.so`` on Linux, or ``libVkLayer_GLES_RenderDoc.so`` on Android. That way you don't need to know the path to where RenderDoc is running from. This will vary by platform however so consult your platform's OS documentation.

One you have a library handle you can use ``GetProcAddress`` or ``dlsym`` to fetch the ``RENDERDOC_GetAPI`` function using the typedef above.

.. cpp:function:: int RENDERDOC_GetAPI(RENDERDOC_Version version, void **outAPIPointers)

    This function is the only entry point actually exported from the RenderDoc module. You call this function with the desired API version, and pass it the address of a pointer to the appropriate struct type. If successful, RenderDoc will set the pointer to point to a struct containing the function pointers for the API functions (detailed below) and return 1.

    Note that version numbers follow `semantic versioning <http://semver.org>`_ which means the implementation returned may have a higher minor and/or patch version than requested.

    :param RENDERDOC_Version version: is the version number of the API for which you want the interface struct.
    :param void** outAPIPointers: will be filled with the address of the API's function pointer struct, if supported. E.g. if ``eRENDERDOC_API_Version_1_1_1`` is requested, ``outAPIPointers`` will be filled with ``RENDERDOC_API_1_1_1*`` or any newer version that is compatible with API 1.1.1, but nothing lower.
    :return: The function returns 1 if the API version is valid and available, and the struct pointer is filled. The function returns 0 if the API version is invalid or not supported, or the pointer parameter is invalid.

.. _renderdoc-api-example:

Example code:

.. highlight:: c++
.. code:: c++

    #include "renderdoc_app.h"

    RENDERDOC_API_1_1_2 *rdoc_api = NULL;

    // At init, on windows
    if(HMODULE mod = GetModuleHandleA("renderdoc.dll"))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api);
        assert(ret == 1);
    }

    // At init, on linux/android.
    // For android replace librenderdoc.so with libVkLayer_GLES_RenderDoc.so
    if(void *mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api);
        assert(ret == 1);
    }

    // To start a frame capture, call StartFrameCapture.
    // You can specify NULL, NULL for the device to capture on if you have only one device and
    // either no windows at all or only one window, and it will capture from that device.
    // See the documentation below for a longer explanation
    if(rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);

    // Your rendering should happen here
    
    // stop the capture
    if(rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);

API versioning
--------------

.. cpp:enum:: RENDERDOC_Version

    This enum defines the versions of this API, if you have a new header it may define API versions newer than the version of RenderDoc you are running.
    
    Version numbers follow `semantic versioning <http://semver.org>`_ which means the implementation returned may have a higher minor and/or patch version than requested: New patch versions are identical and backwards compatible in functionality. New minor versions add new functionality in a backwards compatible way.

    e.g. It is safe to request API version 1.1.0 and use it as if it were 1.1.0, even if the actual implementation is 1.5.0

.. cpp:function:: void GetAPIVersion(int *major, int *minor, int *patch)

    This function returns the actual API version of the implementation returned, which may be higher than the version requested
    
    :param int* major: will be filled with the major version of the implementation's version.
    :param int* minor: will be filled with the minor version of the implementation's version.
    :param int* patch: which will be filled with the patch version of the implementation's version.
    :return: None

Controlling captures
--------------------

The most common use of the in-application API is to control captures by starting and stopping them at will. This can be done on any API and can use any logic the application wishes to decide what region of work to capture. Using this API is independent of any captures triggered by hotkeys using RenderDoc's normal process of capturing between two presents on a given window.

In these functions you can specify where to capture you can specify an API-specific :cpp:type:`device object <RENDERDOC_DevicePointer>` in case you have multiple APIs or instances of an API in your program, and you can also identify a window for the purposes of the preview thumbnail using a platform-specific :cpp:type:`window handle <RENDERDOC_WindowHandle>`.

It is also possible to pass ``NULL`` for one or both of the device and the window parameters, in which case RenderDoc picks an arbitrary valid device and window that matches. In most cases if you are only using one API and you have at most one window, there is only one choice so the parameters can safely be left as ``NULL``.

.. cpp:type:: void *RENDERDOC_DevicePointer

    ``RENDERDOC_DevicePointer`` is a typedef to ``void *``. The contents of it are API specific:

    * For D3D11 it must be the ``ID3D11Device`` device object.
    * For D3D12 it must be the ``ID3D12Device`` device object.
    * For OpenGL it must be the ``HGLRC``, ``GLXContext``, or ``EGLContext`` context object.
    * For OpenGLES it must be the ``EGLContext`` context object.
    * For Vulkan it must be the dispatch table pointer within the ``VkInstance``. This is a pointer-sized value at the location pointed to by the ``VkInstance``. NOTE - this is not the actual ``VkInstance`` pointer itself. You can use the ``RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE()`` helper macro defined in the renderdoc header to obtain this pointer from any VkInstance.

    Passing ``NULL`` for the device pointer indicates that there is only one graphics API in use so it will be chosen by default. If there are multiple APIs in use, one will be picked at random and this may lead to unexpected or undefined behaviour.

.. cpp:type:: void *RENDERDOC_WindowHandle

    ``RENDERDOC_WindowHandle`` is a typedef to ``void *``. It is the platform specific Windows ``HWND``, Xcb ``xcb_window_t``, Xlib ``Window`` / ``Drawable``, Wayland ``wl_surface*``, or Android ``ANativeWindow*``.

    Passing ``NULL`` for the window indicates that no specific window is needed or that the application is headless with no windows. The thumbnail preview may be taken arbitrarily e.g. the last window presented if there is one.

.. cpp:function:: void StartFrameCapture(RENDERDOC_DevicePointer device, RENDERDOC_WindowHandle wndHandle)

    This function will immediately begin a capture for the specified device/window combination.

    :param RENDERDOC_DevicePointer device: is a handle to the API :cpp:type:`'device' object <RENDERDOC_DevicePointer>` that will be set active. May be ``NULL`` to wildcard match.
    :param RENDERDOC_WindowHandle wndHandle: is a handle to the platform :cpp:type:`window handle <RENDERDOC_WindowHandle>` that will be set active. May be ``NULL`` to wildcard match.

.. cpp:function:: uint32_t EndFrameCapture(RENDERDOC_DevicePointer device, RENDERDOC_WindowHandle wndHandle)

    This function will immediately end an active capture for the specified device/window combination.

    :param RENDERDOC_DevicePointer device: is a handle to the API :cpp:type:`'device' object <RENDERDOC_DevicePointer>` that will be set active. May be ``NULL`` to wildcard match.
    :param RENDERDOC_WindowHandle wndHandle: is a handle to the platform :cpp:type:`window handle <RENDERDOC_WindowHandle>` that will be set active. May be ``NULL`` to wildcard match.

    :return: Returns ``1`` if the capture succeeded, and 0 if there was an error capturing.

    .. important::
        There will be undefined results if there is not an active frame capture for the device/window combination.

.. cpp:function:: void TriggerCapture()

    This function will trigger a capture as if the user had pressed one of the capture hotkeys. The capture will be taken from the next frame presented to whichever window is considered current. It will have no effect if the program is not presenting and the capture hotkeys would not work otherwise.

    See :cpp:func:`SetActiveWindow` for controlling which window is active.

.. cpp:function:: void TriggerMultiFrameCapture(uint32_t numFrames)

    This function will trigger multiple sequential frame captures as if the user had pressed one of the capture hotkeys before each frame. The captures will be taken from the next frames presented to whichever window is considered current.

    Each capture will be taken independently and saved to a separate file, with no reference to the other frames.

    :param uint32_t numFrames: the number of frames to capture, as an unsigned integer.

    .. note::

        Added in API version 1.1.0

.. cpp:function:: uint32_t IsFrameCapturing()

    This function returns a value to indicate whether the current frame is capturing.

    :return: Returns ``1`` if the frame is currently capturing, or ``0`` otherwise.

.. cpp:function:: void SetActiveWindow(RENDERDOC_DevicePointer device, RENDERDOC_WindowHandle wndHandle)

    This function will explicitly set which window is considered active. The active window is the one that will be captured when the keybind to trigger a capture is pressed.

    :param RENDERDOC_DevicePointer device: is a handle to the API :cpp:type:`'device' object <RENDERDOC_DevicePointer>` that will be set active. Must be valid.
    :param RENDERDOC_WindowHandle wndHandle: is a handle to the platform :cpp:type:`window handle <RENDERDOC_WindowHandle>` that will be set active. Must be valid.

.. cpp:function:: uint32_t DiscardFrameCapture(RENDERDOC_DevicePointer device, RENDERDOC_WindowHandle wndHandle)

    This function is similar to :cpp:func:`EndFrameCapture` but the capture contents will be discarded immediately, and not processed and written to disk. This will be more efficient than :cpp:func:`EndFrameCapture` if the frame capture is not needed.

    :param RENDERDOC_DevicePointer device: is a handle to the API :cpp:type:`'device' object <RENDERDOC_DevicePointer>` that will be set active. May be ``NULL`` to wildcard match.
    :param RENDERDOC_WindowHandle wndHandle: is a handle to the platform :cpp:type:`window handle <RENDERDOC_WindowHandle>` that will be set active. May be ``NULL`` to wildcard match.

    :return: Returns ``1`` if the capture was discarded, and 0 if there was an error or no capture was in progress.

    .. important::
        There will be undefined results if there is not an active frame capture for the device/window combination.

    .. note::

        Added in API version 1.4.0

Capture options
---------------

.. cpp:function:: int SetCaptureOptionU32(RENDERDOC_CaptureOption opt, uint32_t val)

    Set one of the options for tweaking some behaviours of capturing. Note that each option only takes effect from after it is set - so it is advised to set these options as early as possible, ideally before any graphics API has been initialised.

    :param RENDERDOC_CaptureOption opt: specifies which capture option should be set.
    :param uint32_t val: the unsigned integer value to set for the above option.
    :return: The function returns 1 if the option is valid, and the value set on the option is within valid ranges. The function returns 0 if the option is not a :cpp:enum:`RENDERDOC_CaptureOption` enum, or the value is not valid for the option.

.. cpp:function:: int SetCaptureOptionF32(RENDERDOC_CaptureOption opt, float val)

    Set one of the options for tweaking some behaviours of capturing. Note that each option only takes effect from after it is set - so it is advised to set these options as early as possible, ideally before any graphics API has been initialised..

    :param RENDERDOC_CaptureOption opt: specifies which capture option should be set.
    :param float val: the floating point value to set for the above option.
    :return: The function returns 1 if the option is valid, and the value set on the option is within valid ranges. The function returns 0 if the option is not a :cpp:enum:`RENDERDOC_CaptureOption` enum, or the value is not valid for the option.

.. cpp:function:: uint32_t GetCaptureOptionU32(RENDERDOC_CaptureOption opt)

    Gets the current value of one of the different options listed above in :cpp:func:`SetCaptureOptionU32`.

    :param RENDERDOC_CaptureOption opt: specifies which capture option should be retrieved.
    :return: The function returns the value of the capture option, if the option is a valid :cpp:enum:`RENDERDOC_CaptureOption` enum. Otherwise returns ``0xffffffff``.

.. cpp:function:: float GetCaptureOptionF32(RENDERDOC_CaptureOption opt)

    Gets the current value of one of the different options listed above in :cpp:func:`SetCaptureOptionF32`.

    :param RENDERDOC_CaptureOption opt: specifies which capture option should be retrieved.
    :return: The function returns the value of the capture option, if the option is a valid :cpp:enum:`RENDERDOC_CaptureOption` enum. Otherwise returns `-FLT_MAX`.

.. cpp:enum:: RENDERDOC_CaptureOption

    Many values in this enum correspond to options in the :doc:`window/capture_attach` window, so more documentation about their meaning can be found there. Some values here are only available programmatically through this API.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_AllowVSync

    specifies whether the application is allowed to enable vsync. Default is on.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_AllowFullscreen

    specifies whether the application is allowed to enter exclusive fullscreen. Default is on.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_APIValidation

    specifies whether (where possible) API-specific debugging is enabled. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_CaptureCallstacks

    specifies whether each API call should save a callstack. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_CaptureCallstacksOnlyActions

    specifies whether - if ``CaptureCallstacks`` is enabled - callstacks are only saved on actions. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_DelayForDebugger

    specifies a delay in seconds after launching a process to pause, to allow debuggers to attach. This will only apply to child processes since the delay happens at process startup. Default is 0.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_VerifyBufferWrites

    specifies whether any mapped memory updates should be bounds-checked for overruns, and uninitialised buffers are initialised to ``0xdddddddd`` to catch use of uninitialised data. Only supported on D3D11 and OpenGL. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_HookIntoChildren

    specifies whether child processes launched by the initial application should be hooked as well - commonly if a launcher process is needed to run the application. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_RefAllResources

    specifies whether all live resources at the time of capture should be included in the capture, even if they are not referenced by the frame. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_CaptureAllCmdLists

    specifies whether all command lists should be captured on D3D11 where multithreaded submission is not optimal, rather than only capturing those recorded after frame capture begins. Default is off.

.. cpp:enumerator:: RENDERDOC_CaptureOption::eRENDERDOC_Option_DebugOutputMute

    specifies whether to mute any API debug output messages when ``APIValidation`` is enabled, and not pass them along to the application. Default is on.

.. cpp:function:: void SetCaptureFilePathTemplate(const char *pathtemplate)

    Set the template for new captures. The template can either be a relative or absolute path, which determines where captures will be saved and how they will be named. If the path template is ``my_captures/example`` then captures saved will be e.g. ``my_captures/example_frame123.rdc`` and ``my_captures/example_frame456.rdc``. Relative paths will be saved relative to the process's current working directory. The default template is in a folder controlled by the UI - initially the system temporary folder, and the filename is the executable's filename.

    :param const char* pathtemplate: specifies the capture path template to set, as UTF-8 null-terminated string.

    .. note::

        This function was renamed, in earlier versions of the API it was declared as ``SetLogFilePathTemplate``. This rename is backwards compatible as the function signature did not change.

.. cpp:function:: const char *GetCaptureFilePathTemplate()

    Get the current capture path template, see :cpp:func:`SetCaptureFilePathTemplate`.

    :return: the current capture path template as a UTF-8 null-terminated string.

    .. note::

        This function was renamed, in earlier versions of the API it was declared as ``GetLogFilePathTemplate``. This rename is backwards compatible as the function signature did not change.

.. cpp:function:: void SetCaptureTitle(const char *title)

    This function sets a given title for the currently in-progress capture, which will be displayed in the UI. This can be used either with a user-defined capture using a manual start and end, or an automatic capture triggered by :cpp:func:`TriggerCapture` or a keypress.

    If multiple captures are ongoing at once, the title will be applied to the first capture to end only. Any subsequent captures will not get any title unless the function is called again.

    This function can only be called while a capture is in-progress, after :cpp:func:`StartFrameCapture` and before :cpp:func:`EndFrameCapture`. If it is called elsewhere it will have no effect. If it is called multiple times within a capture, only the last title will have any effect.

    .. note::

        Added in API version 1.6.0

.. cpp:function:: void SetCaptureFileComments(const char *filePath, const char *comments)

    This function adds an arbitrary comments field to an existing capture on disk, which will then be displayed in the UI to anyone opening the capture.

    :param const char* filePath: specifies the path to the capture file to set comments in, as UTF-8 null-terminated string. If this path is ``NULL`` or an empty string, the most recent capture file that has been created will be used.
    :param const char* comments: specifies the comments to set in the capture file, as UTF-8 null-terminated string.
    
    .. note::

        Added in API version 1.2.0

Changing Keybinds
-----------------

.. cpp:function:: void SetFocusToggleKeys(RENDERDOC_InputButton *keys, int num)

    This function changes the key bindings in-application for changing the focussed window.

    :param RENDERDOC_InputButton* keys: lists the keys to bind. If this parameter is ``NULL``, ``num`` must be 0.
    :param int num: specifies the number of keys in the ``keys`` array. If 0, the keybinding is disabled.

.. cpp:function:: void SetCaptureKeys(RENDERDOC_InputButton *keys, int num)

    This function changes the key bindings in-application for triggering a capture on the current window.

    :param RENDERDOC_InputButton* keys: lists the keys to bind. If this parameter is ``NULL``, ``num`` must be 0.
    :param int num: specifies the number of keys in the ``keys`` array. If 0, the keybinding is disabled.

.. cpp:enum:: RENDERDOC_InputButton

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_0

    ``eRENDERDOC_Key_0`` to ``eRENDERDOC_Key_9`` are the number keys. The values of these match ASCII for '0' .. '9'.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_A

    ``eRENDERDOC_Key_A`` to ``eRENDERDOC_Key_Z`` are the letter keys. The values of these match ASCII for 'A' .. 'Z'.


.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Divide

    is the Divide key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Multiply

    is the Multiply key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Subtract

    is the Subtract key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Plus

    is the Plus key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_F1

    ``eRENDERDOC_Key_F1`` to ``eRENDERDOC_Key_F12`` are the function keys.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Home

    is the Home key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_End

    is the End key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Insert

    is the Insert key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Delete

    is the Delete key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_PageUp

    is the PageUp key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_PageDn

    is the PageDn key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Backspace

    is the Backspace key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Tab

    is the Tab key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_PrtScrn

    is the PrtScrn key.

.. cpp:enumerator:: RENDERDOC_InputButton::eRENDERDOC_Key_Pause

    is the Pause key.

Configuring overlay
-------------------

.. cpp:function:: uint32_t GetOverlayBits()

    This function returns the current mask which determines what sections of the overlay render on each window.

    :return: A mask containing bits from :cpp:enum:`RENDERDOC_OverlayBits`.

.. cpp:function:: void MaskOverlayBits(uint32_t And, uint32_t Or)

    This function modifies the current mask which determines what sections of the overlay render on each window.

    :param uint32_t And: is a 32-bit value that will be combined using binary AND with the mask first, to remove bits.
    :param uint32_t Or: is a 32-bit value that will be combined using binary OR with the mask second, to add bits.

.. cpp:enum:: RENDERDOC_OverlayBits

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_Enabled

    is an overall enable/disable bit. If this is disabled, no overlay renders.

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_FrameRate

    shows the average, min and max frame time in milliseconds, and the average framerate.

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_FrameNumber

    shows the current frame number, as counted by the number of presents.

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_CaptureList

    shows how many total captures have been made, and a list of captured frames in the last few seconds.

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_Default

    is the default set of bits, which is the value of the mask at startup.

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_All

    is equal to ``~0U`` so all bits are enabled.

.. cpp:enumerator:: RENDERDOC_OverlayBits::eRENDERDOC_Overlay_None

    is equal to ``0`` so all bits are disabled.

Shutdown
--------

.. cpp:function:: void RemoveHooks()

    This function will attempt to remove RenderDoc and its hooks from the target process. It must be called as early as possible in the process, and will have undefined results if any graphics API functions have been called.

    .. important::

        This process is only possible on Windows, and even then it is not well defined so may not be possible in all circumstances. This function is provided at your own risk.

    .. note::

        This function was renamed, in earlier versions of the API it was declared as ``Shutdown``. This rename is backwards compatible as the function signature did not change.

.. cpp:function:: void UnloadCrashHandler()

    This function will remove RenderDoc's crash handler from the target process. If you have your own crash handler that you want to handle any exceptions, RenderDoc's handler could interfere so it can be disabled.

Querying state
--------------

.. cpp:function:: uint32_t GetNumCaptures()

    This function returns the number of frame captures that have been made.

    :return: the number of frame captures that have been made

.. cpp:function:: uint32_t GetCapture(uint32_t idx, char *filename, uint32_t *pathlength, uint64_t *timestamp)

    This function returns the details of a particular frame capture, as specified by an index from 0 to :cpp:func:`GetNumCaptures` - 1.

    :param uint32_t idx: specifies which capture to return the details of. Must be less than the return value of :cpp:func:`GetNumCaptures`.
    :param char* filename: is an optional parameter filled with the UTF-8 null-terminated path to the file. There must be enough space in the array to contain all characters including the null terminator. If set to NULL, nothing is written.
    :param uint32_t* pathlength: is an optional parameter filled with the byte length of the above ``filename`` including the null terminator. If set to NULL, nothing is written.
    :param uint64_t* timestamp: is an optional parameter filled with the 64-bit timestamp of the file - equivalent to the ``time()`` system call. If set to NULL, nothing is written.
    :return: Returns ``1`` if the capture index was valid, or ``0`` if it was out of range.

    .. note::

        It is advised to call this function twice - first to obtain ``pathlength`` so that sufficient space can be allocated. Then again to actually retrieve the path.


The path follows the template set in :cpp:func:`SetCaptureFilePathTemplate` so it may not be an absolute path.

.. cpp:function:: uint32_t IsTargetControlConnected()

    This function returns a value to indicate whether the RenderDoc UI is currently connected to the current process.

    :return: Returns ``1`` if the RenderDoc UI is currently connected, or ``0`` otherwise.

    .. note::

        This function was renamed, in earlier versions of the API it was declared as ``IsRemoteAccessConnected``. This rename is backwards compatible as the function signature did not change.

UI integration
--------------

.. cpp:function:: uint32_t LaunchReplayUI(uint32_t connectTargetControl, const char *cmdline)

    This function will determine the closest matching replay UI executable for the current RenderDoc module and launch it.

    :param uint32_t connectTargetControl: should be set to 1 if the UI should immediately connect to the application.
    :param const char* cmdline: is an optional UTF-8 null-terminated string to be appended to the command line, e.g. a capture filename. If this parameter is NULL, the command line will be unmodified.
    :return: If the UI was successfully launched, this function will return the PID of the new process. Otherwise it will return ``0``.

.. cpp:function:: uint32_t ShowReplayUI()

    This function requests that the currently connected replay UI raise its window to the top. This is only possible if an instance of the replay UI is currently connected, otherwise this function does nothing. This can be used in conjunction with IsTargetControlConnected and LaunchReplayUI to intelligently handle showing the UI after making a capture.

    Given OS differences it is not guaranteed that the UI will be successfully raised even if the request is passed on. On some OSs it may only be highlighted or otherwise indicated to the user.

    :return: If the request to be shown was passed onto the UI successfully this function will return ``1``. If there is no UI connected currently or some other error occurred it will return ``0``.

    .. note::

        Added in API version 1.5.0

Custom annotations
------------------

This functionality was all added in API version 1.7.0

.. cpp:function:: uint32_t SetObjectAnnotation(RENDERDOC_DevicePointer device, void* object, const char *key, RENDERDOC_AnnotationType valueType, uint32_t valueVectorWidth, const RENDERDOC_AnnotationValue* value)

    This function allows associating custom rich annotations with API objects. These annotations can be examined in the :doc:`window/resource_inspector`.
    
    See the :doc:`window/annotation_viewer` documentation for more detailed information on the annotation system.

    :param RENDERDOC_DevicePointer device: is a handle to the API :cpp:type:`'device' object <RENDERDOC_DevicePointer>` that will be set active. May be ``NULL`` to wildcard match.
    :param void* object: is a handle to the API object that will be annotated. Must not be ``NULL``.
    :param const char* key: is a dot separated path for the annotation to update. Must not be ``NULL`` or empty.
    :param RENDERDOC_AnnotationType valueType: is the type of value to set, including basic scalar types as well as strings and API objects.
    :param uint32_t valueVectorWidth: is the vector width of the value to set, or 0 for scalars. Must be no greater than 4.
    :param const RENDERDOC_AnnotationValue* value: is a pointer to the value itself.

    :return: Returns ``0`` if the annotation was successfully set.

       Returns ``1`` if the device is unknown or invalid.

       Returns ``2`` if the device is valid but the annotation is not recognised or not supported for API-specific reasons, such as an unrecognised or invalid object or queue/commandbuffer.

       Returns ``3`` if the call is ill-formed or invalid e.g. empty is specified with a value pointer, or non-empty is specified with a NULL value pointer.

    Deleting an annotation can be done by setting a value with ``valueType`` equal to ``eRENDERDOC_Empty`` and ``value`` equal to ``NULL``. This deletes the annotation at the specific key, as well as any children of that key.

    You can set ``valueVectorWidth`` to 0 for single values, which is equivalent to setting 1. This is required for strings and API objects, which can't be vectors.

    .. note::

        ``device`` can be set to ``NULL`` if there is only one graphics API in use, see :cpp:type:`RENDERDOC_DevicePointer`.

.. cpp:union:: RENDERDOC_AnnotationVectorValue

    A union with 4-wide vector members for the different scalar types. This does not include strings or API objects.

.. cpp:member:: bool RENDERDOC_AnnotationVectorValue::boolean[4]

    An array of 4 bool values.

.. cpp:member:: int32_t RENDERDOC_AnnotationVectorValue::int32[4]

    An array of 4 32-bit signed integer values.

.. cpp:member:: int64_t RENDERDOC_AnnotationVectorValue::int64[4]

    An array of 4 64-bit signed integer values.

.. cpp:member:: uint32_t RENDERDOC_AnnotationVectorValue::uint32[4]

    An array of 4 32-bit unsigned integer values.

.. cpp:member:: uint64_t RENDERDOC_AnnotationVectorValue::uint64[4]

    An array of 4 64-bit unsigned integer values.

.. cpp:member:: float RENDERDOC_AnnotationVectorValue::float32[4]

    An array of 4 32-bit floating point values.

.. cpp:member:: double RENDERDOC_AnnotationVectorValue::float64[4]

    An array of 4 64-bit floating point values.

.. cpp:union:: RENDERDOC_AnnotationValue

    A union with different members for the different types of annotations that are possible. It includes scalar values, a vector member, a C style ``NULL``-terminated string and a ``void *`` for API objects.

    A helper structure ``RDAnnotationHelper`` is available in C++ which can create this in-line as a temporary object.

.. cpp:member:: bool RENDERDOC_AnnotationValue::boolean

    A bool value.

.. cpp:member:: int32_t RENDERDOC_AnnotationValue::int32

    A 32-bit signed integer value.

.. cpp:member:: int64_t RENDERDOC_AnnotationValue::int64

    A 64-bit signed integer value.

.. cpp:member:: uint32_t RENDERDOC_AnnotationValue::uint32

    A 32-bit unsigned integer value.

.. cpp:member:: uint64_t RENDERDOC_AnnotationValue::uint64

    A 64-bit unsigned integer value.

.. cpp:member:: float RENDERDOC_AnnotationValue::float32

    A 32-bit floating point value.

.. cpp:member:: double RENDERDOC_AnnotationValue::float64

    A 64-bit floating point value.

.. cpp:member:: RENDERDOC_AnnotationVectorValue RENDERDOC_AnnotationValue::vector

    A union containing a vector type - used for annotations that have a vector size greater than 1.

.. cpp:member:: const char *RENDERDOC_AnnotationValue::string

    A ``NULL``-terminated UTF-8 string.

.. cpp:member:: void *RENDERDOC_AnnotationValue::apiObject

    A pointer to an API object handle. For most APIs this is directly the API handle itself, on OpenGL this should be a pointer to an instance of :cpp:struct:`RENDERDOC_GLResourceReference`.

    .. warning::

        For Vulkan, annotating ``VkInstance``, ``VkPhysicalDevice``, or ``VkDevice`` objects may encounter problems due to loader wrapping. To address this, you can use ``vkSetDebugUtilsObjectTagEXT`` to set a tag with the ``tagName`` set to ``RENDERDOC_APIObjectAnnotationHelper`` and the ``pTag`` set to the handle of the object itself. After doing that once RenderDoc will be able to recognise those handles.

.. cpp:struct:: RENDERDOC_GLResourceReference

    This struct refers to a GL object handle on the context that is *currently active* on the thread where this is called.

    A helper structure ``RDGLObjectHelper`` is available in C++ which can create this in-line as a temporary object.

.. cpp:member:: uint32_t RENDERDOC_GLResourceReference::identifier

    This identifier must be the same ``GLenum`` enum as is passed to ``glObjectLabel``, e.g. ``GL_TEXTURE``.

.. cpp:member:: uint32_t RENDERDOC_GLResourceReference::name

    This name must be the integer name of the object being referred to.

.. cpp:enum:: RENDERDOC_AnnotationType

.. cpp:enumerator:: RENDERDOC_AnnotationType::eRENDERDOC_Empty

    ``eRENDERDOC_Empty`` is used to delete an annotation. All children are also deleted.

.. cpp:enumerator:: RENDERDOC_AnnotationType::eRENDERDOC_Bool

    ``eRENDERDOC_Bool`` indicates that :cpp:member:`RENDERDOC_AnnotationValue::boolean` or :cpp:member:`RENDERDOC_AnnotationVectorValue::boolean` is valid.

.. cpp:enumerator:: RENDERDOC_AnnotationType::eRENDERDOC_Int32

    ``eRENDERDOC_Int32`` indicates that :cpp:member:`RENDERDOC_AnnotationValue::int32` or :cpp:member:`RENDERDOC_AnnotationVectorValue::int32` is valid.

.. cpp:enumerator:: RENDERDOC_AnnotationType::eRENDERDOC_UInt32

    ``eRENDERDOC_UInt32`` indicates that :cpp:member:`RENDERDOC_AnnotationValue::uint32` or :cpp:member:`RENDERDOC_AnnotationVectorValue::uint32` is valid.

.. cpp:enumerator:: RENDERDOC_AnnotationType::eRENDERDOC_Int64

    ``eRENDERDOC_Int64`` indicates that :cpp:member:`RENDERDOC_AnnotationValue::int64` or :cpp:member:`RENDERDOC_AnnotationVectorValue::int64` is valid.

.. cpp:enumerator:: RENDERDOC_AnnotationType::eRENDERDOC_UInt64

    ``eRENDERDOC_UInt64`` indicates that :cpp:member:`RENDERDOC_AnnotationValue::uint64` or :cpp:member:`RENDERDOC_AnnotationVectorValue::uint64` is valid.

.. cpp:enumerator:: RENDERDOC_AnnotationType::eRENDERDOC_Float

    ``eRENDERDOC_Float`` indicates that :cpp:member:`RENDERDOC_AnnotationValue::float32` or :cpp:member:`RENDERDOC_AnnotationVectorValue::float32` is valid.

.. cpp:enumerator:: RENDERDOC_AnnotationType::eRENDERDOC_Double

    ``eRENDERDOC_Double`` indicates that :cpp:member:`RENDERDOC_AnnotationValue::float64` or :cpp:member:`RENDERDOC_AnnotationVectorValue::float64` is valid.

.. cpp:enumerator:: RENDERDOC_AnnotationType::eRENDERDOC_String

    ``eRENDERDOC_String`` indicates that :cpp:member:`RENDERDOC_AnnotationValue::string` is valid.
 
.. cpp:enumerator:: RENDERDOC_AnnotationType::eRENDERDOC_APIObject

    ``eRENDERDOC_APIObject`` indicates that :cpp:member:`RENDERDOC_AnnotationValue::apiObject` is valid.
 
.. cpp:function:: uint32_t SetCommandAnnotation(RENDERDOC_DevicePointer device, void* queueOrCommandBuffer, const char* key, RENDERDOC_AnnotationType valueType, uint32_t valueVectorWidth, const RENDERDOC_AnnotationValue* value)

    This function allows adding custom rich annotations to command streams. These annotations can be examined in the :doc:`window/annotation_viewer` which has more information on the annotation system.

    For more information see the page on the :doc:`../window/annotation_viewer`.

    :param RENDERDOC_DevicePointer device: is a handle to the API :cpp:type:`'device' object <RENDERDOC_DevicePointer>` that will be set active. May be ``NULL`` to wildcard match.
    :param void* queueOrCommandBuffer: is a handle to the API-specific command buffer or queue.
    :param const char* key: is a dot separated path for the annotation to update. Must not be ``NULL`` or empty.
    :param RENDERDOC_AnnotationType valueType: is the type of value to set, including basic scalar types as well as strings and API objects.
    :param uint32_t valueVectorWidth: is the vector width of the value to set, or 0 for scalars. Must be no greater than 4.
    :param const RENDERDOC_AnnotationValue* value: is a pointer to the value itself.

    :return: Returns ``0`` if the annotation was successfully set.

       Returns ``1`` if the device is unknown or invalid.

       Returns ``2`` if the device is valid but the annotation is not recognised or not supported for API-specific reasons, such as an unrecognised or invalid object or queue/commandbuffer.

       Returns ``3`` if the call is ill-formed or invalid e.g. empty is specified with a value pointer, or non-empty is specified with a NULL value pointer.

    Deleting an annotation can be done by setting a value with ``valueType`` equal to ``eRENDERDOC_Empty`` and ``value`` equal to ``NULL``. This deletes the annotation at the specific key, as well as any children of that key.

    You can set ``valueVectorWidth`` to 0 for single values, which is equivalent to setting 1. This is required for strings and API objects, which can't be vectors.

    The ``queueOrCommandBuffer`` parameter refers to a different object depending on the API:

    * On Vulkan, it can either be a ``VkCommandBuffer`` in the recording state, or a ``VkQueue``.
    * On OpenGL, it must be ``NULL``.
    * On D3D11 it must either be ``NULL`` or the immediate context ``ID3D11DeviceContext*``.
    * On D3D12 it must either be an ``ID3D12GraphicsCommandList*`` or a ``ID3D12CommandQueue*``.

    .. note::

        ``device`` can be set to ``NULL`` if there is only one graphics API in use, see :cpp:type:`RENDERDOC_DevicePointer`.
