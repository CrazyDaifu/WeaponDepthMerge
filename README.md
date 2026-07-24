# WeaponDepthMerge

WeaponDepthMerge is an independent ReShade 6.7.3 add-on that merges native D3D9 first-person weapon/cockpit geometry into the scene depth buffer. Its first confirmed target is the 32-bit Direct3D 9 version of Battlefield 2.

战地 2 会分别渲染场景、第一人称武器和手臂。普通 ReShade 深度只能得到不完整的结果，AO 等深度特效可能穿过手臂或武器。本插件在选定的深度清除点之后，将第一人称几何额外写入保留下来的 INTZ 场景深度，同时维持游戏正常的武器颜色和遮挡渲染。

## Current status

- Battlefield 2 runtime test: successful.
- Native D3D9 depth merge: runtime-confirmed working.
- DXVK startup regression: fixed by 1.1 RC1; the Vulkan path remains inactive.
- Target: D3D9, x86, ReShade 6.7.3 Full Add-on Support.
- Output: `build/WeaponDepthMerge.addon32`.
- Version: `1.1-rc6`.
- Current test target: Alt+Tab stability in native D3D9 and DXVK. After focus loss, merging stays disabled until process restart.
- DXVK/Vulkan compatibility: safe no-op loading; depth merging remains native D3D9 only.

## Quick installation

1. Install the 32-bit D3D9 build of ReShade 6.7.3 with full add-on support.
2. Copy `build/WeaponDepthMerge.addon32` beside ReShade's `d3d9.dll`.
3. Keep Generic Depth enabled.
4. Disable Generic Depth's `Copy depth buffer before clear operations` option.
5. Disable in-game MSAA.
6. Start with `Clear index = 1` and `First-person depth bias = 0.5`.

The add-on page should show a full-resolution `Combined INTZ` buffer and an increasing `Merged draw calls` counter.

## How it works

After the chosen meaningful depth clear, each ordinary D3D9 draw is replayed twice:

1. Depth-only replay into the preserved INTZ scene depth, using the historical `MinZ/MaxZ - 0.5` viewport bias.
2. Normal game draw against a separate clearable scratch depth surface.

The merged INTZ view is then bound to ReShade effects through the `DEPTH` semantic. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the detailed state machine.

## Offline build

All source dependencies needed by the project are included under `third_party/`. No Git submodule or dependency download is needed.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-release.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\verify-binary.ps1
```

The build still requires a locally installed Windows C++ toolchain: Visual Studio 2017 `v141` and Windows 10 SDK `10.0.19041.0` are the confirmed environment.

GitHub Actions builds the same Win32 target with Visual Studio 2022 `v143`, verifies PE32/x86 and the required ReShade exports, and uploads the resulting DLL as an artifact.

## Repository map

| Path | Purpose |
| --- | --- |
| `_项目移交.md` | First document for the next AI or maintainer |
| `src/` | Add-on implementation |
| `WeaponDepthMerge.sln` | Visual Studio solution |
| `build/` | Confirmed x86 binary and generated build output |
| `releases/` | Frozen binaries and checksums for previous versions |
| `third_party/` | Vendored ReShade API and Dear ImGui dependencies |
| `reference_sources/` | Offline historical and official source references |
| `docs/` | Architecture, building, and testing documentation |
| `scripts/` | Repeatable build and binary verification |

## Publish to GitHub

Double-click `publish-to-github.cmd` (or `上传到GitHub.cmd`) to push the current release branch and trigger the online build. It does not create the final `v1.1` tag unless `-PublishRelease` is explicitly supplied after runtime testing.

See [docs/PUBLISHING.md](docs/PUBLISHING.md) for repository-name and visibility options.

## Technical references

The implementation is based on official ReShade 6.7.3 and Thalixte's verified commit `6dbe6c6d8fb45af8af11a53f53dd67043c809c23`.

## Limitations

- D3D9 x86 only.
- DXVK/Vulkan is startup-safe in 1.1, but the merge feature is inactive there.
- No MSAA support yet.
- UP draw calls are intentionally not merged.
- The selected clear boundary may need adjustment in another game.
- All eligible draws after that boundary are treated as first-person geometry.
- Generic Depth must create the full-size INTZ replacement.

## Documentation

- [Project handoff / 项目移交](_项目移交.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Testing](docs/TESTING.md)
- [Bug history](docs/BUGS.md)
- [Offline building](docs/BUILDING.md)
- [Publishing to GitHub](docs/PUBLISHING.md)
- [Sources](SOURCES.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

## License

The project code is distributed under the BSD 3-Clause License. Vendored and reference materials retain their upstream licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
