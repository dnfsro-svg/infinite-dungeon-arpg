param(
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDirectory
)

$ErrorActionPreference = 'Stop'
$expected = [ordered]@{
    'combat' = 'normal_combat'
    'low-health' = 'low_health_status'
    'cleared' = 'cleared_exit'
    'abyss-warning' = 'abyss_abandon'
    'level-up' = 'level_up_points'
    'debug' = 'debug_overlay'
}
function ConvertFrom-Codepoints([int[]]$Codepoints) {
    return -join ($Codepoints | ForEach-Object { [char]$_ })
}
$cn = @{
    Remaining = ConvertFrom-Codepoints @(0x5269,0x4F59)
    ExitReady = ConvertFrom-Codepoints @(0x51FA,0x53E3,0x5DF2,0x5F00,0x653E)
    Reward = ConvertFrom-Codepoints @(0x5956,0x52B1)
    LevelUp = ConvertFrom-Codepoints @(0x5347,0x7EA7)
    Abyss = ConvertFrom-Codepoints @(0x6DF1,0x6E0A)
    Abandon = ConvertFrom-Codepoints @(0x653E,0x5F03)
    DoorAbandon = ConvertFrom-Codepoints @(0x79BB,0x5F00,0x540E,0x518D,0x6B21,0x89E6,0x78B0,0x540C,0x4E00,0x51FA,0x53E3,0x4EE5,0x653E,0x5F03,0x5168,0x90E8,0x5269,0x4F59,0x5956,0x52B1)
}
$report = Join-Path $EvidenceDirectory 'stage11c-hud-evidence.txt'
if (-not (Test-Path -LiteralPath $report)) { throw "missing formal evidence report: $report" }
$reportValues = @{}
Get-Content -LiteralPath $report -Encoding UTF8 | ForEach-Object {
    $pair = $_ -split '=', 2
    if ($pair.Count -eq 2) { $reportValues[$pair[0]] = $pair[1] }
}
if ($reportValues.result -ne 'pass') { throw 'formal evidence aggregate did not pass' }

Add-Type -AssemblyName System.Drawing
function Read-Values([string]$Path) {
    $result = @{}
    Get-Content -LiteralPath $Path -Encoding UTF8 | ForEach-Object {
        $pair = $_ -split '=', 2
        if ($pair.Count -eq 2) { $result[$pair[0]] = $pair[1] }
    }
    return $result
}

function Read-Rect([hashtable]$Values, [string]$Name, [string]$Scenario) {
    $rect = @($Values[$Name] -split ',' | ForEach-Object { [double]$_ })
    if ($rect.Count -ne 4 -or $rect[2] -le 0 -or $rect[3] -le 0) {
        throw "non-positive HUD rect ${Name} in ${Scenario}"
    }
    return $rect
}

function Measure-HudTextRegion($Bitmap, [double[]]$Rect, [bool]$CheckMissingBoxes) {
    $left = [Math]::Max(0, [int][Math]::Floor($Rect[0]))
    $top = [Math]::Max(0, [int][Math]::Floor($Rect[1]))
    $right = [Math]::Min($Bitmap.Width, [int][Math]::Ceiling($Rect[0] + $Rect[2]))
    $bottom = [Math]::Min($Bitmap.Height, [int][Math]::Ceiling($Rect[1] + $Rect[3]))
    $bright = 0
    $dark = 0
    $total = [Math]::Max(1, ($right - $left) * ($bottom - $top))
    for ($y = $top; $y -lt $bottom; ++$y) {
        for ($x = $left; $x -lt $right; ++$x) {
            $color = $Bitmap.GetPixel($x, $y)
            $luma = ([int]$color.R + [int]$color.G + [int]$color.B) / 3
            if ($luma -ge 125) { ++$bright }
            if ($luma -le 75) { ++$dark }
        }
    }
    $boxes = 0
    if ($CheckMissingBoxes) {
        for ($y = $top; $y -le $bottom - 18; ++$y) {
            for ($x = $left; $x -le $right - 12; ++$x) {
                $boundaryBright = 0
                $boundaryTotal = 0
                for ($dx = 0; $dx -lt 12; ++$dx) {
                    foreach ($dy in @(0,17)) {
                        $c = $Bitmap.GetPixel($x + $dx, $y + $dy)
                        if (([int]$c.R + [int]$c.G + [int]$c.B) / 3 -ge 180) { ++$boundaryBright }
                        ++$boundaryTotal
                    }
                }
                for ($dy = 1; $dy -lt 17; ++$dy) {
                    foreach ($dx in @(0,11)) {
                        $c = $Bitmap.GetPixel($x + $dx, $y + $dy)
                        if (([int]$c.R + [int]$c.G + [int]$c.B) / 3 -ge 180) { ++$boundaryBright }
                        ++$boundaryTotal
                    }
                }
                $interiorDark = 0
                $interiorTotal = 0
                for ($dy = 3; $dy -lt 15; $dy += 3) {
                    for ($dx = 2; $dx -lt 10; $dx += 2) {
                        $c = $Bitmap.GetPixel($x + $dx, $y + $dy)
                        if (([int]$c.R + [int]$c.G + [int]$c.B) / 3 -le 80) { ++$interiorDark }
                        ++$interiorTotal
                    }
                }
                if ($boundaryBright / $boundaryTotal -ge 0.9 -and
                        $interiorDark / $interiorTotal -ge 0.85) {
                    ++$boxes
                    $x += 10
                }
            }
        }
    }
    return [pscustomobject]@{
        BrightPixels = $bright
        DarkRatio = $dark / $total
        HollowBoxes = $boxes
    }
}

