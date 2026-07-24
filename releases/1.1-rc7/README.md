# WeaponDepthMerge 1.1 RC7

Purpose: test Alt+Tab recovery when ReShade reloads shaders from the Battlefield 2 main menu (where no depth buffer may be selected).

Changes from RC6:

- Guards the effects callback before dereferencing missing D3D9 private state.
- Holds depth interception and `DEPTH` binding updates for 120 effect frames after a ReShade effect reload.

Status: compiled and structurally verified; awaiting Battlefield 2 runtime confirmation.
