# Architecture

## Problem

Battlefield 2 renders the world, first-person weapon, and first-person arms in separate depth phases. A normal post-processing depth reader sees the preserved world depth but not a correct combined first-person depth, so AO can appear through the arms or weapon.

## Data flow

```text
Generic Depth replaces the main D3D9 depth with full-size INTZ
                         |
                         v
world geometry writes scene depth into INTZ
                         |
                         v
selected meaningful depth clear is intercepted
       |                                  |
       |                                  +--> clear ordinary scratch depth
       +--> keep INTZ world depth
                         |
                         v
each later normal Draw/DrawIndexed is replayed twice
       |                                  |
       |                                  +--> normal color draw + scratch depth
       +--> color/stencil writes off + biased viewport + INTZ depth
                         |
                         v
merged INTZ is bound to ReShade semantic DEPTH
```

## Core state

The per-D3D9-device state tracks:

- the currently bound depth-stencil view and viewport;
- the selected full-size INTZ resource, DSV, and SRV;
- the ordinary D3D9 scratch depth surface;
- meaningful draw segments split by depth clears;
- the selected clear index and whether the weapon phase is active;
- native stream/index validation and diagnostic counters.

## Boundary selection

A clear becomes meaningful only when the preceding segment has more than four draw calls and more than twenty vertices. `Clear index` selects among meaningful clears, not all D3D9 clear calls. Zero enables automatic selection based on the previous frame's largest vertex segment.

## Draw replay

After the selected boundary, eligible normal draw calls are suppressed and replayed through ReShade's raw D3D9 command-list methods, which call the underlying D3D9 device without recursively raising add-on draw events.

The preserved-depth replay uses the current pipeline and geometry with:

- the INTZ replacement as depth target;
- `MinZ` and `MaxZ` reduced by the configured bias, default `0.5`;
- all four color-write masks set to zero;
- stencil write mask set to zero.

Then the original draw is executed with normal color/stencil masks and the separate scratch depth. The combined INTZ is rebound afterward so application-visible depth behavior remains close to old ReShade's internal replacement model.

## Intentional limitations

- ReShade 6.7.3's D3D9 `bind_vertex_buffers` event activates internal fake buffers for `DrawPrimitiveUP`. Those handles are released but not cleared during Reset, so the add-on must not register that event. WeaponDepthMerge instead validates native stream-0 and index-buffer bindings immediately before replay and passes unbound or likely UP draws through unchanged.
- D3D9 x86 only.
- MSAA is unsupported.
- `DrawPrimitiveUP` and `DrawIndexedPrimitiveUP` pass through without merging, matching the verified historical behavior.
- Every eligible draw after the selected boundary is treated as first-person geometry.
- The plug-in relies on official Generic Depth to create the INTZ replacement.
- The scratch depth tries D24S8, D24X8, then D16; unusual format compatibility may fail.
