[CmdletBinding()]
param(
    [ValidateSet('windows-msvc-debug')]
    [string] $Preset = 'windows-msvc-debug'
)

$ErrorActionPreference = 'Stop'

function Invoke-ArpgNative {
    param(
        [Parameter(Mandatory)]
        [string] $FilePath,
        [string[]] $ArgumentList = @()
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

function Invoke-PlatformSelector {
    param(
        [Parameter(Mandatory)]
        [string] $Executable,
        [Parameter(Mandatory)]
        [string] $Selector,
        [Parameter(Mandatory)]
        [ValidateRange(1, [int]::MaxValue)]
        [int] $ExpectedCaseCount
    )

    if ($Selector -notmatch '^ARPG_.*_ONLY$') {
        throw "Invalid focused-test selector: $Selector"
    }

    $saved = @{}
    Get-ChildItem Env: | Where-Object {
        $_.Name -match '^ARPG_.*_ONLY$'
    } | ForEach-Object {
        $saved[$_.Name] = $_.Value
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }

    try {
        Set-Item -LiteralPath "Env:$Selector" -Value '1'
        $output = @(Invoke-ArpgNative -FilePath $Executable)
        $summaryLines = @($output | ForEach-Object {
            $_.ToString().TrimEnd()
        } | Where-Object {
            $_ -match '^[0-9]+ cases, [0-9]+ failures$'
        })
        $expectedSummary = "$ExpectedCaseCount cases, 0 failures"
        if ($summaryLines.Count -ne 1 `
                -or $summaryLines[0] -cne $expectedSummary) {
            throw "Focused selector $Selector expected exact summary '$expectedSummary'; observed '$($summaryLines -join '; ')'"
        }
        $output | Write-Output
    }
    finally {
        Get-ChildItem Env: | Where-Object {
            $_.Name -match '^ARPG_.*_ONLY$'
        } | ForEach-Object {
            Remove-Item -LiteralPath "Env:$($_.Name)"
        }
        foreach ($entry in $saved.GetEnumerator()) {
            Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value
        }
    }
}

function Invoke-ExactCTest {
    param(
        [Parameter(Mandatory)]
        [string] $CTestExecutable,
        [Parameter(Mandatory)]
        [string] $Preset,
        [Parameter(Mandatory)]
        [ValidateNotNullOrEmpty()]
        [string[]] $TestNames
    )

    foreach ($name in $TestNames) {
        if ([string]::IsNullOrWhiteSpace($name)) {
            throw 'Exact CTest name cannot be empty'
        }
        $exactRegex = '^' + [regex]::Escape($name) + '$'
        Invoke-ArpgNative -FilePath $CTestExecutable -ArgumentList @(
            '--preset', $Preset,
            '--output-on-failure', '--no-tests=error',
            '-R', $exactRegex,
            '--parallel', '1'
        )
    }
}

function Invoke-GitDiffChecks {
    param(
        [Parameter(Mandatory)]
        [string] $GitExecutable
    )

    Invoke-ArpgNative -FilePath $GitExecutable `
        -ArgumentList @('diff', '--check')
    Invoke-ArpgNative -FilePath $GitExecutable `
        -ArgumentList @('diff', '--cached', '--check')
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$launcherGuard = Join-Path $root 'tests\launcher\launcher_window_smoke_test.ps1'
$gateSelfTest = Join-Path $PSScriptRoot `
    'full_chinese_menu_hud_completion_gate_self_test.ps1'
$platformTests = Join-Path $root "out\build\$Preset\bin\arpg_platform_tests.exe"

Push-Location $root
try {
    Invoke-ArpgNative -FilePath 'powershell.exe' -ArgumentList @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $gateSelfTest,
        '-GateScript', $PSCommandPath
    )

    . (Join-Path $root 'scripts\Configure.ps1') -Preset $Preset

    Invoke-ArpgNative -FilePath 'cmake.exe' -ArgumentList @(
        '--build', '--preset', $Preset,
        '--target',
        'arpg_platform_tests',
        'arpg_launcher_tests',
        'arpg_launcher',
        'arpg_stage11c_hud_formal',
        '--parallel', '2'
    )

    Invoke-PlatformSelector -Executable $platformTests `
        -Selector 'ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY' `
        -ExpectedCaseCount 183
    Invoke-PlatformSelector -Executable $platformTests `
        -Selector 'ARPG_FULL_CHINESE_UI_MENU_HUD_COMPLETION_ONLY' `
        -ExpectedCaseCount 86

    $guardTests = @(
        'platform.pause_menu_state_boundary',
        'platform.pause_menu_host_layout_source',
        'platform.hud_notice_structured_binding_source',
        'platform.cpp_source_function_body_self_test',
        'platform.hud_notice_structured_binding_source_self_test',
        'platform.hud_renderer_structured_model_source',
        'platform.hud_renderer_structured_model_source_self_test',
        'platform.hole_prompt_binding_source',
        'platform.hole_prompt_binding_source_self_test',
        'platform.task6_recovery_screen_source_guard',
        'platform.task6_recovery_screen_source_guard_self_test',
        'platform.task7_debug_overlay_source_guard',
        'platform.task7_debug_overlay_source_guard_self_test'
    )
    Invoke-ExactCTest -CTestExecutable 'ctest.exe' -Preset $Preset `
        -TestNames $guardTests

    Invoke-ExactCTest -CTestExecutable 'ctest.exe' -Preset $Preset `
        -TestNames @('launcher.units')
    Invoke-ArpgNative -FilePath 'powershell.exe' -ArgumentList @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $launcherGuard,
        '-SelfTestChineseCopyGuard'
    )
    Invoke-ExactCTest -CTestExecutable 'ctest.exe' -Preset $Preset `
        -TestNames @('launcher.window_smoke')

    Invoke-GitDiffChecks -GitExecutable 'git.exe'
}
finally {
    Pop-Location
}
