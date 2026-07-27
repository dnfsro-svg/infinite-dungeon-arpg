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

function Get-Sha256([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try { $digest = $sha256.ComputeHash($stream) }
        finally { $sha256.Dispose() }
    } finally { $stream.Dispose() }
    return [System.BitConverter]::ToString($digest).Replace(
        '-', '').ToLowerInvariant()
}

function Read-ReportRoi([string]$Directory, [string]$Key) {
    $report = Get-Content -Raw -LiteralPath (Join-Path $Directory `
        'stage12-material-evidence.txt') -Encoding UTF8
    $match = [regex]::Match($report,
        "(?m)^$([regex]::Escape($Key))=(\d+),(\d+),(\d+),(\d+)\r?$")
    if (-not $match.Success) { throw "missing ROI report field: $Key" }
    return [System.Drawing.Rectangle]::new(
        [int]$match.Groups[1].Value, [int]$match.Groups[2].Value,
        [int]$match.Groups[3].Value, [int]$match.Groups[4].Value)
}

function Replace-Required([string]$Content, [string]$OldValue,
        [string]$NewValue, [string]$MutationName) {
    if (-not $Content.Contains($OldValue)) {
        throw "mutation source text missing: $MutationName"
    }
    $updated = $Content.Replace($OldValue, $NewValue)
    if ($updated -eq $Content) { throw "mutation was a no-op: $MutationName" }
    return $updated
}

function Replace-RegexRequired([string]$Content, [string]$Pattern,
        [string]$Replacement, [string]$MutationName) {
    $updated = [regex]::Replace($Content, $Pattern, $Replacement)
    if ($updated -eq $Content) { throw "mutation was a no-op: $MutationName" }
    return $updated
}

function Invoke-Validator([string]$Directory,
        [string]$PublishedItemAtlas = '') {
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $validatorArguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass',
            '-File', $Validator, '-EvidenceDirectory', $Directory)
        if (-not [string]::IsNullOrWhiteSpace($PublishedItemAtlas)) {
            $validatorArguments += @('-ItemAtlasPath', $PublishedItemAtlas)
        }
        & powershell.exe @validatorArguments *> $null
        return [int]$LASTEXITCODE
    } finally { $ErrorActionPreference = $savedPreference }
}

$pristineExitCode = Invoke-Validator $EvidenceDirectory
if ($pristineExitCode -ne 0) {
    throw "validator rejected pristine evidence: exit=$pristineExitCode"
}

$mutations = @()

function New-AtlasPaletteCollapse([string]$Name, [int]$Cell) {
    $source = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot `
        '..\..\assets\stage12\items_ui.png'))
    $target = Join-Path $MutationRoot "$Name-items_ui.png"
    Copy-Item -LiteralPath $source -Destination $target -Force
    Save-MutatedBitmap $target {
        param($bitmap)
        [int]$originX = ($Cell % 8) * 128
        [int]$originY = [Math]::Floor($Cell / 8) * 128
        for ([int]$y = 0; $y -lt 128; ++$y) {
            for ([int]$x = 0; $x -lt 128; ++$x) {
                $pixel = $bitmap.GetPixel($originX + $x, $originY + $y)
                if ($pixel.A -ge 96) {
                    $bitmap.SetPixel($originX + $x, $originY + $y,
                        [System.Drawing.Color]::FromArgb(
                            $pixel.A, 96, 96, 96))
                }
            }
        }
    }
    return $target
}

foreach ($atlasMutation in @(
        @{Name='collapsed-item-atlas-material0-palette'; Cell=12},
        @{Name='collapsed-item-atlas-material6-palette'; Cell=18},
        @{Name='collapsed-item-atlas-material9-palette'; Cell=21},
        @{Name='collapsed-item-atlas-material10-palette'; Cell=22})) {
    $mutations += @{
        Name = $atlasMutation.Name
        Path = $EvidenceDirectory
        ItemAtlasPath = New-AtlasPaletteCollapse `
            $atlasMutation.Name $atlasMutation.Cell
    }
}

function New-PartialItemDegradation([string]$Name,
        [int]$X, [int]$Y, [int]$Width, [int]$Height) {
    $target = New-Mutation $Name
    $baseline = [System.Drawing.Bitmap]::FromFile(
        (Join-Path $target 'items-icons-baseline-1280x720.png'))
    try {
        $region = [System.Drawing.Rectangle]::new(
            $X, $Y, $Width, $Height)
        Save-MutatedBitmap (Join-Path $target `
            'items-icons-1280x720.png') {
            param($bitmap)
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.DrawImage($baseline, $region, $region,
                    [System.Drawing.GraphicsUnit]::Pixel)
            } finally { $graphics.Dispose() }
        }
    } finally { $baseline.Dispose() }
    return $target
}

