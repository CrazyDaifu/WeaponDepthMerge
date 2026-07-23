# Version archive

Each completed release keeps its tested binary and checksum in a versioned directory.

- `1.0/`: original native D3D9 implementation, known to crash when loaded by ReShade on DXVK/Vulkan.
- `1.1-rc1/`: cross-API startup fix; runtime-confirmed to stop the DXVK startup crash, but still affected by WDM-002.
- `1.1-rc2/`: lost-device/state validation; prevents the dark return frame but still hangs frequently after Alt+Tab.
- `1.1-rc3/`: candidate adding foreground-focus gating, a stable-present delay, and exact native depth-surface restoration.
- `1.1/`: generated only after RC3 passes native D3D9, DXVK startup, and repeated Alt+Tab tests.

Development output remains under `build/` and may be overwritten by a rebuild. Files under `releases/<version>/` are frozen baselines and must not be overwritten by another version.
