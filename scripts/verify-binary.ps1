[CmdletBinding()]
param(
    [string]$DumpbinPath = "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\bin\HostX86\x86\dumpbin.exe"
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$binary = Join-Path $repoRoot 'build\WeaponDepthMerge.addon32'

if (-not (Test-Path -LiteralPath $binary)) {
    throw "Binary not found: '$binary'. Run build-release.ps1 first."
}
if (-not (Test-Path -LiteralPath $DumpbinPath)) {
    throw "dumpbin was not found at '$DumpbinPath'. Pass -DumpbinPath with a valid path."
}

$headers = (& $DumpbinPath /headers $binary | Out-String)
$exports = (& $DumpbinPath /exports $binary | Out-String)

if ($headers -notmatch '14C machine \(x86\)') {
    throw 'Binary is not PE32/x86.'
}
if ($exports -notmatch '\bNAME\b' -or $exports -notmatch '\bDESCRIPTION\b') {
    throw 'Required ReShade add-on exports NAME and DESCRIPTION were not found.'
}

$item = Get-Item -LiteralPath $binary
$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $binary
Write-Host "Verified PE32/x86 add-on: $($item.FullName)"
Write-Host "Size: $($item.Length) bytes"
Write-Host "SHA-256: $($hash.Hash)"

