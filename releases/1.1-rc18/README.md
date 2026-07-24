# WeaponDepthMerge 1.1 RC18

Purpose: restore functional depth merging while avoiding the confirmed ReShade 6.7.3 D3D9 fake `DrawPrimitiveUP` buffer crash path.

This build never registers `bind_vertex_buffers`. Before replaying a weapon-phase draw, it validates native stream-0 and index-buffer bindings and passes unbound or likely UP draws through unchanged.

Status: functional candidate; awaiting Battlefield 2 depth and Reset/Alt+Tab testing.
