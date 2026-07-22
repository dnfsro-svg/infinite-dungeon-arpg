param(
    [Parameter(Mandatory = $true)][string]$EvidenceDirectory,
    [Parameter(Mandatory = $true)][string]$Validator,
    [Parameter(Mandatory = $true)][string]$MutationRoot
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function New-Mutation([string]$Name) {
    $target = Join-Path $MutationRoot $Name
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Copy-Item -Path (Join-Path $EvidenceDirectory '*') `
        -Destination $target -Recurse -Force
    return $target
}

function Save-MutatedBitmap([string]$Path, [scriptblock]$Mutation) {
    $source = [System.Drawing.Bitmap]::FromFile($Path)
    try { $copy = [System.Drawing.Bitmap]::new($source) }
    finally { $source.Dispose() }
    try {
        & $Mutation $copy
        $temporary = $Path + '.mutated.png'
        $copy.Save($temporary, [System.Drawing.Imaging.ImageFormat]::Png)
        Move-Item -LiteralPath $temporary -Destination $Path -Force
    } finally { $copy.Dispose() }
}

$mutations = @()
$solidItems = New-Mutation 'solid-gray-items'
Save-MutatedBitmap (Join-Path $solidItems 'items-materials-1280x720.png') {
    param($bitmap)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try { $graphics.Clear([System.Drawing.Color]::FromArgb(255, 64, 64, 64)) }
    finally { $graphics.Dispose() }
}
$mutations += @{ Name='solid-gray-items'; Path=$solidItems }

$noItems = New-Mutation 'no-item-capture'
Copy-Item -LiteralPath (Join-Path $noItems 'items-baseline-1280x720.png') `
    -Destination (Join-Path $noItems 'items-materials-1280x720.png') -Force
$mutations += @{ Name='no-item-capture'; Path=$noItems }

$missingItemRuntime = New-Mutation 'missing-item-runtime-draw'
$reportPath = Join-Path $missingItemRuntime 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
    'item_runtime_draws=pass', 'item_runtime_draws=fail') |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='missing-item-runtime-draw'; Path=$missingItemRuntime }

$hiddenUi = New-Mutation 'hidden-ui-gallery'
Copy-Item -LiteralPath (Join-Path $hiddenUi 'ui-baseline-1280x720.png') `
    -Destination (Join-Path $hiddenUi 'ui-gallery-1280x720.png') -Force
$mutations += @{ Name='hidden-ui-gallery'; Path=$hiddenUi }

$missingUiRuntime = New-Mutation 'missing-ui-runtime-draw'
$reportPath = Join-Path $missingUiRuntime 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
    'ui_runtime_draws=pass', 'ui_runtime_draws=fail') |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='missing-ui-runtime-draw'; Path=$missingUiRuntime }

foreach ($page in @('hud','inventory','skill','pause')) {
    $missingPageRuntime = New-Mutation "missing-$page-ui-runtime-draw"
    $reportPath = Join-Path $missingPageRuntime 'stage12-material-evidence.txt'
    (Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
        "${page}_ui_runtime_draws=pass", "${page}_ui_runtime_draws=fail") |
        Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
    $mutations += @{ Name="missing-$page-ui-runtime-draw"; Path=$missingPageRuntime }
}

foreach ($page in @('hud','inventory','skill','pause')) {
    $missingPageRuntime = New-Mutation "missing-$page-ui-runtime-draw-1920"
    $reportPath = Join-Path $missingPageRuntime 'stage12-material-evidence.txt'
    (Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
        "${page}_ui_runtime_draws_1920=pass",
        "${page}_ui_runtime_draws_1920=fail") |
        Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
    $mutations += @{
        Name="missing-$page-ui-runtime-draw-1920"; Path=$missingPageRuntime }
}

$missingBundledFont = New-Mutation 'missing-bundled-font-runtime'
$reportPath = Join-Path $missingBundledFont 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
    'bundled_font_runtime=pass', 'bundled_font_runtime=fail') |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='missing-bundled-font-runtime'; Path=$missingBundledFont }

$emptyFontSubset = New-Mutation 'empty-bundled-font-subset'
$reportPath = Join-Path $emptyFontSubset 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace
    'bundled_font_glyph_count=\d+', 'bundled_font_glyph_count=0' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='empty-bundled-font-subset'; Path=$emptyFontSubset }

$lowResolutionFont = New-Mutation 'low-resolution-bundled-font'
$reportPath = Join-Path $lowResolutionFont 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace
    'bundled_font_source_base_size=\d+', 'bundled_font_source_base_size=32' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='low-resolution-bundled-font'; Path=$lowResolutionFont }

$oversizeFontAtlas = New-Mutation 'oversize-bundled-font-atlas'
$reportPath = Join-Path $oversizeFontAtlas 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace
    'bundled_font_atlas_bytes=\d+', 'bundled_font_atlas_bytes=999999999' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='oversize-bundled-font-atlas'; Path=$oversizeFontAtlas }

$lowTextContrast = New-Mutation 'low-ui-text-contrast'
$reportPath = Join-Path $lowTextContrast 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
    'ui_text_contrast=pass', 'ui_text_contrast=fail') |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='low-ui-text-contrast'; Path=$lowTextContrast }

$gradientTextFill = New-Mutation 'non-solid-ui-text-fill'
$reportPath = Join-Path $gradientTextFill 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
    'ui_text_solid_fill=pass', 'ui_text_solid_fill=fail') |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='non-solid-ui-text-fill'; Path=$gradientTextFill }

foreach ($suffix in @('', '_1920')) {
    foreach ($page in @('hud','inventory','skill','pause')) {
        $stretch = New-Mutation "stretched-$page$suffix"
        $reportPath = Join-Path $stretch 'stage12-material-evidence.txt'
        (Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
            "${page}_decorative_stretch${suffix}=pass",
            "${page}_decorative_stretch${suffix}=fail") |
            Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
        $mutations += @{ Name="stretched-$page$suffix"; Path=$stretch }

        $overlap = New-Mutation "overlap-$page$suffix"
        $reportPath = Join-Path $overlap 'stage12-material-evidence.txt'
        (Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
            "${page}_text_layout${suffix}=pass",
            "${page}_text_layout${suffix}=fail") |
            Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
        $mutations += @{ Name="overlap-$page$suffix"; Path=$overlap }
    }
}

$fallbackHud = New-Mutation 'fallback-hud-ui'
Copy-Item -LiteralPath (Join-Path $fallbackHud 'ui-baseline-1280x720.png') `
    -Destination (Join-Path $fallbackHud 'ui-hud-1280x720.png') -Force
