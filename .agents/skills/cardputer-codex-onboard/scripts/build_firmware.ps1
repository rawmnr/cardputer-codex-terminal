param(
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

Push-Location $firmwareDir
try {
    python -m platformio run
} finally {
    Pop-Location
}

$binPath = Join-Path $firmwareDir "cardputer-codex-terminal.bin"
if (-not (Test-Path $binPath)) {
    $candidate = Get-ChildItem -Path $firmwareDir -Recurse -Filter "cardputer-codex-terminal.bin" -File | Select-Object -First 1
    if ($candidate) {
        $binPath = $candidate.FullName
    } else {
        throw "cardputer-codex-terminal.bin was not found after the build."
    }
}

Write-Output $binPath

