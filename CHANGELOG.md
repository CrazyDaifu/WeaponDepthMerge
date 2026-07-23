# Changelog

## 1.1 RC2 - 2026-07-24

- Added a candidate fix for the Alt+Tab/device-reset crash present since version 1.0.
- Stop intercepting draws and depth clears as soon as the native D3D9 device reports a lost or unavailable state.
- Validate all captured and restored D3D9 viewport, color-write, stencil-write, depth-surface, and clear operations.
- Fall back to the game's original draw when interception fails before the normal color draw has executed.
- Suspend interception after a native API failure and resume only after ReShade reports command-list reinitialization.
- Clear the ReShade `DEPTH` binding while interception is suspended or the merged view is unavailable.
- Added an overlay diagnostic showing whether interception is active or suspended.
- Awaiting native D3D9 and DXVK Alt+Tab runtime confirmation.

## 1.1 RC1 - 2026-07-24

- Added a candidate fix for a startup crash when Battlefield 2 runs through DXVK and ReShade uses the Vulkan backend.
- Added API and private-state guards to every event path that can run outside native D3D9.
- The add-on now loads as an inert no-op on DXVK/Vulkan instead of dereferencing missing D3D9 state.
- Added an overlay compatibility message for non-D3D9 runtimes.
- Preserved native D3D9 depth merging behavior from version 1.0.
- Archived the exact version 1.0 binary and checksum under `releases/1.0/`.
- Made candidate publishing resynchronize the versioned binary archive after each rebuild and reject final tags while `VERSION` is still a pre-release.
- Made GitHub Actions artifact names safe for `release/*` branch names.
- Runtime confirmed that DXVK reaches the game without the previous startup crash; RC1 still contains WDM-002.

## 1.0 - 2026-07-24

- Created an independent ReShade 6.7.3 D3D9 x86 add-on.
- Reproduced Thalixte's first-person weapon/cockpit depth replay behavior.
- Added a clearable scratch depth surface so normal weapon rendering remains correct while the preserved INTZ depth receives the merged geometry.
- Added manual and automatic clear-boundary selection, depth bias, viewport filtering, diagnostics, and `DEPTH` semantic binding.
- Added device-reset and selected-resource destruction handling.
- Verified the output as PE32/x86 with `NAME` and `DESCRIPTION` exports.
- Battlefield 2 runtime test reported successful; no bug observed in the initial test.
- Fixed one-click GitHub publishing on Windows PowerShell 5.1 when the local repository does not yet have an `origin` remote or release tag.
