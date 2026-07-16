param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,
    [Parameter(Mandatory = $true)]
    [string]$CapturePath
)

$ErrorActionPreference = 'Stop'
Remove-Item -LiteralPath $CapturePath -Force -ErrorAction SilentlyContinue
$started = [DateTime]::UtcNow
& $Executable
if ($LASTEXITCODE -ne 0) {
    throw "Stage 10 validation game exited with code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $CapturePath -PathType Leaf)) {
    throw "Stage 10 validation game did not produce $CapturePath"
}
if ((Get-Item -LiteralPath $CapturePath).LastWriteTimeUtc -lt $started) {
    throw "Stage 10 capture is stale: $CapturePath"
}

Add-Type -AssemblyName System.Drawing
$image = [System.Drawing.Bitmap]::new($CapturePath)
try {
    if ($image.Width -ne 1280 -or $image.Height -ne 720) {
        throw "Unexpected Stage 10 capture size: $($image.Width)x$($image.Height)"
    }
    $colors = [System.Collections.Generic.HashSet[int]]::new()
    $nonBackground = 0
    for ($y = 0; $y -lt $image.Height; $y += 12) {
        for ($x = 0; $x -lt $image.Width; $x += 12) {
            $pixel = $image.GetPixel($x, $y)
            [void]$colors.Add(($pixel.R -shl 16) -bor ($pixel.G -shl 8) -bor $pixel.B)
            if ($pixel.R -ne 13 -or $pixel.G -ne 17 -or $pixel.B -ne 27) {
                $nonBackground++
            }
        }
    }
    if ($colors.Count -lt 24 -or $nonBackground -lt 200) {
        throw "Stage 10 capture is blank: colors=$($colors.Count) non_background=$nonBackground"
    }
} finally {
    $image.Dispose()
}
