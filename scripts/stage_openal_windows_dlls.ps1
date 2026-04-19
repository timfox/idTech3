#requires -Version 5.1
<#
.SYNOPSIS
  Stage OpenAL Soft Windows DLLs next to idtech3*.exe so OpenAL works without a separate Creative / oalinst install.

.DESCRIPTION
  Downloads the official openal-soft *-bin.zip from kcat/openal-soft releases and copies:
    - router/<WinNN>/OpenAL32.dll  (router; loads soft_oal.dll from the app directory)
    - bin/<WinNN>/soft_oal.dll     (actual OpenAL Soft implementation)

  Override the release with env OPENAL_SOFT_BIN_VERSION (e.g. 1.24.3). Set SKIP_OPENAL_DLL_BUNDLE=1 to no-op.

  Note: upstream bin zips currently ship Win32/Win64 only; ARM64 native Windows is skipped with a message.

.PARAMETER BinDir
  Directory containing (or to receive) the PE files, relative to the repo root or absolute.

.PARAMETER Arch
  x64 | x86 | arm64 — selects Win64 vs Win32 paths inside the zip; arm64 skips (no WinARM64 in official bin zip).
#>
param(
    [Parameter(Mandatory = $false)]
    [string] $BinDir = "bin",

    [Parameter(Mandatory = $false)]
    [ValidateSet("x64", "x86", "arm64")]
    [string] $Arch = "x64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($env:SKIP_OPENAL_DLL_BUNDLE -eq "1") {
    Write-Host "SKIP_OPENAL_DLL_BUNDLE=1 -> skipping OpenAL DLL staging"
    exit 0
}

$version = $env:OPENAL_SOFT_BIN_VERSION
if ([string]::IsNullOrWhiteSpace($version)) {
    $version = "1.24.3"
}

$workspace = if ($env:GITHUB_WORKSPACE) { $env:GITHUB_WORKSPACE } else { (Get-Location).Path }
$dest = if ([System.IO.Path]::IsPathRooted($BinDir)) { $BinDir } else { Join-Path $workspace $BinDir }

if (-not (Test-Path -LiteralPath $dest -PathType Container)) {
    throw "Bin directory not found: $dest"
}

$winSubdir = switch ($Arch) {
    "x64" { "Win64" }
    "x86" { "Win32" }
    "arm64" {
        Write-Host "OpenAL Soft official bin zip has no Windows ARM64 DLLs; skipping bundle (MSVC ARM64: use WASAPI / install OpenAL separately)."
        exit 0
    }
}

$zipName = "openal-soft-$version-bin.zip"
$url = "https://github.com/kcat/openal-soft/releases/download/$version/$zipName"
$cacheRoot = Join-Path $workspace ".ci-openal-soft-cache"
$zipPath = Join-Path $cacheRoot $zipName
$extractRoot = Join-Path $cacheRoot "extract-$version"

New-Item -ItemType Directory -Force -Path $cacheRoot | Out-Null
New-Item -ItemType Directory -Force -Path $dest | Out-Null

if (-not (Test-Path -LiteralPath $zipPath -PathType Leaf)) {
    Write-Host "Downloading OpenAL Soft $version Windows binaries..."
    Invoke-WebRequest -Uri $url -OutFile $zipPath -UseBasicParsing
}

$inner = Join-Path $extractRoot "openal-soft-$version-bin"
$routerDll = Join-Path $inner "router\$winSubdir\OpenAL32.dll"
$softDll = Join-Path $inner "bin\$winSubdir\soft_oal.dll"

if (-not (Test-Path -LiteralPath $routerDll) -or -not (Test-Path -LiteralPath $softDll)) {
    if (Test-Path -LiteralPath $extractRoot) {
        Remove-Item -LiteralPath $extractRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
    New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null
    Write-Host "Extracting $zipName ..."
    Expand-Archive -LiteralPath $zipPath -DestinationPath $extractRoot -Force
}

if (-not (Test-Path -LiteralPath $routerDll)) {
    throw "Expected router DLL missing after extract: $routerDll"
}
if (-not (Test-Path -LiteralPath $softDll)) {
    throw "Expected soft_oal.dll missing after extract: $softDll"
}

$copying = Join-Path $inner "COPYING"
if (Test-Path -LiteralPath $copying) {
    Copy-Item -LiteralPath $copying -Destination (Join-Path $dest "OpenAL-Soft-COPYING.txt") -Force
}

Copy-Item -LiteralPath $routerDll -Destination (Join-Path $dest "OpenAL32.dll") -Force
Copy-Item -LiteralPath $softDll -Destination (Join-Path $dest "soft_oal.dll") -Force
Write-Host "Staged OpenAL32.dll + soft_oal.dll into $dest (OpenAL Soft $version, $winSubdir)"
