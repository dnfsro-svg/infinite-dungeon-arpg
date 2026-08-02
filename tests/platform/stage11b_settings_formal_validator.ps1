param(
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDirectory
)

$ErrorActionPreference = 'Stop'
$report = Join-Path $EvidenceDirectory 'stage11b-settings-evidence.txt'
if (-not (Test-Path -LiteralPath $report)) { throw "missing formal evidence report: $report" }
$values = @{}
Get-Content -LiteralPath $report | ForEach-Object {
    $pair = $_ -split '=', 2
    if ($pair.Count -eq 2) { $values[$pair[0]] = $pair[1] }
}
foreach ($key in 'character_hash_scope','run_a_hash_before','run_a_hash_after','run_b_hash_before','run_b_hash_after','run_a_size_before','run_a_size_after','run_b_size_before','run_b_size_after','final_run_a_v9_valid','final_run_b_v9_valid','final_run_a_revision','final_run_b_revision','rebound_old_attack','rebound_new_attack','paused_tick_before','paused_tick_after','pause_capture_while_paused','resume_tick_before','resume_tick_after','player_monster_hash_before','player_monster_hash_after','committed_revision','restart_binding','single_slot_status','corrupt_default_status','corrupt_notice_visible','swap_pair','result') {
    if (-not $values.ContainsKey($key)) { throw "missing evidence field: $key" }
}
if ($values.result -ne 'pass' -or $values.character_hash_scope -ne 'settings_transaction' -or $values.final_run_a_v9_valid -ne '1' -or $values.final_run_b_v9_valid -ne '1' -or [uint64]$values.final_run_a_revision -eq 0 -or [uint64]$values.final_run_b_revision -eq 0 -or $values.rebound_old_attack -ne '0' -or [int]$values.rebound_new_attack -lt 1 -or [uint64]$values.paused_tick_before -le 0 -or $values.paused_tick_before -ne $values.paused_tick_after -or $values.pause_capture_while_paused -ne '1' -or [uint64]$values.resume_tick_after -ne ([uint64]$values.resume_tick_before + 1) -or $values.player_monster_hash_before -ne $values.player_monster_hash_after -or [uint64]$values.player_monster_hash_before -eq 0 -or [uint64]$values.committed_revision -lt 1 -or $values.restart_binding -ne 'U' -or $values.single_slot_status -ne '2' -or $values.corrupt_default_status -ne '3' -or $values.corrupt_notice_visible -ne '1' -or $values.swap_pair -ne 'K/J' -or [uint64]$values.run_a_hash_before -eq 0 -or [uint64]$values.run_b_hash_before -eq 0 -or [uint64]$values.run_a_size_before -le 0 -or [uint64]$values.run_b_size_before -le 0 -or $values.run_a_hash_before -ne $values.run_a_hash_after -or $values.run_b_hash_before -ne $values.run_b_hash_after -or $values.run_a_size_before -ne $values.run_a_size_after -or $values.run_b_size_before -ne $values.run_b_size_after) {
    throw 'formal evidence values are invalid'
}
Add-Type -AssemblyName System.Drawing
foreach ($name in 'pause.png','settings.png','rebound.png','corrupt.png') {
    $path = Join-Path $EvidenceDirectory $name
    if (-not (Test-Path -LiteralPath $path)) { throw "missing screenshot: $path" }
    $image = [System.Drawing.Image]::FromFile($path)
    try {
        if ($image.Width -ne 1280 -or $image.Height -ne 720) { throw "wrong screenshot size: $name" }
    } finally { $image.Dispose() }
}
