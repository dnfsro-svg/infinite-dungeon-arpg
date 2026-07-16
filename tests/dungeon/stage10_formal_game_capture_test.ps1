param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDirectory
)

$ErrorActionPreference = 'Stop'
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
Add-Type -AssemblyName System.Drawing
foreach ($name in $names) {
    $path = Join-Path $EvidenceDirectory $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing formal Stage 10 capture: $path"
    }
    if ((Get-Item -LiteralPath $path).LastWriteTimeUtc -lt $started) {
        throw "Formal Stage 10 capture is stale: $path"
    }
    $image = [System.Drawing.Bitmap]::new($path)
    try {
        if ($image.Width -ne 1280 -or $image.Height -ne 720) {
            throw "Unexpected formal capture size: $path $($image.Width)x$($image.Height)"
        }
        $colors = [System.Collections.Generic.HashSet[int]]::new()
        $nonBackground = 0
        for ($y = 0; $y -lt $image.Height; $y += 16) {
            for ($x = 0; $x -lt $image.Width; $x += 16) {
                $pixel = $image.GetPixel($x, $y)
                [void]$colors.Add(($pixel.R -shl 16) -bor ($pixel.G -shl 8) -bor $pixel.B)
                if ($pixel.R -ne 13 -or $pixel.G -ne 17 -or $pixel.B -ne 27) {
                    $nonBackground++
                }
            }
        }
        if ($colors.Count -lt 20 -or $nonBackground -lt 100) {
            throw "Formal capture is blank: $path colors=$($colors.Count) non_background=$nonBackground"
        }
    } finally {
        $image.Dispose()
    }
}
Write-Output "stage10_formal_captures=PASS count=$($names.Count) directory=$EvidenceDirectory"
