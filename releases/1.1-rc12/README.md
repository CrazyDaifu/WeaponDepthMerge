# WeaponDepthMerge 1.1 RC12

Purpose: remove WeaponDepthMerge from ReShade's Shader reload lifecycle.

The add-on no longer creates an INTZ SRV, handles effect begin/reload events, or calls `update_texture_bindings`. Generic Depth supplies the `DEPTH` binding for the same INTZ resource; WeaponDepthMerge only writes merged geometry into it.

Status: compiled candidate; awaiting depth-function and stability confirmation.
