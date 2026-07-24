# WeaponDepthMerge 1.1 RC4

Purpose: prioritize Alt+Tab stability by permanently disabling depth interception after the first focus loss.

Status: compiled and structurally verified, awaiting Battlefield 2 runtime confirmation.

Test native D3D9 and DXVK separately. Enter a map, switch to another application, return, and repeat at least six times. The game must not freeze or exit. After the first focus loss, the overlay should report paused interception and depth merging is expected to remain disabled until process restart.

Do not publish this candidate as final `v1.1` until both paths pass.
