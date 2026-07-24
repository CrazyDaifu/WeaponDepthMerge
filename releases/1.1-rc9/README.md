# WeaponDepthMerge 1.1 RC9

Purpose: test the remaining second-reload crash after the first Alt+Tab or the Esc menu transition.

Changes from RC8:

- Do not call ReShade global texture-binding updates when the add-on is suspended, focus-paused, waiting for effect reload completion, or has no combined depth view.
- Preserve existing ReShade descriptors during those transitions.

Status: compiled candidate; awaiting Battlefield 2 runtime confirmation.
