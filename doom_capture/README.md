# Optional DOOM startup adapter

This x64 DLL is an opt-in adapter on `codex/doom-capture-bootstrap`. It is built
separately from RenderDoc and is not part of the default release package.

The standard runtime continues to filter unsupported NV mesh and opacity
micromap extensions. For the verified DOOM executable, the adapter sets the
game's existing KHR fallback CVARs in process memory before `vkCreateInstance`:
`r_allowRayTracingOnOlderGPUs=1`, `r_omms=0`, and optionally
`r_pathtracingEnabled=1`. It does not modify the executable on disk or implement
NV mesh/OMM capture support.

## Supported executable and scope

- Name: `DOOMTheDarkAges.exe`
- SHA-256: `b7aece5fe2842035efd9036a1049edd53ee5efa25564811e20c39ca18362c018`
- PE image size: `0xb74f000`
- Tested GPU/driver: RTX 5060 Ti / 616.56

The DLL verifies the entire executable before installing its import wrapper.
Other processes load the normal capture runtime without game memory changes.
An unrecognised DOOM executable is logged and receives no game patch. It must
be investigated again before updating the offsets/hash.

Inject before GPU initialisation, through launch or global hook. Attaching after
the Vulkan instance has already been created cannot apply this startup adapter.
The worker is started by DllMain; forwarded control exports wait for it outside
DllMain. The import wrapper and module remain resident for the process lifetime.

## Build and install

Read `../docs/CUSTOM_RELEASE_BUILD.md` in full before building. Build the ordinary
Release x64 `renderdoc.vcxproj` and `renderdoc_no_export.vcxproj`, then this
standalone project with VS 2022 MSBuild:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  doom_capture\doom_capture.vcxproj /t:Build /m:1 /nologo /v:minimal `
  '/p:Configuration=Release;Platform=x64' `
  /fl '/flp:logfile=build-doom-bootstrap\adapter-build.log;verbosity=normal'
```

The output is `build-doom-bootstrap/bin/doom_capture.dll`. Place it and
`doom_capture.ini` beside the matching `system_load.dll` in the custom runtime
directory. Choose `doom_capture.dll` in Settings → Core → Config Editor → Inject
→ CustomDLLPath, then restart the capture GUI before starting global hook.
Clear this setting and restart the GUI to return to normal injection.

INI options:

- `EnablePathTracing=1`: requests PT; `0` preserves the game's PT CVAR.
- `UseIATHooks=1`: temporarily supplies process environment overrides for
  D3D12/DXGI IAT hook mode while loading the capture runtime. The previous
  environment is restored afterwards. It does not edit shared hook settings.

These IAT hooks preserve the runtime's driver exclusions. Inline hooks were
observed wrapping the NVIDIA Vulkan driver's internal D3D12/DXGI presentation
path. In the tested startup sequence, IAT mode allowed the title and UI to render.
The general override API accepts only `0`, `1`, or `2` in
`RENDERDOC_HOOK_MODE_<DLL basename in uppercase>`; invalid values leave the
configured mode intact.

## Replay

For the tested captures, enable Settings → Core → Config Editor → Vulkan → Hack
→ RestoreASInCreationOrder, then reopen the capture. This option affects replay
only. Its default remains false because creation order is a heuristic for reused
AS backing storage, not the final GPU write order of every application.

The branch also repairs config XML serialization: `Hook_DLL_ws2_32` previously
emitted invalid `<32>` tags, truncating config parsing before Vulkan settings.
The fixed runtime preserves the public setting path and migrates that legacy
block to `<node name="32">`. Other XML/read failures preserve the original file
and disable saves for that process. Use the fixed runtime to edit settings; an
older custom runtime can write the invalid format again.

Run an existing capture with the saved config (use a new output directory):

```powershell
& scripts\test_doom_rt.ps1 `
  -Capture build-doom-bootstrap\pt-adapter-iat-v2\doom-bootstrap_frame1650.rdc `
  -Output build-doom-bootstrap\my-replay `
  -Runtime C:\RenderSocoDoc
```

`-RestoreASCreationOrder` explicitly enables the option during the test. Without
it the tool preserves the saved setting. The lower-level replay script also
accepts `--no-restore-as-creation-order` for a negative control.

Before running `scripts/test_config_persistence.py`, close all capture/replay GUI
and host processes. It temporarily changes the user's config, backs up the
original in its output directory, and restores it in a finally block. It tests
legacy migration, saved settings across fresh processes, malformed XML
protection, a wrong root, and creation of a first-run config.

## Verified result and remaining limits

`doom-bootstrap_frame1650.rdc` (4,826,328,565 bytes) was captured with this optional
adapter and the ordinary Vulkan capture code. With the fixed saved setting,
replay succeeded, all three TraceRays events succeeded, and the final PNG showed
the title and start prompt. The installed GUI also loaded it successfully.
There are 68 existing capture issues; this is not a warning-free result.

The verified capture is a title scene. Actual gameplay, long sessions, other
executable versions, GPUs and drivers remain unverified. Loading and executing
TraceRays does not establish that every PT feature is correct or provide
ray-tracing shader single-step debugging. See `../docs/DOOM_VULKAN_RT_TEST.md`
for the investigation and evidence.
