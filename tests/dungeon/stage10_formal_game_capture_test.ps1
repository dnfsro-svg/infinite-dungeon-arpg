param(
    [string]$Executable,
    [string]$EvidenceDirectory,
    [string]$ValidateOnlyPath
)

$ErrorActionPreference = 'Stop'

function Test-Stage10CaptureContent {
    param([Parameter(Mandatory = $true)][string]$Path)

    $image = [System.Drawing.Bitmap]::new($Path)
    try {
        if ($image.Width -ne 1280 -or $image.Height -ne 720) {
            throw "Unexpected formal capture size: $Path $($image.Width)x$($image.Height)"
        }
        $colors = [System.Collections.Generic.HashSet[int]]::new()
        $nonBackground = 0
        $transitions = 0
        $minimumLuminance = [int]::MaxValue
        $maximumLuminance = [int]::MinValue
        $previousRow = [int[]]::new(80)
        $hasPreviousRow = $false
        for ($y = 0; $y -lt $image.Height; $y += 16) {
            $column = 0
            $leftColor = -1
            for ($x = 0; $x -lt $image.Width; $x += 16) {
                $pixel = $image.GetPixel($x, $y)
                $color = ($pixel.R -shl 16) -bor ($pixel.G -shl 8) -bor $pixel.B
                [void]$colors.Add($color)
                if ($pixel.R -ne 13 -or $pixel.G -ne 17 -or $pixel.B -ne 27) {
                    $nonBackground++
                }
                $luminance = $pixel.R * 299 + $pixel.G * 587 + $pixel.B * 114
                $minimumLuminance = [Math]::Min($minimumLuminance, $luminance)
                $maximumLuminance = [Math]::Max($maximumLuminance, $luminance)
                if (($leftColor -ne -1 -and $leftColor -ne $color) -or
                        ($hasPreviousRow -and $previousRow[$column] -ne $color)) {
                    $transitions++
                }
                $leftColor = $color
                $previousRow[$column] = $color
                $column++
            }
            $hasPreviousRow = $true
        }
        $luminanceRange = $maximumLuminance - $minimumLuminance
        $structuredLowColorScene = $colors.Count -ge 8 -and
            $transitions -ge 200 -and $luminanceRange -ge 12000
        if ($nonBackground -lt 100 -or
                ($colors.Count -lt 20 -and -not $structuredLowColorScene)) {
            throw "Formal capture is blank: $Path colors=$($colors.Count) non_background=$nonBackground transitions=$transitions luminance_range=$luminanceRange"
        }
        Write-Output "stage10_capture_metrics=PASS colors=$($colors.Count) non_background=$nonBackground transitions=$transitions luminance_range=$luminanceRange path=$Path"
    } finally {
        $image.Dispose()
    }
}

Add-Type -AssemblyName System.Drawing
if ($ValidateOnlyPath) {
    Test-Stage10CaptureContent -Path $ValidateOnlyPath
    Write-Output "stage10_capture_content=PASS path=$ValidateOnlyPath"
    return
}
if (-not $Executable -or -not $EvidenceDirectory) {
    throw 'Executable and EvidenceDirectory are required outside validation-only mode'
}

$started = [DateTime]::UtcNow
& $Executable
if ($LASTEXITCODE -ne 0) {
    throw "Stage 10 formal game validation exited with code $LASTEXITCODE"
}

$names = @(
    '01-abyss-door.png',
    '02-thunderstorm-warning.png',
    '03-hunting-flames-warning.png',
    '04-chaos-expansion.png',
    '05-reward-chest.png',
    '06-pending-reward.png',
    '07-exit-confirmation.png'
)
foreach ($name in $names) {
    $path = Join-Path $EvidenceDirectory $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing formal Stage 10 capture: $path"
    }
    if ((Get-Item -LiteralPath $path).LastWriteTimeUtc -lt $started) {
        throw "Formal Stage 10 capture is stale: $path"
    }
    Test-Stage10CaptureContent -Path $path
}
Write-Output "stage10_formal_captures=PASS count=$($names.Count) directory=$EvidenceDirectory"
