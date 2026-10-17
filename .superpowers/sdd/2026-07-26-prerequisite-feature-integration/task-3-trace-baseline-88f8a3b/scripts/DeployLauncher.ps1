[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GameBuildDirectory,
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'InfiniteDungeon\app'),
    [string]$DesktopDirectory = [Environment]::GetFolderPath('Desktop')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$requiredAssetDirectories = @(
    'assets/fonts',
    'assets/player',
    'assets/skills',
    'assets/stage12',
    'assets/stage14/audio',
    'assets/stage15/audio'
)

function Resolve-OutputPath([string]$Path, [string]$Name) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "$Name must not be empty."
    }
    return [System.IO.Path]::GetFullPath($Path)
}

function Assert-ApplicationLayout([string]$Directory, [string]$Description) {
    foreach ($executable in @('无限地下城启动器.exe', 'arpg_game.exe')) {
        $path = Join-Path $Directory $executable
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "$Description is missing required file: $executable"
        }
    }
    foreach ($assetDirectory in $requiredAssetDirectories) {
        $path = Join-Path $Directory $assetDirectory
        if (-not (Test-Path -LiteralPath $path -PathType Container)) {
            throw "$Description is missing required asset directory: $assetDirectory"
        }
    }
}

function Assert-ReleaseBuildDirectory([string]$Directory) {
    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        throw "GameBuildDirectory does not exist or is not a directory: $Directory"
    }
    if ((Split-Path -Path $Directory -Leaf) -ine 'bin') {
        throw "GameBuildDirectory must be an explicit Release bin directory: $Directory"
    }

    Assert-ApplicationLayout -Directory $Directory -Description 'GameBuildDirectory'

    $cachePath = Join-Path (Split-Path -Path $Directory -Parent) 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
        throw "GameBuildDirectory has no sibling CMakeCache.txt: $cachePath"
    }
    $cacheContents = Get-Content -LiteralPath $cachePath -Raw
    $buildTypeAssignments = [regex]::Matches($cacheContents, '(?m)^CMAKE_BUILD_TYPE:STRING=([^\r\n]*)\r?$')
    if ($buildTypeAssignments.Count -ne 1 -or $buildTypeAssignments[0].Groups[1].Value -cne 'Release') {
        throw "GameBuildDirectory is not a Release CMake build: $cachePath"
    }
}

function New-SiblingName([string]$BaseName, [string]$Role) {
    $timestamp = Get-Date -Format 'yyyyMMddHHmmssfff'
    return "$BaseName.$Role.$timestamp.$PID.$([Guid]::NewGuid().ToString('N'))"
}

$resolvedGameBuildDirectory = Resolve-OutputPath -Path $GameBuildDirectory -Name 'GameBuildDirectory'
if (-not (Test-Path -LiteralPath $resolvedGameBuildDirectory -PathType Container)) {
    throw "GameBuildDirectory does not exist or is not a directory: $resolvedGameBuildDirectory"
}
$resolvedGameBuildDirectory = (Resolve-Path -LiteralPath $resolvedGameBuildDirectory).ProviderPath
$resolvedInstallRoot = Resolve-OutputPath -Path $InstallRoot -Name 'InstallRoot'
$resolvedDesktopDirectory = Resolve-OutputPath -Path $DesktopDirectory -Name 'DesktopDirectory'

Assert-ReleaseBuildDirectory -Directory $resolvedGameBuildDirectory

$installParent = Split-Path -Path $resolvedInstallRoot -Parent
$installLeaf = Split-Path -Path $resolvedInstallRoot -Leaf
if ([string]::IsNullOrWhiteSpace($installParent) -or [string]::IsNullOrWhiteSpace($installLeaf)) {
    throw "InstallRoot must name an application directory: $resolvedInstallRoot"
}

New-Item -ItemType Directory -Path $installParent -Force | Out-Null
$stagingPath = Join-Path $installParent (New-SiblingName -BaseName $installLeaf -Role 'staging')
$backupPath = $null
$stagePublished = $false
$previousMoved = $false

try {
    New-Item -ItemType Directory -Path $stagingPath -ErrorAction Stop | Out-Null
    foreach ($executable in @('无限地下城启动器.exe', 'arpg_game.exe')) {
        Copy-Item -LiteralPath (Join-Path $resolvedGameBuildDirectory $executable) -Destination (Join-Path $stagingPath $executable) -ErrorAction Stop
    }
    Copy-Item -LiteralPath (Join-Path $resolvedGameBuildDirectory 'assets') -Destination (Join-Path $stagingPath 'assets') -Recurse -ErrorAction Stop
    Assert-ApplicationLayout -Directory $stagingPath -Description 'Staging deployment'

    if (Test-Path -LiteralPath $resolvedInstallRoot) {
        if (-not (Test-Path -LiteralPath $resolvedInstallRoot -PathType Container)) {
            throw "InstallRoot exists but is not a directory: $resolvedInstallRoot"
        }
        $backupPath = Join-Path $installParent (New-SiblingName -BaseName $installLeaf -Role 'previous')
        Rename-Item -LiteralPath $resolvedInstallRoot -NewName (Split-Path -Path $backupPath -Leaf) -ErrorAction Stop
        $previousMoved = $true
    }

    Rename-Item -LiteralPath $stagingPath -NewName $installLeaf -ErrorAction Stop
    $stagePublished = $true
    Assert-ApplicationLayout -Directory $resolvedInstallRoot -Description 'Published deployment'
}
catch {
    $deploymentError = $_
    if ($stagePublished -and (Test-Path -LiteralPath $resolvedInstallRoot)) {
        Remove-Item -LiteralPath $resolvedInstallRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
    if ($previousMoved -and $null -ne $backupPath -and (Test-Path -LiteralPath $backupPath) -and -not (Test-Path -LiteralPath $resolvedInstallRoot)) {
        Rename-Item -LiteralPath $backupPath -NewName $installLeaf -ErrorAction SilentlyContinue
    }
    throw $deploymentError
}
finally {
    if (Test-Path -LiteralPath $stagingPath) {
        Remove-Item -LiteralPath $stagingPath -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if ($previousMoved -and $null -ne $backupPath -and (Test-Path -LiteralPath $backupPath)) {
    Remove-Item -LiteralPath $backupPath -Recurse -Force -ErrorAction Stop
}

New-Item -ItemType Directory -Path $resolvedDesktopDirectory -Force | Out-Null
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut((Join-Path $resolvedDesktopDirectory '无限地下城.lnk'))
$shortcut.TargetPath = Join-Path $resolvedInstallRoot '无限地下城启动器.exe'
$shortcut.WorkingDirectory = $resolvedInstallRoot
$shortcut.IconLocation = "$($shortcut.TargetPath),0"
$shortcut.Description = '启动无限地下城'
$shortcut.Save()
