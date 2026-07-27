param([Parameter(Mandatory = $true)][string]$EvidenceDirectory)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$run = [System.IO.Path]::GetFullPath($EvidenceDirectory)
if ([System.IO.Path]::GetFileName($run) -ne 'stage17-run') {
    throw "evidence directory must be the controlled stage17-run directory: $run"
}
if (-not (Test-Path -LiteralPath $run -PathType Container)) {
    throw "missing evidence directory: $run"
}

$marker = Join-Path $run 'run.marker'
$statePath = Join-Path $run 'stage17-skill-stones-state.txt'
if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) {
    throw "missing run marker: $marker"
}
if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) {
    throw "missing state evidence: $statePath"
}
$markerTime = (Get-Item -LiteralPath $marker).LastWriteTimeUtc

function Read-EvidenceFields([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "missing summary evidence: $Path"
    }
    if ((Get-Item -LiteralPath $Path).LastWriteTimeUtc -le $markerTime) {
        throw "summary evidence is not newer than run marker: $Path"
    }
    $result = @{}
    foreach ($line in @(Get-Content -LiteralPath $Path -Encoding UTF8)) {
        if (-not $line.Contains('=')) { throw "malformed summary line: $line" }
        $pair = $line -split '=', 2
        if ($result.ContainsKey($pair[0])) {
            throw "duplicate summary field: $($pair[0])"
        }
        $result[$pair[0]] = $pair[1]
    }
    return $result
}

function Assert-EvidenceFields([hashtable]$Fields, $Expected) {
    foreach ($key in $Expected.Keys) {
        if (-not $Fields.ContainsKey($key)) { throw "missing summary field: $key" }
        if ($Expected[$key] -eq '<positive>') {
            [uint64]$value = 0
            if (-not [uint64]::TryParse($Fields[$key], [ref]$value) -or
                    $value -eq 0) {
                throw "summary field is not positive: $key=$($Fields[$key])"
            }
        } elseif ($Fields[$key] -cne $Expected[$key]) {
            throw "summary field rejected: $key=$($Fields[$key])"
        }
    }
}

$productionFields = Read-EvidenceFields (Join-Path $run 'production-summary.txt')
Assert-EvidenceFields $productionFields ([ordered]@{
    scenario = 'production_sequence'
    result = 'pass'
    draw_accepted = '1'
    draw_windup_captured = '1'
    draw_frame_peak = '35'
    draw_hit_count = '2'
    storm_accepted = '0'
    storm_strike_hit_count = '0'
    storm_finisher_hit_count = '0'
    draw_renderer_samples = '<positive>'
    draw_material_frame_drawn = '1'
    draw_base_player_drawn = '0'
    draw_procedural_main_visual_peak = '0'
    draw_renderer_status_valid = '1'
    public_input_path = '1'
    production_transactions = '1'
    production_inventory_closed = '1'
    production_cooldown_wait_started = '1'
    production_cooldown_start_ticks = '<positive>'
    production_cooldown_wait_ticks = '<positive>'
    production_cooldowns_zero_before_shutdown = '1'
    clean_shutdown_exact_ready = '1'
})
$stormFields = Read-EvidenceFields (Join-Path $run 'storm-summary.txt')
Assert-EvidenceFields $stormFields ([ordered]@{
    scenario = 'storm_sequence'
    result = 'pass'
    draw_accepted = '1'
    draw_hit_count = '2'
    storm_prelude_draw_hit_count = '2'
    storm_accepted = '1'
    storm_strike_hit_count = '3'
    storm_finisher_hit_count = '0'
    storm_strike_count = '12'
    storm_sword_peak = '24'
    storm_invulnerable_seen = '1'
    storm_finisher_phase_seen = '1'
    storm_aerial_captured = '1'
    active_skill_atlases_ready = '1'
    storm_renderer_samples = '<positive>'
    storm_material_frame_drawn = '1'
    storm_base_player_drawn = '0'
    storm_procedural_main_visual_peak = '0'
    storm_renderer_status_valid = '1'
    renderer_status_failure_latched = '0'
    storm_center_locked = '1'
    storm_isolation_invalidated = '0'
    storm_player_moved = '1'
    public_input_path = '1'
    production_transactions = '0'
    clean_shutdown_exact_ready = '1'
})
$restartFields = Read-EvidenceFields (Join-Path $run 'restart-summary.txt')
Assert-EvidenceFields $restartFields ([ordered]@{
    scenario = 'restarted_loadout'
    result = 'pass'
    initial_slots = 'none,draw_slash,none,none,storm_swords'
    restart_persisted = '1'
    restart_cooldowns_zero = '1'
    clean_shutdown_exact_ready = '1'
})

