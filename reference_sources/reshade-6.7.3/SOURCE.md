# Source identification

- Upstream: `https://github.com/crosire/reshade`
- Version: ReShade 6.7.3
- Files preserved from the official local source package:
  - `examples/09-depth/generic_depth_addon.cpp`
  - `source/d3d9/d3d9_device.cpp`
  - `source/d3d9/d3d9_impl_command_list.cpp`
  - `source/d3d9/d3d9_impl_device.cpp`

Important checks:

- `generic_depth_addon.cpp`: INTZ replacement and effect-time `DEPTH` binding.
- `d3d9_device.cpp`: draw/clear add-on event semantics and UP-draw temporary buffers.
- `d3d9_impl_command_list.cpp`: raw `draw`/`draw_indexed` calls bypass add-on event recursion.
- `d3d9_impl_device.cpp`: D3D9 resource-view creation and DSV unbinding behavior.

These files are research references only and are not part of the add-on build.

