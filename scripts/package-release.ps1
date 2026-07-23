[CmdletBinding()]
param(
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$parent = Split-Path -Parent $repoRoot

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $parent 'WeaponDepthMerge-1.0-GitHub.zip'
}

if (Test-Path -LiteralPath $OutputPath) {
    throw "Output archive already exists: '$OutputPath'. Remove or rename it explicitly before packaging."
}

$stagingRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("WeaponDepthMerge-package-" + [guid]::NewGuid().ToString('N'))
$stagingRepo = Join-Path $stagingRoot 'WeaponDepthMerge'

try {
    New-Item -ItemType Directory -Path $stagingRepo | Out-Null

    Get-ChildItem -LiteralPath $repoRoot -Recurse -Force | ForEach-Object {
        $relative = $_.FullName.Substring($repoRoot.Length).TrimStart('\')
        if ($relative -match '^(\.git|\.vs)(\\|$)' -or
            $relative -match '^build\\intermediate(\\|$)' -or
            $relative -match '^build\\WeaponDepthMerge\.(exp|lib|pdb)$') {
            return
        }

        $destination = Join-Path $stagingRepo $relative
        if ($_.PSIsContainer) {
            New-Item -ItemType Directory -Force -Path $destination | Out-Null
        }
        else {
            $destinationParent = Split-Path -Parent $destination
            New-Item -ItemType Directory -Force -Path $destinationParent | Out-Null
            Copy-Item -LiteralPath $_.FullName -Destination $destination
        }
    }

    Compress-Archive -Path $stagingRepo -DestinationPath $OutputPath -CompressionLevel Optimal
}
finally {
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
}

Write-Host "Created: $OutputPath"
