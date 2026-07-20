param([Parameter(Mandatory = $true)][string]$EvidenceDirectory)
$ErrorActionPreference = 'Stop'
$path = Join-Path $EvidenceDirectory 'stage15-audio-evidence.txt'
if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "missing evidence: $path" }
$lines = @(Get-Content -LiteralPath $path -Encoding UTF8)
$fields = @{}
$assets = @($lines | Where-Object { $_.StartsWith('asset|') })
foreach ($line in $lines | Where-Object { -not $_.Contains('|') -and $_.Contains('=') }) {
    $pair = $line -split '=', 2
    if ($fields.ContainsKey($pair[0])) { throw "duplicate field: $($pair[0])" }
    $fields[$pair[0]] = $pair[1]
}
$required = @('schema','asset_count','stream_count','ui_count','audio_device',
    'file_total_bytes','file_budget_limit','playing_streams','valid_ui',
    'fallback_count','scene_route','result')
foreach ($key in $required) { if (-not $fields.ContainsKey($key)) { throw "missing field: $key" } }
if ($fields.schema -ne 'stage15-audio-evidence-v1' -or $fields.result -ne 'PASS' -or
        $fields.audio_device -ne 'ready' -or $fields.scene_route -ne 'PASS') { throw 'evidence result rejected' }
if ([int]$fields.asset_count -ne 10 -or $assets.Count -ne 10 -or
        [int]$fields.stream_count -ne 4 -or [int]$fields.ui_count -ne 6 -or
        [int]$fields.playing_streams -ne 4 -or [int]$fields.valid_ui -ne 6 -or
        [int]$fields.fallback_count -ne 0) { throw 'asset/playback counts rejected' }
if ([uint64]$fields.file_budget_limit -ne 16777216 -or
        [uint64]$fields.file_total_bytes -gt [uint64]$fields.file_budget_limit) { throw 'file budget rejected' }
$expected = @('music_explore','music_combat','ambience_room','ambience_abyss',
    'ui_navigate','ui_confirm','ui_cancel','ui_open','ui_close','ui_reward')
[uint64]$sum = 0
for ($i = 0; $i -lt $assets.Count; ++$i) {
    $columns = $assets[$i] -split '\|', -1
    if ($columns.Count -ne 10 -or $columns[0] -ne 'asset' -or $columns[1] -ne $expected[$i]) {
        throw "malformed or out-of-order asset $i"
    }
    if ([uint64]$columns[4] -eq 0 -or [uint64]$columns[5] -eq 0 -or
            [uint64]$columns[6] -eq 0 -or [uint64]$columns[7] -eq 0 -or
            [double]$columns[8] -le 0) { throw "invalid metadata at $i" }
    if (($i -lt 4 -and ($columns[3] -ne 'stream' -or $columns[9] -ne 'PLAY')) -or
            ($i -ge 4 -and ($columns[3] -ne 'ui' -or $columns[9] -ne 'VALID'))) {
        throw "decode/playback state rejected at $i"
    }
    $sum += [uint64]$columns[4]
}
if ($sum -ne [uint64]$fields.file_total_bytes) { throw 'file byte total mismatch' }
Write-Output '[stage15-audio-evidence] PASS'
