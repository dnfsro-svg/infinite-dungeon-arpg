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
        [string] $Selector
    )

    $selectors = @(
        'ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY',
        'ARPG_FULL_CHINESE_UI_MENU_HUD_COMPLETION_ONLY'
    )
    $saved = @{}
    foreach ($name in $selectors) {
        $item = Get-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
        if ($null -ne $item) {
            $saved[$name] = $item.Value
        }
        Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
    }

    try {
        Set-Item -LiteralPath "Env:$Selector" -Value '1'
        Invoke-ArpgNative -FilePath $Executable
    }
    finally {
        foreach ($name in $selectors) {
            Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
            if ($saved.ContainsKey($name)) {
                Set-Item -LiteralPath "Env:$name" -Value $saved[$name]
            }
        }
    }
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$launcherGuard = Join-Path $root 'tests\launcher\launcher_window_smoke_test.ps1'
$platformTests = Join-Path $root "out\build\$Preset\bin\arpg_platform_tests.exe"

Push-Location $root
try {
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
        -Selector 'ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY'
    Invoke-PlatformSelector -Executable $platformTests `
        -Selector 'ARPG_FULL_CHINESE_UI_MENU_HUD_COMPLETION_ONLY'

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
    $guardRegex = '^(' + (($guardTests | ForEach-Object {
        [regex]::Escape($_)
    }) -join '|') + ')$'
    Invoke-ArpgNative -FilePath 'ctest.exe' -ArgumentList @(
        '--preset', $Preset,
        '--output-on-failure',
        '-R', $guardRegex,
        '--parallel', '1'
    )

    Invoke-ArpgNative -FilePath 'ctest.exe' -ArgumentList @(
        '--preset', $Preset,
        '--output-on-failure',
        '-R', '^launcher\.units$',
        '--parallel', '1'
    )
    Invoke-ArpgNative -FilePath 'powershell.exe' -ArgumentList @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $launcherGuard,
        '-SelfTestChineseCopyGuard'
    )
    Invoke-ArpgNative -FilePath 'ctest.exe' -ArgumentList @(
        '--preset', $Preset,
        '--output-on-failure',
        '-R', '^launcher\.window_smoke$',
        '--parallel', '1'
    )

    Invoke-ArpgNative -FilePath 'git.exe' -ArgumentList @('diff', '--check')
}
finally {
    Pop-Location
}
