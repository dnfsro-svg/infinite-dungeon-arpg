[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$AssetDirectory
)

$ErrorActionPreference = 'Stop'

$expectedSources = [ordered]@{
    'swing-light-1.wav' = '4CD96DC630BED9840C15F1DD2306DA2CC56A4DA26A5D3F1A03C5A7265AC5E54F'
    'swing-light-2.wav' = '6C2064D0EF988D1EC3D56868E823EA8823A5CAC00F2742560052633529407DEF'
    'swing-finisher.wav' = 'D00C2B3C9FFF07E376145C8C8C45C90E5084EC192F6CE0387DB233F7B86F1486'
    'swing-launcher.wav' = 'A11AE62FB1A628425769D11A9DE394980AD8909C31F4C9A4316F226963E21CAF'
    'impact-1.wav' = '486988AA2D6440FFC4C62A0E8CCF3C23673BA84424BD4723378D451B7255EB5C'
    'impact-2.wav' = 'E71D23ABCC3F10D9D6BC9615B2A431FFF70B12333A5FCDD9964457564ACE4349'
    'impact-3.wav' = '6492F9CCBC8D24DCEF607BB1B48790B7870961474561F7FE5601E1C73A3018EF'
    'impact-low.wav' = 'F4C0C3EB8AB6517583B8218ED03F28923D1B687379AAB4C1D5A6D4C10CF8E500'
    'player-hurt.wav' = 'D00286A2DC62EE5CB4D42BF56120E7050A855F69E655FC1294410AADD337EBA3'
    'landing.wav' = '24DD8DB2413E5B81AD181E1205C12A1C7801DA5F85715D471B92A4B2F632E45E'
    'enemy-defeat.wav' = '6D65B463C0555DD5BE16B8DB6D2CBE23A94E07A4637779B8AD17D0DB3E500A87'
    'warning-blink.wav' = '49FE4FAFA2001BD0D312976796824571CA8429851F285997E1327D17BB34FD00'
    'warning-chain.wav' = '03E10F800199C7166208E41C11CF4ACFAC6CC265467F02357DA71972BC967295'
    'warning-death.wav' = 'C7742B0E33C1733DDAF902AA68464A4F5364E4D4CB6FF11B76A7AA4151A138DD'
}

function Get-Stage14Sha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = [System.IO.File]::OpenRead($Path)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-', '')
    } finally {
        $algorithm.Dispose()
        $stream.Dispose()
    }
}

function Read-Stage14WaveInfo {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = [System.IO.File]::OpenRead($Path)
    $reader = [System.IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 44) { throw "WAV is too short: $Path" }
        $riff = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
        [void]$reader.ReadUInt32()
        $wave = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
        if ($riff -ne 'RIFF' -or $wave -ne 'WAVE') { throw "WAV header is not RIFF/WAVE: $Path" }

        $format = $null
        $channels = $null
        $sampleRate = $null
        $bitsPerSample = $null
        [UInt64]$totalDataBytes = 0
        while ($stream.Position + 8 -le $stream.Length) {
            $chunkId = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
            [UInt64]$chunkSize = $reader.ReadUInt32()
            if ($chunkSize -gt [UInt64]($stream.Length - $stream.Position)) {
                throw "WAV chunk '$chunkId' exceeds file bounds: $Path"
            }
            if ($chunkId -eq 'fmt ') {
                if ($chunkSize -lt 16) { throw "WAV fmt chunk is too short: $Path" }
                $format = $reader.ReadUInt16()
                $channels = $reader.ReadUInt16()
                $sampleRate = $reader.ReadUInt32()
                [void]$reader.ReadUInt32()
                [void]$reader.ReadUInt16()
                $bitsPerSample = $reader.ReadUInt16()
                $remaining = [Int64]$chunkSize - 16
                if ($remaining -gt 0) { [void]$reader.ReadBytes($remaining) }
            } elseif ($chunkId -eq 'data') {
                $totalDataBytes += $chunkSize
                $stream.Seek([Int64]$chunkSize, [System.IO.SeekOrigin]::Current) | Out-Null
            } else {
                $stream.Seek([Int64]$chunkSize, [System.IO.SeekOrigin]::Current) | Out-Null
            }
            if (($chunkSize % 2) -ne 0 -and $stream.Position -lt $stream.Length) {
                $stream.Seek(1, [System.IO.SeekOrigin]::Current) | Out-Null
            }
        }

        if ($null -eq $format -or $totalDataBytes -eq 0) { throw "WAV requires fmt and non-empty data chunks: $Path" }
        [pscustomobject]@{
            Format = $format; Channels = $channels; SampleRate = $sampleRate
            BitsPerSample = $bitsPerSample; DataBytes = $totalDataBytes
        }
    } finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

try {
    $directory = [System.IO.Path]::GetFullPath($AssetDirectory)
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        throw "Stage 14 audio directory is missing: $directory"
    }

    $expectedNames = @($expectedSources.Keys) + 'SOURCES.md'
    $children = @(Get-ChildItem -LiteralPath $directory -Force)
    $unexpected = @($children | Where-Object { $_.PSIsContainer -or $_.Name -notin $expectedNames })
    if ($unexpected.Count -gt 0) {
        throw "Stage 14 audio directory contains unexpected entries: $($unexpected.Name -join ', ')"
    }
    foreach ($name in $expectedNames) {
        if (-not (Test-Path -LiteralPath (Join-Path $directory $name) -PathType Leaf)) {
            throw "Stage 14 audio resource is missing: $name"
        }
    }

    $sourcesPath = Join-Path $directory 'SOURCES.md'
    $sources = Get-Content -LiteralPath $sourcesPath -Raw
    [UInt64]$totalPcmBytes = 0
    $passed = 0
    foreach ($entry in $expectedSources.GetEnumerator()) {
        $path = Join-Path $directory $entry.Key
        if ((Get-Item -LiteralPath $path).Length -eq 0) { throw "Audio resource is empty: $($entry.Key)" }
        $info = Read-Stage14WaveInfo -Path $path
        if ($info.Format -ne 1 -or $info.Channels -ne 1 -or $info.SampleRate -ne 44100 -or $info.BitsPerSample -ne 16) {
            throw "Audio format must be PCM s16le mono 44100 Hz: $($entry.Key)"
        }
        $duration = [double]$info.DataBytes / ($info.SampleRate * $info.Channels * ($info.BitsPerSample / 8))
        if ($duration -gt 1.5) { throw "Audio duration exceeds 1.5 seconds: $($entry.Key) ($duration seconds)" }
        $totalPcmBytes += $info.DataBytes
        $releaseSha = Get-Stage14Sha256 -Path $path
        if ($sources -notmatch [regex]::Escape($entry.Value)) { throw "SOURCES.md is missing source SHA-256 for $($entry.Key)" }
        if ($sources -notmatch [regex]::Escape($releaseSha)) { throw "SOURCES.md is missing release SHA-256 for $($entry.Key)" }
        $passed++
    }
    if ($totalPcmBytes -gt 8MB) { throw "Total PCM audio exceeds 8 MiB: $totalPcmBytes bytes" }
    Write-Host "$passed/14 PASS - Stage 14 audio sources are valid; total PCM: $totalPcmBytes bytes."
    exit 0
} catch {
    [Console]::Error.WriteLine("Stage 14 audio validation FAILED: $($_.Exception.Message)")
    exit 1
}
