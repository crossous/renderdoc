# Windows custom capture DLLs

`Inject_CustomDLLPath` selects an x86/x64 DLL instead of the standard
`system_load.dll`. An arbitrary DLL can be injected without RenderDoc control
exports. A custom capture runtime or bootstrap that provides a real
`INTERNAL_GetTargetControlIdent` function also receives the ordinary capture
options, output path, log path and environment modifications, so target control
can connect. Missing export RVAs are never called at the module base address.

PE forwarded exports contain a target string inside the export directory rather
than local executable code. RVA lookup rejects them for remote calls. A capture
bootstrap must implement actual forwarding functions. Optional probing does not
log errors when an arbitrary custom DLL lacks the control protocol. This change
does not otherwise replace the existing PE parsing implementation.

## Process hook-mode overrides

A launcher can supply `RENDERDOC_HOOK_MODE_<DLL basename in uppercase>` before
the capture runtime registers hooks. Values must be exactly `0` (disabled), `1`
(IAT), or `2` (Inline). Missing or invalid values preserve the shared config.
For example, `RENDERDOC_HOOK_MODE_D3D12=1` and `RENDERDOC_HOOK_MODE_DXGI=1` select
IAT mode for one process without saving those modes into the user's settings.

The known-DLL lookup is shared with the existing per-DLL config; this is not an
executable-name rule. Launchers that set temporary process environment variables
should restore their previous values after synchronous runtime loading so later
children do not inherit the overrides. Other existing registration rules, such
as API-set aliases following the kernel32 config, remain in effect.

Normal release packages contain only the standard capture runtime. Separate
application adapters can be maintained and packaged on their own branches.
