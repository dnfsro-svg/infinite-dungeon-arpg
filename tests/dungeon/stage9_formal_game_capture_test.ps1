param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,
    [Parameter(Mandatory = $true)]
    [string]$CapturePath
)

$ErrorActionPreference = 'Stop'

& $Executable
if ($LASTEXITCODE -ne 0) {
    throw "Stage 9 formal game validation exited with code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $CapturePath)) {
    throw "Formal game did not capture its submitted frame: $CapturePath"
}

Add-Type -AssemblyName System.Drawing
$image = [System.Drawing.Bitmap]::new($CapturePath)
try {
    if ($image.Width -ne 1280 -or $image.Height -ne 720) {
        throw "Unexpected formal capture size: $($image.Width)x$($image.Height)"
    }
    $uniqueColors = [System.Collections.Generic.HashSet[int]]::new()
    $nonBackgroundSamples = 0
    $brightSamples = 0
    for ($y = 0; $y -lt $image.Height; $y += 12) {
        for ($x = 0; $x -lt $image.Width; $x += 12) {
            $pixel = $image.GetPixel($x, $y)
            $packed = ($pixel.R -shl 16) -bor ($pixel.G -shl 8) -bor $pixel.B
            [void]$uniqueColors.Add($packed)
            if ($pixel.R -ne 13 -or $pixel.G -ne 17 -or $pixel.B -ne 27) {
                $nonBackgroundSamples++
            }
            if ($pixel.R -ge 180 -and $pixel.G -ge 180 -and $pixel.B -ge 180) {
                $brightSamples++
            }
        }
    }
    if (($uniqueColors.Count -lt 24) -or ($nonBackgroundSamples -lt 200) -or ($brightSamples -lt 12)) {
        throw "Formal game capture is blank or not submitted content: colors=$($uniqueColors.Count) non_background=$nonBackgroundSamples bright=$brightSamples"
    }
    Write-Output "formal_capture=PASS colors=$($uniqueColors.Count) non_background=$nonBackgroundSamples bright=$brightSamples path=$CapturePath"
} finally {
    $image.Dispose()
}
