# RenderDoc Custom Fork — AI Reference

This is a custom fork of [RenderDoc](https://github.com/baldurk/renderdoc) with feature-hiding modifications. The upstream remote is `upstream` (baldurk/renderdoc), our fork is `origin` (crossous/renderdoc).

## Project Structure Overview

This fork renames/hides RenderDoc identifiers to avoid detection. Key customizations sit on top of the official codebase as separate commits (viewable via `git log HEAD --not upstream/v1.x`).

### Custom Output Names (vs official names)

| Official Name | Custom Name | Source Project |
|---|---|---|
| `qrenderdoc.exe` | `q_render_soco_doc.exe` | `qrenderdoc/qrenderdoc_local.vcxproj` |
| `renderdoccmd.exe` | `render_soco_doccmd.exe` | `renderdoccmd/renderdoccmd.vcxproj` |
| `renderdocshim32.dll` | `render_soco_docshim32.dll` | `renderdocshim/renderdocshim.vcxproj` |
| `renderdocshim64.dll` | `render_soco_docshim64.dll` | (same) |
| `renderdoc.dll` | `renderdoc.dll` (unchanged) | `renderdoc/renderdoc.vcxproj` |
| *(no equivalent)* | `system_load.dll` | `renderdoc/renderdoc_no_export.vcxproj` |
| `librenderdoc.so` | `libVIVO50_SystemLoad.so` | (cmake, see `renderdoc/common/globalconfig.h`) |

**`system_load.dll`** is the `renderdoc_no_export` project — a copy of renderdoc.dll built without COM exports, used for injection into target processes while hiding the renderdoc identity. Its vcxproj must be kept in sync with `renderdoc.vcxproj` when upstream adds new source files (this has caused link errors before, e.g. `controlflow.cpp` was missing after v1.43 rebase).

### Other Custom Modifications

- **Detours inline hooking** (`renderdoc/3rdparty/Detours/`) — replaces IAT hooking for DX11/DX12 (`acb33f563`)
- **EGLImage support on Android** — custom GL extension wrappers (`df84c23fc`)
- **UI customizations** — LiveCapture window style changes, Diagnostic_Log window
- **PE parsing for injection** — `renderdoc/os/win32/win32_pe_parse.cpp`
- **AGS suppression** — `renderdoc/driver/d3d12/d3d12_device_wrap.cpp`

---

## Release Build Guide

When the user says "帮我发布" or "pack a release", follow these steps.

### Environment (already installed)

| Tool | Path | Env Var |
|---|---|---|
| JDK 8 | `C:\Program Files\Java\jdk8u422-b05` | `JAVA_HOME` |
| Android SDK | `D:\Android\Sdk` | `ANDROID_HOME` |
| Android NDK r14b | `D:\Android\Sdk\ndk\android-ndk-r14b` | `ANDROID_NDK_HOME` |
| MSYS2 | `C:\msys64` | — |
| VS2022 Community | `C:\Program Files\Microsoft Visual Studio\2022\Community` | — |
| MSBuild | `...\MSBuild\Current\Bin\MSBuild.exe` | — |

Verify environment vars are set before building: `echo $JAVA_HOME $ANDROID_HOME $ANDROID_NDK_HOME`

### Step 1: Windows x86 Release

```bash
MSBUILD="/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe"
cd "D:/Project/CPP/renderdoc"
MSYS2_ARG_CONV_EXCL="*" "$MSBUILD" renderdoc.sln /t:Rebuild /p:"Configuration=Release;Platform=x86" /m /nologo
```

Expected outputs in `Win32/Release/`:
`renderdoc.dll`, `system_load.dll`, `q_render_soco_doc.exe`, `render_soco_doccmd.exe`, `render_soco_docshim32.dll`

### Step 2: Windows x64 Release

```bash
MSYS2_ARG_CONV_EXCL="*" "$MSBUILD" renderdoc.sln /t:Rebuild /p:"Configuration=Release;Platform=x64" /m /nologo
```

Expected outputs in `x64/Release/`: same set but with `render_soco_docshim64.dll`.

**Tips:** Steps 1 & 2 can run sequentially. Use `/t:Build` instead of `/t:Rebuild` for incremental builds.

### Step 3: Android ARM32

Must run inside MSYS2 bash (`/c/msys64/usr/bin/bash.exe -l`):

```bash
export JAVA_HOME="/c/Program Files/Java/jdk8u422-b05"
export PATH="$JAVA_HOME/bin:$PATH"
export ANDROID_HOME="D:/Android/Sdk"
export ANDROID_NDK_HOME="D:/Android/Sdk/ndk/android-ndk-r14b"

cd "D:/Project/CPP/renderdoc"
rm -rf build-android-arm32 && mkdir build-android-arm32 && cd build-android-arm32
cmake -G "MSYS Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DCMAKE_ANDROID_STL_TYPE=c++_static \
  -DBUILD_ANDROID=1 -DANDROID_ABI=armeabi-v7a -DANDROID_STL=c++_static \
  -DANDROID_TOOLCHAIN=clang -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On ..
make -j$(nproc)
```

Expected output: `bin/org.renderdoc.renderdoccmd.arm32.apk`

### Step 4: Android ARM64

Same environment, different ABI:

```bash
cd "D:/Project/CPP/renderdoc"
rm -rf build-android-arm64 && mkdir build-android-arm64 && cd build-android-arm64
cmake -G "MSYS Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DCMAKE_ANDROID_STL_TYPE=c++_static \
  -DBUILD_ANDROID=1 -DANDROID_ABI=arm64-v8a -DANDROID_STL=c++_static \
  -DANDROID_TOOLCHAIN=clang -DCMAKE_BUILD_TYPE=Release -DSTRIP_ANDROID_LIBRARY=On ..
make -j$(nproc)
```

Expected output: `bin/org.renderdoc.renderdoccmd.arm64.apk`

**Tips:** ARM32 and ARM64 can build in parallel if launched from separate MSYS2 shells.

### Step 5: Package

```bash
cd "D:/Project/CPP/renderdoc"
VERSION=$(grep -E "#define RENDERDOC_VERSION_(MAJOR|MINOR)" renderdoc/api/replay/version.h | tr -dc '[0-9\n]' | tr '\n' '.' | grep -Eo '[0-9]+\.[0-9]+')
PKGNAME="RenderSocoDoc_${VERSION}"

rm -rf dist package
mkdir -p dist/Release64 dist/Release32

# Copy build outputs (exclude obj/, .lib, pymodules, incremental pdb)
cd x64/Release && find . -not -path './obj*' -not -name '*.lib' -not -path './pymodules*' -not -name '*.ipdb' -not -name '*.iobj' -type f -exec cp --parents '{}' ../../dist/Release64/ \; && cd ../..
cd Win32/Release && find . -not -path './obj*' -not -name '*.lib' -not -path './pymodules*' -not -name '*.ipdb' -not -name '*.iobj' -type f -exec cp --parents '{}' ../../dist/Release32/ \; && cd ../..

# Copy d3dcompiler from Windows SDK
cp "C:/Program Files (x86)/Windows Kits/10/Redist/D3D/x64/d3dcompiler_47.dll" dist/Release64/
cp "C:/Program Files (x86)/Windows Kits/10/Redist/D3D/x86/d3dcompiler_47.dll" dist/Release32/

# Copy LICENSE
cp LICENSE.md dist/Release64/
cp LICENSE.md dist/Release32/

# Copy Android APKs
mkdir -p dist/Release64/plugins/android/
cp build-android-arm32/bin/org.renderdoc.renderdoccmd.arm32.apk dist/Release64/plugins/android/
cp build-android-arm64/bin/org.renderdoc.renderdoccmd.arm64.apk dist/Release64/plugins/android/

# Bundle x86 core files into x64 package
mkdir -p dist/Release64/x86
for f in d3dcompiler_47.dll renderdoc.dll renderdoc.json render_soco_docshim32.dll render_soco_doccmd.exe system_load.dll system_load.json dbghelp.dll symsrv.dll symsrv.yes; do
    [ -f "dist/Release32/$f" ] && cp "dist/Release32/$f" dist/Release64/x86/
done

# Remove PDBs and build artifacts from release
find dist/Release64/ -name '*.pdb' -exec rm '{}' \;
rm -f dist/Release64/*.{exp,lib,metagen,xml} dist/Release64/*.vshost.*

# Create zip
mkdir -p package
cp -R dist/Release64 "package/${PKGNAME}_64"
cd package
powershell -Command "Compress-Archive -Path '${PKGNAME}_64' -DestinationPath '${PKGNAME}_64.zip' -Force"
```

Output: `package/RenderSocoDoc_<version>_64.zip`

### Expected Package Contents

```
RenderSocoDoc_X.XX_64/
├── q_render_soco_doc.exe          # GUI app
├── renderdoc.dll                  # Core library
├── system_load.dll                # Stealth injection DLL
├── render_soco_doccmd.exe         # CLI tool
├── render_soco_docshim64.dll      # Global hook shim
├── renderdoc.json / system_load.json
├── d3dcompiler_47.dll
├── Qt5*.dll, python36.dll, dbghelp.dll, symsrv.dll, ...
├── LICENSE.md
├── x86/                           # 32-bit support
│   ├── renderdoc.dll
│   ├── system_load.dll
│   ├── render_soco_doccmd.exe
│   ├── render_soco_docshim32.dll
│   └── ...
└── plugins/android/
    ├── org.renderdoc.renderdoccmd.arm32.apk
    └── org.renderdoc.renderdoccmd.arm64.apk
```

---

## Upstream Sync Guide

When syncing to a new upstream release:

1. `git fetch upstream --tags`
2. Create backup: `git branch backup-before-<version>`
3. Find custom commits: `git log --oneline HEAD --not upstream/v1.x`
4. Rebase: `git rebase --onto <new_tag> <old_merge_base> v1.x`
5. Resolve conflicts (common in: `core/core.h`, `.rc` binary files, `.vcxproj.filters`)
6. **Critical:** After rebase, diff `renderdoc.vcxproj` vs `renderdoc_no_export.vcxproj` to find any new source files that need to be added to `renderdoc_no_export`. Missing files cause link errors.

---

## Pitfalls & Troubleshooting

### Android cmake STL error (`gnustl_static include directory not found`)
MSYS2's cmake (4.x) has built-in Android platform modules that conflict with NDK r14b. **Fix:** always pass these two cmake flags:
```
-DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"
-DCMAKE_ANDROID_STL_TYPE=c++_static
```

### Android cmake can't find `java`
MSYS2 bash doesn't inherit Windows user env vars by default. **Fix:** explicitly export at the top of the MSYS2 session:
```bash
export JAVA_HOME="/c/Program Files/Java/jdk8u422-b05"
export PATH="$JAVA_HOME/bin:$PATH"
```

### `renderdoc_no_export` link errors after upstream sync
Upstream added new .cpp/.h files to `renderdoc.vcxproj` that are missing from `renderdoc_no_export.vcxproj`. **Fix:** compare the two vcxproj files and add missing entries to both `.vcxproj` and `.vcxproj.filters`.

### Binary `.rc` file conflicts during rebase
The `.rc` files (e.g. `qrenderdoc.rc`, `renderdoccmd.rc`) may be UTF-16LE binary. Git can't merge them. **Fix:** take the upstream version and re-apply the name changes (e.g. `"RenderDoc"` → `"RenderSocoDocWOW"`) using python byte-level replacement or manual editing.