$mutations += @{ Name='fallback-hud-ui'; Path=$fallbackHud }

$fallbackInventory = New-Mutation 'fallback-inventory-ui'
Copy-Item -LiteralPath (Join-Path $fallbackInventory 'ui-baseline-1280x720.png') `
    -Destination (Join-Path $fallbackInventory 'ui-inventory-1280x720.png') -Force
$mutations += @{ Name='fallback-inventory-ui'; Path=$fallbackInventory }

$fallbackSkill = New-Mutation 'fallback-skill-ui'
Copy-Item -LiteralPath (Join-Path $fallbackSkill 'ui-baseline-1280x720.png') `
    -Destination (Join-Path $fallbackSkill 'ui-skill-stones-1280x720.png') -Force
$mutations += @{ Name='fallback-skill-ui'; Path=$fallbackSkill }

$fallbackPause = New-Mutation 'fallback-pause-ui'
Copy-Item -LiteralPath (Join-Path $fallbackPause 'ui-baseline-1280x720.png') `
    -Destination (Join-Path $fallbackPause 'ui-pause-1280x720.png') -Force
$mutations += @{ Name='fallback-pause-ui'; Path=$fallbackPause }

foreach ($page in @(
        @('hud','ui-hud-1920x1080.png'),
        @('inventory','ui-inventory-1920x1080.png'),
        @('skill','ui-skill-stones-1920x1080.png'),
        @('pause','ui-pause-1920x1080.png'))) {
    $fallbackPage = New-Mutation "fallback-$($page[0])-ui-1920"
    Copy-Item -LiteralPath (Join-Path $fallbackPage 'ui-baseline-1920x1080.png') `
        -Destination (Join-Path $fallbackPage $page[1]) -Force
    $mutations += @{
        Name="fallback-$($page[0])-ui-1920"; Path=$fallbackPage }
}

$solid = New-Mutation 'solid-gray'
Save-MutatedBitmap (Join-Path $solid 'lightning-monsters-1280x720.png') {
    param($bitmap)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try { $graphics.Clear([System.Drawing.Color]::FromArgb(255, 64, 64, 64)) }
    finally { $graphics.Dispose() }
}
$mutations += @{ Name='solid-gray'; Path=$solid }

