# Changelog

## 1.1 RC1 - 2026-07-24

- Added a candidate fix for a startup crash when Battlefield 2 runs through DXVK and ReShade uses the Vulkan backend.
- Added API and private-state guards to every event path that can run outside native D3D9.
- The add-on now loads as an inert no-op on DXVK/Vulkan instead of dereferencing missing D3D9 state.
- Added an overlay compatibility message for non-D3D9 runtimes.
- Preserved native D3D9 depth merging behavior from version 1.0.
- Archived the exact version 1.0 binary and checksum under `releases/1.0/`.
- Made candidate publishing resynchronize the versioned binary archive after each rebuild and reject final tags while `VERSION` is still a pre-release.
- Made GitHub Actions artifact names safe for `release/*` branch names.
- Awaiting runtime confirmation before the final `v1.1` tag is created.

## 1.0 - 2026-07-24

- Created an independent ReShade 6.7.3 D3D9 x86 add-on.
- Reproduced Thalixte's first-person weapon/cockpit depth replay behavior.
- Added a clearable scratch depth surface so normal weapon rendering remains correct while the preserved INTZ depth receives the merged geometry.
- Added manual and automatic clear-boundary selection, depth bias, viewport filtering, diagnostics, and `DEPTH` semantic binding.
- Added device-reset and selected-resource destruction handling.
- Verified the output as PE32/x86 with `NAME` and `DESCRIPTION` exports.
- Battlefield 2 runtime test reported successful; no bug observed in the initial test.
- Fixed one-click GitHub publishing on Windows PowerShell 5.1 when the local repository does not yet have an `origin` remote or release tag.
