[CmdletBinding()]
param(
    [string] $GateScript
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($GateScript)) {
    $GateScript = Join-Path $PSScriptRoot `
        'full_chinese_menu_hud_completion_gate.ps1'
}

function Assert-True {
    param(
        [Parameter(Mandatory)]
        [bool] $Condition,
        [Parameter(Mandatory)]
        [string] $Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Get-OnlyEnvironment {
    $snapshot = @{}
    Get-ChildItem Env: | Where-Object {
        $_.Name -match '^ARPG_.*_ONLY$'
    } | ForEach-Object {
        $snapshot[$_.Name] = $_.Value
    }
    return $snapshot
}

function Set-OnlyEnvironment {
    param([Parameter(Mandatory)][hashtable] $Values)

    Get-ChildItem Env: | Where-Object {
        $_.Name -match '^ARPG_.*_ONLY$'
    } | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach ($entry in $Values.GetEnumerator()) {
        Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value
    }
}

function Assert-EnvironmentEquals {
    param(
        [Parameter(Mandatory)][hashtable] $Expected,
        [Parameter(Mandatory)][hashtable] $Actual,
        [Parameter(Mandatory)][string] $Label
    )

    Assert-True ($Expected.Count -eq $Actual.Count) `
        "$Label count mismatch: expected $($Expected.Count), actual $($Actual.Count)"
    foreach ($entry in $Expected.GetEnumerator()) {
        Assert-True ($Actual.ContainsKey($entry.Key)) `
            "$Label missing $($entry.Key)"
        Assert-True ($Actual[$entry.Key] -ceq $entry.Value) `
            "$Label changed $($entry.Key)"
    }
}

$resolvedGate = (Resolve-Path -LiteralPath $GateScript).Path
$tokens = $null
$parseErrors = $null
$gateAst = [System.Management.Automation.Language.Parser]::ParseFile(
    $resolvedGate, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count -ne 0) {
    throw "Gate script has parse errors: $($parseErrors.Message -join '; ')"
}