$partialMaterial6 = New-PartialItemDegradation `
    'partial-material6-degradation' 839 504 3 29
$mutations += @{
    Name='partial-material6-degradation'; Path=$partialMaterial6 }
$partialMaterial9 = New-PartialItemDegradation `
    'partial-material9-degradation' 567 563 6 6
$mutations += @{
    Name='partial-material9-degradation'; Path=$partialMaterial9 }
$partialMaterial10 = New-PartialItemDegradation `
    'partial-material10-degradation' 636 562 8 8
$mutations += @{
    Name='partial-material10-degradation'; Path=$partialMaterial10 }

$solidItems = New-Mutation 'solid-gray-items'
Save-MutatedBitmap (Join-Path $solidItems 'items-icons-1280x720.png') {
    param($bitmap)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try { $graphics.Clear([System.Drawing.Color]::FromArgb(255, 64, 64, 64)) }
    finally { $graphics.Dispose() }
}
$mutations += @{ Name='solid-gray-items'; Path=$solidItems }

$noItems = New-Mutation 'no-item-capture'
Copy-Item -LiteralPath (Join-Path $noItems 'items-icons-baseline-1280x720.png') `
    -Destination (Join-Path $noItems 'items-icons-1280x720.png') -Force
$mutations += @{ Name='no-item-capture'; Path=$noItems }

$aliasedItemIcons = New-Mutation 'aliased-item-icon-evidence'
$reportPath = Join-Path $aliasedItemIcons 'stage12-material-evidence.txt'
Replace-Required `
    (Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) `
    'item_icon_screenshot=items-icons-1280x720.png' `
    'item_icon_screenshot=items-materials-1280x720.png' `
    'aliased-item-icon-evidence' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='aliased-item-icon-evidence'; Path=$aliasedItemIcons }

$missingWeaponRoi = New-Mutation 'missing-equipment-weapon-roi'
$weaponBaseline = [System.Drawing.Bitmap]::FromFile(
    (Join-Path $missingWeaponRoi 'items-icons-baseline-1280x720.png'))
try {
    Save-MutatedBitmap (Join-Path $missingWeaponRoi `
        'items-icons-1280x720.png') {
        param($bitmap)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $region = [System.Drawing.Rectangle]::new(431, 317, 45, 45)
            $graphics.DrawImage($weaponBaseline, $region, $region,
                [System.Drawing.GraphicsUnit]::Pixel)
        } finally { $graphics.Dispose() }
    }
} finally { $weaponBaseline.Dispose() }
$mutations += @{ Name='missing-equipment-weapon-roi'; Path=$missingWeaponRoi }

$missingMaterial6Roi = New-Mutation 'missing-material6-roi'
$material6Baseline = [System.Drawing.Bitmap]::FromFile(
    (Join-Path $missingMaterial6Roi 'items-icons-baseline-1280x720.png'))
try {
    Save-MutatedBitmap (Join-Path $missingMaterial6Roi `
        'items-icons-1280x720.png') {
        param($bitmap)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $region = [System.Drawing.Rectangle]::new(822, 496, 37, 37)
            $graphics.DrawImage($material6Baseline, $region, $region,
                [System.Drawing.GraphicsUnit]::Pixel)
        } finally { $graphics.Dispose() }
    }
} finally { $material6Baseline.Dispose() }
$mutations += @{ Name='missing-material6-roi'; Path=$missingMaterial6Roi }

$missingMaterial9Roi = New-Mutation 'missing-material9-roi'
$material9Baseline = [System.Drawing.Bitmap]::FromFile(
    (Join-Path $missingMaterial9Roi 'items-icons-baseline-1280x720.png'))
