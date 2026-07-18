param(
    [Parameter(Mandatory = $true)][string]$EvidenceDirectory,
    [Parameter(Mandatory = $true)][string]$Validator,
    [Parameter(Mandatory = $true)][string]$MutationRoot
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
public static class Stage11CFnv1aSelfTest {
    public static ulong Hash(byte[] bytes) {
        const ulong Offset = 1469598103934665603UL;
        const ulong Prime = 1099511628211UL;
        unchecked {
            ulong hash = Offset;
            foreach (byte value in bytes) {
                hash ^= value;
                hash *= Prime;
            }
            return hash;
        }
    }
}
'@

function New-Mutation([string]$Name) {
    $target = Join-Path $MutationRoot $Name
    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Copy-Item -Path (Join-Path $EvidenceDirectory '*') -Destination $target -Recurse -Force
    return $target
}

function Save-MutatedBitmap([string]$Path, [scriptblock]$Mutation) {
    $source = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        $copy = [System.Drawing.Bitmap]::new($source)
    } finally {
        $source.Dispose()
    }
    try {
        & $Mutation $copy
        $temporary = $Path + '.mutated.png'
        $copy.Save($temporary, [System.Drawing.Imaging.ImageFormat]::Png)
        Move-Item -LiteralPath $temporary -Destination $Path -Force
    } finally {
        $copy.Dispose()
    }
}

function Update-AggregateImageHash([string]$Root, [string]$Name) {
    $image = Join-Path $Root ($Name + '.png')
    [uint64]$hash = [Stage11CFnv1aSelfTest]::Hash(
        [System.IO.File]::ReadAllBytes($image))
    $report = Join-Path $Root 'stage11c-hud-evidence.txt'
    $content = Get-Content -Raw -LiteralPath $report -Encoding UTF8
    $content = $content -replace ('(?m)^' + [regex]::Escape($Name) +
        '_image_hash=.*$'), ($Name + '_image_hash=' + $hash)
    Set-Content -LiteralPath $report -Value $content -Encoding UTF8
}

$mutations = @()

$removedChinese = New-Mutation 'removed_chinese'
$clearedSummary = Join-Path $removedChinese 'cleared.txt'
$content = Get-Content -Raw -LiteralPath $clearedSummary -Encoding UTF8
$content = $content -replace '(?m)^objective=.*$', 'objective=EXIT READY'
$content = $content -replace '(?m)^notice_texts=.*$', 'notice_texts=ROOM CLEAR|REWARD XP'
Set-Content -LiteralPath $clearedSummary -Value $content -Encoding UTF8
$mutations += @{ Name='removed_chinese'; Path=$removedChinese; Reason='Chinese semantic' }

$missingGlyph = New-Mutation 'missing_glyph_boxes'
Save-MutatedBitmap (Join-Path $missingGlyph 'abyss-warning.png') {
    param($bitmap)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.FillRectangle([System.Drawing.Brushes]::Black, 340, 570, 600, 70)
        $pen = [System.Drawing.Pen]::new([System.Drawing.Color]::White, 1)
        try {
            for ($index = 0; $index -lt 10; ++$index) {
                $graphics.DrawRectangle($pen, 365 + ($index * 18), 582, 11, 17)
            }
        } finally { $pen.Dispose() }
    } finally { $graphics.Dispose() }
}
Update-AggregateImageHash $missingGlyph 'abyss-warning'
$mutations += @{ Name='missing_glyph_boxes'; Path=$missingGlyph; Reason='missing-glyph boxes' }

$clearedPanel = New-Mutation 'cleared_chinese_panel'
Save-MutatedBitmap (Join-Path $clearedPanel 'level-up.png') {
    param($bitmap)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try { $graphics.FillRectangle([System.Drawing.Brushes]::Black, 340, 570, 600, 70) }
    finally { $graphics.Dispose() }
}
Update-AggregateImageHash $clearedPanel 'level-up'
$mutations += @{ Name='cleared_chinese_panel'; Path=$clearedPanel; Reason='Chinese text region is blank' }

$tamperedHash = New-Mutation 'tampered_snapshot_hash'
$report = Join-Path $tamperedHash 'stage11c-hud-evidence.txt'
$content = Get-Content -Raw -LiteralPath $report -Encoding UTF8
$content = $content -replace '(?m)^abyss-warning_snapshot_hash=.*$', 'abyss-warning_snapshot_hash=1'
Set-Content -LiteralPath $report -Value $content -Encoding UTF8
$mutations += @{ Name='tampered_snapshot_hash'; Path=$tamperedHash; Reason='snapshot hash mismatch' }

$tamperedPixel = New-Mutation 'tampered_png_pixel'
Save-MutatedBitmap (Join-Path $tamperedPixel 'combat.png') {
    param($bitmap)
    $original = $bitmap.GetPixel(0, 0)
    $replacement = if ($original.ToArgb() -eq [System.Drawing.Color]::Magenta.ToArgb()) {
        [System.Drawing.Color]::Lime
    } else {
        [System.Drawing.Color]::Magenta
    }
    $bitmap.SetPixel(0, 0, $replacement)
}
$mutations += @{ Name='tampered_png_pixel'; Path=$tamperedPixel; Reason='image hash mismatch: combat' }

$tamperedImageHash = New-Mutation 'tampered_aggregate_image_hash'
$report = Join-Path $tamperedImageHash 'stage11c-hud-evidence.txt'
$content = Get-Content -Raw -LiteralPath $report -Encoding UTF8
$content = $content -replace '(?m)^combat_image_hash=.*$', 'combat_image_hash=1'
Set-Content -LiteralPath $report -Value $content -Encoding UTF8
$mutations += @{ Name='tampered_aggregate_image_hash'; Path=$tamperedImageHash; Reason='image hash mismatch: combat' }

$zeroRect = New-Mutation 'zero_sized_rect'
$summary = Join-Path $zeroRect 'combat.txt'
$content = Get-Content -Raw -LiteralPath $summary -Encoding UTF8
$content = $content -replace '(?m)^player_rect=.*$', 'player_rect=16,560,0,144'
Set-Content -LiteralPath $summary -Value $content -Encoding UTF8
$mutations += @{ Name='zero_sized_rect'; Path=$zeroRect; Reason='non-positive HUD rect' }

$failures = @()
foreach ($mutation in $mutations) {
    $savedErrorPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Validator `
        -EvidenceDirectory $mutation.Path 2>&1 | Out-String
    $validatorExitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedErrorPreference
    if ($validatorExitCode -eq 0) {
        $failures += "validator accepted named mutation: $($mutation.Name)"
    } elseif ($output -notmatch [regex]::Escape($mutation.Reason)) {
        $failures += "mutation $($mutation.Name) rejected for wrong reason: $output"
    }
}
if ($failures.Count -ne 0) { throw ($failures -join [Environment]::NewLine) }
Write-Output 'stage11c HUD validator self-test rejected all named mutations'
