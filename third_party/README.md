# Vendored dependencies

This directory contains all project source dependencies needed after the Windows C++ compiler and SDK are installed.

- `reshade/include/`: official ReShade 6.7.3 public add-on headers.
- `imgui/`: Dear ImGui `v1.92.5-docking`, commit `3912b3d9a9c1b3f17431aebafd86d2f40ee6e59c`.
- `source_archives/reshade-v6.7.3.zip`: complete official ReShade source snapshot used for offline auditing.
- `source_archives/thalixte-reshade-6dbe6c6d.zip`: complete Git archive of the verified Thalixte commit used for historical behavior analysis.

No dependency manager, submodule initialization, or network download is needed to compile `weapon_depth_merge.vcxproj`.
