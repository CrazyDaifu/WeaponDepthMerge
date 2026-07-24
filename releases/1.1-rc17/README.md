# WeaponDepthMerge 1.1 RC17

Purpose: verify that ReShade 6.7.3's D3D9 fake `DrawPrimitiveUP` buffer path is the WDM-002 trigger.

This build is RC16 with only `bind_vertex_buffers` removed. Lifecycle, resource-view destruction, render-target/depth binding, viewport, and present callbacks remain active. Draw, depth-clear, effect-runtime, overlay, and depth merging remain disabled.

Status: diagnostic build; not a feature candidate.