try {
    Save-MutatedBitmap (Join-Path $missingMaterial9Roi `
        'items-icons-1280x720.png') {
        param($bitmap)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $region = [System.Drawing.Rectangle]::new(552, 548, 37, 37)
            $graphics.DrawImage($material9Baseline, $region, $region,
                [System.Drawing.GraphicsUnit]::Pixel)
        } finally { $graphics.Dispose() }
    }
} finally { $material9Baseline.Dispose() }
$mutations += @{ Name='missing-material9-roi'; Path=$missingMaterial9Roi }

$missingItemRuntime = New-Mutation 'missing-item-runtime-draw'
$reportPath = Join-Path $missingItemRuntime 'stage12-material-evidence.txt'
Replace-Required `
    (Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) `
    'item_runtime_draws=pass' 'item_runtime_draws=fail' `
    'missing-item-runtime-draw' |
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

$missingResidentPeak = New-Mutation 'missing-resident-peak-bytes'
$reportPath = Join-Path $missingResidentPeak 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace
    '(?m)^resident_peak_bytes=\d+\r?\n?', '' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='missing-resident-peak-bytes'; Path=$missingResidentPeak }

$oversizeResidentPeak = New-Mutation 'oversize-resident-peak-bytes'
$reportPath = Join-Path $oversizeResidentPeak 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace
    'resident_peak_bytes=\d+', 'resident_peak_bytes=268435457' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='oversize-resident-peak-bytes'; Path=$oversizeResidentPeak }

$twoFieldsLowReport = New-Mutation 'two-fields-low-report'
$reportPath = Join-Path $twoFieldsLowReport 'stage12-material-evidence.txt'
$lowReport = Replace-RegexRequired `
    (Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) `
    '(?m)^atlas_bytes=\d+\r?$' 'atlas_bytes=0' 'two-fields-low-report-atlas'
$lowReport = Replace-RegexRequired $lowReport `
    '(?m)^full_pack_bytes=\d+\r?$' 'full_pack_bytes=0' `
    'two-fields-low-report-full-pack'
$lowReport | Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='two-fields-low-report'; Path=$twoFieldsLowReport }

$lowResidentPeak = New-Mutation 'low-resident-peak-bytes'
$reportPath = Join-Path $lowResidentPeak 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace
    'resident_peak_bytes=\d+', 'resident_peak_bytes=1' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='low-resident-peak-bytes'; Path=$lowResidentPeak }

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

$aliasedLightning = New-Mutation 'aliased-lightning-isolated-evidence'
$reportPath = Join-Path $aliasedLightning 'stage12-material-evidence.txt'
Replace-Required `
    (Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) `
    'lightning_isolated_monster_screenshot=lightning-monsters-isolated-1280x720.png' `
    'lightning_isolated_monster_screenshot=lightning-monsters-1280x720.png' `
    'aliased-lightning-isolated-evidence' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{
    Name='aliased-lightning-isolated-evidence'; Path=$aliasedLightning }

$missingMonsters = New-Mutation 'missing-lightning-monsters'
$background = [System.Drawing.Bitmap]::FromFile(
    (Join-Path $missingMonsters `
        'lightning-background-isolated-1280x720.png'))
try {
Save-MutatedBitmap (Join-Path $missingMonsters `
    'lightning-monsters-isolated-1280x720.png') {
    param($bitmap)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        foreach ($region in @(
                (Read-ReportRoi $missingMonsters 'lightning_shooter_roi'),
                (Read-ReportRoi $missingMonsters 'lightning_dasher_roi'))) {
            $graphics.DrawImage($background, $region, $region,
                [System.Drawing.GraphicsUnit]::Pixel)
        }
    } finally {
        $graphics.Dispose()
    }
}
} finally { $background.Dispose() }
$mutations += @{ Name='missing-lightning-monsters'; Path=$missingMonsters }

