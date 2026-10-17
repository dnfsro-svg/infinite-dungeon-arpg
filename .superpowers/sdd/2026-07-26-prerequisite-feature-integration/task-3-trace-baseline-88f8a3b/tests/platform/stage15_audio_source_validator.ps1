[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$AssetDirectory
)

$ErrorActionPreference = 'Stop'

$expected = [ordered]@{
    'music-explore.ogg'  = 'B5095B1517E0563141E4158554B8730E30E82FCB2000C1081BD448DD4B722A24'
    'music-combat.ogg'   = '783FA3DB1B466DD44B4F2097601DD05DF9FE39FDB505AB3B073B73681250FA93'
    'ambience-room.ogg'  = 'DF491823E4877371C34DBDA4E9321CD83A4A14FA7573CEE0EBCA1AE423B70E6E'
    'ambience-abyss.ogg' = '5C841EF1A7BACD2A038801ECEE0741E200D1CF1CD08F36A6A89CCE95BCFB6A8A'
    'ui-navigate.wav'    = '671496A390CDF1052A59B6204EB10DB0CFC3B01EC5F8F6FB0B3B31FAEDC9FED5'
    'ui-confirm.wav'     = '985993A23D40387D02EBB7CBBED7B4B09869788A8E282E545E3037B8084BC695'
    'ui-cancel.wav'      = '9F31B9AEE0551E50932717A76B10F5536BF912DB161CB80D2075C0E25FB9C857'
    'ui-open.wav'        = '6965E9AFF1ED62E3B40749DAD1E9871A29BF775BB9DC6A9AFF6B34AD0AB0A6CA'
    'ui-close.wav'       = 'BF655C861B6B6E4ECED0057FBD2C9BD6625C844606FF91B5569D54D0A8073186'
    'ui-reward.wav'      = '36E3E28F1365F80001925F699654131F476269D39D79709B82385266650AEB56'
}

function Get-Sha256([string]$Path) {
    $stream=[IO.File]::OpenRead($Path)
    $algorithm=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','') }
    finally { $algorithm.Dispose(); $stream.Dispose() }
}

function Read-WaveInfo([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 44) { throw "WAV too short: $Path" }
        if ([Text.Encoding]::ASCII.GetString($reader.ReadBytes(4)) -ne 'RIFF') { throw "not RIFF: $Path" }
        [void]$reader.ReadUInt32()
        if ([Text.Encoding]::ASCII.GetString($reader.ReadBytes(4)) -ne 'WAVE') { throw "not WAVE: $Path" }
        $format = $null; $channels = $null; $rate = $null; $bits = $null; [UInt64]$data = 0
        while ($stream.Position + 8 -le $stream.Length) {
            $id = [Text.Encoding]::ASCII.GetString($reader.ReadBytes(4)); [UInt64]$size = $reader.ReadUInt32()
            if ($size -gt [UInt64]($stream.Length - $stream.Position)) { throw "chunk exceeds bounds: $Path" }
            if ($id -eq 'fmt ') {
                if ($size -lt 16) { throw "short fmt: $Path" }
                $format=$reader.ReadUInt16(); $channels=$reader.ReadUInt16(); $rate=$reader.ReadUInt32()
                [void]$reader.ReadUInt32(); [void]$reader.ReadUInt16(); $bits=$reader.ReadUInt16()
                if ($size -gt 16) { $stream.Seek([Int64]$size-16,[IO.SeekOrigin]::Current) | Out-Null }
            } elseif ($id -eq 'data') {
                $data += $size; $stream.Seek([Int64]$size,[IO.SeekOrigin]::Current) | Out-Null
            } else { $stream.Seek([Int64]$size,[IO.SeekOrigin]::Current) | Out-Null }
            if (($size % 2) -ne 0 -and $stream.Position -lt $stream.Length) { $stream.Seek(1,[IO.SeekOrigin]::Current) | Out-Null }
        }
        if ($null -eq $format -or $data -eq 0) { throw "missing WAV chunks: $Path" }
        return [pscustomobject]@{Format=$format;Channels=$channels;Rate=$rate;Bits=$bits;Data=$data}
    } finally { $reader.Dispose(); $stream.Dispose() }
}

