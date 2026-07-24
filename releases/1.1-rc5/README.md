# WeaponDepthMerge 1.1 RC5

Purpose: reduce ReShade shader-reload contention by caching the last `DEPTH` semantic binding.

The add-on now updates the binding only when its resource view changes or when ReShade reports that effects finished reloading. This is intended to address the Alt+Tab observation that ReShade reloads shaders much slower with the add-on installed.

Status: compiled and structurally verified, awaiting Battlefield 2 runtime confirmation.

Do not publish this candidate as final `v1.1` until native D3D9 and DXVK repeated Alt+Tab tests pass.
