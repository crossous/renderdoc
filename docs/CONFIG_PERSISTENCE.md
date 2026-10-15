# Configuration XML persistence

The custom Windows DLL hook setting `Hook_DLL_ws2_32` contains a numeric path
component. Config registration separates `_` into categories, yielding
`Hook/DLL/ws2/32`. Older custom builds wrote `<32>` into `renderdoc.conf`, which
is not an XML element name. The loader accepted the partial parse and refreshed
the file using defaults for all settings after that node, including Inject, NV,
and Vulkan. Saving a Vulkan option did not make it persist across restarts.

The fixed serializer preserves the public setting path and encodes XML names
outside its supported identifier subset as `<node name="32">`. The loader
decodes the attribute and migrates the known legacy ws2/32 block. It then
requires the complete document to parse before applying settings. Failed reads,
invalid XML, missing config roots and failed conversion leave the file intact;
the process uses defaults and refuses later saves until the file is repaired
and the application is restarted. Import also owns each converted node once.

This is a general configuration repair. It does not change the default for
`Vulkan_Hack_RestoreASInCreationOrder`, which remains false. That replay option
is an opt-in heuristic for applications reusing acceleration-structure backing
storage; creation order does not represent the last GPU write of every program.

Use the fixed runtime for editing settings. Older custom runtimes can generate
the invalid legacy tags again. Config location remains
`%APPDATA%/renderdoc/renderdoc.conf` (the Windows roaming Known Folder).

## Regression test

Close capture/replay GUIs and hosts before running this test. It backs up the
existing user config in its output directory and restores the original bytes in
`finally`, while starting fresh bundled replay hosts for each assertion:

```powershell
& C:\Python314\python.exe scripts\test_config_persistence.py `
  --runtime C:\RenderSocoDoc `
  --output build-config-regression\run-1
```

Use a new output directory. The test covers legacy migration, preservation of
Inject/NV/Vulkan values following the legacy node, saving and reading the numeric
path in a new process, malformed XML and wrong-root file protection, and
first-run creation of valid XML. Bundled Python is hosted through
`scripts/doom_rt_python.py`; no Python module installation is required.
