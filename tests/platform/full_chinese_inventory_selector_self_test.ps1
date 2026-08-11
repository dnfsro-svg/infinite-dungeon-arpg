[CmdletBinding()]
param(
    [string] $SelectorScript
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($SelectorScript)) {
    $SelectorScript = Join-Path $PSScriptRoot `
        'full_chinese_inventory_selector.ps1'
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
        Assert-True $Actual.ContainsKey($entry.Key) `
            "$Label missing $($entry.Key)"
        Assert-True ($Actual[$entry.Key] -ceq $entry.Value) `
            "$Label changed $($entry.Key)"
    }
}

$resolvedSelector = (Resolve-Path -LiteralPath $SelectorScript).Path
$tokens = $null
$parseErrors = $null
$selectorAst = [System.Management.Automation.Language.Parser]::ParseFile(
    $resolvedSelector, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count -ne 0) {
    throw "Selector script has parse errors: $($parseErrors.Message -join '; ')"
}

$selectorFunction = $selectorAst.Find({
    param($candidate)
    $candidate -is `
        [System.Management.Automation.Language.FunctionDefinitionAst] `
        -and $candidate.Name -ceq 'Invoke-FullChineseInventorySelector'
}, $true)
if ($null -eq $selectorFunction) {
    throw 'Invoke-FullChineseInventorySelector is missing'
}
. ([scriptblock]::Create($selectorFunction.Extent.Text))

$script:FakeMode = ''
$script:ObservedOnlyNames = @()
$script:FakeInvocationCount = 0

function Invoke-InventorySelectorProcess {
    param([Parameter(Mandatory)][string] $ExecutablePath)

    ++$script:FakeInvocationCount
    $script:ObservedOnlyNames = @(Get-ChildItem Env: | Where-Object {
        $_.Name -match '^ARPG_.*_ONLY$'
    } | ForEach-Object { $_.Name } | Sort-Object)

    switch ($script:FakeMode) {
        'success' {
            return [pscustomobject]@{
                Output = @('fixture detail', '120 cases, 0 failures')
                ExitCode = 0
            }
        }
        'nonzero' {
            return [pscustomobject]@{
                Output = @('120 cases, 0 failures')
                ExitCode = 9
            }
        }
        'failure-summary' {
            return [pscustomobject]@{
                Output = @('120 cases, 1 failures')
                ExitCode = 0
            }
        }
        'wrong-summary' {
            return [pscustomobject]@{
                Output = @('119 cases, 0 failures')
                ExitCode = 0
            }
        }
        'duplicate-summary' {
            return [pscustomobject]@{
                Output = @(
                    '120 cases, 0 failures',
                    '120 cases, 0 failures'
                )
                ExitCode = 0
            }
        }
        'mixed-summary' {
            return [pscustomobject]@{
                Output = @(
                    '119 cases, 0 failures',
                    '120 cases, 0 failures'
                )
                ExitCode = 0
            }
        }
        'throw' {
            throw 'fixture process exception'
        }
        default {
            throw "Unknown fake mode: $($script:FakeMode)"
        }
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
        Write-Output "FAIL: $Name - $($_.Exception.Message)"
    }
}

$originalOnlyEnvironment = Get-OnlyEnvironment
$fixtureEnvironment = @{
    ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY = 'foundation-before'
    ARPG_FULL_CHINESE_UI_INVENTORY_ONLY = 'inventory-before'
    ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY = 'menu-before'
    ARPG_CPLAY999_FIXTURE_ONLY = 'custom-before'
}
try {
    Invoke-SelfTest `
        'success isolates selector and restores every ARPG *_ONLY variable' {
        Set-OnlyEnvironment $fixtureEnvironment
        $script:FakeMode = 'success'
        $script:FakeInvocationCount = 0
        $output = @(Invoke-FullChineseInventorySelector `
            -ExecutablePath 'fixture-platform.exe')
        Assert-True ($script:FakeInvocationCount -eq 1) `
            'fake runner was not called exactly once'
        Assert-True ($script:ObservedOnlyNames.Count -eq 1) `
            'selector process observed more than one ARPG *_ONLY variable'
        Assert-True ($script:ObservedOnlyNames[0] -ceq `
                'ARPG_FULL_CHINESE_UI_INVENTORY_ONLY') `
            "selector process observed $($script:ObservedOnlyNames -join ', ')"
        Assert-True ($output[-1] -ceq '120 cases, 0 failures') `
            'accepted output did not preserve the exact summary'
        Assert-EnvironmentEquals $fixtureEnvironment (Get-OnlyEnvironment) `
            'success restoration'
    }

    foreach ($mode in @(
        'nonzero',
        'failure-summary',
        'wrong-summary',
        'duplicate-summary',
        'mixed-summary',
        'throw'
    )) {
        Invoke-SelfTest "$mode fails closed and restores the environment" {
            Set-OnlyEnvironment $fixtureEnvironment
            $script:FakeMode = $mode
            $script:FakeInvocationCount = 0
            $threw = $false
            try {
                Invoke-FullChineseInventorySelector `
                    -ExecutablePath 'fixture-platform.exe' | Out-Null
            }
            catch {
                $threw = $true
            }
            Assert-True $threw "$mode was accepted"
            Assert-True ($script:FakeInvocationCount -eq 1) `
                "$mode did not reach the fake runner exactly once"
            Assert-EnvironmentEquals $fixtureEnvironment `
                (Get-OnlyEnvironment) "$mode restoration"
        }
    }
}
finally {
    Set-OnlyEnvironment $originalOnlyEnvironment
}

if ($failures.Count -ne 0) {
    throw "Inventory selector self-test failures: $($failures -join '; ')"
}

Write-Output 'Inventory selector self-test passed'
