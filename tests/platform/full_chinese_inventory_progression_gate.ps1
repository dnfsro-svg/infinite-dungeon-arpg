[CmdletBinding()]
param([ValidateSet('windows-msvc-debug')][string] $Preset = 'windows-msvc-debug')
$ErrorActionPreference = 'Stop'

function Invoke-ArpgNativeCapture {
    param([Parameter(Mandatory)][string] $FilePath, [string[]] $ArgumentList = @())
    $output = @(& $FilePath @ArgumentList 2>&1)
    [pscustomobject]@{ Output = $output; ExitCode = $LASTEXITCODE }
}
function Invoke-ArpgNative {
    param([Parameter(Mandatory)][string] $FilePath, [string[]] $ArgumentList = @())
    $result = Invoke-ArpgNativeCapture -FilePath $FilePath -ArgumentList $ArgumentList
    if ($result.ExitCode -ne 0) { throw "$FilePath failed with exit code $($result.ExitCode)`n$($result.Output -join "`n")" }
    $result.Output
}
function Get-OnlyEnvironment {
    $saved = @{}
    Get-ChildItem Env: | Where-Object { $_.Name -match '^ARPG_.*_ONLY$' } | ForEach-Object { $saved[$_.Name] = $_.Value }
    $saved
}
function Restore-OnlyEnvironment([hashtable] $Saved) {
    Get-ChildItem Env: | Where-Object { $_.Name -match '^ARPG_.*_ONLY$' } | ForEach-Object { Remove-Item -LiteralPath "Env:$($_.Name)" }
    foreach ($entry in $Saved.GetEnumerator()) { Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value }
}
function Invoke-PlatformSelector {
    param([Parameter(Mandatory)][string] $Executable, [Parameter(Mandatory)][string] $Selector, [Parameter(Mandatory)][int] $ExpectedCaseCount)
    if ($Selector -notmatch '^ARPG_.*_ONLY$') { throw "Invalid selector: $Selector" }
    $saved = Get-OnlyEnvironment
    try {
        Restore-OnlyEnvironment @{}
        Set-Item -LiteralPath "Env:$Selector" -Value '1'
        $output = @(Invoke-ArpgNative -FilePath $Executable)
        $summaries = @($output | ForEach-Object { $_.ToString().TrimEnd() } | Where-Object { $_ -match '^[0-9]+ cases, [0-9]+ failures$' })
        $expected = "$ExpectedCaseCount cases, 0 failures"
        if ($summaries.Count -ne 1 -or $summaries[0] -cne $expected) { throw "Selector $Selector expected one '$expected'; observed '$($summaries -join '; ')'" }
        $output
    } finally { Restore-OnlyEnvironment $saved }
}
function Invoke-ExactCTest {
    param([Parameter(Mandatory)][string] $CTestExecutable, [Parameter(Mandatory)][string] $Preset, [Parameter(Mandatory)][string[]] $TestNames)
    foreach ($name in $TestNames) {
        if ([string]::IsNullOrWhiteSpace($name)) { throw 'Exact CTest name cannot be empty' }
        Invoke-ArpgNative -FilePath $CTestExecutable -ArgumentList @('--preset',$Preset,'--output-on-failure','--no-tests=error','--parallel','1','-R',('^' + [regex]::Escape($name) + '$')) | Write-Output
    }
}
function Invoke-GitDiffChecks([Parameter(Mandatory)][string] $GitExecutable) {
    Invoke-ArpgNative -FilePath $GitExecutable -ArgumentList @('diff','--check') | Out-Null
    Invoke-ArpgNative -FilePath $GitExecutable -ArgumentList @('diff','--cached','--check') | Out-Null
}
function Invoke-InventorySelectorSelfTest([Parameter(Mandatory)][string] $ScriptPath) {
    Invoke-ArpgNative -FilePath 'powershell.exe' -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',$ScriptPath) | Write-Output
}
function Invoke-ExpectedPlatformBaseline([Parameter(Mandatory)][string] $Executable) {
    $saved = Get-OnlyEnvironment
    try {
        Restore-OnlyEnvironment @{}
        $result = Invoke-ArpgNativeCapture -FilePath $Executable
        $output = @($result.Output); $exitCode = $result.ExitCode
        $summaries = @($output | ForEach-Object { $_.ToString().TrimEnd() } | Where-Object { $_ -match '^[0-9]+ cases, [0-9]+ failures$' })
        $failLines = @($output | ForEach-Object { $_.ToString().TrimEnd() } | Where-Object { $_ -match '^\[FAIL\]' })
        $hasEcology = @($failLines | Where-Object { $_ -match '^\[FAIL\] ecology_material_coverage\.ecology props match emitted layout records: definition->alpha_bounds\.x == record\.alpha_bbox\[0\] \(.+ecology_material_coverage_tests\.cpp:1163\)$' }).Count -eq 1
        $hasPreservedCount = @($failLines | Where-Object { $_ -match 'actual 676 != expected 675' }).Count -eq 1
        if ($exitCode -eq 0 -or $summaries.Count -ne 1 -or $summaries[0] -cne '676 cases, 2 failures' -or $failLines.Count -ne 2 -or -not $hasEcology -or -not $hasPreservedCount) {
            throw "Full platform baseline changed or gained failures (exit $exitCode): $($summaries -join '; ') / $($failLines -join '; ')"
        }
        $output | Write-Output
    } finally { Restore-OnlyEnvironment $saved }
}
function Invoke-GateConfigure([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Preset) {
    . (Join-Path $Root 'scripts\Configure.ps1') -Preset $Preset
}
function Assert-InventoryProgressionRecipe {
    param([string[]] $BuildTargets, [object[]] $Selectors, [string[]] $Guards)
    $expectedBuild = @('arpg_platform_tests','arpg_passive_tests','arpg_stage12_material_formal','arpg_stage16_loot_reinforcement_game_validation','arpg_stage17_skill_stones_game_validation')
    $expectedSelectors = @('ARPG_FULL_CHINESE_UI_INVENTORY_ONLY:120','ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY:15','ARPG_CPLAY015_PASSIVE_FEEDBACK_ONLY:33','ARPG_CPLAY021_LOOT_SUCTION_ONLY:12','ARPG_CPLAY021_LOOT_SUCTION_RUNTIME_ONLY:1','ARPG_CPLAY022_SKILL_EMPTY_SLOT_ONLY:6','ARPG_CPLAY027_INVENTORY_BINDING_LABEL_ONLY:16','ARPG_CPLAY032_DEATH_RETRY_HINT_ONLY:16','ARPG_CPLAY033_PASSIVE_CJK_ONLY:16','ARPG_CPLAY034_PASSIVE_FEEDBACK_ONLY:8')
    $expectedGuards = @('platform.full_chinese_inventory_renderer_source','platform.full_chinese_inventory_renderer_source_self_test','platform.full_chinese_ground_loot_renderer_source','platform.full_chinese_ground_loot_renderer_source_self_test','platform.full_chinese_passive_tree_renderer_source','platform.full_chinese_passive_tree_renderer_source_self_test','platform.full_chinese_death_overlay_renderer_source','platform.full_chinese_death_overlay_renderer_source_self_test','platform.full_chinese_inventory_progression_source','platform.full_chinese_inventory_progression_source_self_test','platform.passive_tree_binding_label_source','platform.passive_tree_feedback_source')
    $actualSelectors = @($Selectors | ForEach-Object { "$($_[0]):$($_[1])" })
    if (($BuildTargets -join '|') -cne ($expectedBuild -join '|') -or ($actualSelectors -join '|') -cne ($expectedSelectors -join '|') -or ($Guards -join '|') -cne ($expectedGuards -join '|')) { throw 'aggregate gate recipe is incomplete or reordered' }
}
function Invoke-InventoryProgressionRecipe {
    param([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Preset, [Parameter(Mandatory)][string] $Platform)
    $buildTargets = @('arpg_platform_tests','arpg_passive_tests','arpg_stage12_material_formal','arpg_stage16_loot_reinforcement_game_validation','arpg_stage17_skill_stones_game_validation')
    $selectors = @(@('ARPG_FULL_CHINESE_UI_INVENTORY_ONLY',120),@('ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY',15),@('ARPG_CPLAY015_PASSIVE_FEEDBACK_ONLY',33),@('ARPG_CPLAY021_LOOT_SUCTION_ONLY',12),@('ARPG_CPLAY021_LOOT_SUCTION_RUNTIME_ONLY',1),@('ARPG_CPLAY022_SKILL_EMPTY_SLOT_ONLY',6),@('ARPG_CPLAY027_INVENTORY_BINDING_LABEL_ONLY',16),@('ARPG_CPLAY032_DEATH_RETRY_HINT_ONLY',16),@('ARPG_CPLAY033_PASSIVE_CJK_ONLY',16),@('ARPG_CPLAY034_PASSIVE_FEEDBACK_ONLY',8))
    $guards = @('platform.full_chinese_inventory_renderer_source','platform.full_chinese_inventory_renderer_source_self_test','platform.full_chinese_ground_loot_renderer_source','platform.full_chinese_ground_loot_renderer_source_self_test','platform.full_chinese_passive_tree_renderer_source','platform.full_chinese_passive_tree_renderer_source_self_test','platform.full_chinese_death_overlay_renderer_source','platform.full_chinese_death_overlay_renderer_source_self_test','platform.full_chinese_inventory_progression_source','platform.full_chinese_inventory_progression_source_self_test','platform.passive_tree_binding_label_source','platform.passive_tree_feedback_source')
    Assert-InventoryProgressionRecipe $buildTargets $selectors $guards
    Invoke-GateConfigure $Root $Preset
    Invoke-ArpgNative -FilePath 'cmake.exe' -ArgumentList (@('--build','--preset',$Preset,'--target') + $buildTargets + @('--parallel','2')) | Write-Output
    foreach($item in $selectors) { Invoke-PlatformSelector $Platform $item[0] $item[1] | Write-Output }
    Invoke-ExactCTest 'ctest.exe' $Preset @('passives.units')
    Invoke-ExactCTest 'ctest.exe' $Preset $guards
    Invoke-ArpgNative -FilePath 'powershell.exe' -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $global:GateScriptRoot 'full_chinese_menu_hud_completion_gate.ps1'),'-Preset',$Preset) | Write-Output
    Invoke-ExpectedPlatformBaseline $Platform
    Invoke-GitDiffChecks 'git.exe'
}
function Invoke-InventoryProgressionGate {
    param([Parameter(Mandatory)][string] $Root, [Parameter(Mandatory)][string] $Preset, [Parameter(Mandatory)][string] $Platform, [Parameter(Mandatory)][string] $GateSelfTest, [Parameter(Mandatory)][string] $SelectorSelfTest)
    Invoke-ArpgNative -FilePath 'powershell.exe' -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',$GateSelfTest,'-GateScript',$PSCommandPath) | Write-Output
    Invoke-InventorySelectorSelfTest $SelectorSelfTest
    Invoke-InventoryProgressionRecipe $Root $Preset $Platform
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$global:GateScriptRoot = $PSScriptRoot
$platform = Join-Path $root "out\build\$Preset\bin\arpg_platform_tests.exe"
Push-Location $root
try {
    Invoke-InventoryProgressionGate $root $Preset $platform (Join-Path $PSScriptRoot 'full_chinese_inventory_progression_gate_self_test.ps1') (Join-Path $PSScriptRoot 'full_chinese_inventory_selector_self_test.ps1')
} finally { Pop-Location }
