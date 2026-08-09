param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [Parameter(Mandatory = $true)][string]$EvidenceDirectory
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function Get-PanelHash {
    param([System.Drawing.Bitmap]$Image)
    # Hash the opaque authored body, including all body and prompt text.  The
    # title and decorated frame outside this rectangle are translucent and
    # legitimately expose different live-room pixels before and after restart.
    $panelLeft = 212
    $panelTop = 112
    $panelRightExclusive = 1068
    $panelBottomExclusive = 612
    $bytes = [byte[]]::new(
        ($panelRightExclusive - $panelLeft) *
        ($panelBottomExclusive - $panelTop) * 3)
    $offset = 0
    for ($y = $panelTop; $y -lt $panelBottomExclusive; ++$y) {
        for ($x = $panelLeft; $x -lt $panelRightExclusive; ++$x) {
            $pixel = $Image.GetPixel($x, $y)
            $bytes[$offset] = $pixel.R
            $bytes[$offset + 1] = $pixel.G
            $bytes[$offset + 2] = $pixel.B
            $offset += 3
        }
    }
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return (($sha.ComputeHash($bytes) | ForEach-Object {
            $_.ToString('x2')
        }) -join '')
    } finally {
        $sha.Dispose()
    }
}

function Test-Capture {
    param([string]$Path, [bool]$RequirePanel)
    $image = [System.Drawing.Bitmap]::new($Path)
    try {
        if ($image.Width -ne 1280 -or $image.Height -ne 720) {
            throw "Unexpected Stage 11 capture size: $Path $($image.Width)x$($image.Height)"
        }
        $colors = [System.Collections.Generic.HashSet[int]]::new()
        $nonBackground = 0
        $minimumLuminance = [int]::MaxValue
        $maximumLuminance = [int]::MinValue
        for ($y = 0; $y -lt 720; $y += 12) {
            for ($x = 0; $x -lt 1280; $x += 12) {
                $pixel = $image.GetPixel($x, $y)
                $packed = ($pixel.R -shl 16) -bor ($pixel.G -shl 8) -bor $pixel.B
                [void]$colors.Add($packed)
                if ($pixel.R -ne 13 -or $pixel.G -ne 17 -or $pixel.B -ne 27) {
                    $nonBackground++
                }
                $luminance = $pixel.R * 299 + $pixel.G * 587 + $pixel.B * 114
                $minimumLuminance = [Math]::Min($minimumLuminance, $luminance)
                $maximumLuminance = [Math]::Max($maximumLuminance, $luminance)
            }
        }
        if ($colors.Count -lt 20 -or $nonBackground -lt 200 -or
                ($maximumLuminance - $minimumLuminance) -lt 12000) {
            throw "Stage 11 capture lacks visible content: $Path colors=$($colors.Count) nonBackground=$nonBackground"
        }
        if ($RequirePanel) {
            $panelDark = 0
            $panelAccent = 0
            $panelAuthored = 0
            for ($y = 80; $y -lt 640; $y += 10) {
                for ($x = 180; $x -lt 1100; $x += 10) {
                    $pixel = $image.GetPixel($x, $y)
                    if ($pixel.R -lt 40 -and $pixel.G -lt 40 -and $pixel.B -lt 50) { $panelDark++ }
                    if ($pixel.R -gt 150 -and $pixel.G -gt 50) { $panelAccent++ }
                    $greenDelta = [int]$pixel.G - [int]$pixel.R
                    $blueDelta = [int]$pixel.B - [int]$pixel.G
                    if ($pixel.R -ge 45 -and $pixel.R -le 80 -and
                            $pixel.G -ge 55 -and $pixel.G -le 100 -and
                            $pixel.B -ge 70 -and $pixel.B -le 120 -and
                            $greenDelta -ge 8 -and $greenDelta -le 30 -and
                            $blueDelta -ge 8 -and $blueDelta -le 30) {
                        $panelAuthored++
                    }
                }
            }
            if ($panelDark -lt 390 -or $panelAccent -lt 15 -or
                    $panelAuthored -lt 2350) {
                throw "Death panel content missing: $Path dark=$panelDark accent=$panelAccent authored=$panelAuthored"
            }
        }
        return Get-PanelHash -Image $image
    } finally {
        $image.Dispose()
    }
}

$started = [DateTime]::UtcNow
& $Executable
if ($LASTEXITCODE -ne 0) { throw "Stage 11 formal game exited $LASTEXITCODE" }
$names = @('01-normal-death.png', '02-restarted-death.png',
    '03-deep-continued.png', '04-floor-one-continued.png', '05-abyss-death.png')
$hashes = @{}
for ($index = 0; $index -lt $names.Count; ++$index) {
    $path = Join-Path $EvidenceDirectory $names[$index]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing $path" }
    if ((Get-Item -LiteralPath $path).LastWriteTimeUtc -lt $started) { throw "Stale $path" }
    $hashes[$names[$index]] = Test-Capture -Path $path -RequirePanel ($index -in 0, 1, 4)
}
if ($hashes['01-normal-death.png'] -ne $hashes['02-restarted-death.png']) {
    throw 'Normal/restarted death panel hashes differ'
}
$summaryPath = Join-Path $EvidenceDirectory 'formal-path-summary.txt'
if (-not (Test-Path -LiteralPath $summaryPath)) { throw 'Missing formal summary' }
$summary = Get-Content -Raw -LiteralPath $summaryPath
foreach ($marker in @('normal_death_recap=PASS', 'restart_same_recap=PASS',
        'deep_continue=PASS death_depth=2 target_depth=1 final_depth=1',
        'floor_one_continue=PASS target_depth=1 final_depth=1 clamp=1',
        'abyss_death_recap=PASS failed_resolution=1')) {
    if (-not $summary.Contains($marker)) { throw "Missing summary marker: $marker" }
}
$normalLine = [regex]::Match($summary, 'normal_death_recap=PASS target=(\d+) hash=(\d+)')
$restartLine = [regex]::Match($summary, 'restart_same_recap=PASS target=(\d+) hash=(\d+)')
if (-not $normalLine.Success -or -not $restartLine.Success -or
        $normalLine.Groups[1].Value -ne $restartLine.Groups[1].Value -or
        $normalLine.Groups[2].Value -ne $restartLine.Groups[2].Value) {
    throw 'Restart target/hash invariant failed'
}
Write-Output "stage11_death_formal=PASS count=5 panel_hash=$($hashes['01-normal-death.png'])"
