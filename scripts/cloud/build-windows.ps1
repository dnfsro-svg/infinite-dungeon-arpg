[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$BuildDirectory = 'out/build/cloud-windows'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$buildPath = Join-Path $repoRoot $BuildDirectory

Write-Host "[cloud] configure $Configuration build in $buildPath"
cmake -S $repoRoot -B $buildPath -G 'Visual Studio 17 2022' -A x64 -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }

cmake --build $buildPath --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed: $LASTEXITCODE" }

ctest --test-dir $buildPath -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "CTest failed: $LASTEXITCODE" }

$game = Get-ChildItem -LiteralPath $buildPath -Filter 'arpg_game.exe' -Recurse -File |
    Select-Object -First 1
if (-not $game) {
    throw 'Build completed but arpg_game.exe was not found.'
}

$artifactDirectory = Join-Path $repoRoot 'out/artifacts/arpg-windows'
New-Item -ItemType Directory -Force -Path $artifactDirectory | Out-Null
Copy-Item -LiteralPath $game.FullName -Destination $artifactDirectory -Force

foreach ($assetName in @('assets', 'resources')) {
    $assetPath = Join-Path $repoRoot $assetName
    if (Test-Path -LiteralPath $assetPath) {
        Copy-Item -LiteralPath $assetPath -Destination $artifactDirectory -Recurse -Force
    }
}

Write-Host "[cloud] artifact ready: $artifactDirectory"
