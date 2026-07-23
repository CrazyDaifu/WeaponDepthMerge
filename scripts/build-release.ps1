[CmdletBinding()]
param(
    [string]$MsBuildPath = "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe"
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $repoRoot 'weapon_depth_merge.vcxproj'

if (-not (Test-Path -LiteralPath $MsBuildPath)) {
    throw "MSBuild was not found at '$MsBuildPath'. Pass -MsBuildPath with a valid Visual Studio MSBuild path."
}

& $MsBuildPath $project /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /m
if ($LASTEXITCODE -ne 0) {
    throw "Release build failed with exit code $LASTEXITCODE."
}

Write-Host "Built: $(Join-Path $repoRoot 'build\WeaponDepthMerge.addon32')"

