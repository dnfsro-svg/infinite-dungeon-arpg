[CmdletBinding()]
param(
    [ValidateSet(
        'windows-msvc-debug',
        'windows-msvc-release',
        'windows-msvc-core-debug')]
    [string]$Preset = 'windows-msvc-debug',
    [switch]$Fresh
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Build.ps1') -Preset $Preset -Fresh:$Fresh

Push-Location $root
try {
    Invoke-ArpgNative -FilePath 'ctest.exe' -ArgumentList @('--preset', $Preset, '--no-tests=error')
}
finally {
    Pop-Location
}
