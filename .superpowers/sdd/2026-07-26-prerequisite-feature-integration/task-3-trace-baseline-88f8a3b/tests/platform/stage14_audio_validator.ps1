param(
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDirectory
)

$ErrorActionPreference = 'Stop'

$ExpectedAssets = @(
    [pscustomobject]@{ Id = 'swing_light_1'; Path = 'assets/stage14/audio/swing-light-1.wav' },
    [pscustomobject]@{ Id = 'swing_light_2'; Path = 'assets/stage14/audio/swing-light-2.wav' },
    [pscustomobject]@{ Id = 'swing_finisher'; Path = 'assets/stage14/audio/swing-finisher.wav' },
    [pscustomobject]@{ Id = 'swing_launcher'; Path = 'assets/stage14/audio/swing-launcher.wav' },
    [pscustomobject]@{ Id = 'impact_1'; Path = 'assets/stage14/audio/impact-1.wav' },
    [pscustomobject]@{ Id = 'impact_2'; Path = 'assets/stage14/audio/impact-2.wav' },
    [pscustomobject]@{ Id = 'impact_3'; Path = 'assets/stage14/audio/impact-3.wav' },
    [pscustomobject]@{ Id = 'impact_low'; Path = 'assets/stage14/audio/impact-low.wav' },
    [pscustomobject]@{ Id = 'player_hurt'; Path = 'assets/stage14/audio/player-hurt.wav' },
    [pscustomobject]@{ Id = 'landing'; Path = 'assets/stage14/audio/landing.wav' },
    [pscustomobject]@{ Id = 'enemy_defeat'; Path = 'assets/stage14/audio/enemy-defeat.wav' },
    [pscustomobject]@{ Id = 'warning_blink'; Path = 'assets/stage14/audio/warning-blink.wav' },
    [pscustomobject]@{ Id = 'warning_chain'; Path = 'assets/stage14/audio/warning-chain.wav' },
    [pscustomobject]@{ Id = 'warning_death'; Path = 'assets/stage14/audio/warning-death.wav' }
)

$ExpectedTraces = @(
    [pscustomobject]@{ Cue = 'swing_light'; Event = 'swing:j1'; Asset = 'swing_light_1' },
    [pscustomobject]@{ Cue = 'swing_light'; Event = 'swing:j2'; Asset = 'swing_light_2' },
    [pscustomobject]@{ Cue = 'swing_finisher'; Event = 'swing:j3'; Asset = 'swing_finisher' },
    [pscustomobject]@{ Cue = 'swing_launcher'; Event = 'swing:launcher'; Asset = 'swing_launcher' },
    [pscustomobject]@{ Cue = 'impact'; Event = 'hit:light'; Asset = 'impact_1' },
    [pscustomobject]@{ Cue = 'impact'; Event = 'hit:medium'; Asset = 'impact_2' },
    [pscustomobject]@{ Cue = 'impact'; Event = 'hit:heavy'; Asset = 'impact_3' },
    [pscustomobject]@{ Cue = 'impact_low'; Event = 'impact_summary:heavy'; Asset = 'impact_low' },
    [pscustomobject]@{ Cue = 'player_hurt'; Event = 'player_hit:heavy'; Asset = 'player_hurt' },
    [pscustomobject]@{ Cue = 'landing'; Event = 'landing:light'; Asset = 'landing' },
    [pscustomobject]@{ Cue = 'enemy_defeat'; Event = 'defeated:heavy'; Asset = 'enemy_defeat' },
    [pscustomobject]@{ Cue = 'warning_blink'; Event = 'affix_blink_warning:light'; Asset = 'warning_blink' },
    [pscustomobject]@{ Cue = 'warning_chain'; Event = 'affix_chain_warning:light'; Asset = 'warning_chain' },
    [pscustomobject]@{ Cue = 'warning_death'; Event = 'affix_death_warning:heavy'; Asset = 'warning_death' }
)

function Read-U16([byte[]]$Bytes, [int]$Offset) {
    if ($Offset -lt 0 -or $Offset + 2 -gt $Bytes.Length) { throw 'truncated WAV uint16' }
    return [uint32]$Bytes[$Offset] -bor ([uint32]$Bytes[$Offset + 1] -shl 8)
}

function Read-U32([byte[]]$Bytes, [int]$Offset) {
    if ($Offset -lt 0 -or $Offset + 4 -gt $Bytes.Length) { throw 'truncated WAV uint32' }
    return [uint64]$Bytes[$Offset] -bor ([uint64]$Bytes[$Offset + 1] -shl 8) -bor
        ([uint64]$Bytes[$Offset + 2] -shl 16) -bor ([uint64]$Bytes[$Offset + 3] -shl 24)
}

function Read-FourCC([byte[]]$Bytes, [int]$Offset) {
    if ($Offset -lt 0 -or $Offset + 4 -gt $Bytes.Length) { throw 'truncated WAV FourCC' }
    return [System.Text.Encoding]::ASCII.GetString($Bytes, $Offset, 4)
}

