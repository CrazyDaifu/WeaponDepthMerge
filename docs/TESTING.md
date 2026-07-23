# Testing guide

## Confirmed configuration

- Battlefield 2
- 32-bit Direct3D 9
- ReShade 6.7.3 with full add-on support
- Generic Depth enabled
- Generic Depth `Copy depth buffer before clear operations` disabled
- In-game MSAA disabled

Initial native D3D9 functional testing confirmed that depth merging works; later focus-switch testing exposed WDM-002.

## Alt+Tab/device-reset regression

Version 1.0 and 1.1 RC1 can become very dark, freeze, and eventually exit after switching away from Battlefield 2 and back. RC2 prevents the dark frame but still hangs in roughly two out of three attempts. RC3 pauses on foreground focus loss, waits for 30 stable foreground presents before resuming, and restores the exact native depth surface after replay.

Run this test in both native D3D9 and DXVK:

1. Reach the main menu, then enter a map and spawn.
2. Alt+Tab to another application for at least five seconds.
3. Return to Battlefield 2 and wait at least thirty seconds.
4. Repeat the cycle three times from the menu and three times while spawned.
5. In native D3D9, confirm that the scene and weapon colors recover and `Merged draw calls` resumes increasing after the short 30-present stabilization delay.
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
