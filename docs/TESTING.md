# Testing guide

## Confirmed configuration

- Battlefield 2
- 32-bit Direct3D 9
- ReShade 6.7.3 with full add-on support
- Generic Depth enabled
- Generic Depth `Copy depth buffer before clear operations` disabled
- In-game MSAA disabled

Initial native D3D9 functional testing confirmed that depth merging works; later focus-switch testing exposed WDM-002.

## RC17 `bind_vertex_buffers` isolation

RC17 is not a functional depth-merge candidate. Disable ShaderToggler and Generic Depth, then install only `WeaponDepthMerge.addon32`. ReShade should list the add-on as loaded, but there should be no WeaponDepthMerge settings page and no depth merge. It retains RC16's passive callbacks except `bind_vertex_buffers`; draw and clear callbacks remain absent.

Test main-menu Alt+Tab, in-map Alt+Tab twice, and Esc after the first return. RC14 and RC15 passed; RC16 failed every case. If RC17 passes, the ReShade 6.7.3 fake `DrawPrimitiveUP` buffer path is confirmed as the trigger. If it fails, split the other four passive callbacks.

## Alt+Tab/device-reset regression

Version 1.0 and 1.1 RC1 can become very dark, freeze, and eventually exit after switching away from Battlefield 2 and back. RC2 prevents the dark frame but still hangs frequently. RC3 made the crash rate near 100%. RC4 permanently disables interception after focus loss. RC5 additionally removes the per-frame global `DEPTH` binding update that can slow ReShade's shader reload.

Run this test in both native D3D9 and DXVK:

1. Reach the main menu, then enter a map and spawn.
2. Alt+Tab to another application for at least five seconds.
3. Return to Battlefield 2 and wait at least thirty seconds.
4. Repeat the cycle three times from the menu and three times while spawned.
5. In native D3D9, confirm the game remains stable. It is expected that `Merged draw calls` stops increasing after the first focus loss until process restart.
6. In DXVK, confirm that the game remains stable and the add-on remains inactive when ReShade reports Vulkan.

Failure evidence should include the end of `ReShade.log` and the last visible values of both `Device interception` and `Focus interception`.

## DXVK regression

Version 1.0 could crash at startup when Battlefield 2 used DXVK and ReShade therefore ran on Vulkan. Version 1.1 guards all non-D3D9 event callbacks. Expected behavior under DXVK is now:

- the game reaches its normal startup/menu instead of black-screen exiting;
- the add-on can be listed by ReShade;
- the overlay states that the add-on is inactive because the runtime is not native D3D9;
- no weapon-depth merging is attempted on Vulkan.

This is a crash-compatibility fix, not a Vulkan implementation of the merge algorithm.

## Installation

Copy `build/WeaponDepthMerge.addon32` beside the ReShade D3D9 DLL. Start with:

- `Enable weapon/cockpit depth merge`: enabled
- `Clear index`: `1`
- `First-person depth bias`: `0.5`
- `Require full-size viewport`: enabled

## Expected diagnostics

- `Combined INTZ` shows the game resolution.
- `Merged draw calls` increases while the first-person model is rendered.
- `Failed intercepted draws` remains zero.
- AO or other depth effects no longer pass through the first-person arms/weapon.

## Information needed for a failure report

1. Screenshot of the add-on settings page.
2. Values in `Last frame clear segments`.
3. Whether `Combined INTZ` was found.
4. Merged, skipped-UP, and failed-draw counters.
5. ReShade version and whether Generic Depth preserve-copy is disabled.
6. Game resolution, MSAA state, GPU, driver, and `ReShade.log`.

## Regression cases

- Enter and leave a server or map.
- Change resolution or toggle fullscreen, forcing a D3D9 reset.
- Die, respawn, enter vehicles, switch seats, and switch weapons.
- Open menus, loading screens, scope views, and commander/map screens.
- Test AO both enabled and disabled to separate depth bugs from effect settings.
- Start once through DXVK/Vulkan to verify safe no-op loading and no startup crash.
