param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,
    [Parameter(Mandatory = $true)]
    [string]$CapturePath
)

$ErrorActionPreference = 'Stop'
Remove-Item -LiteralPath $CapturePath -Force -ErrorAction SilentlyContinue

& $Executable --capture-at-frame-60-and-exit
if ($LASTEXITCODE -ne 0) {
    throw "Stage 9 validation game exited with code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $CapturePath)) {
    throw "Stage 9 validation game did not produce $CapturePath"
}

Add-Type -AssemblyName System.Drawing
$image = [System.Drawing.Bitmap]::new($CapturePath)
try {
    if ($image.Width -ne 1280 -or $image.Height -ne 720) {
        throw "Unexpected capture size: $($image.Width)x$($image.Height)"
    }
    $corner = $image.GetPixel(0, 0)
    if ($corner.R -ne 13 -or $corner.G -ne 17 -or $corner.B -ne 27) {
        throw "Capture did not contain the submitted Stage 9 background: $corner"
    }
    $brightPixels = 0
    for ($y = 0; $y -lt $image.Height; $y += 8) {
        for ($x = 0; $x -lt $image.Width; $x += 8) {
            $pixel = $image.GetPixel($x, $y)
            if ($pixel.R -ge 200 -and $pixel.G -ge 200 -and $pixel.B -ge 200) {
                $brightPixels++
            }
        }
    }
    if ($brightPixels -lt 50) {
        throw "Capture did not contain the presented Stage 9 UI; bright sampled pixels: $brightPixels"
    }
} finally {
    $image.Dispose()
}
