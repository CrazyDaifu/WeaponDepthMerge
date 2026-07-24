# WeaponDepthMerge 1.1 RC15

Purpose: determine whether device and command-list private-state lifecycle participates in WDM-002.

This build registers only `init_device`, `destroy_device`, `init_command_list`, and `destroy_command_list`. It has no resource-binding, draw, clear, present, effect-runtime, or overlay callbacks and performs no depth merging.

Status: diagnostic build; not a feature candidate.