$requiredFunctions = @(
    'Invoke-PlatformSelector',
    'Invoke-ExactCTest',
    'Invoke-GitDiffChecks'
)
$loadedFunctions = @{}
foreach ($name in $requiredFunctions) {
    $node = $gateAst.Find({
        param($candidate)
        $candidate -is [System.Management.Automation.Language.FunctionDefinitionAst] `
            -and $candidate.Name -ceq $name
    }, $true)
    if ($null -ne $node) {
        . ([scriptblock]::Create($node.Extent.Text))
        $loadedFunctions[$name] = $true
    }
}

$script:FakeMode = ''
$script:FakeInvocations = [System.Collections.Generic.List[object]]::new()
$script:ObservedOnlyNames = @()
$script:FailOnRegex = ''

function Invoke-ArpgNative {
    param(
        [Parameter(Mandatory)]
        [string] $FilePath,
        [string[]] $ArgumentList = @()
    )

    $script:FakeInvocations.Add([pscustomobject]@{
        FilePath = $FilePath
        Arguments = @($ArgumentList)
    })

    if ($script:FakeMode -eq 'platform-throw') {
        throw 'fixture platform failure'
    }
    if ($script:FakeMode -like 'platform-*') {
        $script:ObservedOnlyNames = @(Get-ChildItem Env: | Where-Object {
            $_.Name -match '^ARPG_.*_ONLY$'
        } | ForEach-Object { $_.Name } | Sort-Object)
        if ($script:FakeMode -eq 'platform-wrong-summary') {
            Write-Output '182 cases, 0 failures'
            return
        }
        if (Test-Path Env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY) {
            Write-Output '15 cases, 0 failures'
            return
        }
        if (Test-Path Env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY) {
            Write-Output '183 cases, 0 failures'
            return
        }
        if (Test-Path Env:ARPG_FULL_CHINESE_UI_MENU_HUD_COMPLETION_ONLY) {
            Write-Output '86 cases, 0 failures'
            return
        }
        Write-Output '670 cases, 2 failures'
        return
    }

    if ($script:FakeMode -eq 'record-ctest' `
            -and -not [string]::IsNullOrEmpty($script:FailOnRegex) `
            -and $ArgumentList -contains $script:FailOnRegex) {
        throw "fixture ctest failure: $($script:FailOnRegex)"
    }
}

$failures = [System.Collections.Generic.List[string]]::new()
function Invoke-SelfTest {
    param(
        [Parameter(Mandatory)][string] $Name,
        [Parameter(Mandatory)][scriptblock] $Body
    )

    try {
        & $Body
        Write-Output "PASS: $Name"
    }
    catch {
        $failures.Add("${Name}: $($_.Exception.Message)")
        Write-Output "FAIL: $Name — $($_.Exception.Message)"
    }
}

$originalOnlyEnvironment = Get-OnlyEnvironment
try {
    Invoke-SelfTest 'selectors isolate every ARPG *_ONLY and restore on success' {
        Assert-True $loadedFunctions.ContainsKey('Invoke-PlatformSelector') `
            'Invoke-PlatformSelector is missing'
        $fixture = @{
            ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY = 'foundation-before'
            ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY = 'menu-before'
            ARPG_FULL_CHINESE_UI_MENU_HUD_COMPLETION_ONLY = 'completion-before'
            ARPG_CPLAY999_FIXTURE_ONLY = 'custom-before'
        }
        foreach ($case in @(
            @{ Selector = 'ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY'; Count = 183 },
            @{ Selector = 'ARPG_FULL_CHINESE_UI_MENU_HUD_COMPLETION_ONLY'; Count = 86 }
        )) {
            Set-OnlyEnvironment $fixture
            $script:FakeMode = 'platform-success'
            $script:FakeInvocations.Clear()
            $script:ObservedOnlyNames = @()
            Invoke-PlatformSelector -Executable 'fixture-platform.exe' `
                -Selector $case.Selector -ExpectedCaseCount $case.Count | Out-Null
            Assert-True ($script:ObservedOnlyNames.Count -eq 1) `
                'selector invocation observed more than one ARPG *_ONLY variable'
            Assert-True ($script:ObservedOnlyNames[0] -ceq $case.Selector) `
                "selector invocation observed $($script:ObservedOnlyNames -join ', ')"
            Assert-EnvironmentEquals $fixture (Get-OnlyEnvironment) `
                'success restoration'
        }
    }

    Invoke-SelfTest 'selector restores every ARPG *_ONLY after exception' {
        $fixture = @{
            ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY = 'foundation-before'
            ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY = 'menu-before'
            ARPG_FULL_CHINESE_UI_MENU_HUD_COMPLETION_ONLY = 'completion-before'
            ARPG_CPLAY999_FIXTURE_ONLY = 'custom-before'
        }
        Set-OnlyEnvironment $fixture
        $script:FakeMode = 'platform-throw'
        $script:FakeInvocations.Clear()
        $threw = $false
        try {
            Invoke-PlatformSelector -Executable 'fixture-platform.exe' `
                -Selector 'ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY' `
                -ExpectedCaseCount 183 | Out-Null
        }
        catch {
            $threw = $true
        }
        Assert-True $threw 'fixture exception was swallowed'
        Assert-True ($script:FakeInvocations.Count -eq 1) `
            'fixture executable was not reached before the exception'
        Assert-EnvironmentEquals $fixture (Get-OnlyEnvironment) `
            'exception restoration'
    }

    Invoke-SelfTest 'selector rejects a successful process with wrong case summary' {
        Set-OnlyEnvironment @{}
        $script:FakeMode = 'platform-wrong-summary'
        $script:FakeInvocations.Clear()
        $threw = $false
        try {
            Invoke-PlatformSelector -Executable 'fixture-platform.exe' `
                -Selector 'ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY' `
                -ExpectedCaseCount 183 | Out-Null
        }
        catch {
            $threw = $true
        }
        Assert-True $threw 'wrong case summary was accepted'
        Assert-True ($script:FakeInvocations.Count -eq 1) `
            'wrong-summary fixture executable was not reached'
        Assert-EnvironmentEquals @{} (Get-OnlyEnvironment) `
            'wrong-summary restoration'
    }

    $guardNames = @(
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
    $guardRegexes = @(
        '^platform\.pause_menu_state_boundary$',
        '^platform\.pause_menu_host_layout_source$',
        '^platform\.hud_notice_structured_binding_source$',
        '^platform\.cpp_source_function_body_self_test$',
        '^platform\.hud_notice_structured_binding_source_self_test$',
        '^platform\.hud_renderer_structured_model_source$',
        '^platform\.hud_renderer_structured_model_source_self_test$',
        '^platform\.hole_prompt_binding_source$',
        '^platform\.hole_prompt_binding_source_self_test$',
        '^platform\.task6_recovery_screen_source_guard$',
        '^platform\.task6_recovery_screen_source_guard_self_test$',
        '^platform\.task7_debug_overlay_source_guard$',
        '^platform\.task7_debug_overlay_source_guard_self_test$'
    )

    Invoke-SelfTest 'all thirteen guards run as exact fail-closed CTest calls' {
        Assert-True $loadedFunctions.ContainsKey('Invoke-ExactCTest') `
            'Invoke-ExactCTest is missing'
        $script:FakeMode = 'record-ctest'
        $script:FailOnRegex = ''
        $script:FakeInvocations.Clear()
        Invoke-ExactCTest -CTestExecutable 'ctest.exe' `
            -Preset 'windows-msvc-debug' -TestNames $guardNames
        Assert-True ($script:FakeInvocations.Count -eq 13) `
            "expected 13 CTest calls, got $($script:FakeInvocations.Count)"
        for ($index = 0; $index -lt 13; ++$index) {
            $actual = $script:FakeInvocations[$index]
            $expectedArguments = @(
                '--preset', 'windows-msvc-debug',
                '--output-on-failure', '--no-tests=error',
                '-R', $guardRegexes[$index], '--parallel', '1'
            )
            Assert-True ($actual.FilePath -ceq 'ctest.exe') `
                "guard $index used the wrong executable"
            Assert-True (($actual.Arguments -join "`n") -ceq `
                    ($expectedArguments -join "`n")) `
                "guard $index arguments were not exact and fail-closed"
        }
    }

    Invoke-SelfTest 'launcher units and smoke run as exact fail-closed CTest calls' {
        $script:FakeMode = 'record-ctest'
        $script:FailOnRegex = ''
        $script:FakeInvocations.Clear()
        Invoke-ExactCTest -CTestExecutable 'ctest.exe' `
            -Preset 'windows-msvc-debug' `
            -TestNames @('launcher.units', 'launcher.window_smoke')
        $expectedRegexes = @('^launcher\.units$', '^launcher\.window_smoke$')
        Assert-True ($script:FakeInvocations.Count -eq 2) `
            'launcher CTest call count mismatch'
        for ($index = 0; $index -lt 2; ++$index) {
            $arguments = $script:FakeInvocations[$index].Arguments
            Assert-True ($arguments -contains '--no-tests=error') `
                "launcher call $index omitted --no-tests=error"
            Assert-True (($arguments[$arguments.IndexOf('-R') + 1]) -ceq `
                    $expectedRegexes[$index]) `
                "launcher call $index regex is not exact"
        }
    }

    Invoke-SelfTest 'an individual missing or failing CTest aborts the sequence' {
        $script:FakeMode = 'record-ctest'
        $script:FailOnRegex = '^platform\.hud_notice_structured_binding_source$'
        $script:FakeInvocations.Clear()
        $threw = $false
        try {
            Invoke-ExactCTest -CTestExecutable 'ctest.exe' `
                -Preset 'windows-msvc-debug' -TestNames $guardNames
        }
        catch {
            $threw = $true
        }
        Assert-True $threw 'individual CTest failure was swallowed'
        Assert-True ($script:FakeInvocations.Count -eq 3) `
            'CTest sequence did not stop at the failing exact test'
    }

    Invoke-SelfTest 'git checks cover both working tree and cached diff' {
        Assert-True $loadedFunctions.ContainsKey('Invoke-GitDiffChecks') `
            'Invoke-GitDiffChecks is missing'
        $script:FakeMode = 'record-git'
        $script:FakeInvocations.Clear()
        Invoke-GitDiffChecks -GitExecutable 'git.exe'
        Assert-True ($script:FakeInvocations.Count -eq 2) `
            'git diff check call count mismatch'
        Assert-True (($script:FakeInvocations[0].Arguments -join ' ') -ceq `
                'diff --check') 'working-tree diff check is missing'
        Assert-True (($script:FakeInvocations[1].Arguments -join ' ') -ceq `
                'diff --cached --check') 'cached diff check is missing'
    }
}
finally {
    Set-OnlyEnvironment $originalOnlyEnvironment
}

if ($failures.Count -ne 0) {
    throw "Completion gate self-test failures: $($failures -join '; ')"
}

Write-Output 'Completion gate self-test passed'
