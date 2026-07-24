# WeaponDepthMerge 1.1 RC6

Purpose: remove the add-on from ReShade's Alt+Tab Shader reload transition.

When ReShade signals that effects were reloaded, the add-on marks a reload transition as pending. It skips draw/clear interception and `DEPTH` binding updates until the first effects frame after the reload finishes.

Status: compiled and structurally verified, awaiting Battlefield 2 runtime confirmation.

Do not publish this candidate as final `v1.1` until native D3D9 and DXVK repeated Alt+Tab tests pass.
