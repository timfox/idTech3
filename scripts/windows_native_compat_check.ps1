# Windows native compatibility gate: PE machine type must match across shipped exes/dlls.
# Catches x86/x64/ARM64 mixups that surface as "missing DLL" or load failures.
# Usage: pwsh ./scripts/windows_native_compat_check.ps1 -BinDir bin
param(
    [Parameter(Mandatory = $true)]
    [string] $BinDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-PeMachine {
    param([string] $Path)
    $fs = [System.IO.File]::OpenRead($Path)
    try {
        $br = New-Object System.IO.BinaryReader($fs)
        if ($fs.Length -lt 64) { return $null }
        [void]$fs.Seek(0x3C, [System.IO.SeekOrigin]::Begin)
        $peOff = $br.ReadUInt32()
        if ($peOff + 6 -gt $fs.Length) { return $null }
        [void]$fs.Seek($peOff + 4, [System.IO.SeekOrigin]::Begin)
        return $br.ReadUInt16()
    }
    finally {
        $br.Dispose()
        $fs.Dispose()
    }
}

function MachineName {
    param([uint16] $M)
    switch ($M) {
        0x014c { 'x86' }
        0x8664 { 'x64' }
        0xAA64 { 'ARM64' }
        0x01c0 { 'ARM/Thumb' }
        default { "unknown(0x{0:X4})" -f $M }
    }
}

if (-not (Test-Path -LiteralPath $BinDir)) {
    throw "BinDir not found: $BinDir"
}

$items = @(Get-ChildItem -LiteralPath $BinDir -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in '.exe', '.dll' })
if ($items.Count -eq 0) {
    throw "No .exe or .dll under $BinDir"
}

$rows = foreach ($f in $items) {
    $m = Get-PeMachine -Path $f.FullName
    if ($null -eq $m) {
        throw "Could not read PE header: $($f.FullName)"
    }
    [PSCustomObject]@{ Name = $f.Name; Machine = $m; Arch = (MachineName $m) }
}

Write-Host "=== Windows native PE machine types ($BinDir) ==="
$rows | Format-Table -AutoSize | Out-String | Write-Host

$exeRows = @($rows | Where-Object { $_.Name -like '*.exe' })
if ($exeRows.Count -eq 0) {
    throw "No .exe found in $BinDir (expected at least one client or server executable)"
}

$ref = $exeRows[0].Machine
$refName = $exeRows[0].Name
$bad = @($rows | Where-Object { $_.Machine -ne $ref })
if ($bad.Count -gt 0) {
    Write-Host "ERROR: PE machine mismatch. Reference exe $refName is $(MachineName $ref)." -ForegroundColor Red
    foreach ($b in $bad) {
        Write-Host "  $($b.Name): $(MachineName $b.Machine) (expected $(MachineName $ref))" -ForegroundColor Red
    }
    exit 1
}

Write-Host "OK: all $($rows.Count) PE image(s) match $($refName) ($(MachineName $ref))."
exit 0
