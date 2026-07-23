# WeaponDepthMerge 1.1 RC3

Purpose: test foreground-focus gating and exact native depth-surface restoration while retaining the RC1 and RC2 guards.

Status: compiled and structurally verified, awaiting Battlefield 2 runtime confirmation.

Test native D3D9 and DXVK separately. Enter a map, switch to another application for at least five seconds, return, wait thirty seconds, and repeat at least six times. Native D3D9 should resume depth merging after a short 30-present delay. DXVK should remain stable while Vulkan depth merging stays inactive.

Do not publish this candidate as final `v1.1` until both paths pass.