function Read-PcmWave([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "missing WAV: $Path" }
    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 44 -or (Read-FourCC $bytes 0) -ne 'RIFF' -or
            (Read-FourCC $bytes 8) -ne 'WAVE') { throw "invalid RIFF/WAVE: $Path" }
    if ((Read-U32 $bytes 4) -ne $bytes.Length - 8) { throw "invalid RIFF size: $Path" }

    $format = $null
    [byte[]]$pcm = $null
    $offset = 12
    while ($offset + 8 -le $bytes.Length) {
        $chunkId = Read-FourCC $bytes $offset
        [uint64]$chunkSize = Read-U32 $bytes ($offset + 4)
        [uint64]$dataStart = $offset + 8
        [uint64]$dataEnd = $dataStart + $chunkSize
        if ($dataEnd -gt $bytes.Length) { throw "truncated WAV chunk: $Path" }
        if ($chunkId -eq 'fmt ') {
            if ($chunkSize -lt 16) { throw "short fmt chunk: $Path" }
            $format = @{
                Tag = Read-U16 $bytes ([int]$dataStart)
                Channels = Read-U16 $bytes ([int]$dataStart + 2)
                SampleRate = Read-U32 $bytes ([int]$dataStart + 4)
                ByteRate = Read-U32 $bytes ([int]$dataStart + 8)
                BlockAlign = Read-U16 $bytes ([int]$dataStart + 12)
                Bits = Read-U16 $bytes ([int]$dataStart + 14)
            }
        } elseif ($chunkId -eq 'data') {
            if ($null -ne $pcm) { throw "duplicate data chunk: $Path" }
            $pcm = New-Object byte[] ([int]$chunkSize)
            [Array]::Copy($bytes, [int]$dataStart, $pcm, 0, [int]$chunkSize)
        }
        $offset = [int]($dataEnd + ($chunkSize % 2))
    }
    if ($null -eq $format -or $null -eq $pcm) { throw "missing fmt/data chunk: $Path" }
    if ($format.Tag -ne 1 -or $format.Channels -ne 1 -or
            $format.SampleRate -ne 44100 -or $format.Bits -ne 16 -or
            $format.BlockAlign -ne 2 -or $format.ByteRate -ne 88200 -or
            ($pcm.Length % 2) -ne 0) { throw "WAV is not 44.1 kHz 16-bit mono PCM: $Path" }
    return @{ Bytes = $bytes; Pcm = $pcm; Frames = [uint64]($pcm.Length / 2); Format = $format }
}

function Get-Peak([byte[]]$Pcm) {
    [uint32]$peak = 0
    for ($i = 0; $i -lt $Pcm.Length; $i += 2) {
        [uint16]$raw = [uint16]$Pcm[$i] -bor ([uint16]$Pcm[$i + 1] -shl 8)
        [int]$sample = if ($raw -ge 32768) { [int]$raw - 65536 } else { [int]$raw }
        [uint32]$magnitude = if ($sample -eq -32768) { 32768 } else { [Math]::Abs($sample) }
        if ($magnitude -gt $peak) { $peak = $magnitude }
    }
    return $peak
}

function Get-Sha256([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        [byte[]]$digest = $algorithm.ComputeHash($stream)
        return -join ($digest | ForEach-Object { $_.ToString('x2') })
    } finally {
        $algorithm.Dispose()
        $stream.Dispose()
    }
}

if (-not (Test-Path -LiteralPath $EvidenceDirectory -PathType Container)) {
    throw "missing evidence directory: $EvidenceDirectory"
}
$evidencePath = Join-Path $EvidenceDirectory 'stage14-audio-evidence.txt'
$showcasePath = Join-Path $EvidenceDirectory 'stage14-audio-showcase.wav'
if (-not (Test-Path -LiteralPath $evidencePath -PathType Leaf)) {
    throw "missing evidence file: $evidencePath"
}
if (-not (Test-Path -LiteralPath $showcasePath -PathType Leaf)) {
    throw "missing showcase WAV: $showcasePath"
}

