param(
    [Parameter(Mandatory = $true)]
    [string]$Destination,
    [string]$RepoRoot
)

$ErrorActionPreference = "Stop"

function Get-RepoRoot {
    param([string]$Hint)

    if ($Hint) {
        return (Resolve-Path $Hint).Path
    }

    $path = $PSScriptRoot
    for ($i = 0; $i -lt 4; $i++) {
        $path = Split-Path -Parent $path
    }
    return (Resolve-Path $path).Path
}

$repoRoot = Get-RepoRoot -Hint $RepoRoot
$firmwareDir = Join-Path $repoRoot "firmware"

$binPath = Join-Path $firmwareDir "cardputer-codex-terminal.bin"
if (-not (Test-Path $binPath)) {
    $candidate = Get-ChildItem -Path $firmwareDir -Recurse -Filter "cardputer-codex-terminal.bin" -File | Select-Object -First 1
    if ($candidate) {
        $binPath = $candidate.FullName
    } else {
        throw "cardputer-codex-terminal.bin was not found. Build the firmware first."
    }
}

$destinationIsDir = $false
if (Test-Path $Destination -PathType Container) {
    $destinationIsDir = $true
} elseif ($Destination.EndsWith("\") -or $Destination.EndsWith("/")) {
    $destinationIsDir = $true
}

if ($destinationIsDir) {
    if (-not (Test-Path $Destination)) {
        New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    }
    $Destination = Join-Path $Destination (Split-Path $binPath -Leaf)
} else {
    $destinationDir = Split-Path -Parent $Destination
    if ($destinationDir -and -not (Test-Path $destinationDir)) {
        New-Item -ItemType Directory -Force -Path $destinationDir | Out-Null
    }
}

Copy-Item -Path $binPath -Destination $Destination -Force
Write-Output $Destination
