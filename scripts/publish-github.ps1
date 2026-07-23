[CmdletBinding()]
param(
    [string]$Repository = 'WeaponDepthMerge',
    [ValidateSet('public', 'private')]
    [string]$Visibility = 'public',
    [string]$Description = 'ReShade 6.7.3 x86 D3D9 add-on for merging first-person weapon and cockpit depth.',
    [string]$ReleaseTag = 'v1.0'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Program,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed ($LASTEXITCODE): $Program $($Arguments -join ' ')"
    }
}

function Test-NativeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Program,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    # PowerShell 5.1 turns expected native stderr output into a terminating error
    # when ErrorActionPreference is Stop. Probes only need the process exit code.
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'SilentlyContinue'
        & $Program @Arguments *> $null
        $succeeded = $LASTEXITCODE -eq 0
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    return $succeeded
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Git is not installed or is not available in PATH.'
}

$ghCommand = Get-Command gh -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
if (-not $ghCommand) {
    $winget = Get-Command winget -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
    if (-not $winget) {
        throw 'GitHub CLI is required and winget is unavailable. Install GitHub CLI from https://cli.github.com/ and run this script again.'
    }

    Write-Host 'GitHub CLI is not installed. Installing it with winget...'
    Invoke-Checked $winget install --id GitHub.cli --exact --accept-package-agreements --accept-source-agreements

    $ghCandidates = @(
        (Join-Path $env:ProgramFiles 'GitHub CLI\gh.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'GitHub CLI\gh.exe')
    )
    $ghCommand = $ghCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $ghCommand) {
        $ghCommand = Get-Command gh -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
    }
    if (-not $ghCommand) {
        throw 'GitHub CLI installation completed, but gh.exe could not be located. Open a new terminal and run the script again.'
    }
}

Push-Location $repoRoot
try {
    if (-not (Test-NativeCommand $ghCommand auth status)) {
        Write-Host 'GitHub authentication is required. Follow the interactive login prompts.'
        Invoke-Checked $ghCommand auth login --web --git-protocol https
    }

    if (-not (Test-Path -LiteralPath '.git')) {
        Invoke-Checked git init -b main
    }
    else {
        Invoke-Checked git branch -M main
    }

    Write-Host 'Building and verifying the local 1.0 binary before upload...'
    Invoke-Checked powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build-release.ps1')
    Invoke-Checked powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'verify-binary.ps1')
    $localHash = (Get-FileHash -Algorithm SHA256 -LiteralPath '.\build\WeaponDepthMerge.addon32').Hash
    "$localHash *WeaponDepthMerge.addon32" | Set-Content -Encoding ASCII '.\build\SHA256SUMS.txt'

    $githubLogin = (& $ghCommand api user --jq .login).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($githubLogin)) {
        throw 'Unable to determine the authenticated GitHub account.'
    }

    if ([string]::IsNullOrWhiteSpace((& git config --local user.name))) {
        Invoke-Checked git config --local user.name $githubLogin
    }
    if ([string]::IsNullOrWhiteSpace((& git config --local user.email))) {
        Invoke-Checked git config --local user.email "$githubLogin@users.noreply.github.com"
    }

    Invoke-Checked git add --all
    if (-not (Test-NativeCommand git diff --cached --quiet)) {
        Invoke-Checked git commit -m 'Release 1.0'
    }

    if (-not (Test-NativeCommand git remote get-url origin)) {
        if (Test-NativeCommand $ghCommand repo view "$githubLogin/$Repository") {
            Invoke-Checked git remote add origin "https://github.com/$githubLogin/$Repository.git"
        }
        else {
            $visibilityFlag = "--$Visibility"
            Invoke-Checked $ghCommand repo create $Repository $visibilityFlag --source $repoRoot --remote origin --description $Description
        }
    }

    Invoke-Checked git push --set-upstream origin main

    if (-not (Test-NativeCommand git rev-parse --verify "refs/tags/$ReleaseTag")) {
        Invoke-Checked git tag -a $ReleaseTag -m "WeaponDepthMerge 1.0"
    }
    elseif ((& git rev-list -n 1 $ReleaseTag).Trim() -ne (& git rev-parse HEAD).Trim()) {
        throw "Local tag '$ReleaseTag' already points to a different commit. Delete or rename that tag before publishing."
    }
    Invoke-Checked git push origin $ReleaseTag

    $url = (& $ghCommand repo view --json url --jq .url).Trim()
    Write-Host ''
    Write-Host "Published: $url"
    Write-Host "Actions: $url/actions"
    Write-Host "Release workflow was triggered by tag $ReleaseTag."
}
finally {
    Pop-Location
}
