[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$DeployScript
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        throw $Message
    }
}

function New-ReleaseFixture([string]$Root, [bool]$IncludeSkills = $true) {
    $binDirectory = Join-Path $Root 'bin'
    New-Item -ItemType Directory -Path $binDirectory -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $Root 'CMakeCache.txt') -Value 'CMAKE_BUILD_TYPE:STRING=Release' -NoNewline

    New-Item -ItemType File -Path (Join-Path $binDirectory '无限地下城启动器.exe') -Force | Out-Null
    New-Item -ItemType File -Path (Join-Path $binDirectory 'arpg_game.exe') -Force | Out-Null
    $assetDirectories = @(
        'assets/fonts',
        'assets/player',
        'assets/stage12',
        'assets/stage14/audio',
        'assets/stage15/audio'
    )
    if ($IncludeSkills) {
        $assetDirectories += 'assets/skills'
    }
    foreach ($assetDirectory in $assetDirectories) {
        New-Item -ItemType Directory -Path (Join-Path $binDirectory $assetDirectory) -Force | Out-Null
    }
    return $binDirectory
}

function Require-InstalledLayout([string]$InstallRoot) {
    foreach ($relativePath in @(
        '无限地下城启动器.exe',
        'arpg_game.exe',
        'assets/fonts',
        'assets/player',
        'assets/skills',
        'assets/stage12',
        'assets/stage14/audio',
        'assets/stage15/audio'
    )) {
        Require (Test-Path -LiteralPath (Join-Path $InstallRoot $relativePath)) "Missing installed item: $relativePath"
    }
}

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("InfiniteDungeon-DeploySelfTest-" + [Guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $temporaryRoot -Force | Out-Null
    $completeBin = New-ReleaseFixture -Root (Join-Path $temporaryRoot 'complete')
    $installParent = Join-Path $temporaryRoot 'InfiniteDungeon'
    $installRoot = Join-Path $installParent 'app'
    $desktopRoot = Join-Path $temporaryRoot 'Desktop'
    New-Item -ItemType Directory -Path $desktopRoot -Force | Out-Null

    $saveDirectory = Join-Path $installParent 'save'
    New-Item -ItemType Directory -Path $saveDirectory -Force | Out-Null
    $saveSentinel = Join-Path $saveDirectory 'keep.txt'
    Set-Content -LiteralPath $saveSentinel -Value 'keep' -NoNewline

    & $DeployScript -GameBuildDirectory $completeBin -InstallRoot $installRoot -DesktopDirectory $desktopRoot
    Require-InstalledLayout -InstallRoot $installRoot

    $shortcutPath = Join-Path $desktopRoot '无限地下城.lnk'
    Require (Test-Path -LiteralPath $shortcutPath -PathType Leaf) 'Desktop shortcut was not created.'
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $expectedLauncher = Join-Path $installRoot '无限地下城启动器.exe'
    Require ($shortcut.TargetPath -eq $expectedLauncher) "Shortcut target differs: $($shortcut.TargetPath)"
    Require ($shortcut.WorkingDirectory -eq $installRoot) "Shortcut working directory differs: $($shortcut.WorkingDirectory)"
    Require ($shortcut.IconLocation -eq "$expectedLauncher,0") "Shortcut icon differs: $($shortcut.IconLocation)"

    $sentinelPath = Join-Path $installRoot 'sentinel.txt'
    Set-Content -LiteralPath $sentinelPath -Value 'preserve-me' -NoNewline
    $incompleteBin = New-ReleaseFixture -Root (Join-Path $temporaryRoot 'incomplete') -IncludeSkills:$false
    $invalidDeploymentFailed = $false
    try {
        & $DeployScript -GameBuildDirectory $incompleteBin -InstallRoot $installRoot -DesktopDirectory $desktopRoot
    }
    catch {
        $invalidDeploymentFailed = $true
    }
    Require $invalidDeploymentFailed 'Missing assets/skills input unexpectedly deployed.'
    Require (Test-Path -LiteralPath $sentinelPath -PathType Leaf) 'Invalid input changed the existing installed app.'
    Require ((Get-Content -LiteralPath $sentinelPath -Raw) -eq 'preserve-me') 'Invalid input changed the existing installed app sentinel.'

    $conflictedRoot = Join-Path $temporaryRoot 'conflicted-cache'
    $conflictedBin = New-ReleaseFixture -Root $conflictedRoot
    Set-Content -LiteralPath (Join-Path $conflictedRoot 'CMakeCache.txt') -Value @(
        'CMAKE_BUILD_TYPE:STRING=Release',
        'CMAKE_BUILD_TYPE:STRING=Debug'
    )
    $conflictedDeploymentFailed = $false
    try {
        & $DeployScript -GameBuildDirectory $conflictedBin -InstallRoot $installRoot -DesktopDirectory $desktopRoot
    }
    catch {
        $conflictedDeploymentFailed = $true
    }
    Require $conflictedDeploymentFailed 'Conflicting CMAKE_BUILD_TYPE assignments unexpectedly deployed.'
    Require (Test-Path -LiteralPath $sentinelPath -PathType Leaf) 'Conflicting CMakeCache input changed the existing installed app.'
    Require ((Get-Content -LiteralPath $sentinelPath -Raw) -eq 'preserve-me') 'Conflicting CMakeCache input changed the existing installed app sentinel.'

    Require (Test-Path -LiteralPath $saveDirectory -PathType Container) 'Deployment removed the existing save directory.'
    Require (Test-Path -LiteralPath $saveSentinel -PathType Leaf) 'Deployment moved or removed the existing save sentinel.'
    Require (-not (Test-Path -LiteralPath (Join-Path $installRoot 'save'))) 'Deployment created a save directory in the app layout.'
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
