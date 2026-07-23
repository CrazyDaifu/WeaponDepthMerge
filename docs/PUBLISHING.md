# Publishing to GitHub

## One-time prerequisites

Git for Windows must be installed. The publishing script installs GitHub CLI through winget when it is missing and opens its browser login flow when authorization is required.

No repository needs to be created manually.

## One-click publish

Double-click `publish-to-github.cmd` or `上传到GitHub.cmd` in the repository root.

The default operation commits all repository files and pushes the current branch. For `release/1.1`, this uploads the candidate and triggers the build workflow without creating a final release.

Because every link produces a new binary hash, the script also copies the freshly built DLL and checksum into `releases/<VERSION>/` when that directory exists. This keeps `build/` and the versioned candidate archive identical.

Equivalent PowerShell command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\publish-github.ps1
```

To use a different repository name or make it private:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\publish-github.ps1 -Repository MyWeaponDepthMerge -Visibility private
```

After the 1.1 candidate passes both runtime tests, publish the final tag and GitHub Release with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\publish-github.ps1 -PublishRelease -ReleaseTag v1.1
```

The script rejects `-PublishRelease` while `VERSION` contains a pre-release suffix such as `1.1-rc1`, or when the requested tag does not exactly match `v<VERSION>`.

## What happens online

- `.github/workflows/build.yml` builds and verifies the x86 add-on on every push and pull request.
- `.github/workflows/release.yml` builds the tagged version again and publishes `WeaponDepthMerge.addon32` plus `SHA256SUMS.txt` on the GitHub Releases page.

For 1.1, do not run the publishing script until the `releases/1.1-rc1/` candidate passes both native D3D9 and DXVK startup testing.

After upload, open the repository's Actions page. Both workflows should turn green. The release appears under the repository's Releases section after the tag workflow finishes.
