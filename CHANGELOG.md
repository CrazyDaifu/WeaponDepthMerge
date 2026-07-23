# Changelog

## 1.0 - 2026-07-24

- Created an independent ReShade 6.7.3 D3D9 x86 add-on.
- Reproduced Thalixte's first-person weapon/cockpit depth replay behavior.
- Added a clearable scratch depth surface so normal weapon rendering remains correct while the preserved INTZ depth receives the merged geometry.
- Added manual and automatic clear-boundary selection, depth bias, viewport filtering, diagnostics, and `DEPTH` semantic binding.
- Added device-reset and selected-resource destruction handling.
- Verified the output as PE32/x86 with `NAME` and `DESCRIPTION` exports.
- Battlefield 2 runtime test reported successful; no bug observed in the initial test.