$lines = @(Get-Content -LiteralPath $evidencePath -Encoding UTF8)
$scalars = @{}
$assets = @($lines | Where-Object { $_.StartsWith('asset|') })
$traces = @($lines | Where-Object { $_.StartsWith('trace|') })
foreach ($line in $lines | Where-Object { -not $_.Contains('|') -and $_.Contains('=') }) {
    $pair = $line -split '=', 2
    if ($scalars.ContainsKey($pair[0])) { throw "duplicate evidence field: $($pair[0])" }
    $scalars[$pair[0]] = $pair[1]
}
foreach ($key in @('schema','asset_count','pcm_total_bytes','pcm_budget_limit',
        'single_resource_fallback','fallback_corrupt_id','fallback_count',
        'cue_trace_count','playback_budget_allowed','showcase_frames','showcase_sha256','result')) {
    if (-not $scalars.ContainsKey($key)) { throw "missing evidence field: $key" }
}
if ($scalars.schema -ne 'stage14-audio-evidence-v1' -or $scalars.result -ne 'PASS') {
    throw 'evidence schema/result rejected'
}
if ([int]$scalars.asset_count -ne 14 -or $assets.Count -ne 14) {
    throw "expected exactly 14 asset entries, found $($assets.Count)"
}
if ([uint64]$scalars.pcm_budget_limit -ne 8388608 -or
        [uint64]$scalars.pcm_total_bytes -gt [uint64]$scalars.pcm_budget_limit) {
    throw 'PCM budget rejected'
}
if ($scalars.single_resource_fallback -ne 'PASS' -or
        $scalars.fallback_corrupt_id -ne 'impact_1' -or [int]$scalars.fallback_count -ne 1) {
    throw 'single-resource fallback evidence rejected'
}
if ([int]$scalars.cue_trace_count -ne 14 -or $traces.Count -ne 14 -or
        [int]$scalars.playback_budget_allowed -ne 14) {
    throw 'cue trace count/budget rejected'
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
[uint64]$totalPcmBytes = 0
$decodedAssets = @()
for ($index = 0; $index -lt $ExpectedAssets.Count; ++$index) {
    $columns = $assets[$index] -split '\|', -1
    if ($columns.Count -ne 11 -or $columns[0] -ne 'asset') {
        throw "malformed asset entry $index"
    }
    $expected = $ExpectedAssets[$index]
    if ($columns[1] -ne $expected.Id -or $columns[2] -ne $expected.Path) {
        throw "asset order/id/path mismatch at $index"
    }
    $assetPath = Join-Path $projectRoot ($expected.Path -replace '/', '\')
    $wave = Read-PcmWave $assetPath
    $hash = Get-Sha256 $assetPath
    $peak = Get-Peak $wave.Pcm
    $expectedDurationUs = [uint64][Math]::Round(
        ([double]$wave.Frames * 1000000.0 / 44100.0),
        0, [MidpointRounding]::AwayFromZero)
    if ($columns[3] -cne $hash -or [uint64]$columns[4] -ne 44100 -or
            [uint64]$columns[5] -ne 16 -or [uint64]$columns[6] -ne 1 -or
            [uint64]$columns[7] -ne $wave.Frames -or
            [uint64]$columns[8] -ne $expectedDurationUs -or
            [uint64]$columns[9] -ne $peak -or
            [uint64]$columns[10] -ne $wave.Pcm.Length) {
        throw "decoded metadata/hash mismatch for $($expected.Id)"
    }
    if ($wave.Frames -eq 0 -or $wave.Frames -gt 66150 -or $peak -eq 0 -or $peak -ge 32768) {
        throw "silence, clipping, or duration rejected for $($expected.Id)"
    }
    $totalPcmBytes += $wave.Pcm.Length
    $decodedAssets += ,$wave
}
if ($totalPcmBytes -ne [uint64]$scalars.pcm_total_bytes) {
    throw 'PCM byte total mismatch'
}

for ($index = 0; $index -lt $ExpectedTraces.Count; ++$index) {
    $columns = $traces[$index] -split '\|', -1
    $expected = $ExpectedTraces[$index]
    if ($columns.Count -ne 7 -or $columns[0] -ne 'trace' -or
            [int]$columns[1] -ne $index -or $columns[2] -ne $expected.Cue -or
            $columns[3] -ne $expected.Event -or $columns[4] -ne $expected.Asset -or
            $columns[5] -ne 'ALLOW' -or [uint64]$columns[6] -ne (100 + $index * 20)) {
        throw "cue trace mismatch at $index"
    }
}

$showcase = Read-PcmWave $showcasePath
$silenceFrames = 11025
[uint64]$expectedShowcaseFrames = $totalPcmBytes / 2 +
    [uint64](($ExpectedAssets.Count - 1) * $silenceFrames)
if ($showcase.Frames -ne $expectedShowcaseFrames -or
        [uint64]$scalars.showcase_frames -ne $expectedShowcaseFrames) {
    throw 'showcase frame count mismatch'
}
$showcaseHash = Get-Sha256 $showcasePath
if ($scalars.showcase_sha256 -cne $showcaseHash) { throw 'showcase hash mismatch' }
$showcaseOffset = 0
for ($index = 0; $index -lt $decodedAssets.Count; ++$index) {
    $sourcePcm = $decodedAssets[$index].Pcm
    for ($byteIndex = 0; $byteIndex -lt $sourcePcm.Length; ++$byteIndex) {
        if ($showcase.Pcm[$showcaseOffset + $byteIndex] -ne $sourcePcm[$byteIndex]) {
            throw "showcase segment mismatch at asset $index"
        }
    }
    $showcaseOffset += $sourcePcm.Length
    if ($index + 1 -lt $decodedAssets.Count) {
        $silenceBytes = $silenceFrames * 2
        for ($byteIndex = 0; $byteIndex -lt $silenceBytes; ++$byteIndex) {
            if ($showcase.Pcm[$showcaseOffset + $byteIndex] -ne 0) {
                throw "showcase separator is not 250 ms silence after asset $index"
            }
        }
        $showcaseOffset += $silenceBytes
    }
}
if ($showcaseOffset -ne $showcase.Pcm.Length) { throw 'showcase trailing data rejected' }

Write-Output "stage14 audio evidence validated: 14 real PCM decodes, one-resource fallback, 14 cue traces, showcase WAV"
