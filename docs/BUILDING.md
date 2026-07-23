# Building offline

All headers needed by this add-on are vendored in `third_party/`; no dependency download or Git submodule initialization is required.

## Supported build environment

- Windows
- Visual Studio 2017 C++ toolset `v141`
- Windows 10 SDK `10.0.19041.0`
- PowerShell 5 or newer

GitHub Actions overrides these project defaults with the Visual Studio 2022 `v143` toolset and the latest installed Windows 10 SDK. The source is compatible with both build environments.

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-release.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\verify-binary.ps1
```

`-ExecutionPolicy Bypass` applies only to that PowerShell process and is useful on systems whose default policy blocks local scripts.

The output is `build/WeaponDepthMerge.addon32`.

The project file uses only vendored headers plus the Windows SDK and `d3d9.lib`. Dear ImGui source files are present for license-complete offline packaging but are not compiled into the add-on; ReShade supplies the overlay function table at runtime.
