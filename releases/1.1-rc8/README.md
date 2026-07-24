# WeaponDepthMerge 1.1 RC8

Purpose: test the map-start desktop crash and Alt+Tab regression.

Changes from RC7:

- Check D3D9 cooperative-level state before selecting or creating the combined depth resource.
- Preserve RC7's main-menu null-state guard and post-reload cooldown.

Status: compiled candidate; awaiting Battlefield 2 runtime confirmation.
