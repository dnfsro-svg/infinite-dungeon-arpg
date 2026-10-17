[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$assetDirectory = Join-Path $repositoryRoot 'assets/stage14/audio'
$downloads = @(
    [pscustomobject]@{
        Name = 'rpg'
        Url = 'https://kenney.nl/media/pages/assets/rpg-audio/8e99002d76-1677590336/kenney_rpg-audio.zip'
    },
    [pscustomobject]@{
        Name = 'impact'
        Url = 'https://kenney.nl/media/pages/assets/impact-sounds/87b4ddecda-1677589768/kenney_impact-sounds.zip'
    }
)
$assets = @(
    [pscustomobject]@{ Output = 'swing-light-1.wav'; Source = 'RPG Audio/knifeSlice.ogg'; SourceSha256 = '4CD96DC630BED9840C15F1DD2306DA2CC56A4DA26A5D3F1A03C5A7265AC5E54F'; Package = 'rpg' },
    [pscustomobject]@{ Output = 'swing-light-2.wav'; Source = 'RPG Audio/knifeSlice2.ogg'; SourceSha256 = '6C2064D0EF988D1EC3D56868E823EA8823A5CAC00F2742560052633529407DEF'; Package = 'rpg' },
    [pscustomobject]@{ Output = 'swing-finisher.wav'; Source = 'RPG Audio/chop.ogg'; SourceSha256 = 'D00C2B3C9FFF07E376145C8C8C45C90E5084EC192F6CE0387DB233F7B86F1486'; Package = 'rpg' },
    [pscustomobject]@{ Output = 'swing-launcher.wav'; Source = 'RPG Audio/drawKnife3.ogg'; SourceSha256 = 'A11AE62FB1A628425769D11A9DE394980AD8909C31F4C9A4316F226963E21CAF'; Package = 'rpg' },
    [pscustomobject]@{ Output = 'impact-1.wav'; Source = 'Impact Audio/impactPunch_medium_000.ogg'; SourceSha256 = '486988AA2D6440FFC4C62A0E8CCF3C23673BA84424BD4723378D451B7255EB5C'; Package = 'impact' },
    [pscustomobject]@{ Output = 'impact-2.wav'; Source = 'Impact Audio/impactPunch_medium_001.ogg'; SourceSha256 = 'E71D23ABCC3F10D9D6BC9615B2A431FFF70B12333A5FCDD9964457564ACE4349'; Package = 'impact' },
    [pscustomobject]@{ Output = 'impact-3.wav'; Source = 'Impact Audio/impactPunch_medium_002.ogg'; SourceSha256 = '6492F9CCBC8D24DCEF607BB1B48790B7870961474561F7FE5601E1C73A3018EF'; Package = 'impact' },
    [pscustomobject]@{ Output = 'impact-low.wav'; Source = 'Impact Audio/impactPunch_heavy_004.ogg'; SourceSha256 = 'F4C0C3EB8AB6517583B8218ED03F28923D1B687379AAB4C1D5A6D4C10CF8E500'; Package = 'impact' },
    [pscustomobject]@{ Output = 'player-hurt.wav'; Source = 'Impact Audio/impactSoft_heavy_003.ogg'; SourceSha256 = 'D00286A2DC62EE5CB4D42BF56120E7050A855F69E655FC1294410AADD337EBA3'; Package = 'impact' },
    [pscustomobject]@{ Output = 'landing.wav'; Source = 'Impact Audio/footstep_concrete_004.ogg'; SourceSha256 = '24DD8DB2413E5B81AD181E1205C12A1C7801DA5F85715D471B92A4B2F632E45E'; Package = 'impact' },
    [pscustomobject]@{ Output = 'enemy-defeat.wav'; Source = 'Impact Audio/impactMetal_heavy_004.ogg'; SourceSha256 = '6D65B463C0555DD5BE16B8DB6D2CBE23A94E07A4637779B8AD17D0DB3E500A87'; Package = 'impact' },
    [pscustomobject]@{ Output = 'warning-blink.wav'; Source = 'Impact Audio/impactBell_heavy_004.ogg'; SourceSha256 = '49FE4FAFA2001BD0D312976796824571CA8429851F285997E1327D17BB34FD00'; Package = 'impact' },
    [pscustomobject]@{ Output = 'warning-chain.wav'; Source = 'Impact Audio/impactMetal_medium_004.ogg'; SourceSha256 = '03E10F800199C7166208E41C11CF4ACFAC6CC265467F02357DA71972BC967295'; Package = 'impact' },
    [pscustomobject]@{ Output = 'warning-death.wav'; Source = 'Impact Audio/impactGlass_heavy_004.ogg'; SourceSha256 = 'C7742B0E33C1733DDAF902AA68464A4F5364E4D4CB6FF11B76A7AA4151A138DD'; Package = 'impact' }
)

function Assert-PathWithinDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Directory,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $fullDirectory = [System.IO.Path]::GetFullPath($Directory).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $prefix = $fullDirectory + [System.IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description escapes its temporary directory: $fullPath"
    }
    return $fullPath
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

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('stage14-audio-' + [System.Guid]::NewGuid().ToString('N'))
try {
    $null = Get-Command ffmpeg -ErrorAction Stop
    New-Item -ItemType Directory -Path $temporaryRoot -Force | Out-Null
    $packageRoots = @{}
    foreach ($download in $downloads) {
        $zipPath = Assert-PathWithinDirectory -Path (Join-Path $temporaryRoot ($download.Name + '.zip')) -Directory $temporaryRoot -Description 'Download archive'
        $extractDirectory = Assert-PathWithinDirectory -Path (Join-Path $temporaryRoot ($download.Name + '-extract')) -Directory $temporaryRoot -Description 'Extraction directory'
        Invoke-WebRequest -Uri $download.Url -OutFile $zipPath
        Expand-Archive -LiteralPath $zipPath -DestinationPath $extractDirectory -Force
        $packageRoots[$download.Name] = $extractDirectory
    }

    $stagingDirectory = Assert-PathWithinDirectory -Path (Join-Path $temporaryRoot 'release') -Directory $temporaryRoot -Description 'Release staging directory'
    New-Item -ItemType Directory -Path $stagingDirectory -Force | Out-Null
    $releaseRows = @()
    foreach ($asset in $assets) {
        # Kenney's ZIPs store both named packages under Audio/.  The Source field
        # remains the fixed package-qualified provenance path required by Stage 14.
        $archiveRelativePath = Join-Path 'Audio' ([System.IO.Path]::GetFileName($asset.Source))
        $sourcePath = Assert-PathWithinDirectory -Path (Join-Path $packageRoots[$asset.Package] $archiveRelativePath) -Directory $packageRoots[$asset.Package] -Description 'Source audio path'
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) { throw "Expected source audio is missing: $($asset.Source)" }
        $actualSourceSha = Get-Stage14Sha256 -Path $sourcePath
        if ($actualSourceSha -ne $asset.SourceSha256) { throw "Source SHA-256 mismatch for $($asset.Source): expected $($asset.SourceSha256), got $actualSourceSha" }
        $outputPath = Assert-PathWithinDirectory -Path (Join-Path $stagingDirectory $asset.Output) -Directory $stagingDirectory -Description 'Release audio path'
        & ffmpeg -nostdin -y -i $sourcePath -ac 1 -ar 44100 -c:a pcm_s16le $outputPath
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $outputPath -PathType Leaf)) { throw "ffmpeg failed for $($asset.Source)" }
        $releaseRows += [pscustomobject]@{ Asset = $asset; ReleaseSha256 = Get-Stage14Sha256 -Path $outputPath }
    }

    $sourceLines = @(
        '# Stage 14 audio sources',
        '',
        'All source files are from the official Kenney packages below. The preparation script verifies every original SHA-256 before conversion.',
        '',
        "- RPG Audio: $($downloads[0].Url)",
        "- Impact Audio: $($downloads[1].Url)",
        '',
        '| Published WAV | Original path | Original SHA-256 | Release SHA-256 |',
        '| --- | --- | --- | --- |'
    )
    foreach ($row in $releaseRows) {
        $sourceLines += "| $($row.Asset.Output) | $($row.Asset.Source) | $($row.Asset.SourceSha256) | $($row.ReleaseSha256) |"
    }
    Set-Content -LiteralPath (Join-Path $stagingDirectory 'SOURCES.md') -Value $sourceLines -Encoding UTF8

    New-Item -ItemType Directory -Path $assetDirectory -Force | Out-Null
    $unexpected = @(Get-ChildItem -LiteralPath $assetDirectory -Force | Where-Object { $_.PSIsContainer -or $_.Name -notin (@($assets.Output) + 'SOURCES.md') })
    if ($unexpected.Count -gt 0) { throw "Refusing to alter asset directory with unexpected entries: $($unexpected.Name -join ', ')" }
    foreach ($name in @($assets.Output) + 'SOURCES.md') {
        Copy-Item -LiteralPath (Join-Path $stagingDirectory $name) -Destination (Join-Path $assetDirectory $name) -Force
    }
    Write-Host "Prepared $($assets.Count) Stage 14 audio assets in $assetDirectory"
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        $safeTemporaryRoot = Assert-PathWithinDirectory -Path $temporaryRoot -Directory ([System.IO.Path]::GetTempPath()) -Description 'Cleanup directory'
        Remove-Item -LiteralPath $safeTemporaryRoot -Recurse -Force
    }
}
