param(
    [string]$Executable,
    [string]$CapturePath,
    [string]$ValidateOnlyPath,
    [DateTime]$MinimumWriteTimeUtc = [DateTime]::MinValue
)

$ErrorActionPreference = 'Stop'

function Test-Stage9FormalCapture {
    param([Parameter(Mandatory = $true)][string]$Path)

    $image = [System.Drawing.Bitmap]::new($Path)
    try {
        if ($image.Width -ne 1280 -or $image.Height -ne 720) {
            throw "Stage9 formal capture rejects unexpected size: $($image.Width)x$($image.Height)"
        }
        # The material quad covers the viewport, but Intel OpenGL 3.3 can leave
        # a few outer-edge fragments to the production clear color.  Sample an
        # authored interior texel so the contract measures material residency
        # instead of implementation-defined triangle-edge rasterization.
        $backgroundAnchor = $image.GetPixel(8, 8)
        if ($backgroundAnchor.R -ne 17 -or $backgroundAnchor.G -ne 18 -or
                $backgroundAnchor.B -ne 19) {
            throw "Stage9 formal capture rejects submitted background mismatch: $backgroundAnchor"
        }

        # These whole-frame metrics are intentionally independent of any Stage11-C
        # panel rectangle.  Fresh native fire-room references measured at least
        # 1152 colors, 3433 non-background samples, 3351 2D transitions, a 253875
        # luminance span, 2951 dark samples and 607 mid-tone samples.  The gates
        # retain wide margins while rejecting sparse or exposure-only mutations.
        $uniqueColors = [System.Collections.Generic.HashSet[int]]::new()
        $nonBackgroundSamples = 0
        $transitions = 0
        $minimumLuminance = [int]::MaxValue
        $maximumLuminance = [int]::MinValue
        $darkSamples = 0
        $midToneSamples = 0
        $previousRow = [int[]]::new(80)
        $hasPreviousRow = $false
        for ($y = 0; $y -lt $image.Height; $y += 16) {
            $column = 0
            $leftColor = -1
            for ($x = 0; $x -lt $image.Width; $x += 16) {
                $pixel = $image.GetPixel($x, $y)
                $packed = ([int]$pixel.R -shl 16) -bor
                    ([int]$pixel.G -shl 8) -bor [int]$pixel.B
                [void]$uniqueColors.Add($packed)
                if ($pixel.R -ne 17 -or $pixel.G -ne 18 -or $pixel.B -ne 19) {
                    $nonBackgroundSamples++
                }
                $luminance = $pixel.R * 299 + $pixel.G * 587 + $pixel.B * 114
                $minimumLuminance = [Math]::Min($minimumLuminance, $luminance)
                $maximumLuminance = [Math]::Max($maximumLuminance, $luminance)
                if ($luminance -le 35000) {
                    $darkSamples++
                } elseif ($luminance -lt 220000) {
                    $midToneSamples++
                }
                if (($leftColor -ne -1 -and $leftColor -ne $packed) -or
                        ($hasPreviousRow -and $previousRow[$column] -ne $packed)) {
                    $transitions++
                }
                $leftColor = $packed
                $previousRow[$column] = $packed
                $column++
            }
            $hasPreviousRow = $true
        }

        $luminanceRange = $maximumLuminance - $minimumLuminance
        if ($uniqueColors.Count -lt 24) {
            throw "Stage9 formal capture rejects low color diversity: colors=$($uniqueColors.Count)"
        }
        if ($nonBackgroundSamples -lt 200) {
            throw "Stage9 formal capture rejects insufficient content: non_background=$nonBackgroundSamples"
        }
        if ($transitions -lt 400) {
            throw "Stage9 formal capture rejects missing 2d structure: transitions=$transitions"
        }
        if ($luminanceRange -lt 150000) {
            throw "Stage9 formal capture rejects insufficient luminance range: luminance_range=$luminanceRange"
        }
        if ($darkSamples -lt 500 -or $midToneSamples -lt 500) {
            throw "Stage9 formal capture rejects unbalanced exposure: dark=$darkSamples mid_tone=$midToneSamples"
        }
        Write-Output "formal_capture=PASS colors=$($uniqueColors.Count) non_background=$nonBackgroundSamples transitions=$transitions luminance_range=$luminanceRange dark=$darkSamples mid_tone=$midToneSamples path=$Path"
    } finally {
        $image.Dispose()
    }
}

Add-Type -AssemblyName System.Drawing
if (-not [string]::IsNullOrWhiteSpace($ValidateOnlyPath)) {
    if (-not (Test-Path -LiteralPath $ValidateOnlyPath -PathType Leaf)) {
        throw "Stage9 formal capture validation path is missing: $ValidateOnlyPath"
    }
    if ($MinimumWriteTimeUtc -ne [DateTime]::MinValue -and
            (Get-Item -LiteralPath $ValidateOnlyPath).LastWriteTimeUtc -lt
                $MinimumWriteTimeUtc.ToUniversalTime()) {
        throw "Stage9 formal capture rejects stale capture: $ValidateOnlyPath"
    }
    Test-Stage9FormalCapture -Path $ValidateOnlyPath
    return
}

if ([string]::IsNullOrWhiteSpace($Executable) -or
        [string]::IsNullOrWhiteSpace($CapturePath)) {
    throw 'Executable and CapturePath are required outside validation-only mode'
}

$runStartedUtc = [DateTime]::UtcNow
if (Test-Path -LiteralPath $CapturePath -PathType Leaf) {
    Remove-Item -LiteralPath $CapturePath -Force
} elseif (Test-Path -LiteralPath $CapturePath) {
    throw "Formal game capture path is not a file: $CapturePath"
}

& $Executable
if ($LASTEXITCODE -ne 0) {
    throw "Stage 9 formal game validation exited with code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $CapturePath -PathType Leaf)) {
    throw "Formal game did not capture its submitted frame: $CapturePath"
}
$captureItem = Get-Item -LiteralPath $CapturePath
if ($captureItem.LastWriteTimeUtc -lt $runStartedUtc) {
    throw "Stage9 formal capture rejects stale capture: $CapturePath"
}
Test-Stage9FormalCapture -Path $CapturePath
