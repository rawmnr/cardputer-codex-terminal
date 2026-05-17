param(
    [string]$BridgeToken,
    [switch]$RealCodex,
    [switch]$Preview,
    [switch]$Mcp,
    [string]$CodexCommand = "codex",
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
$middlewareDir = Join-Path $repoRoot "middleware"

$uvArgs = @("run", "cardputer-codex-middleware")
if ($Preview) {
    $uvArgs += "--preview"
}
if ($Mcp) {
    $uvArgs += "--mcp"
}
if ($RealCodex) {
    $uvArgs += "--real-codex"
}
if ($BridgeToken) {
    $uvArgs += "--bridge-token"
    $uvArgs += $BridgeToken
}
if ($RealCodex -or $Preview -or $Mcp) {
    $uvArgs += "--codex-command"
    $uvArgs += $CodexCommand
}

Push-Location $middlewareDir
try {
    & uv @uvArgs
} finally {
    Pop-Location
}
