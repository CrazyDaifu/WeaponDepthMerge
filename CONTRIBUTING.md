# Contributing

Read `_项目移交.md` before changing the implementation.

## Rules

1. Keep the first supported target D3D9 x86 until the Battlefield 2 implementation is stable.
2. Preserve the independent add-on architecture; do not require patching the ReShade core.
3. Keep changes traceable to official ReShade source, the verified Thalixte reference, or reproducible runtime evidence.
4. Build with `scripts/build-release.ps1` and report the result of `scripts/verify-binary.ps1`.
5. For rendering changes, include before/after screenshots, clear-segment diagnostics, and relevant `ReShade.log` lines.

## Commit scope

Prefer small commits separating implementation, diagnostics, documentation, and dependency updates. Do not silently replace vendored dependencies; update `THIRD_PARTY_NOTICES.md` and record the exact version or commit.