$horizontalShooter = New-Mutation 'lightning-shooter-horizontal-strip'
$shooterRoi = Read-ReportRoi $horizontalShooter 'lightning_shooter_roi'
$background = [System.Drawing.Bitmap]::FromFile(
    (Join-Path $horizontalShooter `
        'lightning-background-isolated-1280x720.png'))
try {
Save-MutatedBitmap (Join-Path $horizontalShooter `
    'lightning-monsters-isolated-1280x720.png') {
    param($bitmap)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.DrawImage($background, $shooterRoi, $shooterRoi,
            [System.Drawing.GraphicsUnit]::Pixel)
    } finally { $graphics.Dispose() }
    $left = $shooterRoi.X + [Math]::Floor(($shooterRoi.Width - 90) / 2)
    $top = $shooterRoi.Y + [Math]::Floor(($shooterRoi.Height - 20) / 2)
    for ($y = 0; $y -lt 20; ++$y) {
        for ($x = 0; $x -lt 90; ++$x) {
            $bitmap.SetPixel($left + $x, $top + $y,
                [System.Drawing.Color]::FromArgb(255,
                    128 + (($x * 7 + $y * 3) % 128),
                    24 + (($x * 5 + $y) % 88),
                    168 + (($x + $y * 3) % 88)))
        }
    }
}
} finally { $background.Dispose() }
$mutations += @{
    Name='lightning-shooter-horizontal-strip'; Path=$horizontalShooter }

$grayscaleShooter = New-Mutation 'lightning-shooter-grayscale'
$shooterRoi = Read-ReportRoi $grayscaleShooter 'lightning_shooter_roi'
$background = [System.Drawing.Bitmap]::FromFile(
    (Join-Path $grayscaleShooter `
        'lightning-background-isolated-1280x720.png'))
try {
Save-MutatedBitmap (Join-Path $grayscaleShooter `
    'lightning-monsters-isolated-1280x720.png') {
    param($bitmap)
    for ($y = $shooterRoi.Y; $y -lt $shooterRoi.Bottom; ++$y) {
        for ($x = $shooterRoi.X; $x -lt $shooterRoi.Right; ++$x) {
            $pixel = $bitmap.GetPixel($x, $y)
            $base = $background.GetPixel($x, $y)
            $difference = [Math]::Abs([int]$pixel.R - [int]$base.R) +
                [Math]::Abs([int]$pixel.G - [int]$base.G) +
                [Math]::Abs([int]$pixel.B - [int]$base.B)
            if ($difference -ge 72) {
                $bitmap.SetPixel($x, $y,
                    [System.Drawing.Color]::FromArgb(255, 224, 224, 224))
            }
        }
    }
}
} finally { $background.Dispose() }
$mutations += @{
    Name='lightning-shooter-grayscale'; Path=$grayscaleShooter }

$missingRuntime = New-Mutation 'missing-runtime-draw'
$reportPath = Join-Path $missingRuntime 'stage12-material-evidence.txt'
Replace-Required `
    (Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) `
    'lightning_shooter_drawn=pass' 'lightning_shooter_drawn=fail' `
    'missing-runtime-draw' |
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
Replace-Required `
    (Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) `
    'chaos_chaser_drawn=pass' 'chaos_chaser_drawn=fail' `
    'missing-chaos-runtime-draw' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='missing-chaos-runtime-draw'; Path=$missingChaosRuntime }

$invalidChaosFrame = New-Mutation 'invalid-chaos-runtime-frame'
$reportPath = Join-Path $invalidChaosFrame 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace `
    'chaos_hazard_frame=\d+', 'chaos_hazard_frame=65535' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='invalid-chaos-runtime-frame'; Path=$invalidChaosFrame }

$missingNative1080 = New-Mutation 'missing-fire-background-only-1080'
Remove-Item -LiteralPath (Join-Path $missingNative1080 `
    'fire-background-only-1920x1080.png') -Force
$mutations += @{
    Name='missing-fire-background-only-1080'; Path=$missingNative1080 }

$upscaledNative = New-Mutation 'upscaled-native-background'
$reportPath = Join-Path $upscaledNative 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
    'fire_background_scale_1920=3/4', 'fire_background_scale_1920=5/4') |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='upscaled-native-background'; Path=$upscaledNative }