$images = @(
    '01-new-default-1280x720.png',
    '02-draw-slash-windup-1280x720.png',
    '03-draw-slash-hit-1280x720.png',
    '04-storm-ground-array-1280x720.png',
    '05-storm-aerial-array-1280x720.png',
    '06-storm-finisher-1280x720.png',
    '07-restarted-loadout-1280x720.png'
)
$pixelHashes = [System.Collections.Generic.HashSet[string]]::new()
$pixelHashesByImage = [ordered]@{}
foreach ($name in $images) {
    $path = Join-Path $run $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "missing PNG evidence: $path"
    }
    $file = Get-Item -LiteralPath $path
    if ($file.Length -le 4096) { throw "PNG evidence is too small: $path" }
    if ($file.LastWriteTimeUtc -le $markerTime) {
        throw "PNG evidence is not newer than run marker: $path"
    }

    $bitmap = [System.Drawing.Bitmap]::new($path)
    try {
        if ($bitmap.Width -ne 1280 -or $bitmap.Height -ne 720) {
            throw "PNG dimensions rejected for $name`: $($bitmap.Width)x$($bitmap.Height)"
        }
        $rect = [System.Drawing.Rectangle]::new(0, 0, $bitmap.Width, $bitmap.Height)
        $data = $bitmap.LockBits($rect,
            [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $length = [Math]::Abs($data.Stride) * $data.Height
            $pixels = [byte[]]::new($length)
            [System.Runtime.InteropServices.Marshal]::Copy(
                $data.Scan0, $pixels, 0, $length)
            $sha = [System.Security.Cryptography.SHA256]::Create()
            try {
                $hash = [BitConverter]::ToString(
                    $sha.ComputeHash($pixels)).Replace('-', '')
            } finally {
                $sha.Dispose()
            }
        } finally {
            $bitmap.UnlockBits($data)
        }
    } finally {
        $bitmap.Dispose()
    }
    if (-not $pixelHashes.Add($hash)) {
        throw "duplicate decoded pixel content: $name"
    }
    $pixelHashesByImage[$name] = $hash.ToLowerInvariant()
}

$lines = @(Get-Content -LiteralPath $statePath -Encoding UTF8)
$fields = @{}
foreach ($line in $lines) {
    if (-not $line.Contains('=')) { throw "malformed state line: $line" }
    $pair = $line -split '=', 2
    if ($fields.ContainsKey($pair[0])) { throw "duplicate state field: $($pair[0])" }
    $fields[$pair[0]] = $pair[1]
}
$expected = [ordered]@{
    schema = 'stage17-active-skill-rework-evidence-v3'
    result = 'PASS'
    renderer = 'raylib-6.0-opengl'
    window = '1280x720'
    fixture_path = 'production-raylib-host'
    showcase_capture_count = '0'
    save_version = '9'
    initial_slots = 'draw_slash,storm_swords,none,none,none'
    final_slots = 'none,draw_slash,none,none,storm_swords'
    restarted_slots = 'none,draw_slash,none,none,storm_swords'
    owned_active_bits = '3'
    support_none_count = '25'
    digit_1_cast = 'accepted'
    digit_2_cast = 'accepted'
    digit_3_5_effect = 'none'
    draw_windup_captured = 'true'
    draw_frame_peak = '35'
    draw_slash_frame_union = '0..35'
    draw_slash_frame_zero = 'true'
    draw_slash_frame_last = 'true'
    draw_slash_hit_count = '2'
    storm_strike_hit_count = '3'
    storm_finisher_hit_count = '0'
    storm_strike_count = '12'
    storm_sword_peak = '24'
    storm_swords_frame_union = '0..23'
    storm_swords_frame_zero = 'true'
    storm_swords_frame_last = 'true'
    storm_invulnerable_seen = 'true'
    storm_finisher_phase_seen = 'true'
    storm_aerial_captured = 'true'
    active_skill_atlases_ready = 'true'
    renderer_status_source = 'production-summary.txt,storm-summary.txt'
    draw_renderer_samples = '<positive>'
    draw_material_frame_drawn = '1'
    draw_base_player_drawn = '0'
    draw_procedural_main_visual_peak = '0'
    draw_renderer_status_valid = '1'
    storm_renderer_samples = '<positive>'
    storm_material_frame_drawn = '1'
    storm_base_player_drawn = '0'
    storm_procedural_main_visual_peak = '0'
    storm_renderer_status_valid = '1'
    renderer_status_failure_latched = '0'
    storm_center_locked = 'true'
    loadout_transactions = 'remove1,equip5,swap2_5'
    restart_persisted = 'true'
    cooldown_persisted = 'false'
    production_inventory_closed = 'true'
    production_cooldown_start_ticks = '<positive>'
    production_cooldown_wait_ticks = '<positive>'
    cooldown_naturally_elapsed = 'true'
    clean_shutdown_exact_ready = 'true'
    public_input_path = 'true'
    production_transactions = 'true'
}
if ($fields.Count -ne $expected.Count) {
    throw "state field count rejected: expected $($expected.Count), got $($fields.Count)"
}
foreach ($key in $expected.Keys) {
    if (-not $fields.ContainsKey($key)) { throw "missing state field: $key" }
    if ($expected[$key] -eq '<positive>') {
        [uint64]$sampleCount = 0
        if (-not [uint64]::TryParse($fields[$key], [ref]$sampleCount) -or
                $sampleCount -eq 0) {
            throw "state sample count rejected: $key=$($fields[$key])"
        }
    } elseif ($fields[$key] -cne $expected[$key]) {
        throw "state field rejected: $key=$($fields[$key])"
    }
}
foreach ($name in $images) {
    $path = Join-Path $run $name
    Write-Output ("[stage17-skill-stones-evidence] PNG path={0} pixel_sha256={1}" -f
        $path, $pixelHashesByImage[$name])
}
Write-Output '[stage17-skill-stones-evidence] PASS'