function Read-VorbisInfo([string]$Path) {
    [byte[]]$bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 64 -or [Text.Encoding]::ASCII.GetString($bytes,0,4) -ne 'OggS') { throw "not Ogg: $Path" }
    $marker = [byte[]](1,118,111,114,98,105,115)
    $header = -1
    for ($i=0; $i -le $bytes.Length-$marker.Length; $i++) {
        $match=$true; for($j=0;$j-lt$marker.Length;$j++){if($bytes[$i+$j]-ne$marker[$j]){$match=$false;break}}
        if($match){$header=$i;break}
    }
    if($header-lt0 -or $header+16-ge$bytes.Length){throw "missing Vorbis identification header: $Path"}
    $channels=[int]$bytes[$header+11]
    $rate=[BitConverter]::ToUInt32($bytes,$header+12)
    $last=-1
    for($i=$bytes.Length-4;$i-ge0;$i--){if($bytes[$i]-eq79-and$bytes[$i+1]-eq103-and$bytes[$i+2]-eq103-and$bytes[$i+3]-eq83){$last=$i;break}}
    if($last-lt0 -or $last+14-ge$bytes.Length){throw "missing final Ogg page: $Path"}
    [UInt64]$granule=[BitConverter]::ToUInt64($bytes,$last+6)
    if($channels-lt1 -or $channels-gt2 -or $rate-lt8000 -or $rate-gt192000 -or $granule-eq0){throw "invalid Vorbis metadata: $Path"}
    return [pscustomobject]@{Channels=$channels;Rate=$rate;Duration=([double]$granule/$rate)}
}

try {
    $directory=[IO.Path]::GetFullPath($AssetDirectory)
    if(-not(Test-Path -LiteralPath $directory -PathType Container)){throw "missing Stage 15 directory: $directory"}
    $names=@($expected.Keys)+'SOURCES.md'; $children=@(Get-ChildItem -LiteralPath $directory -Force)
    $unexpected=@($children|Where-Object{$_.PSIsContainer-or$_.Name-notin$names})
    if($unexpected.Count-gt0){throw "unexpected entries: $($unexpected.Name -join ', ')"}
    foreach($name in $names){if(-not(Test-Path -LiteralPath (Join-Path $directory $name) -PathType Leaf)){throw "missing resource: $name"}}
    $sources=Get-Content -LiteralPath (Join-Path $directory 'SOURCES.md') -Raw
    [UInt64]$total=0; $streams=0; $ui=0
    foreach($entry in $expected.GetEnumerator()){
        $path=Join-Path $directory $entry.Key; $file=Get-Item -LiteralPath $path; $total += $file.Length
        $sha=Get-Sha256 $path
        if($sha-ne$entry.Value){throw "SHA-256 mismatch: $($entry.Key)"}
        if($sources-notmatch[regex]::Escape($sha)){throw "SOURCES.md missing release hash: $($entry.Key)"}
        if($entry.Key.EndsWith('.ogg')){
            $info=Read-VorbisInfo $path
            if($info.Duration-lt30-or$info.Duration-gt300){throw "stream duration outside 30-300 seconds: $($entry.Key)"}
            $streams++
        }else{
            $info=Read-WaveInfo $path
            if($info.Format-ne1-or$info.Channels-ne1-or$info.Rate-ne44100-or$info.Bits-ne16){throw "UI WAV must be PCM s16le mono 44100 Hz: $($entry.Key)"}
            $duration=[double]$info.Data/($info.Rate*2)
            if($duration-gt0.75){throw "UI WAV exceeds 0.75 seconds: $($entry.Key)"}
            $ui++
        }
    }
    if($streams-ne4-or$ui-ne6){throw "wrong resource kinds: streams=$streams ui=$ui"}
    if($total-gt16MB){throw "Stage 15 audio exceeds 16 MiB: $total"}
    Write-Host "10/10 PASS - Stage 15 audio sources valid; 4 streams, 6 UI WAV, $total bytes."
    exit 0
} catch {
    [Console]::Error.WriteLine("Stage 15 audio validation FAILED: $($_.Exception.Message)")
    exit 1
}
