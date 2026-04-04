# Verify PE export tables for shipped Windows native modules (compat harness).
# - Renderer plugins (MinGW/CMake dlopen): idtech3_vulkan.dll / idtech3_opengl.dll must export GetRefAPI
# - Game native DLLs (if present): qagame*.dll / cgame*.dll / ui*.dll must export vmMain and dllEntry
#
# Usage:
#   pwsh ./scripts/windows_pe_exports_check.ps1 -BinDir bin [-ExpectRendererDlls] [-SkipRendererDlls]
param(
    [Parameter(Mandatory = $true)]
    [string] $BinDir,
    [switch] $ExpectRendererDlls,
    [switch] $SkipRendererDlls
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-PeRvaToFileOffset {
    param(
        [System.IO.BinaryReader] $Br,
        [long] $FsLength,
        [uint32] $SectionTableOffset,
        [uint16] $NumSections,
        [uint32] $Rva
    )
    if ($Rva -eq 0) { return $null }
    for ($i = 0; $i -lt $NumSections; $i++) {
        $pos = $SectionTableOffset + ($i * 40)
        if ($pos + 40 -gt $FsLength) { return $null }
        [void]$Br.BaseStream.Seek($pos, [System.IO.SeekOrigin]::Begin)
        $name = [System.Text.Encoding]::ASCII.GetString($Br.ReadBytes(8)).TrimEnd([char]0)
        $virtualSize = $Br.ReadUInt32()
        $virtualAddr = $Br.ReadUInt32()
        $rawSize = $Br.ReadUInt32()
        $rawPtr = $Br.ReadUInt32()
        [void]$Br.ReadBytes(16)
        if ($virtualAddr -ne 0 -and $rawPtr -ne 0 -and $Rva -ge $virtualAddr -and $Rva -lt ($virtualAddr + [Math]::Max($virtualSize, $rawSize))) {
            return [long]$rawPtr + ([long]$Rva - [long]$virtualAddr)
        }
    }
    return $null
}

function Get-PeExportNames {
    param([string] $Path)
    $fs = [System.IO.File]::OpenRead($Path)
    try {
        $br = New-Object System.IO.BinaryReader($fs)
        $len = $fs.Length
        if ($len -lt 0x200) { return @() }

        [void]$fs.Seek(0x3C, [System.IO.SeekOrigin]::Begin)
        $peOff = $br.ReadUInt32()
        if ($peOff -lt 4 -or $peOff + 0xF8 -gt $len) { return @() }

        [void]$fs.Seek($peOff, [System.IO.SeekOrigin]::Begin)
        if ($br.ReadUInt32() -ne 0x00004550) { return @() } # PE\0\0
        $br.ReadUInt16() | Out-Null # machine
        $numSections = $br.ReadUInt16()
        $br.ReadUInt32() | Out-Null # TimeDateStamp
        $br.ReadUInt32() | Out-Null # PointerToSymbolTable
        $br.ReadUInt32() | Out-Null # NumberOfSymbols
        $sizeOfOptional = $br.ReadUInt16()
        $br.ReadUInt16() | Out-Null # Characteristics
        $optStart = $peOff + 24
        if ($optStart + $sizeOfOptional -gt $len) { return @() }
        [void]$fs.Seek($optStart, [System.IO.SeekOrigin]::Begin)
        $magic = $br.ReadUInt16()
        # IMAGE_OPTIONAL_HEADER32: DataDirectory[0] at +96; PE32+ at +112
        if ($magic -eq 0x10b) {
            $ddOffset = $optStart + 96
        }
        elseif ($magic -eq 0x20b) {
            $ddOffset = $optStart + 112
        }
        else {
            return @()
        }
        $sectionTable = $optStart + $sizeOfOptional
        if ($ddOffset + 8 -gt $len) { return @() }
        [void]$fs.Seek($ddOffset, [System.IO.SeekOrigin]::Begin)
        $exportRva = $br.ReadUInt32()
        $exportSize = $br.ReadUInt32()
        if ($exportRva -eq 0 -or $exportSize -eq 0) { return @() }

        $expOff = Get-PeRvaToFileOffset -Br $br -FsLength $len -SectionTableOffset $sectionTable -NumSections $numSections -Rva $exportRva
        if ($null -eq $expOff -or $expOff + 40 -gt $len) { return @() }

        [void]$fs.Seek($expOff, [System.IO.SeekOrigin]::Begin)
        $br.ReadUInt32() | Out-Null # characteristics
        $br.ReadUInt32() | Out-Null # time
        $br.ReadUInt16() | Out-Null # major
        $br.ReadUInt16() | Out-Null # minor
        $br.ReadUInt32() | Out-Null # name RVA
        $br.ReadUInt32() | Out-Null # base
        $numFunctions = $br.ReadUInt32()
        $numNames = $br.ReadUInt32()
        $addrFuncRva = $br.ReadUInt32()
        $addrNamesRva = $br.ReadUInt32()
        $addrOrdRva = $br.ReadUInt32()

        if ($numNames -eq 0) { return @() }

        $namesOff = Get-PeRvaToFileOffset -Br $br -FsLength $len -SectionTableOffset $sectionTable -NumSections $numSections -Rva $addrNamesRva
        if ($null -eq $namesOff) { return @() }

        $out = New-Object System.Collections.Generic.List[string]
        for ($i = 0; $i -lt [int]$numNames; $i++) {
            $slot = $namesOff + ($i * 4)
            if ($slot + 4 -gt $len) { break }
            [void]$fs.Seek($slot, [System.IO.SeekOrigin]::Begin)
            $nameRva = $br.ReadUInt32()
            $nameOff = Get-PeRvaToFileOffset -Br $br -FsLength $len -SectionTableOffset $sectionTable -NumSections $numSections -Rva $nameRva
            if ($null -eq $nameOff) { continue }
            [void]$fs.Seek($nameOff, [System.IO.SeekOrigin]::Begin)
            $chars = New-Object System.Collections.Generic.List[byte]
            while ($fs.Position -lt $len) {
                $b = $br.ReadByte()
                if ($b -eq 0) { break }
                [void]$chars.Add($b)
            }
            if ($chars.Count -gt 0) {
                $out.Add([System.Text.Encoding]::ASCII.GetString($chars.ToArray()))
            }
        }
        return ,$out.ToArray()
    }
    finally {
        $br.Dispose()
        $fs.Dispose()
    }
}

function Assert-Exports {
    param(
        [string] $Path,
        [string[]] $Required
    )
    $names = @(Get-PeExportNames -Path $Path)
    if ($names.Count -eq 0) {
        throw "No exports found in $Path (not a PE DLL or no export table)"
    }
    $set = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($n in $names) { [void]$set.Add($n) }
    foreach ($r in $Required) {
        if (-not $set.Contains($r)) {
            throw "Missing export '$r' in $Path (PE exports: $($names -join ', '))"
        }
    }
}

if (-not (Test-Path -LiteralPath $BinDir)) {
    throw "BinDir not found: $BinDir"
}

Write-Host "=== PE export compatibility check ($BinDir) ==="

# Renderer plugins (CMake USE_RENDERER_DLOPEN on Windows MinGW)
if (-not $SkipRendererDlls) {
    $vk = Join-Path $BinDir 'idtech3_vulkan.dll'
    $gl = Join-Path $BinDir 'idtech3_opengl.dll'
    $haveVk = Test-Path -LiteralPath $vk
    $haveGl = Test-Path -LiteralPath $gl
    if ($ExpectRendererDlls) {
        if (-not $haveVk -or -not $haveGl) {
            throw "Expected idtech3_vulkan.dll and idtech3_opengl.dll in $BinDir (USE_RENDERER_DLOPEN build). Missing: $(@($(if(-not $haveVk){'vulkan'}), $(if(-not $haveGl){'opengl'})) -join ', ')"
        }
    }
    if ($haveVk) {
        Assert-Exports -Path $vk -Required @('GetRefAPI')
        Write-Host "OK: $vk exports GetRefAPI"
    }
    if ($haveGl) {
        Assert-Exports -Path $gl -Required @('GetRefAPI')
        Write-Host "OK: $gl exports GetRefAPI"
    }
    if (-not $ExpectRendererDlls -and -not $haveVk -and -not $haveGl) {
        Write-Host "(No renderer DLLs in bin — static renderer build; skipped GetRefAPI check)"
    }
}

# Native game modules (optional in CI; verify if shipped)
# Match qagamex86_64.dll, ui.dll, server.dll, etc.; exclude idtech3_* renderer plugins
$gamePat = '^(qagame|cgame|ui|game|server|client|frontend)[^.]*\.dll$'
$gameDlls = @(Get-ChildItem -LiteralPath $BinDir -File -Filter '*.dll' -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match $gamePat })
foreach ($g in $gameDlls) {
    Assert-Exports -Path $g.FullName -Required @('vmMain', 'dllEntry')
    Write-Host "OK: $($g.Name) exports vmMain, dllEntry"
}
if ($gameDlls.Count -eq 0) {
    Write-Host "(No qagame/cgame/ui native DLLs in bin — skipped vmMain/dllEntry check)"
}

Write-Host "PE export check passed."
exit 0
