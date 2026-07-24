# Bug history

## WDM-001: DXVK/Vulkan startup crash

- First affected version: 1.0
- Candidate fix: 1.1 RC1
- Runtime confirmation: fixed in 1.1 RC1; final `v1.1` remains blocked by the separate WDM-002 investigation
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

## WDM-002: Alt+Tab return freezes and crashes

- First affected version: 1.0
- Also affected: 1.1 RC1
- Partial fix: 1.1 RC2 removes the near-black/dark return frame, but the hang remains in roughly two out of three attempts
- RC3 result: near-100% Alt+Tab crash/freeze rate
- RC4 result: pending/obsolete for this shader-reload investigation
- RC5 result: improved reload speed but still insufficient
- Current diagnostic: 1.1 RC16 passive-state callback build
- Final fixed version: pending runtime confirmation
- Environment: Battlefield 2 in both native D3D9 and the tested DXVK configuration
- Symptom: after switching to another application and returning to the game, the image becomes much darker (almost black), then the game freezes and eventually exits

### Scope correction

The brief desktop switch while starting a Battlefield 2 map is a game/Windows compatibility behavior and is not caused by ReShade or WeaponDepthMerge. It is excluded from WDM-002. The remaining confirmed scope is Alt+Tab recovery and the Esc/menu transition after an Alt+Tab.

### Cause assessment

Alt+Tab causes a D3D9 lost-device/reset transition. Versions 1.0 and RC1 assumed every native state read, state write, depth-surface switch, clear, and replay operation succeeded. During the transition, failed `GetViewport` or `GetRenderState` calls could leave zero-initialized values that were then written back. A failed scratch-depth switch after the depth-only replay could also suppress the game's original color draw. Both paths can explain the dark frame and subsequent invalid rendering state.

The DXVK report is handled by the same fail-safe logic when the chain is exposed as D3D9. If ReShade exposes Vulkan directly, the add-on remains a no-op as established by WDM-001.

### Candidate fix

RC2 checks `TestCooperativeLevel` and every relevant D3D9 state operation. It stops interception on the first failure, lets the original game draw run when safe, clears unavailable depth bindings, and stays suspended until ReShade initializes the command list after reset. Testing confirmed that this prevents the dark frame, but it acts too late in some focus transitions because D3D9 may still report an operational device during the first part of Alt+Tab.

RC3 additionally detected whether the Battlefield 2 process owned the foreground window and attempted to resume after 30 presents. Runtime testing showed a near-100% crash/freeze rate, so this recovery approach is rejected. RC4 permanently disables interception on the first focus loss for the current process, restores fixed callback registration, and avoids double-releasing the combined depth view when ReShade destroys resource views during reset.

The new RC5 hypothesis is that `on_begin_effects` called `update_texture_bindings("DEPTH", ...)` every frame. ReShade's implementation walks all loaded effects and permutations and rewrites matching descriptors. During Alt+Tab, ReShade is already rebuilding those effects; repeating this global update on every frame can substantially slow reload and increase reset/reload contention. RC5 caches the last bound view and updates only on a real change or the explicit `reshade_reloaded_effects` event.

RC6 extended that fix by marking the effect-reload transition as pending. RC7-RC9 added further null, map-reset, and binding guards. RC10 disabled merging after the first post-activation reload, but runtime behavior remained identical even though depth merging stopped. RC11 returned to the minimal RC1 implementation and reproduced the same issue; the exact frozen 1.0 binary also reproduced it in the current environment. RC12 removed duplicate effect-runtime integration but still reproduced the issue. Windows event logs then showed a stable access violation at ReShade D3D9 offset `0x120d13`; disassembly maps this to `get_resource_desc` calling `GetType()` on an invalid resource pointer. RC13 stopped WeaponDepthMerge from using that API, but the same external offset still crashed. The second Reset and Shader rebuild complete successfully before the crash, so Shader compilation time is not causal. Disabling ShaderToggler and then Generic Depth made no difference; with Generic Depth disabled RC13 selected no INTZ resource and performed no merge. RC14 registered no callbacks and passed all reported Alt+Tab/Esc cases, followed by a clean ReShade shutdown. RC15 restored device and command-list lifecycle callbacks and also passed. RC16 adds only passive render-state tracking, with draw and clear interception still absent.

### Expected behavior after the fix

- Alt+Tab away and back: ReShade shader reload should complete at normal speed without the plugin adding a per-frame global binding update.
- Native D3D9 after recovery: depth merging resumes after command-list reinitialization.
- DXVK/Vulkan: startup remains safe; Vulkan depth merging remains inactive.