$missingProvenance = New-Mutation 'missing-native-provenance'
$reportPath = Join-Path $missingProvenance 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace `
    '(?m)^fire_background_provenance=.*\r?\n?', '' |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='missing-native-provenance'; Path=$missingProvenance }

$forgedNativeHash = New-Mutation 'forged-native-background-hash'
$reportPath = Join-Path $forgedNativeHash 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace `
    '(?m)^fire_background_runtime_sha256=[0-9a-f]{64}\r?$', `
    ('fire_background_runtime_sha256=' + ('0' * 64)) |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='forged-native-background-hash'; Path=$forgedNativeHash }

$duplicateNativeReportField = New-Mutation 'duplicate-native-report-field'
$reportPath = Join-Path $duplicateNativeReportField `
    'stage12-material-evidence.txt'
Add-Content -LiteralPath $reportPath -Encoding UTF8 `
    -Value 'native_background_status=native-background-verified'
$mutations += @{
    Name='duplicate-native-report-field'; Path=$duplicateNativeReportField }

$emptyReportKey = New-Mutation 'empty-report-key'
$reportPath = Join-Path $emptyReportKey 'stage12-material-evidence.txt'
$reportContent = Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8
Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline `
    -Value ("=invalid`r`n" + $reportContent)
$mutations += @{ Name='empty-report-key'; Path=$emptyReportKey }

$duplicateEcology = New-Mutation 'duplicate-native-background-ecology'
Copy-Item -LiteralPath (Join-Path $duplicateEcology `
    'fire-background-only-1920x1080.png') -Destination (Join-Path `
    $duplicateEcology 'water-background-only-1920x1080.png') -Force
$reportPath = Join-Path $duplicateEcology 'stage12-material-evidence.txt'
$duplicateHash = Get-Sha256 (Join-Path $duplicateEcology `
    'water-background-only-1920x1080.png')
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace
    '(?m)^water_background_only_screenshot_1920_sha256=[0-9a-f]{64}\r?$',
    "water_background_only_screenshot_1920_sha256=$duplicateHash" |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{
    Name='duplicate-native-background-ecology'; Path=$duplicateEcology }

$duplicateGameplay = New-Mutation 'duplicate-native-gameplay'
Copy-Item -LiteralPath (Join-Path $duplicateGameplay `
    'fire-background-only-1280x720.png') -Destination (Join-Path `
    $duplicateGameplay 'fire-gameplay-1280x720.png') -Force
$reportPath = Join-Path $duplicateGameplay 'stage12-material-evidence.txt'
$duplicateHash = Get-Sha256 (Join-Path $duplicateGameplay `
    'fire-gameplay-1280x720.png')
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8) -replace
    '(?m)^fire_gameplay_screenshot_1280_sha256=[0-9a-f]{64}\r?$',
    "fire_gameplay_screenshot_1280_sha256=$duplicateHash" |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{ Name='duplicate-native-gameplay'; Path=$duplicateGameplay }

$wrongHudEcology = New-Mutation 'wrong-native-gameplay-hud-ecology'
$reportPath = Join-Path $wrongHudEcology 'stage12-material-evidence.txt'
(Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8).Replace(
    'water_gameplay_hud_ecology_1920=pass',
    'water_gameplay_hud_ecology_1920=fail') |
    Set-Content -LiteralPath $reportPath -Encoding UTF8 -NoNewline
$mutations += @{
    Name='wrong-native-gameplay-hud-ecology'; Path=$wrongHudEcology }

if ($mutations.Count -ne 80) {
    throw "stage12 validator mutation inventory changed: expected=80 actual=$($mutations.Count)"
}
$failures = @()
foreach ($mutation in $mutations) {
    $publishedItemAtlas = if ($mutation.ContainsKey('ItemAtlasPath')) {
        $mutation.ItemAtlasPath
    } else {
        ''
    }
    $exitCode = Invoke-Validator $mutation.Path $publishedItemAtlas
    if ($exitCode -eq 0) {
        $failures += "validator accepted mutation: $($mutation.Name)"
    }
}
if ($failures.Count -ne 0) { throw ($failures -join [Environment]::NewLine) }
Write-Output 'stage12 material validator rejected malformed reports, forged native backgrounds, hidden/fallback UI, missing UI/item telemetry, solid/no-item evidence, wrong ecology, and invalid runtime draw/frame proof'
