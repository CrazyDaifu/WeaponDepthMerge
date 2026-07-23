# Third-party notices

This repository vendors source and headers so it can be built and audited offline. Third-party components retain their original copyrights and licenses.

## ReShade 6.7.3

- Upstream: <https://github.com/crosire/reshade>
- Used for: add-on API headers, official D3D9/Generic Depth reference, source archive
- License: BSD 3-Clause
- License file: `third_party/reshade/LICENSE.md`
- Source archive: `third_party/source_archives/reshade-v6.7.3.zip`

## Dear ImGui v1.92.5-docking

- Upstream: <https://github.com/ocornut/imgui>
- Commit: `3912b3d9a9c1b3f17431aebafd86d2f40ee6e59c`
- Used for: add-on overlay API declarations and offline dependency completeness
- License: MIT
- License file: `third_party/imgui/LICENSE.txt`

## Thalixte ReShade reference

- Upstream: <https://github.com/Thalixte/Reshade>
- Verified commit: `6dbe6c6d8fb45af8af11a53f53dd67043c809c23`
- Used for: historical implementation research and behavior traceability only
- Compiled into add-on: no
- Complete source archive: `third_party/source_archives/thalixte-reshade-6dbe6c6d.zip`
- License: BSD 3-Clause
- License file: `reference_sources/thalixte/LICENSE.md`

## Windows SDK and Direct3D 9

The build links against `d3d9.lib` from the locally installed Windows SDK. Microsoft toolchain and SDK files are not redistributed in this repository. They must already exist on the build machine.