$required = @(
    'scenario','safe_rect','player_rect','objective_rect','navigation_rect',
    'primary_notice_rect','secondary_notice_rect','debug_rect','player_values',
    'status_tags','objective','notice_kinds','notice_texts','navigation_values','font_ready',
    'f1','production_snapshot_hash','result'
)
$regionFeatures = @{}
foreach ($name in $expected.Keys) {
    $imagePath = Join-Path $EvidenceDirectory ($name + '.png')
    $summaryPath = Join-Path $EvidenceDirectory ($name + '.txt')
    if (-not (Test-Path -LiteralPath $imagePath)) { throw "missing screenshot: $imagePath" }
    if (-not (Test-Path -LiteralPath $summaryPath)) { throw "missing summary: $summaryPath" }
    $bytes = [System.IO.File]::ReadAllBytes($imagePath)
    if ($bytes.Length -le 1024) { throw "screenshot is empty: $name" }
    $signature = @(137,80,78,71,13,10,26,10)
    for ($index = 0; $index -lt $signature.Count; ++$index) {
        if ($bytes[$index] -ne $signature[$index]) { throw "invalid PNG signature: $name" }
    }
    $width = [uint32]$bytes[16] * 16777216 + [uint32]$bytes[17] * 65536 + [uint32]$bytes[18] * 256 + [uint32]$bytes[19]
    $height = [uint32]$bytes[20] * 16777216 + [uint32]$bytes[21] * 65536 + [uint32]$bytes[22] * 256 + [uint32]$bytes[23]
    if ($width -ne 1280 -or $height -ne 720) { throw "wrong PNG dimensions: $name ${width}x${height}" }
    if ((Get-Item -LiteralPath $imagePath).LastWriteTimeUtc -lt [DateTime]::UtcNow.AddMinutes(-15)) {
        throw "stale screenshot: $name"
    }

    $values = Read-Values $summaryPath
    foreach ($key in $required) {
        if (-not $values.ContainsKey($key)) { throw "missing $name summary field: $key" }
    }
    if ($values.scenario -ne $expected[$name] -or $values.result -ne 'pass') {
        throw "invalid scenario result: $name"
    }
    if ($values.font_ready -ne '1') { throw "CJK HUD font was not ready: $name" }
    if (($name -eq 'debug' -and $values.f1 -ne '1') -or
        ($name -ne 'debug' -and $values.f1 -ne '0')) { throw "invalid F1 state: $name" }
    if ([uint64]$values.production_snapshot_hash -eq 0) { throw "zero production snapshot hash: $name" }
    if ([string]::IsNullOrWhiteSpace($values.objective)) { throw "empty objective: $name" }
    $rects = @{}
    foreach ($rectName in @('safe_rect','player_rect','objective_rect','navigation_rect','primary_notice_rect','secondary_notice_rect','debug_rect')) {
        $rects[$rectName] = Read-Rect $values $rectName $name
    }
    $player = @($values.player_values -split ',' | ForEach-Object { [uint64]$_ })
    if ($player.Count -ne 8 -or $player[1] -eq 0 -or $player[4] -eq 0) { throw "invalid player values: $name" }
    $navigation = @($values.navigation_values -split ',' | ForEach-Object { [uint64]$_ })
    if ($navigation.Count -ne 7 -or $navigation[0] -eq 0 -or $navigation[1] -eq 0) { throw "invalid navigation values: $name" }
    $noticeKinds = @($values.notice_kinds -split ',')
    $noticeTexts = @($values.notice_texts -split '\|')
    $hasChineseObjective = $values.objective -match '[\u4e00-\u9fff]'
    $hasChineseNotice = $values.notice_texts -match '[\u4e00-\u9fff]'
    switch ($name) {
        'combat' {
            if (-not $hasChineseObjective -or $values.objective -notmatch [regex]::Escape($cn.Remaining) -or
                    -not [string]::IsNullOrWhiteSpace($values.status_tags) -or
                    $values.notice_kinds -ne '0,0') { throw 'combat Chinese semantic mismatch' }
        }
        'low-health' {
            if ($player[0] * 4 -gt $player[1] -or [string]::IsNullOrWhiteSpace($values.status_tags) -or
                    -not $hasChineseObjective -or $values.objective -notmatch [regex]::Escape($cn.Remaining)) {
                throw 'low-health Chinese semantic mismatch'
            }
        }
        'cleared' {
            if ($values.objective -ne $cn.ExitReady -or -not $hasChineseNotice -or
                    -not ($noticeKinds -contains '7') -or -not ($noticeKinds -contains '8') -or
                    $values.notice_texts -notmatch [regex]::Escape($cn.Reward) -or
                    $values.notice_texts -notmatch [regex]::Escape($cn.LevelUp)) {
                throw 'cleared Chinese semantic mismatch'
            }
        }
        'abyss-warning' {
            if (-not $hasChineseObjective -or $values.objective -notmatch [regex]::Escape($cn.Abyss) -or
                    -not $hasChineseNotice -or -not ($noticeKinds -contains '3') -or
                    $values.notice_texts -notmatch [regex]::Escape($cn.Abandon) -or
                    $noticeTexts -notcontains $cn.DoorAbandon -or
                    $values.notice_texts -notmatch [regex]::Escape($cn.Reward)) {
                throw 'abyss-warning Chinese semantic mismatch'
            }
        }
        'level-up' {
            if ($player[4] -le 1 -or $player[7] -eq 0 -or
                    -not $hasChineseNotice -or -not ($noticeKinds -contains '8') -or
                    $values.notice_texts -notmatch [regex]::Escape($cn.LevelUp)) {
                throw 'level-up Chinese semantic mismatch'
            }
        }
        'debug' {
            if (-not $hasChineseObjective -or $values.objective -notmatch [regex]::Escape($cn.Remaining) -or
                    $values.notice_kinds -ne '0,0') { throw 'debug Chinese semantic mismatch' }
        }
    }

    $aggregateHashKey = $name + '_snapshot_hash'
    if (-not $reportValues.ContainsKey($aggregateHashKey) -or
            [uint64]$reportValues[$aggregateHashKey] -ne [uint64]$values.production_snapshot_hash) {
        throw "snapshot hash mismatch: $name"
    }

    $bitmap = [System.Drawing.Bitmap]::FromFile($imagePath)
    try {
        $objectiveFeature = Measure-HudTextRegion $bitmap $rects.objective_rect $false
        if ($objectiveFeature.BrightPixels -lt 25 -or $objectiveFeature.DarkRatio -lt 0.25) {
            throw "Chinese objective region is blank: $name"
        }
        if ($name -in @('cleared','abyss-warning','level-up')) {
            $secondary = $rects.secondary_notice_rect
            $primary = $rects.primary_notice_rect
            [double[]]$noticeRegion = @($primary[0], $secondary[1], $primary[2],
                ($primary[1] + $primary[3] - $secondary[1]))
            $noticeFeature = Measure-HudTextRegion $bitmap $noticeRegion $true
            if ($noticeFeature.HollowBoxes -ge 3) {
                throw "missing-glyph boxes in Chinese notice region: $name"
            }
            if ($noticeFeature.BrightPixels -lt 80 -or $noticeFeature.DarkRatio -lt 0.45) {
                throw "Chinese text region is blank: $name"
            }
            $regionFeatures[$name] = "bright=$($noticeFeature.BrightPixels),dark=$([Math]::Round($noticeFeature.DarkRatio,3)),boxes=$($noticeFeature.HollowBoxes)"
        }
    } finally {
        $bitmap.Dispose()
    }
    if ($reportValues[$name + '_valid'] -ne '1' -or [uint64]$reportValues[$name + '_image_hash'] -eq 0) {
        throw "aggregate report rejected scenario: $name"
    }
}
Write-Output ('stage11c HUD evidence validated: ' + (($regionFeatures.GetEnumerator() | Sort-Object Name | ForEach-Object { $_.Name + '[' + $_.Value + ']' }) -join ' '))
