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
$report = Join-Path $EvidenceDirectory 'stage11c-hud-evidence.txt'
if (-not (Test-Path -LiteralPath $report)) { throw "missing formal evidence report: $report" }
$reportValues = @{}
Get-Content -LiteralPath $report -Encoding UTF8 | ForEach-Object {
    $pair = $_ -split '=', 2
    if ($pair.Count -eq 2) { $reportValues[$pair[0]] = $pair[1] }
}
if ($reportValues.result -ne 'pass') { throw 'formal evidence aggregate did not pass' }

Add-Type -AssemblyName System.Drawing
$required = @(
    'scenario','safe_rect','player_rect','objective_rect','navigation_rect',
    'primary_notice_rect','secondary_notice_rect','debug_rect','player_values',
    'status_tags','objective','notice_kinds','navigation_values','font_ready',
    'f1','production_snapshot_hash','result'
)
$pixelSamples = @{}
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

    $values = @{}
    Get-Content -LiteralPath $summaryPath -Encoding UTF8 | ForEach-Object {
        $pair = $_ -split '=', 2
        if ($pair.Count -eq 2) { $values[$pair[0]] = $pair[1] }
    }
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
    foreach ($rectName in @('safe_rect','player_rect','objective_rect','navigation_rect','primary_notice_rect','secondary_notice_rect','debug_rect')) {
        $rect = @($values[$rectName] -split ',' | ForEach-Object { [double]$_ })
        if ($rect.Count -ne 4 -or $rect[2] -lt 0 -or $rect[3] -lt 0) { throw "invalid HUD rect $rectName in $name" }
    }
    $player = @($values.player_values -split ',' | ForEach-Object { [uint64]$_ })
    if ($player.Count -ne 8 -or $player[1] -eq 0 -or $player[4] -eq 0) { throw "invalid player values: $name" }
    $navigation = @($values.navigation_values -split ',' | ForEach-Object { [uint64]$_ })
    if ($navigation.Count -ne 7 -or $navigation[0] -eq 0 -or $navigation[1] -eq 0) { throw "invalid navigation values: $name" }
    if ($name -eq 'low-health') {
        if ($player[0] * 4 -gt $player[1] -or [string]::IsNullOrWhiteSpace($values.status_tags)) {
            throw 'low-health screenshot lacks low HP or a production status tag'
        }
    }
    if ($name -eq 'level-up' -and ($player[4] -le 1 -or $player[7] -eq 0)) {
        throw 'level-up screenshot lacks level and passive-point progression'
    }
    if ($name -eq 'level-up' -and -not (($values.notice_kinds -split ',') -contains '8')) {
        throw 'level-up screenshot lacks the production level-up notice'
    }
    if ($name -eq 'abyss-warning' -and -not ($values.notice_kinds -split ',' -contains '3')) {
        throw 'abyss screenshot lacks abandon warning notice'
    }

    $bitmap = [System.Drawing.Bitmap]::FromFile($imagePath)
    try {
        $sample = [uint64]0
        $unique = [System.Collections.Generic.HashSet[int]]::new()
        foreach ($point in @(@(20,20),@(35,295),@(60,295),@(100,295),@(35,318),@(100,318),@(35,341),@(180,560),@(330,640),@(640,40),@(640,95),@(1040,40),@(1210,140),@(640,570),@(640,620),@(970,680))) {
            $color = $bitmap.GetPixel($point[0], $point[1])
            $packed = [int]$color.R * 65536 + [int]$color.G * 256 + [int]$color.B
            [void]$unique.Add($packed)
            $sample = ($sample + [uint64]($packed + $point[0] * 17 + $point[1] * 31)) % 18446744073709551557
        }
        if ($unique.Count -lt 4 -or $sample -eq 0) { throw "HUD pixel sample is blank: $name" }
        $pixelSamples[$name] = $sample
    } finally {
        $bitmap.Dispose()
    }
    if ($reportValues[$name + '_valid'] -ne '1' -or [uint64]$reportValues[$name + '_image_hash'] -eq 0) {
        throw "aggregate report rejected scenario: $name"
    }
}
if (($pixelSamples.Values | Select-Object -Unique).Count -lt 5) {
    throw 'scenario screenshots do not contain distinct deterministic HUD samples'
}
Write-Output ('stage11c HUD evidence validated: ' + (($pixelSamples.GetEnumerator() | Sort-Object Name | ForEach-Object { $_.Name + '=0x' + ([uint64]$_.Value).ToString('X16') }) -join ' '))
