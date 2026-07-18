param(
    [Parameter(Mandatory = $true)]
    [string]$Validator,
    [Parameter(Mandatory = $true)]
    [string]$ReferenceImage,
    [Parameter(Mandatory = $true)]
    [string]$MutationRoot
)

$ErrorActionPreference = 'Stop'

function Invoke-Stage9Validator {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [string]$ExpectedFailure,
        [DateTime]$MinimumWriteTimeUtc = [DateTime]::MinValue
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & powershell -NoProfile -ExecutionPolicy Bypass -File $Validator `
            -ValidateOnlyPath $Path -MinimumWriteTimeUtc $MinimumWriteTimeUtc 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $text = ($output | Out-String)
    if ([string]::IsNullOrEmpty($ExpectedFailure)) {
        if ($exitCode -ne 0) {
            throw "Stage9 validator rejected valid structured capture: $text"
        }
        return
    }
    if ($exitCode -eq 0) {
        throw "Stage9 validator accepted mutation '$ExpectedFailure'"
    }
    if ($text -notmatch [Regex]::Escape($ExpectedFailure)) {
        throw "Stage9 validator rejected mutation for the wrong reason; expected '$ExpectedFailure': $text"
    }
}

function New-Stage9Bitmap {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][scriptblock]$Draw
    )
    $bitmap = [System.Drawing.Bitmap]::new(1280, 720)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            & $Draw $graphics
        } finally {
            $graphics.Dispose()
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $bitmap.Dispose()
    }
}

if (-not (Test-Path -LiteralPath $ReferenceImage -PathType Leaf)) {
    throw "Stage9 validator self-test reference is missing: $ReferenceImage"
}
New-Item -ItemType Directory -Path $MutationRoot -Force | Out-Null
Add-Type -AssemblyName System.Drawing
$background = [System.Drawing.Color]::FromArgb(22, 27, 39)

Invoke-Stage9Validator -Path $ReferenceImage

$wrongSize = Join-Path $MutationRoot 'wrong-size.png'
$small = [System.Drawing.Bitmap]::new(640, 360)
try {
    $small.Save($wrongSize, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $small.Dispose()
}
Invoke-Stage9Validator -Path $wrongSize -ExpectedFailure 'unexpected size'

Invoke-Stage9Validator -Path $ReferenceImage -ExpectedFailure 'stale capture' `
    -MinimumWriteTimeUtc ([DateTime]::UtcNow.AddMinutes(5))

$blank = Join-Path $MutationRoot 'blank.png'
New-Stage9Bitmap -Path $blank -Draw {
    param($graphics)
    $graphics.Clear($background)
}
Invoke-Stage9Validator -Path $blank -ExpectedFailure 'low color diversity'

$lowColor = Join-Path $MutationRoot 'low-color.png'
New-Stage9Bitmap -Path $lowColor -Draw {
    param($graphics)
    $graphics.Clear($background)
    $red = [System.Drawing.SolidBrush]::new(
        [System.Drawing.Color]::FromArgb(150, 35, 40))
    $blue = [System.Drawing.SolidBrush]::new(
        [System.Drawing.Color]::FromArgb(35, 90, 150))
    try {
        $graphics.FillRectangle($red, 160, 120, 480, 480)
        $graphics.FillRectangle($blue, 640, 120, 480, 480)
    } finally {
        $red.Dispose()
        $blue.Dispose()
    }
}
Invoke-Stage9Validator -Path $lowColor -ExpectedFailure 'low color diversity'

$sparseNoise = Join-Path $MutationRoot 'sparse-noise.png'
New-Stage9Bitmap -Path $sparseNoise -Draw {
    param($graphics)
    $graphics.Clear($background)
    $field = [System.Drawing.SolidBrush]::new(
        [System.Drawing.Color]::FromArgb(55, 58, 64))
    try {
        $graphics.FillRectangle($field, 160, 160, 960, 400)
    } finally {
        $field.Dispose()
    }
    for ($index = 0; $index -lt 24; ++$index) {
        $red = if ($index -eq 23) { 245 } else { 70 + (($index * 29) % 130) }
        $green = 45 + (($index * 47) % 150)
        $blue = 40 + (($index * 61) % 155)
        $brush = [System.Drawing.SolidBrush]::new(
            [System.Drawing.Color]::FromArgb($red, $green, $blue))
        try {
            $graphics.FillRectangle($brush, 192 + ($index * 32), 192, 17, 17)
        } finally {
            $brush.Dispose()
        }
    }
}
Invoke-Stage9Validator -Path $sparseNoise -ExpectedFailure 'missing 2d structure'

$tooDark = Join-Path $MutationRoot 'too-dark.png'
$darkBitmap = [System.Drawing.Bitmap]::new($ReferenceImage)
try {
    for ($y = 0; $y -lt $darkBitmap.Height; ++$y) {
        for ($x = 0; $x -lt $darkBitmap.Width; ++$x) {
            $pixel = $darkBitmap.GetPixel($x, $y)
            $darkBitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(
                [int]($pixel.R / 2),
                [int]($pixel.G / 2),
                [int]($pixel.B / 2)))
        }
    }
    $darkBitmap.SetPixel(0, 0, $background)
    $darkBitmap.Save($tooDark, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $darkBitmap.Dispose()
}
Invoke-Stage9Validator -Path $tooDark -ExpectedFailure 'insufficient luminance range'

$tooBright = Join-Path $MutationRoot 'too-bright.png'
$brightBitmap = [System.Drawing.Bitmap]::new(1280, 720)
try {
    for ($y = 0; $y -lt $brightBitmap.Height; ++$y) {
        for ($x = 0; $x -lt $brightBitmap.Width; ++$x) {
            $brightBitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(
                230 + (([int]($x / 16) + [int]($y / 16)) % 24),
                232 + (([int]($x / 16) * 3 + [int]($y / 16)) % 20),
                234 + (([int]($x / 16) + [int]($y / 16) * 5) % 18)))
        }
    }
    $brightBitmap.SetPixel(0, 0, $background)
    $brightBitmap.Save($tooBright, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $brightBitmap.Dispose()
}
Invoke-Stage9Validator -Path $tooBright -ExpectedFailure 'unbalanced exposure'

Write-Output 'stage9_formal_capture_validator_self_test=PASS mutations=7'
