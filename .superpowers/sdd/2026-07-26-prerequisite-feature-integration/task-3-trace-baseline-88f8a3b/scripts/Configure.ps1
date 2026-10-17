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

function Invoke-ArpgNative {
    param(
        [Parameter(Mandatory)]
        [string]$FilePath,
        [string[]]$ArgumentList = @()
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

function Enter-ArpgMsvcEnvironment {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "vswhere.exe not found: $vswhere"
    }

    $vswhereArgs = @(
        '-latest',
        '-version', '[17.0,18.0)',
        '-products', 'Microsoft.VisualStudio.Product.BuildTools',
        '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        'Microsoft.VisualStudio.Component.Windows11SDK.26100',
        '-property', 'installationPath'
    )
    $installPath = & $vswhere @vswhereArgs
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installPath)) {
        throw 'VS 2022 Build Tools with MSVC x64 and SDK 26100 not found'
    }

    $installPath = ($installPath | Select-Object -First 1).Trim()
    $devShell = Join-Path $installPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
    if (-not (Test-Path -LiteralPath $devShell)) {
        throw "Developer PowerShell module not found: $devShell"
    }

    Import-Module $devShell -Force
    Enter-VsDevShell -VsInstallPath $installPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64 -winsdk=10.0.26100.0' | Out-Null

    if ($env:VSCMD_ARG_HOST_ARCH -ne 'x64' -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
        throw 'MSVC environment is not x64 host/x64 target'
    }
    if ($env:WindowsSDKVersion.TrimEnd('\') -ne '10.0.26100.0') {
        throw "Unexpected Windows SDK: $env:WindowsSDKVersion"
    }

    $compiler = Get-Command cl.exe -ErrorAction Stop
    $version = [Version]$compiler.FileVersionInfo.FileVersion
    if ($version.Major -ne 19 -or $version.Minor -ne 44) {
        throw "MSVC 19.44 required; detected $version"
    }

    $cmake = Get-Command cmake.exe -ErrorAction Stop
    Get-Command ninja.exe -ErrorAction Stop | Out-Null
    $capabilities = & $cmake.Source -E capabilities | ConvertFrom-Json
    if ([Version]$capabilities.version.string -lt [Version]'3.25.0') {
        throw "CMake 3.25+ required; detected $($capabilities.version.string)"
    }
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    Enter-ArpgMsvcEnvironment
    $cmakeArgs = @('--preset', $Preset)
    if ($Fresh) {
        $cmakeArgs += '--fresh'
    }
    Invoke-ArpgNative -FilePath 'cmake.exe' -ArgumentList $cmakeArgs
}
finally {
    Pop-Location
}
