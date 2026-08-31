# RenderSocoDoc Build and Release Pitfalls

This document is the mandatory preflight for building, packaging, or publishing this
custom RenderDoc fork. It records failures reproduced on the local release machine and
the verified recovery paths. Read it before invoking the repository build scripts.

Last verified: 2026-08-31, RenderSocoDoc 1.44, Visual Studio 2022, Android Build Tools
36.0.0, NDK r14b.

Verified machine paths:

- MSBuild: `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe`
- MSYS2: `C:\msys64`
- JDK 8: `C:\Program Files\Java\jdk8u422-b05`
- Android SDK: `D:\Android\Sdk`
- Android NDK: `D:\Android\Sdk\ndk\android-ndk-r14b`

## Keep Agent Output Small

Full compiler output can contain tens of thousands of warning lines. Do not stream it all
into the conversation.

- Redirect each long build to its own log file.
- On success, report only the exit code, elapsed time, and required artifact list.
- On failure, search for `error`, `fatal`, `LNK`, `C1001`, `C1002`, or `Exception`, then
  read at most the last 100 relevant lines.
- Do not repeat a full build when only a deterministic final packaging target failed.

For MSBuild, prefer a file logger and minimal console output:

```powershell
& $msbuild renderdoc.sln /t:Rebuild /m:2 /nologo /v:minimal `
  /fl /flp:"logfile=build-windows-x64.log;verbosity=normal" `
  /p:"Configuration=Release;Platform=x64"
```

For Android builds, redirect stdout and stderr to an ABI-specific log and inspect only
the tail on failure.

## Canonical Paths and Build Semantics

- The all-in-one script is `scripts/build_all.bat`.
- `scripts/build/_all.bat` does not exist.
- `scripts/build_all.bat` currently uses `/t:Build` for Windows and is therefore
  incremental.
- When the user requests a **full**, **clean**, or **public release** build, use
  `/t:Rebuild` for Windows and fresh Android build directories.
- Keep `renderdoc/renderdoc_no_export.vcxproj` synchronized with
  `renderdoc/renderdoc.vcxproj`; `system_load.dll` comes from the no-export project.

## Windows: LTCG Compiler Crash

### Symptom

The Release link can fail or hang during link-time code generation around
`gl_hooks.cpp`, commonly with MSVC `C1001` or `C1002`. Orphaned `link.exe` processes may
remain after MSBuild has stopped making progress.

### Verified fallback

Keep normal Release optimization (`/O2`) but disable cross-module LTCG for this build:

```powershell
$common = 'PreferredToolArchitecture=x64;WholeProgramOptimization=false;LinkTimeCodeGeneration=Default'

& $msbuild renderdoc.sln /t:Rebuild /m:2 /nologo /v:minimal `
  /p:"Configuration=Release;Platform=x86;$common"

& $msbuild renderdoc.sln /t:Rebuild /m:2 /nologo /v:minimal `
  /p:"Configuration=Release;Platform=x64;$common"
```

Before stopping a compiler process, inspect its PID and command line and confirm it
belongs to this workspace. Never kill all compiler or linker processes blindly.

## Windows: Locked `system_load.pdb`

### Symptom

`renderdoc_no_export` fails with `LNK1201` while writing
`x64/Release/system_load.pdb`, even though the other solution targets completed.

### Recovery

1. Confirm all relevant `link.exe` processes have exited.
2. Inspect `mspdbsrv.exe`; stop only the confirmed stale process for this build.
3. Move the stale PDB out of `x64/Release` instead of deleting unrelated files.
4. Rebuild only `renderdoc_no_export.vcxproj`; a second full solution rebuild is not
   required.

```powershell
& $msbuild renderdoc/renderdoc_no_export.vcxproj /t:Build /m:1 /nologo `
  /p:"Configuration=Release;Platform=x64;SolutionDir=D:\Project\CPP\renderdoc\;PreferredToolArchitecture=x64;WholeProgramOptimization=false;LinkTimeCodeGeneration=Default;GenerateDebugInformation=false;BuildProjectReferences=false"
