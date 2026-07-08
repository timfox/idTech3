# Junction-based layout bridge for MSVC/MinGW on Windows (symlinks are unreliable for cl.exe).
param(
    [string]$Root = $env:GITHUB_WORKSPACE
)

$ErrorActionPreference = 'Stop'
Set-Location $Root

function New-RepoJunction {
    param(
        [string]$Link,
        [string]$Target
    )
    $linkPath = Join-Path $Root $Link
    $targetPath = Join-Path $Root $Target
    if (-not (Test-Path -LiteralPath $targetPath)) {
        throw "junction target missing: $targetPath"
    }
    if (Test-Path -LiteralPath $linkPath) {
        Remove-Item -LiteralPath $linkPath -Recurse -Force
    }
    $parent = Split-Path -Parent $linkPath
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    cmd /c "mklink /J `"$linkPath`" `"$targetPath`"" | Out-Null
    if (-not (Test-Path -LiteralPath $linkPath)) {
        throw "failed to create junction $linkPath -> $targetPath"
    }
    Write-Host "[layout_fwd_win] $Link -> $Target"
}

Write-Host '[layout_fwd_win] MSVC/MinGW junction bridge...'
New-RepoJunction 'engine\platform\physics' 'modules\physics'
New-RepoJunction 'engine\platform\botlib' 'modules\botlib'
New-RepoJunction 'engine\platform\renderers' 'renderers'
New-RepoJunction 'engine\platform\navigation' 'modules\navigation'
New-RepoJunction 'engine\platform\cgame' 'runtime\cgame'
New-RepoJunction 'engine\qcommon' 'engine\core'
New-RepoJunction 'engine\platform\qcommon' 'engine\core'
New-RepoJunction 'engine\platform\client' 'runtime\client'
New-RepoJunction 'engine\platform\server' 'runtime\server'
New-RepoJunction 'engine\platform\game' 'runtime\game'
New-RepoJunction 'engine\platform\ui' 'runtime\ui'
New-RepoJunction 'engine\platform\audio' 'modules\audio'
New-RepoJunction 'engine\platform\world' 'modules\world'
New-RepoJunction 'engine\audio' 'modules\audio'
New-RepoJunction 'engine\world' 'modules\world'
New-RepoJunction 'engine\client' 'runtime\client'
New-RepoJunction 'engine\server' 'runtime\server'
Write-Host '[layout_fwd_win] done'
