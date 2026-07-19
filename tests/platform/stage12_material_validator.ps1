param(
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDirectory
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $EvidenceDirectory -PathType Container)) {
    throw "missing evidence directory: $EvidenceDirectory"
}

function Read-Report([string]$Path) {
    $values = @{}
    Get-Content -LiteralPath $Path -Encoding UTF8 | ForEach-Object {
        $pair = $_ -split '=', 2
        if ($pair.Count -eq 2) { $values[$pair[0]] = $pair[1] }
    }
    return $values
}

function Read-PngSize([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 24 -or $bytes[0] -ne 137 -or $bytes[1] -ne 80 -or
            $bytes[2] -ne 78 -or $bytes[3] -ne 71) { throw "invalid PNG: $Path" }
    $width = [uint32]$bytes[16] * 16777216 + [uint32]$bytes[17] * 65536 +
        [uint32]$bytes[18] * 256 + [uint32]$bytes[19]
    $height = [uint32]$bytes[20] * 16777216 + [uint32]$bytes[21] * 65536 +
        [uint32]$bytes[22] * 256 + [uint32]$bytes[23]
    return @($width, $height)
}

$reportPath = Join-Path $EvidenceDirectory 'stage12-material-evidence.txt'
if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { throw 'missing material report' }
$report = Read-Report $reportPath
foreach ($key in @('manifest','atlas_bytes','fallback','input_hole_regression',
        'monsters','screenshot_isolation','result')) {
    if (-not $report.ContainsKey($key)) { throw "missing report field: $key" }
}
if ($report.result -ne 'pass' -or $report.manifest -ne 'pass' -or
        $report.fallback -ne 'pass' -or $report.input_hole_regression -ne 'pass' -or
        $report.screenshot_isolation -ne 'pass' -or [int]$report.monsters -ne 8) {
    throw 'formal material report rejected'
}
if ([uint64]$report.atlas_bytes -gt 67108864) { throw 'texture budget exceeded' }

foreach ($expected in @(@('game-1280x720.png',1280,720),
        @('game-1920x1080.png',1920,1080), @('fallback-1280x720.png',1280,720))) {
    $path = Join-Path $EvidenceDirectory $expected[0]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "missing screenshot: $($expected[0])" }
    if ((Get-Item -LiteralPath $path).Length -le 1024) { throw "empty screenshot: $($expected[0])" }
    $size = Read-PngSize $path
    if ($size[0] -ne $expected[1] -or $size[1] -ne $expected[2]) {
        throw "wrong screenshot dimensions: $($expected[0]) $($size[0])x$($size[1])"
    }
}

$assetRoot = Join-Path $PSScriptRoot '..\..\assets\stage12'
$expectedAtlases = @(@('environment.png',1024,1024), @('actors.png',2048,2048),
    @('effects_ui.png',1024,1024))
foreach ($expected in $expectedAtlases) {
    $path = Join-Path $assetRoot $expected[0]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "missing atlas: $($expected[0])" }
    $size = Read-PngSize $path
    if ($size[0] -ne $expected[1] -or $size[1] -ne $expected[2]) {
        throw "wrong atlas dimensions: $($expected[0])"
    }
}
Write-Output 'stage12 material evidence validated: screenshots, atlases, fallback, input/hole, eight monsters'
