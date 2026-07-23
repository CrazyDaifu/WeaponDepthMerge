# WeaponDepthMerge 1.1 RC2

Purpose: test the lost-device and Alt+Tab recovery fix while retaining the RC1 DXVK startup guard.

Status: runtime-tested. It prevents the near-black/dark return frame, but Battlefield 2 still hangs after Alt+Tab in roughly two out of three attempts. Superseded by RC3.

Test both native D3D9 and DXVK. Enter a map, Alt+Tab away for at least five seconds, return, and repeat at least three times. Native D3D9 must resume normal colors and depth merging. DXVK must remain stable even though Vulkan depth merging is inactive.

Do not publish this candidate as final `v1.1` until both paths pass.
