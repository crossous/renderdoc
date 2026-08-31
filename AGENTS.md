# RenderDoc Custom Fork — AI Reference

This is a custom fork of [RenderDoc](https://github.com/baldurk/renderdoc). The upstream
remote is `upstream` (`baldurk/renderdoc`) and the fork remote is `origin`
(`crossous/renderdoc`). Custom commits can be listed with
`git log --oneline HEAD --not upstream/v1.x`.

## Custom Output Names

| Official name | Custom name | Source project |
|---|---|---|
| `qrenderdoc.exe` | `q_render_soco_doc.exe` | `qrenderdoc/qrenderdoc_local.vcxproj` |
| `renderdoccmd.exe` | `render_soco_doccmd.exe` | `renderdoccmd/renderdoccmd.vcxproj` |
| `renderdocshim32.dll` | `render_soco_docshim32.dll` | `renderdocshim/renderdocshim.vcxproj` |
| `renderdocshim64.dll` | `render_soco_docshim64.dll` | same project |
| `renderdoc.dll` | `renderdoc.dll` | `renderdoc/renderdoc.vcxproj` |
| no equivalent | `system_load.dll` | `renderdoc/renderdoc_no_export.vcxproj` |
| `librenderdoc.so` | `libVIVO50_SystemLoad.so` | CMake; see `renderdoc/common/globalconfig.h` |

`system_load.dll` is the no-COM-export injection build. Keep
`renderdoc_no_export.vcxproj` and its filters synchronized with `renderdoc.vcxproj` when
upstream adds source files.

Other custom areas include Detours inline hooking, Android EGLImage wrappers, capture UI
customizations, Win32 PE parsing, D3D12 AGS suppression, per-DLL hook modes, API Monitor,
custom injection, and Vulkan emulator multi-instance capture bridging.

## Build and Release — Mandatory Preflight

Before any local build, packaging operation, or GitHub Release, read
[`docs/CUSTOM_RELEASE_BUILD.md`](docs/CUSTOM_RELEASE_BUILD.md) **in full** and follow it
as the current source of truth. It records verified compiler, JDK/D8, packaging, logging,
and release-validation pitfalls for this machine.

The canonical all-in-one script is `scripts/build_all.bat`; there is no
`scripts/build/_all.bat`. Its Windows step currently uses incremental `/t:Build`, so use
the full-rebuild procedure from the mandatory document for a clean/full/public release.
Write long compiler output to log files and return only a short success summary or the
relevant failure tail.

## Upstream Sync Guide

1. `git fetch upstream --tags`.
2. Create `backup-before-<version>`.
3. List custom commits with `git log --oneline HEAD --not upstream/v1.x`.
4. Rebase with `git rebase --onto <new_tag> <old_merge_base> v1.x`.
5. Resolve conflicts, especially in `core/core.h`, resource files, and vcxproj filters.
6. Diff `renderdoc.vcxproj` against `renderdoc_no_export.vcxproj` and add every new source
   to the no-export project and filters. Missing files cause `system_load.dll` link errors.

Binary `.rc` files may be UTF-16LE and cannot be merged normally. Take the upstream
version and reapply custom names with a controlled byte-level replacement or manual edit.
