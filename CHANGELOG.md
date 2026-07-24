# Changelog

## 1.1 RC10 - 2026-07-24

- Added an isolation quarantine: after a working combined depth view exists, the next ReShade effect reload permanently disables this add-on for the current process.
- The quarantine stops draw replay, depth clearing, resource selection, and texture-binding updates while leaving ReShade and the game native paths untouched.
- This is a diagnostic candidate to determine whether post-reload add-on callbacks are the remaining crash trigger.

## 1.1 RC9 - 2026-07-24

- Stop calling ReShade's global `update_texture_bindings` after focus loss, while the device is suspended, during an effect reload, or when no combined depth view exists.
- Preserve ReShade's existing descriptors instead of forcing a `DEPTH` clear during the second Alt+Tab or Esc-menu reload path.
- Retain RC8's map-transition cooperative-level guard.
- Awaiting runtime confirmation.

## 1.1 RC8 - 2026-07-24

- Check native D3D9 cooperative-level state before selecting or creating the combined depth view.
- Avoid resource-view creation during map loading and device-reset transitions.
- Retain RC7's null-state guard and post-reload cooldown.
- Awaiting runtime confirmation.

## 1.1 RC7 - 2026-07-24

- Fixed a null-state dereference in the effects callback when the menu has no selected depth buffer.
- Keep depth interception disabled for 120 effect frames after ReShade reports an effect reload, allowing descriptor/shader recreation to settle before the add-on touches D3D9 state again.
- Do not clear or rebind `DEPTH` while the reload cooldown is active.
- Awaiting Battlefield 2 runtime confirmation, with the main-menu depth-zero Alt+Tab path as the primary test.

## 1.1 RC6 - 2026-07-24

- Recorded the refined scope: normal runtime Shader loading is not measurably slowed; the slowdown occurs during Alt+Tab effect reload only.
- Suspend all depth interception while ReShade reports an effect reload transition.
- Avoid `DEPTH` binding updates during the transition and resume them only from the first effects frame after reload completion.
- Awaiting native D3D9 and DXVK runtime confirmation.

## 1.1 RC5 - 2026-07-24

- Reduced repeated ReShade `DEPTH` semantic updates during effect rendering.
- Update the binding only when the resource view changes or ReShade reports that effects finished reloading.
- Targets the observed Alt+Tab regression where ReShade shader reload becomes slow and often crashes while the add-on is installed.
- Awaiting native D3D9 and DXVK runtime confirmation.

## 1.1 RC4 - 2026-07-24

- Recorded that RC3 still produced near-100% Alt+Tab crash/freeze rates in Battlefield 2.
- Reverted dynamic event registration and kept callback registration fixed for the add-on lifetime.
- Once focus is lost, permanently disable depth interception for the current process instead of attempting to resume after reset.
- Hardened resource-view destruction so a ReShade reset cannot double-release the add-on's combined depth view.
- Awaiting runtime confirmation that Alt+Tab is stable, even if depth merging remains disabled after the first focus loss.

## 1.1 RC3 - 2026-07-24

- Recorded that RC2 prevents the near-black/dark return frame, but Battlefield 2 still hangs after Alt+Tab in roughly two out of three attempts.
- Pause interception as soon as the game process is no longer in the foreground, before D3D9 necessarily reports a lost device.
- Require 30 consecutive foreground presents after returning before depth interception resumes.
- Capture and restore the actual native depth-stencil surface around every replay instead of assuming the tracked INTZ surface is still bound.
- Register draw, clear, present, and effect callbacks only while at least one native D3D9 device exists; direct Vulkan runtimes now receive only device-lifecycle checks.
- Retain all RC1 cross-API guards and RC2 native-state failure checks.
- Awaiting repeated native D3D9 and DXVK Alt+Tab runtime testing.

## 1.1 RC2 - 2026-07-24

- Added a candidate fix for the Alt+Tab/device-reset crash present since version 1.0.
- Stop intercepting draws and depth clears as soon as the native D3D9 device reports a lost or unavailable state.
- Validate all captured and restored D3D9 viewport, color-write, stencil-write, depth-surface, and clear operations.
- Fall back to the game's original draw when interception fails before the normal color draw has executed.
- Suspend interception after a native API failure and resume only after ReShade reports command-list reinitialization.
- Clear the ReShade `DEPTH` binding while interception is suspended or the merged view is unavailable.
- Added an overlay diagnostic showing whether interception is active or suspended.
- Runtime result: near-100% Alt+Tab crash/freeze rate; superseded by RC4.

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
