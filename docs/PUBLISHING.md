# Publishing to GitHub

## One-time prerequisites

Git for Windows must be installed. The publishing script installs GitHub CLI through winget when it is missing and opens its browser login flow when authorization is required.

No repository needs to be created manually.

## One-click publish

Double-click `publish-to-github.cmd` or `上传到GitHub.cmd` in the repository root.

The default operation creates a public repository named `WeaponDepthMerge`, commits all repository files, pushes branch `main`, creates tag `v1.0`, and triggers both GitHub Actions workflows.

Equivalent PowerShell command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\publish-github.ps1
```

To use a different repository name or make it private:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\publish-github.ps1 -Repository MyWeaponDepthMerge -Visibility private
```

## What happens online

- `.github/workflows/build.yml` builds and verifies the x86 add-on on every push and pull request.
- `.github/workflows/release.yml` builds version 1.0 again when tag `v1.0` is pushed and publishes `WeaponDepthMerge.addon32` plus `SHA256SUMS.txt` on the GitHub Releases page.

After upload, open the repository's Actions page. Both workflows should turn green. The release appears under the repository's Releases section after the tag workflow finishes.
