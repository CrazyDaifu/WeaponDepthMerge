# Bug history

## WDM-001: DXVK/Vulkan startup crash

- First affected version: 1.0
- Candidate fix: 1.1 RC1
- Final fixed version: pending runtime confirmation
- Environment: Battlefield 2 using DXVK, with ReShade attached to Vulkan
- Symptom: black screen followed by a silent process exit before reaching the game

### Cause

The add-on registers global ReShade events for all graphics APIs but only creates `device_state` for D3D9 devices. In version 1.0, `on_draw`, `on_draw_indexed`, and `on_clear_depth` immediately dereferenced `get_state(cmd_list)`. Under DXVK, ReShade exposes a Vulkan device, so no D3D9 private state exists and the pointer is null.

### Fix

Version 1.1 makes every cross-API callback return without side effects unless the active device is native D3D9 and a valid `device_state` exists. Effect, present, overlay, device, command-list, and resource-view callbacks also explicitly guard their API and object inputs.

### Expected behavior after the fix

- Native D3D9: merge behavior remains enabled and unchanged.
- DXVK/Vulkan: add-on loads safely and remains inactive.
- Vulkan depth merge: not implemented.
