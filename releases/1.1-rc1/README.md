# WeaponDepthMerge 1.1 RC1

Purpose: test the DXVK/Vulkan startup-crash fix while preserving native D3D9 behavior.

Status: compiled and structurally verified, awaiting Battlefield 2 runtime confirmation.

Test both:

1. Native D3D9: confirm the weapon/arm depth merge still works.
2. DXVK + Vulkan ReShade: confirm the game reaches the menu and the add-on remains inactive without crashing.

Do not publish this candidate as final `v1.1` until both checks pass.

