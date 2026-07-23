# Version archive

Each completed release keeps its tested binary and checksum in a versioned directory.

- `1.0/`: original native D3D9 implementation, known to crash when loaded by ReShade on DXVK/Vulkan.
- `1.1-rc1/`: candidate containing the cross-API safety fix, awaiting game testing.
- `1.1/`: generated only after the candidate passes native D3D9 and DXVK startup tests.

Development output remains under `build/` and may be overwritten by a rebuild. Files under `releases/<version>/` are frozen baselines and must not be overwritten by another version.
