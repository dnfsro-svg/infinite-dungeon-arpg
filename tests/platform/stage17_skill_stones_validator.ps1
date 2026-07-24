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
    schema = 'stage17-active-skill-rework-evidence-v2'
    result = 'PASS'
    renderer = 'raylib-6.0-opengl'
    window = '1280x720'
    save_version = '8'
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
    draw_slash_hit_count = '2'
    storm_strike_hit_count = '5'
    storm_finisher_hit_count = '0'
    storm_strike_count = '12'
    storm_sword_peak = '24'
    storm_invulnerable_seen = 'true'
    storm_finisher_phase_seen = 'true'
    storm_aerial_captured = 'true'
    active_skill_atlases_ready = 'true'
    storm_center_locked = 'true'
    loadout_transactions = 'remove1,equip5,swap2_5'
    restart_persisted = 'true'
    cooldown_persisted = 'false'
    public_input_path = 'true'
    production_transactions = 'true'
}
if ($fields.Count -ne $expected.Count) {
    throw "state field count rejected: expected $($expected.Count), got $($fields.Count)"
}
foreach ($key in $expected.Keys) {
    if (-not $fields.ContainsKey($key)) { throw "missing state field: $key" }
    if ($fields[$key] -cne $expected[$key]) {
        throw "state field rejected: $key=$($fields[$key])"
    }
}
foreach ($name in $images) {
    $path = Join-Path $run $name
    Write-Output ("[stage17-skill-stones-evidence] PNG path={0} pixel_sha256={1}" -f
        $path, $pixelHashesByImage[$name])
}
Write-Output '[stage17-skill-stones-evidence] PASS'