$wrongEcology = New-Mutation 'wrong-ecology'
Copy-Item -LiteralPath (Join-Path $wrongEcology 'water-monsters-1280x720.png') `
    -Destination (Join-Path $wrongEcology 'lightning-monsters-1280x720.png') -Force
$mutations += @{ Name='wrong-ecology'; Path=$wrongEcology }

$missingMonsters = New-Mutation 'missing-lightning-monsters'
$background = [System.Drawing.Bitmap]::FromFile(
    (Join-Path $missingMonsters 'lightning-background-1280x720.png'))
try {
Save-MutatedBitmap (Join-Path $missingMonsters 'lightning-monsters-1280x720.png') {
    param($bitmap)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        foreach ($region in @(
                [System.Drawing.Rectangle]::new(520, 390, 115, 155),
                [System.Drawing.Rectangle]::new(635, 390, 115, 155))) {
            $graphics.DrawImage($background, $region, $region,
                [System.Drawing.GraphicsUnit]::Pixel)
        }
    } finally {
        $graphics.Dispose()
    }
}
} finally { $background.Dispose() }
$mutations += @{ Name='missing-lightning-monsters'; Path=$missingMonsters }

$missingRuntime = New-Mutation 'missing-runtime-draw'
$reportPath = Join-Path $missingRuntime 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
    'lightning_shooter_drawn=pass', 'lightning_shooter_drawn=fail') |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='missing-runtime-draw'; Path=$missingRuntime }

$invalidFrame = New-Mutation 'invalid-runtime-frame'
$reportPath = Join-Path $invalidFrame 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace
    'lightning_dasher_frame=\d+', 'lightning_dasher_frame=65535' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='invalid-runtime-frame'; Path=$invalidFrame }

$solidChaos = New-Mutation 'solid-gray-chaos'
Save-MutatedBitmap (Join-Path $solidChaos 'chaos-monsters-1280x720.png') {
    param($bitmap)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try { $graphics.Clear([System.Drawing.Color]::FromArgb(255, 64, 64, 64)) }
    finally { $graphics.Dispose() }
}
$mutations += @{ Name='solid-gray-chaos'; Path=$solidChaos }

$wrongChaos = New-Mutation 'wrong-chaos-ecology'
Copy-Item -LiteralPath (Join-Path $wrongChaos 'lightning-monsters-1280x720.png') `
    -Destination (Join-Path $wrongChaos 'chaos-monsters-1280x720.png') -Force
$mutations += @{ Name='wrong-chaos-ecology'; Path=$wrongChaos }

$missingChaos = New-Mutation 'missing-chaos-monsters'
$chaosBackground = [System.Drawing.Bitmap]::FromFile(
    (Join-Path $missingChaos 'chaos-background-1280x720.png'))
try {
Save-MutatedBitmap (Join-Path $missingChaos 'chaos-monsters-1280x720.png') {
    param($bitmap)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        foreach ($region in @(
                [System.Drawing.Rectangle]::new(635, 390, 115, 155),
                [System.Drawing.Rectangle]::new(750, 390, 115, 155))) {
            $graphics.DrawImage($chaosBackground, $region, $region,
                [System.Drawing.GraphicsUnit]::Pixel)
        }
    } finally { $graphics.Dispose() }
}
} finally { $chaosBackground.Dispose() }
$mutations += @{ Name='missing-chaos-monsters'; Path=$missingChaos }

$missingChaosRuntime = New-Mutation 'missing-chaos-runtime-draw'
$reportPath = Join-Path $missingChaosRuntime 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
    'chaos_chaser_drawn=pass', 'chaos_chaser_drawn=fail') |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='missing-chaos-runtime-draw'; Path=$missingChaosRuntime }

$invalidChaosFrame = New-Mutation 'invalid-chaos-runtime-frame'
$reportPath = Join-Path $invalidChaosFrame 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace `
    'chaos_hazard_frame=\d+', 'chaos_hazard_frame=65535' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='invalid-chaos-runtime-frame'; Path=$invalidChaosFrame }

$failures = @()
foreach ($mutation in $mutations) {
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Validator `
        -EvidenceDirectory $mutation.Path *> $null
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    if ($exitCode -eq 0) {
        $failures += "validator accepted mutation: $($mutation.Name)"
    }
}
if ($failures.Count -ne 0) { throw ($failures -join [Environment]::NewLine) }
Write-Output 'stage12 material validator rejected hidden/fallback UI, missing UI/item telemetry, solid/no-item evidence, wrong ecology, and invalid runtime draw/frame proof'
