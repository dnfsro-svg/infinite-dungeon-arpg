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
foreach ($key in 'character_save_hash_before','character_save_hash_after','rebound_old_attack','rebound_new_attack','paused_tick_before','paused_tick_after','player_monster_hash_before','player_monster_hash_after','committed_revision','restart_binding','single_slot_status','corrupt_default_status','swap_pair','result') {
    if (-not $values.ContainsKey($key)) { throw "missing evidence field: $key" }
}
if ($values.result -ne 'pass' -or $values.rebound_old_attack -ne '0' -or [int]$values.rebound_new_attack -lt 1 -or $values.paused_tick_before -ne $values.paused_tick_after -or $values.player_monster_hash_before -ne $values.player_monster_hash_after -or [uint64]$values.player_monster_hash_before -eq 0 -or [uint64]$values.committed_revision -lt 1 -or $values.restart_binding -ne 'U' -or $values.single_slot_status -ne '2' -or $values.corrupt_default_status -ne '3' -or $values.swap_pair -ne 'K/J' -or $values.character_save_hash_before -ne $values.character_save_hash_after) {
    throw 'formal evidence values are invalid'
}
Add-Type -AssemblyName System.Drawing
foreach ($name in 'pause.png','settings.png','rebound.png') {
    $path = Join-Path $EvidenceDirectory $name
    if (-not (Test-Path -LiteralPath $path)) { throw "missing screenshot: $path" }
    $image = [System.Drawing.Image]::FromFile($path)
    try {
        if ($image.Width -ne 1280 -or $image.Height -ne 720) { throw "wrong screenshot size: $name" }
    } finally { $image.Dispose() }
}
