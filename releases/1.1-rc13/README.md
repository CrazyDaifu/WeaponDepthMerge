# WeaponDepthMerge 1.1 RC13

Purpose: fix the repeated access violation at ReShade D3D9 offset `0x120d13` after map changes or Alt+Tab.

The add-on no longer dereferences a ReShade resource handle to inspect a depth candidate. It queries the currently bound native `IDirect3DSurface9`, validates its INTZ descriptor, retains it explicitly, and releases it during Reset cleanup.

Status: compiled candidate; awaiting Battlefield 2 runtime confirmation.