```

Verify that `x64/Release/system_load.dll` has a current timestamp after recovery.

## Android: JDK 8 and Build Tools 36 D8 Conflict

### Symptom

The native libraries reach 100%, but APK generation fails with:

```text
UnsupportedClassVersionError: com/android/tools/r8/D8 ... class file version 55.0
this Java Runtime only recognizes class file versions up to 52.0
```

Build Tools 36.0.0 D8 requires Java 11 or newer. The current Android CMake flow still
uses JDK 8 `javac`, `-source 1.7`, `-target 1.7`, and the JDK 8 `rt.jar`. Do not simply
configure the whole legacy build with a modern JDK because modern JDKs no longer provide
that `rt.jar` layout.

### Verified two-phase recovery

1. Configure and compile normally with the configured JDK 8. This produces the native
   libraries and Java class files.
2. For the failed `apk` target only, switch `JAVA_HOME` to a Java 11+ runtime so that
   `d8.bat` and `apksigner.bat` can run.
3. Run `make apk`; do not delete the build directory or recompile all native sources.

The runtime verified on this machine is Rider's JBR:

```bash
export JAVA_HOME="/c/Program Files/JetBrains/JetBrains Rider 2026.2.0.2/jbr"
export PATH="$JAVA_HOME/bin:$PATH"

cd /d/Project/CPP/renderdoc/build-android-arm32
make apk -j2

cd /d/Project/CPP/renderdoc/build-android-arm64
make apk -j2
```

If that Rider version is no longer installed, locate another Java 11+ runtime first and
verify it with `java -version`; do not silently fall back to JDK 8 for the APK tools.

The D8 warning about `android.app.NativeActivity` during desugaring did not invalidate
the generated APKs. Treat it as a warning only when `make apk` exits successfully and
signature/ABI verification below passes.

## Android APK Verification

Verify both APKs before packaging:

```powershell
$env:JAVA_HOME = 'C:\Program Files\JetBrains\JetBrains Rider 2026.2.0.2\jbr'
$env:Path = "$env:JAVA_HOME\bin;$env:Path"

& 'D:\Android\Sdk\build-tools\36.0.0\apksigner.bat' verify --verbose `
  build-android-arm32/bin/org.renderdoc.renderdoccmd.arm32.apk
& 'D:\Android\Sdk\build-tools\36.0.0\apksigner.bat' verify --verbose `
  build-android-arm64/bin/org.renderdoc.renderdoccmd.arm64.apk
```

Required results:

- ARM32 contains `lib/armeabi-v7a/libVIVO50_SystemLoad.so` and
  `lib/armeabi-v7a/librenderdoccmd.so`.
- ARM64 contains `lib/arm64-v8a/libVIVO50_SystemLoad.so` and
  `lib/arm64-v8a/librenderdoccmd.so`.
- APK Signature Schemes v1, v2, and v3 verify successfully.

## Packaging Pitfalls

- Package from fresh `dist` and `package` directories. Resolve their absolute paths and
  confirm they are under the repository before moving or removing old output.
- Exclude `obj`, `pymodules`, PDBs, import libraries, LTCG intermediates, and incremental
  linker files.
- In addition to the exclusions in `AGENTS.md`, exclude `*.ilk`. A previously observed
  `test_inject_dll.ilk` is not a runtime dependency and must not ship.
- Include both D3D12 Agility SDK runtimes, both Android APKs, and the required x86 files
  inside the x64 package.
- Run the packaged CLI before release:

```powershell
package/RenderSocoDoc_<version>_64/render_soco_doccmd.exe version
```

It must report the intended version and exact build commit.

After creating the ZIP, enumerate every entry, reject forbidden build artifacts, and
read/decompress every file once. A successful `Compress-Archive` command alone is not
sufficient verification.

## GitHub Release Procedure

1. Query the latest existing GitHub Release and compute the commit comparison range.
2. Use the established date tag format `YYYYMMDD` unless the user specifies another tag.
3. Write Chinese and English release notes covering the changes between the two Release
   tags, with a full comparison link.
4. Create the Release as a draft and upload exactly one final ZIP.
5. Compare the GitHub asset's byte size and `sha256:` digest with the local ZIP.
6. Only after they match, publish the draft and mark it latest.
7. Verify the published tag resolves to the exact build commit and the asset URL uses the
   final tag rather than an `untagged-*` draft URL.

Never publish with unintended tracked source changes. Generated ignored files and
unrelated untracked user files may remain, but list and preserve them. Do not commit
package archives unless the user explicitly asks for repository-hosted binaries in
addition to Release assets.

## Minimum Success Summary

At handoff, report only:

- Release URL and direct asset URL.
- Local ZIP path, byte size, and SHA-256.
- Windows x86/x64 and Android ARM32/ARM64 results.
- Tag and exact source commit.
- Any optimization fallback used, such as LTCG being disabled.
- Remaining tracked/untracked source-tree changes.
