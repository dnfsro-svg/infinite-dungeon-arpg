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
. (Join-Path $PSScriptRoot 'Configure.ps1') -Preset $Preset -Fresh:$Fresh

Push-Location $root
try {
    Invoke-ArpgNative -FilePath 'cmake.exe' -ArgumentList @('--build', '--preset', $Preset)
}
finally {
    Pop-Location
}
