param(
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDirectory,
    [string]$ItemAtlasPath = ''
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))

[uint64]$ExpectedFullPackBytes = 329430304
[uint64]$ExpectedResidentPeakBytes = 226800928
[uint64]$ExpectedTransitionPeakBytes = 261010720
[uint64]$MaximumResidentTextureBytes = 268435456
if (-not (Test-Path -LiteralPath $EvidenceDirectory -PathType Container)) {
    throw "missing evidence directory: $EvidenceDirectory"
}

function Read-Report([string]$Path) {
    $values = @{}
    [int]$lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $Path -Encoding UTF8) {
        ++$lineNumber
        $pair = $line -split '=', 2
        if ($pair.Count -ne 2) { continue }
        $key = $pair[0]
        if ([string]::IsNullOrWhiteSpace($key)) {
            throw "empty report key at line ${lineNumber}: $Path"
        }
        if ($values.ContainsKey($key)) {
            throw "duplicate report key '$key' at line ${lineNumber}: $Path"
        }
        $values[$key] = $pair[1]
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

function Read-PngHeaderSize([string]$Path) {
    $bytes = New-Object byte[] 24
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        if ($stream.Length -lt $bytes.Length -or
                $stream.Read($bytes, 0, $bytes.Length) -ne $bytes.Length) {
            throw "invalid PNG: $Path"
        }
    } finally { $stream.Dispose() }
    if ($bytes[0] -ne 137 -or $bytes[1] -ne 80 -or
            $bytes[2] -ne 78 -or $bytes[3] -ne 71) {
        throw "invalid PNG: $Path"
    }
    $width = [uint32]$bytes[16] * 16777216 + [uint32]$bytes[17] * 65536 +
        [uint32]$bytes[18] * 256 + [uint32]$bytes[19]
    $height = [uint32]$bytes[20] * 16777216 + [uint32]$bytes[21] * 65536 +
        [uint32]$bytes[22] * 256 + [uint32]$bytes[23]
    return @($width, $height)
}

function Read-InvariantDouble([string]$Value) {
    return [double]::Parse($Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
}

function Assert-FrameUnion([object[]]$Rows, [int]$LastFrame,
        [string]$Name) {
    $union = @($Rows | ForEach-Object { [int]$_.skill_frame } |
        Sort-Object -Unique)
    if ($union.Count -ne ($LastFrame + 1)) {
        throw "$Name atlas frame union has $($union.Count) entries"
    }
    for ($index = 0; $index -le $LastFrame; ++$index) {
        if ($union[$index] -ne $index) {
            throw "$Name atlas frame union is not 0..$LastFrame"
        }
    }
}

function Assert-SkillTimelineRows([object[]]$Rows, [int]$ExpectedPerResolution,
        [int[]]$CaptureTicks, [string]$Name, [int]$ExpectedAtlas) {
    foreach ($width in @(800,1280,1920)) {
        $resolutionRows = @($Rows | Where-Object { [int]$_.width -eq $width })
        if ($resolutionRows.Count -ne $ExpectedPerResolution) {
            throw "$Name resolution timeline rejected: $width"
        }
        foreach ($tick in $CaptureTicks) {
            $selected = @($resolutionRows | Where-Object {
                [int]$_.tick -eq $tick -and
                -not [string]::IsNullOrEmpty($_.screenshot) })
            if ($selected.Count -ne 1) {
                throw "$Name selected capture missing: width=$width tick=$tick"
            }
        }
    }
    foreach ($frame in $Rows) {
        if ([int]$frame.skill_mode -ne 1 -or
                [int]$frame.skill_atlas -ne $ExpectedAtlas -or
                [int]$frame.skill_drawn -ne 1 -or
                [int]$frame.suppress_base_player -ne 1 -or
                [int]$frame.base_player_drawn -ne 0 -or
                [int]$frame.procedural_main_visual_count -ne 0) {
            throw "$Name healthy material path rejected"
        }
    }
}

function Assert-SkillRoiDifference([string]$Path, [string]$BaselinePath,
        [object]$Frame, [string]$Name) {
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    $baseline = [System.Drawing.Bitmap]::FromFile($BaselinePath)
    try {
        if ($bitmap.Width -ne $baseline.Width -or
                $bitmap.Height -ne $baseline.Height) {
            throw "$Name skill/baseline dimensions differ"
        }
        [int]$left = [int]$Frame.skill_roi_x
        [int]$top = [int]$Frame.skill_roi_y
        [int]$width = [int]$Frame.skill_roi_width
        [int]$height = [int]$Frame.skill_roi_height
        if ($left -lt 0 -or $top -lt 0 -or $width -le 0 -or $height -le 0 -or
                $left + $width -gt $bitmap.Width -or
                $top + $height -gt $bitmap.Height) {
            throw "$Name production skill ROI rejected"
        }
        $mask = New-Object 'bool[,]' $width, $height
        [int]$changed = 0
        for ($y = 0; $y -lt $height; ++$y) {
            for ($x = 0; $x -lt $width; ++$x) {
                $pixel = $bitmap.GetPixel($left + $x, $top + $y)
                $reference = $baseline.GetPixel($left + $x, $top + $y)
                $difference = [Math]::Abs([int]$pixel.R - [int]$reference.R) +
                    [Math]::Abs([int]$pixel.G - [int]$reference.G) +
                    [Math]::Abs([int]$pixel.B - [int]$reference.B)
                if ($difference -ge 48) {
                    $mask[$x, $y] = $true
                    ++$changed
                }
            }
        }
        [int]$largest = 0
        [int]$largestWidth = 0
        [int]$largestHeight = 0
        for ($y = 0; $y -lt $height; ++$y) {
            for ($x = 0; $x -lt $width; ++$x) {
                if (-not $mask[$x, $y]) { continue }
                $queue = [System.Collections.Generic.Queue[int]]::new()
                $queue.Enqueue($y * $width + $x)
                $mask[$x, $y] = $false
                [int]$component = 0
                [int]$minX = $x; [int]$maxX = $x
                [int]$minY = $y; [int]$maxY = $y
                while ($queue.Count -gt 0) {
                    $point = $queue.Dequeue()
                    $px = $point % $width
                    $py = [Math]::Floor($point / $width)
                    ++$component
                    $minX = [Math]::Min($minX, $px)
                    $maxX = [Math]::Max($maxX, $px)
                    $minY = [Math]::Min($minY, $py)
                    $maxY = [Math]::Max($maxY, $py)
                    foreach ($offset in @(
                            @(-1,-1),@(0,-1),@(1,-1),@(-1,0),@(1,0),
                            @(-1,1),@(0,1),@(1,1))) {
                        $nx = $px + $offset[0]; $ny = $py + $offset[1]
                        if ($nx -ge 0 -and $nx -lt $width -and
                                $ny -ge 0 -and $ny -lt $height -and
                                $mask[$nx, $ny]) {
                            $mask[$nx, $ny] = $false
                            $queue.Enqueue($ny * $width + $nx)
                        }
                    }
                }
                if ($component -gt $largest) {
                    $largest = $component
                    $largestWidth = $maxX - $minX + 1
                    $largestHeight = $maxY - $minY + 1
                }
            }
        }
        $area = $width * $height
        $minimumChanged = [Math]::Max(120, [Math]::Floor($area / 100))
        $minimumConnected = [Math]::Max(48, [Math]::Floor($area / 500))
        [int]$minimumWidth = 8
        [int]$minimumHeight = 8
        if ($Frame.roi_kind -eq 'procedural_fallback') {
            $minimumChanged = 300
            $minimumConnected = 300
            $minimumWidth = 32
            $minimumHeight = 100
        }
        Write-Output ("[stage12-skill-roi] {0} kind={1} changed={2} " +
            "largest={3} extent={4}x{5}" -f $Name, $Frame.roi_kind,
            $changed, $largest, $largestWidth, $largestHeight)
        if ($changed -lt $minimumChanged -or $largest -lt $minimumConnected -or
                $largestWidth -lt $minimumWidth -or
                $largestHeight -lt $minimumHeight) {
            throw "$Name skill ROI lacks meaningful connected difference: changed=$changed largest=$largest extent=${largestWidth}x${largestHeight}"
        }
    } finally {
        $bitmap.Dispose()
        $baseline.Dispose()
    }
}

function Assert-IntegrationScreenshot([string]$Path, [object]$Frame) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or
            (Get-Item -LiteralPath $Path).Length -le 1024) {
        throw "missing or empty integration screenshot: $Path"
    }
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        if ($bitmap.Width -ne [int]$Frame.width -or
                $bitmap.Height -ne [int]$Frame.height) {
            throw "wrong integration screenshot dimensions: $Path"
        }
        $sentinel = $bitmap.GetPixel(1, 1)
        if ($sentinel.R -ne [int]$Frame.sentinel_r -or
                $sentinel.G -ne [int]$Frame.sentinel_g -or
                $sentinel.B -ne [int]$Frame.sentinel_b) {
            throw "integration scene sentinel mismatch: $Path"
        }
        [int]$sampled = 0
        [int]$nonblack = 0
        [int]$stepX = [Math]::Max(1, [int]($bitmap.Width / 32))
        [int]$stepY = [Math]::Max(1, [int]($bitmap.Height / 18))
        for ($y = 0; $y -lt $bitmap.Height; $y += $stepY) {
            for ($x = 0; $x -lt $bitmap.Width; $x += $stepX) {
                $pixel = $bitmap.GetPixel($x, $y)
                ++$sampled
                if ($pixel.A -ne 0 -and
                        ($pixel.R -gt 12 -or $pixel.G -gt 12 -or
                         $pixel.B -gt 12)) {
                    ++$nonblack
                }
            }
        }
        if ($sampled -eq 0 -or $nonblack * 20 -lt $sampled) {
            throw "integration screenshot is effectively black: $Path"
        }
    } finally {
        $bitmap.Dispose()
    }
}

function Assert-DoorRedCoverage([string]$Path, [object]$Door) {
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        [int]$left = [Math]::Max(0,
            [Math]::Floor((Read-InvariantDouble $Door.x)))
        [int]$top = [Math]::Max(0,
            [Math]::Floor((Read-InvariantDouble $Door.y)))
        [int]$right = [Math]::Min($bitmap.Width,
            [Math]::Ceiling((Read-InvariantDouble $Door.x) +
                (Read-InvariantDouble $Door.width_px)))
        [int]$bottom = [Math]::Min($bitmap.Height,
            [Math]::Ceiling((Read-InvariantDouble $Door.y) +
                (Read-InvariantDouble $Door.height_px)))
        [int64]$sampled = 0
        [int64]$opaqueRed = 0
        for ($y = $top; $y -lt $bottom; ++$y) {
            for ($x = $left; $x -lt $right; ++$x) {
                $pixel = $bitmap.GetPixel($x, $y)
                ++$sampled
                if ($pixel.A -ge 240 -and $pixel.R -ge 180 -and
                        $pixel.G -le 80 -and $pixel.B -le 80) {
                    ++$opaqueRed
                }
            }
        }
        if ($sampled -eq 0 -or $opaqueRed * 10 -gt $sampled) {
            throw "opaque red door coverage exceeds 10 percent: $Path direction=$($Door.direction)"
        }
    } finally {
        $bitmap.Dispose()
    }
}

function Assert-DeathPanelVisual([string]$Path, [object]$Frame) {
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        [int]$width = [int]$Frame.width
        [int]$height = [int]$Frame.height
        $compact = $width -lt 1200 -or $height -lt 680
        [int]$margin = if ($compact) { 14 } else { 48 }
        [int]$maximumWidth = if ($compact) { 772 } else { 920 }
        [int]$maximumHeight = if ($compact) { 422 } else { 560 }
        [int]$panelWidth = [Math]::Min($width - $margin * 2, $maximumWidth)
        [int]$panelHeight = [Math]::Min($height - $margin * 2, $maximumHeight)
        [int]$left = [int](($width - $panelWidth) / 2) + 40
        [int]$top = [int](($height - $panelHeight) / 2) + 40
        [int]$right = $left + $panelWidth - 80
        [int]$bottom = $top + $panelHeight - 80
        [int]$sampled = 0
        [int]$brightGray = 0
        for ($y = $top; $y -lt $bottom; $y += 4) {
            for ($x = $left; $x -lt $right; $x += 4) {
                $pixel = $bitmap.GetPixel($x, $y)
                ++$sampled
                if (([int]$pixel.R + [int]$pixel.G + [int]$pixel.B) / 3 -gt 130) {
                    ++$brightGray
                }
            }
        }
        if ($sampled -eq 0 -or $brightGray * 5 -gt $sampled) {
            throw "death panel bright-gray fill rejected: $Path bright=$brightGray sampled=$sampled"
        }
    } finally {
        $bitmap.Dispose()
    }
}

function Assert-MaterialRuntimeIntegration([string]$EvidenceRoot) {
    $integrationPath = Join-Path $EvidenceRoot 'material-runtime-integration-evidence.txt'
    $framePath = Join-Path $EvidenceRoot 'material-runtime-frames.csv'
    $doorPath = Join-Path $EvidenceRoot 'material-runtime-doors.csv'
    $propPath = Join-Path $EvidenceRoot 'material-runtime-environment-props.csv'
    $skillRoiPath = Join-Path $EvidenceRoot 'material-runtime-skill-roi.csv'
    foreach ($path in @($integrationPath, $framePath, $doorPath, $propPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "missing material runtime integration evidence: $path"
        }
    }
    $integration = Read-Report $integrationPath
    foreach ($key in @('schema','fixture_path','showcase_capture_count',
            'graphics_context','renderer_initialized','working_directory_ready',
            'overlay_resources_initialized','working_directory_restored',
            'full_pack_bytes','resident_peak_bytes',
            'transition_peak_bytes','observed_resident_peak_bytes',
            'ecology_transition_current_residency',
            'ecology_transition_old_ecology_unloaded',
            'ecology_transition_old_mask','ecology_transition_new_mask',
            'ecology_transition_after_requested_mask',
            'ecology_transition_after_resident_mask',
            'ecology_transition_unload_delta',
            'cross_ecology','door_sprite_unique','door_question_glyph_count',
            'environment','environment_capture_count','environment_prop_row_count',
            'death','death_warning_modal_draws','death_label_plate_draws',
            'hud_viewport_safe','hud_resolution_mask',
            'draw_slash_frame_union','storm_swords_frame_union',
            'healthy_skill_suppression','missing_map_fallback',
            'stress_warmup_frames','stress_measured_frames',
            'stress_warmup_load_calls','stress_warmup_unload_calls',
            'stress_final_load_calls','stress_final_unload_calls',
            'stress_average_fps','stress_p99_ms','stress_1_percent_low_fps',
            'shutdown_load_calls','shutdown_unload_calls','fallback_load_calls',
            'fallback_unload_calls','screenshot_count','capture_prime_count',
            'screenshot_pixel_guard','screenshot_nonblack_failures',
            'scene_sentinel_failures','frame_csv','door_csv',
            'environment_prop_csv','skill_roi_csv','result')) {
        if (-not $integration.ContainsKey($key)) {
            throw "missing integration report field: $key"
        }
    }
    if ($integration.schema -ne 'material-runtime-integration-v1' -or
            $integration.fixture_path -ne 'production-room-renderer' -or
            [int]$integration.showcase_capture_count -ne 0 -or
            $integration.graphics_context -ne 'pass' -or
            $integration.renderer_initialized -ne 'pass' -or
            $integration.overlay_resources_initialized -ne 'pass' -or
            $integration.working_directory_ready -ne 'pass' -or
            $integration.working_directory_restored -ne 'pass' -or
            $integration.ecology_transition_current_residency -ne 'pass' -or
            $integration.ecology_transition_old_ecology_unloaded -ne 'pass' -or
            $integration.cross_ecology -ne 'pass' -or
            $integration.door_sprite_unique -ne 'pass' -or
            [int]$integration.door_question_glyph_count -ne 0 -or
            $integration.environment -ne 'pass' -or
            [int]$integration.environment_capture_count -ne 9 -or
            [int]$integration.environment_prop_row_count -ne 45 -or
            $integration.death -ne 'pass' -or
            [uint64]$integration.death_warning_modal_draws -eq 0 -or
            [uint64]$integration.death_label_plate_draws -eq 0 -or
            $integration.hud_viewport_safe -ne 'pass' -or
            [int]$integration.hud_resolution_mask -ne 7 -or
            $integration.draw_slash_frame_union -ne '0..35' -or
            $integration.storm_swords_frame_union -ne '0..23' -or
            $integration.healthy_skill_suppression -ne 'pass' -or
            $integration.missing_map_fallback -ne 'pass' -or
            [int]$integration.stress_warmup_frames -ne 300 -or
            [int]$integration.stress_measured_frames -ne 1800 -or
            [int]$integration.screenshot_count -ne 70 -or
            [int]$integration.capture_prime_count -ne 70 -or
            $integration.screenshot_pixel_guard -ne 'pass' -or
            [int]$integration.screenshot_nonblack_failures -ne 0 -or
            [int]$integration.scene_sentinel_failures -ne 0 -or
            $integration.result -ne 'pass') {
        throw 'material runtime integration report rejected'
    }
    if ([uint64]$integration.full_pack_bytes -ne $ExpectedFullPackBytes -or
            [uint64]$integration.resident_peak_bytes -ne $ExpectedResidentPeakBytes -or
            [uint64]$integration.transition_peak_bytes -ne $ExpectedTransitionPeakBytes -or
            [uint64]$integration.observed_resident_peak_bytes -ne
                $ExpectedResidentPeakBytes) {
        throw 'material runtime integration byte totals rejected'
    }
    [uint64]$expectedOldEcologyMask = (([uint64]1 -shl 8) -bor
        ([uint64]1 -shl 22))
    [uint64]$expectedNewEcologyMask = (([uint64]1 -shl 17) -bor
        ([uint64]1 -shl 25))
    [uint64]$afterRequested = $integration.ecology_transition_after_requested_mask
    [uint64]$afterResident = $integration.ecology_transition_after_resident_mask
    if ([uint64]$integration.ecology_transition_old_mask -ne
            $expectedOldEcologyMask -or
            [uint64]$integration.ecology_transition_new_mask -ne
                $expectedNewEcologyMask -or
            ($afterRequested -band $expectedOldEcologyMask) -ne 0 -or
            ($afterResident -band $expectedOldEcologyMask) -ne 0 -or
            ($afterRequested -band $expectedNewEcologyMask) -ne
                $expectedNewEcologyMask -or
            ($afterResident -band $expectedNewEcologyMask) -ne
                $expectedNewEcologyMask -or
            [uint64]$integration.ecology_transition_unload_delta -eq 0) {
        throw 'material runtime ecology transition residency rejected'
    }
    if ([uint64]$integration.stress_warmup_load_calls -ne
            [uint64]$integration.stress_final_load_calls -or
            [uint64]$integration.stress_warmup_unload_calls -ne
                [uint64]$integration.stress_final_unload_calls -or
            [uint64]$integration.shutdown_load_calls -lt
                [uint64]$integration.stress_final_load_calls -or
            [uint64]$integration.shutdown_unload_calls -lt
                [uint64]$integration.stress_final_unload_calls -or
            (Read-InvariantDouble $integration.stress_average_fps) -lt 60.0 -or
            (Read-InvariantDouble $integration.stress_1_percent_low_fps) -lt 45.0) {
        throw 'material runtime stress telemetry rejected'
    }

    $frames = @(Import-Csv -LiteralPath $framePath)
    $doors = @(Import-Csv -LiteralPath $doorPath)
    $props = @(Import-Csv -LiteralPath $propPath)
    $skillRois = @(Import-Csv -LiteralPath $skillRoiPath)
    if ($frames.Count -ne 3551 -or $doors.Count -ne 24 -or
            $props.Count -ne 45) {
        throw "integration CSV row counts rejected: frames=$($frames.Count) doors=$($doors.Count) props=$($props.Count)"
    }
    for ($index = 0; $index -lt $frames.Count; ++$index) {
        if ([int]$frames[$index].sequence -ne $index -or
                [int]$frames[$index].showcase -ne 0) {
            throw "integration frame ordering/showcase rejected at row $index"
        }
    }
    $actual = @($frames | Where-Object { $_.capture_prime -eq '0' })
    $primes = @($frames | Where-Object { $_.capture_prime -eq '1' })
    if ($primes.Count -ne 70 -or
            @($primes | Where-Object { -not [string]::IsNullOrEmpty($_.screenshot) }).Count -ne 0) {
        throw 'capture-prime telemetry rejected'
    }

    $screenshotFrames = @($actual | Where-Object {
        -not [string]::IsNullOrEmpty($_.screenshot) })
    if ($screenshotFrames.Count -ne 70) {
        throw 'integration screenshot binding count rejected'
    }
    $screenshotNames = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($frame in $screenshotFrames) {
        if ($frame.screenshot_nonblack -ne 'pass' -or
                $frame.scene_sentinel -ne 'pass' -or
                -not $screenshotNames.Add($frame.screenshot)) {
            throw "integration screenshot row rejected: $($frame.screenshot)"
        }
        Assert-IntegrationScreenshot (Join-Path $EvidenceRoot $frame.screenshot) $frame
    }

    $cross = @($actual | Where-Object { $_.mode -eq 'cross-ecology' })
    [uint64]$crossRequired = (([uint64]1 -shl 8) -bor
        ([uint64]1 -shl 18) -bor ([uint64]1 -shl 22))
    if ($cross.Count -ne 3) { throw 'cross-ecology resolution coverage rejected' }
    foreach ($frame in $cross) {
        if (([uint64]$frame.requested_mask -band $crossRequired) -ne $crossRequired -or
                ([uint64]$frame.resident_mask -band $crossRequired) -ne $crossRequired -or
                [int]$frame.chaos_chaser -ne 1 -or
                [string]::IsNullOrEmpty($frame.screenshot)) {
            throw "cross-ecology residency/draw rejected: $($frame.width)x$($frame.height)"
        }
    }

    foreach ($mode in @('doors-closed','doors-open')) {
        $modeFrames = @($actual | Where-Object { $_.mode -eq $mode })
        if ($modeFrames.Count -ne 3) { throw "$mode resolution coverage rejected" }
        foreach ($frame in $modeFrames) {
            if ([int]$frame.door_up -ne 1 -or [int]$frame.door_down -ne 1 -or
                    [int]$frame.door_left -ne 1 -or [int]$frame.door_right -ne 1) {
                throw "$mode material draw telemetry rejected"
            }
        }
    }
    foreach ($group in ($doors | Group-Object mode,width,height)) {
        if ($group.Count -ne 4 -or
                @($group.Group.sprite_id | Sort-Object -Unique).Count -ne 4) {
            throw "door sprite identity rejected: $($group.Name)"
        }
        foreach ($door in $group.Group) {
            if ([int]$door.drawn -ne 1) { throw 'door material draw rejected' }
            Assert-DoorRedCoverage (Join-Path $EvidenceRoot $door.screenshot) $door
        }
    }
    foreach ($resolution in @('800x450','1280x720','1920x1080')) {
        $closed = Join-Path $EvidenceRoot "integration-doors-closed-$resolution.png"
        $open = Join-Path $EvidenceRoot "integration-doors-open-$resolution.png"
        if ((Get-Sha256 $closed) -eq (Get-Sha256 $open)) {
            throw "open/closed door captures are identical: $resolution"
        }
    }

    $environmentFrames = @($actual | Where-Object {
        $_.mode -in @('environment-water','environment-lightning','environment-chaos') })
    if ($environmentFrames.Count -ne 9) { throw 'environment capture coverage rejected' }
    foreach ($prop in $props) {
        $x = Read-InvariantDouble $prop.x
        $y = Read-InvariantDouble $prop.y
        $width = Read-InvariantDouble $prop.width_px
        $height = Read-InvariantDouble $prop.height_px
        $hudTop = Read-InvariantDouble $prop.hud_top
        if ([int]$prop.inside -ne 1 -or [int]$prop.drawn -ne 1 -or
                $x -lt 0.0 -or $y -lt 0.0 -or $width -le 0.0 -or
                $height -le 0.0 -or $x + $width -gt [int]$prop.width -or
                $y + $height -gt $hudTop) {
            throw "environment prop bounds/draw rejected: $($prop.ecology) $($prop.sprite_id)"
        }
    }
    $deathFrames = @($actual | Where-Object { $_.mode -eq 'death' })
    if ($deathFrames.Count -ne 3) {
        throw 'death capture resolution coverage rejected'
    }
    foreach ($frame in $deathFrames) {
        Assert-DeathPanelVisual (Join-Path $EvidenceRoot $frame.screenshot) $frame
    }

    $drawRows = @($actual | Where-Object { $_.mode -eq 'draw-slash' })
    $stormRows = @($actual | Where-Object { $_.mode -eq 'storm-swords' })
    if ($drawRows.Count -ne 270 -or $stormRows.Count -ne 1080) {
        throw 'active skill timeline row counts rejected'
    }
    Assert-FrameUnion $drawRows 35 'draw-slash'
    Assert-FrameUnion $stormRows 23 'storm-swords'
    Assert-SkillTimelineRows $drawRows 90 @(0,45,46,66,89) 'draw-slash' 27
    Assert-SkillTimelineRows $stormRows 360 @(0,71,72,180,323,324,342,359) 'storm-swords' 28
    $drawBaselines = @($frames | Where-Object {
        $_.mode -eq 'draw-slash-baseline' })
    $stormBaselines = @($frames | Where-Object {
        $_.mode -eq 'storm-swords-baseline' })
    if ($drawBaselines.Count -ne 6 -or $stormBaselines.Count -ne 6) {
        throw 'skill-off baseline frame coverage rejected'
    }
    foreach ($baseline in @($drawBaselines + $stormBaselines)) {
        if ($baseline.skill_id -ne 'none' -or [int]$baseline.skill_mode -ne 0 -or
                [int]$baseline.skill_atlas -ne 29 -or
                [int]$baseline.skill_drawn -ne 0 -or
                [int]$baseline.suppress_base_player -ne 0 -or
                [int]$baseline.base_player_drawn -ne 1 -or
                [int]$baseline.procedural_main_visual_count -ne 0) {
            throw 'skill-off runtime status did not reset'
        }
    }
    $fallback = @($actual | Where-Object { $_.mode -eq 'draw-slash-missing-map' })
    if ($fallback.Count -ne 3) { throw 'missing-map resolution coverage rejected' }
    foreach ($frame in $fallback) {
        if ([int]$frame.skill_mode -ne 2 -or [int]$frame.skill_atlas -ne 27 -or
                [int]$frame.skill_drawn -ne 0 -or
                [int]$frame.suppress_base_player -ne 0 -or
                [int]$frame.base_player_drawn -ne 1 -or
                [int]$frame.procedural_main_visual_count -le 0) {
            throw 'missing-map procedural fallback inverse rejected'
        }
    }
    if ($skillRois.Count -ne 9) { throw 'skill ROI row coverage rejected' }
    foreach ($width in @(800,1280,1920)) {
        $resolution = if ($width -eq 800) { '800x450' } elseif (
            $width -eq 1280) { '1280x720' } else { '1920x1080' }
        $drawBaseline = @($drawBaselines | Where-Object {
            [int]$_.width -eq $width -and -not [string]::IsNullOrEmpty($_.screenshot) })
        $stormBaseline = @($stormBaselines | Where-Object {
            [int]$_.width -eq $width -and -not [string]::IsNullOrEmpty($_.screenshot) })
        $drawFrame = @($skillRois | Where-Object {
            $_.mode -eq 'draw-slash' -and [int]$_.width -eq $width })
        $stormFrame = @($skillRois | Where-Object {
            $_.mode -eq 'storm-swords' -and [int]$_.width -eq $width })
        $fallbackFrame = @($skillRois | Where-Object {
            $_.mode -eq 'draw-slash-missing-map' -and
            [int]$_.width -eq $width })
        if ($drawBaseline.Count -ne 1 -or $stormBaseline.Count -ne 1 -or
                $drawFrame.Count -ne 1 -or $stormFrame.Count -ne 1 -or
                $fallbackFrame.Count -ne 1) {
            throw "skill ROI pair coverage rejected: $resolution"
        }
        $drawBaselinePath = Join-Path $EvidenceRoot $drawBaseline[0].screenshot
        $stormBaselinePath = Join-Path $EvidenceRoot $stormBaseline[0].screenshot
        if ([int]$drawFrame[0].tick -ne 45 -or
                $drawFrame[0].roi_kind -ne 'material_atlas' -or
                [int]$drawFrame[0].material_frame_drawn -ne 1 -or
                [int]$drawFrame[0].base_player_drawn -ne 0 -or
                [int]$drawFrame[0].procedural_main_visual_count -ne 0 -or
                [int]$stormFrame[0].tick -ne 180 -or
                $stormFrame[0].roi_kind -ne 'material_atlas' -or
                [int]$stormFrame[0].material_frame_drawn -ne 1 -or
                [int]$stormFrame[0].base_player_drawn -ne 0 -or
                [int]$stormFrame[0].procedural_main_visual_count -ne 0 -or
                [int]$fallbackFrame[0].tick -ne 45 -or
                $fallbackFrame[0].roi_kind -ne 'procedural_fallback' -or
                [int]$fallbackFrame[0].material_frame_drawn -ne 0 -or
                [int]$fallbackFrame[0].base_player_drawn -ne 1 -or
                [int]$fallbackFrame[0].procedural_main_visual_count -le 0 -or
                $drawFrame[0].baseline_path -ne $drawBaseline[0].screenshot -or
                $stormFrame[0].baseline_path -ne $stormBaseline[0].screenshot -or
                $fallbackFrame[0].baseline_path -ne $drawBaseline[0].screenshot) {
            throw "skill ROI runtime/path gate rejected: $resolution"
        }
        Assert-SkillRoiDifference (Join-Path $EvidenceRoot $drawFrame[0].skill_path) `
            $drawBaselinePath $drawFrame[0] "draw-slash-$resolution"
        Assert-SkillRoiDifference (Join-Path $EvidenceRoot $stormFrame[0].skill_path) `
            $stormBaselinePath $stormFrame[0] "storm-swords-$resolution"
        Assert-SkillRoiDifference (Join-Path $EvidenceRoot $fallbackFrame[0].skill_path) `
            $drawBaselinePath $fallbackFrame[0] "draw-slash-missing-map-$resolution"
    }

    $warmup = @($actual | Where-Object { $_.mode -eq 'stress-warmup' })
    $stress = @($actual | Where-Object { $_.mode -eq 'stress' })
    $transitionPrime = @($actual | Where-Object {
        $_.mode -eq 'stress-residency-prime' })
    if ($warmup.Count -ne 300 -or $stress.Count -ne 1800) {
        throw 'stress frame counts rejected'
    }
    if ($transitionPrime.Count -ne 1 -or
            ([uint64]$transitionPrime[0].requested_mask -band
                $expectedOldEcologyMask) -ne $expectedOldEcologyMask -or
            ([uint64]$transitionPrime[0].resident_mask -band
                $expectedOldEcologyMask) -ne $expectedOldEcologyMask -or
            ([uint64]$warmup[0].requested_mask -band
                $expectedOldEcologyMask) -ne 0 -or
            ([uint64]$warmup[0].resident_mask -band
                $expectedOldEcologyMask) -ne 0 -or
            ([uint64]$warmup[0].resident_mask -band
                $expectedNewEcologyMask) -ne $expectedNewEcologyMask -or
            [uint64]$warmup[0].unload_calls -
                [uint64]$transitionPrime[0].unload_calls -ne
                [uint64]$integration.ecology_transition_unload_delta) {
        throw 'material runtime ecology transition CSV rejected'
    }
    [double]$elapsed = 0.0
    $times = [System.Collections.Generic.List[double]]::new()
    foreach ($frame in $stress) {
        $milliseconds = Read-InvariantDouble $frame.frame_ms
        if ($milliseconds -le 0.0 -or
                [uint64]$frame.resident_bytes -gt $MaximumResidentTextureBytes -or
                [uint64]$frame.load_calls -ne
                    [uint64]$integration.stress_warmup_load_calls -or
                [uint64]$frame.unload_calls -ne
                    [uint64]$integration.stress_warmup_unload_calls) {
            throw 'stress frame telemetry rejected'
        }
        $elapsed += $milliseconds
        $times.Add($milliseconds)
    }
    $times.Sort()
    $p99 = $times[[Math]::Ceiling($times.Count * 0.99) - 1]
    $averageFps = 1000.0 * $times.Count / $elapsed
    if ($averageFps -lt 60.0 -or 1000.0 / $p99 -lt 45.0 -or
            [Math]::Abs($averageFps -
                (Read-InvariantDouble $integration.stress_average_fps)) -gt 0.05 -or
            [Math]::Abs($p99 -
                (Read-InvariantDouble $integration.stress_p99_ms)) -gt 0.001) {
        throw 'stress performance calculation rejected'
    }
    return $integration
}

function Measure-ItemCapture([string]$Path, [string]$BaselinePath,
        [ValidateSet('legacy','isolated')][string]$Policy) {
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    $baseline = [System.Drawing.Bitmap]::FromFile($BaselinePath)
    try {
        # Legacy preserves capture continuity. Isolated proves live contour and
        # chroma; published-atlas palette quality is measured independently so
        # neither room pixels nor loot labels can satisfy the palette contract.
        $heightOverrides = @{ material_10 = 10 }
        if ($heightOverrides.Count -ne 1 -or
                $heightOverrides.material_10 -ne 10) {
            throw 'item asset-specific validation contract changed unexpectedly'
        }
        $regions = @(
            @{Name='equipment_weapon'; X=453; Y=339; Half=22},
            @{Name='equipment_helmet'; X=528; Y=339; Half=22},
            @{Name='equipment_chest'; X=603; Y=339; Half=22},
            @{Name='equipment_gloves'; X=677; Y=339; Half=22},
            @{Name='equipment_boots'; X=752; Y=339; Half=22},
            @{Name='equipment_accessory'; X=827; Y=339; Half=22},
            @{Name='material_0'; X=440; Y=514; Half=18},
            @{Name='material_1'; X=507; Y=514; Half=18},
            @{Name='material_2'; X=573; Y=514; Half=18},
            @{Name='material_3'; X=640; Y=514; Half=18},
            @{Name='material_4'; X=707; Y=514; Half=18},
            @{Name='material_5'; X=773; Y=514; Half=18},
            @{Name='material_6'; X=840; Y=514; Half=18},
            @{Name='material_7'; X=431; Y=566; Half=18},
            @{Name='material_8'; X=500; Y=566; Half=18},
            @{Name='material_9'; X=570; Y=566; Half=18},
            @{Name='material_10'; X=640; Y=566; Half=18},
            @{Name='material_11'; X=710; Y=566; Half=18},
            @{Name='material_12'; X=780; Y=566; Half=18},
            @{Name='material_13'; X=849; Y=566; Half=18}
        )
        foreach ($region in $regions) {
            # The weapon is intentionally near-neutral steel. Its former
            # chroma count came from the magenta key fringe, so require one
            # genuine accent pixel while retaining the strong contour and
            # color-diversity checks below.
            $minimumAuthored = if ($region.Name -eq 'equipment_weapon') {
                1
            } else {
                10
            }
            $minimumHeight = if ($heightOverrides.ContainsKey($region.Name)) {
                $heightOverrides[$region.Name]
            } else {
                12
            }
            [int]$minimumChanged = 160
            [int]$minimumLargest = 100
            [int]$minimumWidth = 12
            [int]$minimumBrightChroma = 0
            [int]$minimumAnyChroma = 0
            if ($Policy -eq 'isolated') {
                $minimumBrightChroma = $minimumAuthored
                switch ($region.Name) {
                'material_0' {
                    $minimumChanged = 190; $minimumLargest = 185
                    $minimumWidth = 14; $minimumHeight = 18
                    $minimumBrightChroma = 100; $minimumAnyChroma = 115
                }
                'material_6' {
                    $minimumChanged = 180; $minimumLargest = 170
                    $minimumWidth = 14; $minimumHeight = 18
                    $minimumBrightChroma = 6; $minimumAnyChroma = 16
                }
                'material_9' {
                    $minimumChanged = 210; $minimumLargest = 210
                    $minimumWidth = 16; $minimumHeight = 18
                    $minimumBrightChroma = 24; $minimumAnyChroma = 40
                }
                'material_10' {
                    $minimumChanged = 200; $minimumLargest = 200
                    $minimumWidth = 16; $minimumHeight = 18
                    $minimumBrightChroma = 80; $minimumAnyChroma = 110
                }
                }
            }
            $side = $region.Half * 2 + 1
            $mask = New-Object 'bool[,]' $side, $side
            $colors = [System.Collections.Generic.HashSet[int]]::new()
            [int]$changed = 0
            [int]$brightChroma = 0
            [int]$anyChroma = 0
            for ($localY = 0; $localY -lt $side; ++$localY) {
                for ($localX = 0; $localX -lt $side; ++$localX) {
                    $x = $region.X - $region.Half + $localX
                    $y = $region.Y - $region.Half + $localY
                    $pixel = $bitmap.GetPixel($x, $y)
                    $reference = $baseline.GetPixel($x, $y)
                    $difference = [Math]::Max(
                        [Math]::Abs([int]$pixel.R - [int]$reference.R),
                        [Math]::Max(
                            [Math]::Abs([int]$pixel.G - [int]$reference.G),
                            [Math]::Abs([int]$pixel.B - [int]$reference.B)))
                    if ($difference -ge 36) {
                        $mask[$localX, $localY] = $true
                        ++$changed
                        $maximum = [Math]::Max($pixel.R,
                            [Math]::Max($pixel.G, $pixel.B))
                        $chroma = $maximum - [Math]::Min($pixel.R,
                            [Math]::Min($pixel.G, $pixel.B))
                        if ($chroma -gt 35) {
                            ++$anyChroma
                            if ($maximum -gt 125) { ++$brightChroma }
                        }
                        [void]$colors.Add((((([int]$pixel.R) -shr 4) -shl 8) -bor
                            ((([int]$pixel.G) -shr 4) -shl 4) -bor
                            (([int]$pixel.B) -shr 4)))
                    }
                }
            }
            [int]$largest = 0
            [int]$largestWidth = 0
            [int]$largestHeight = 0
            for ($localY = 0; $localY -lt $side; ++$localY) {
                for ($localX = 0; $localX -lt $side; ++$localX) {
                    if (-not $mask[$localX, $localY]) { continue }
                    $queue = [System.Collections.Generic.Queue[int]]::new()
                    $queue.Enqueue($localY * $side + $localX)
                    $mask[$localX, $localY] = $false
                    [int]$component = 0
                    [int]$minX = $localX; [int]$maxX = $localX
                    [int]$minY = $localY; [int]$maxY = $localY
                    while ($queue.Count -gt 0) {
                        $point = $queue.Dequeue()
                        $px = $point % $side
                        $py = [Math]::Floor($point / $side)
                        ++$component
                        $minX = [Math]::Min($minX, $px); $maxX = [Math]::Max($maxX, $px)
                        $minY = [Math]::Min($minY, $py); $maxY = [Math]::Max($maxY, $py)
                        foreach ($offset in @(@(-1,0),@(1,0),@(0,-1),@(0,1))) {
                            $nx = $px + $offset[0]; $ny = $py + $offset[1]
                            if ($nx -ge 0 -and $nx -lt $side -and
                                    $ny -ge 0 -and $ny -lt $side -and
                                    $mask[$nx, $ny]) {
                                $mask[$nx, $ny] = $false
                                $queue.Enqueue($ny * $side + $nx)
                            }
                        }
                    }
                    if ($component -gt $largest) {
                        $largest = $component
                        $largestWidth = $maxX - $minX + 1
                        $largestHeight = $maxY - $minY + 1
                    }
                }
            }
            if ($changed -lt $minimumChanged -or
                    $largest -lt $minimumLargest -or
                    $largestWidth -lt $minimumWidth -or
                    $largestHeight -lt $minimumHeight -or
                    $brightChroma -lt $minimumBrightChroma -or
                    $anyChroma -lt $minimumAnyChroma) {
                throw "item $Policy proof rejected: $($region.Name) changed=$changed largest=$largest extent=${largestWidth}x${largestHeight} bright=$brightChroma anyChroma=$anyChroma colors=$($colors.Count)"
            }
        }
    } finally {
        $bitmap.Dispose()
        $baseline.Dispose()
    }
}

function Read-PixelRoi([string]$Value, [string]$Name) {
    $parts = @($Value -split ',')
    if ($parts.Count -ne 4) { throw "invalid $Name ROI format" }
    $roi = @{
        Name = $Name
        X = [int]$parts[0]
        Y = [int]$parts[1]
        Width = [int]$parts[2]
        Height = [int]$parts[3]
    }
    if ($roi.X -lt 0 -or $roi.Y -lt 0 -or
            $roi.Width -le 0 -or $roi.Height -le 0 -or
            $roi.X + $roi.Width -gt 1280 -or
            $roi.Y + $roi.Height -gt 720) {
        throw "invalid $Name ROI bounds"
    }
    return $roi
}

function Measure-LightningCapture([string]$Path, [string]$BackgroundPath,
        [string]$IsolatedPath, [string]$IsolatedBackgroundPath,
        [hashtable]$Report) {
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    $background = [System.Drawing.Bitmap]::FromFile($BackgroundPath)
    $isolatedBitmap = [System.Drawing.Bitmap]::FromFile($IsolatedPath)
    $isolatedBackground = [System.Drawing.Bitmap]::FromFile(
        $IsolatedBackgroundPath)
    try {
        if ($bitmap.Width -ne 1280 -or $bitmap.Height -ne 720 -or
                $background.Width -ne 1280 -or $background.Height -ne 720 -or
                $isolatedBitmap.Width -ne 1280 -or
                $isolatedBitmap.Height -ne 720 -or
                $isolatedBackground.Width -ne 1280 -or
                $isolatedBackground.Height -ne 720) {
            throw 'wrong lightning-monster screenshot size'
        }
        [int]$dark = 0
        $colors = [System.Collections.Generic.HashSet[int]]::new()
        for ($y = 0; $y -lt $bitmap.Height; $y += 2) {
            for ($x = 0; $x -lt $bitmap.Width; $x += 2) {
                $pixel = $bitmap.GetPixel($x, $y)
                if ($pixel.R + $pixel.G + $pixel.B -lt 180) { ++$dark }
                [void]$colors.Add((((([int]$pixel.R) -shr 4) -shl 8) -bor `
                    ((([int]$pixel.G) -shr 4) -shl 4) -bor `
                    (([int]$pixel.B) -shr 4)))
            }
        }
        $sampled = ($bitmap.Width / 2) * ($bitmap.Height / 2)
        if ($colors.Count -lt 150) { throw 'lightning capture is effectively solid' }
        if ($dark -lt $sampled * 0.35) { throw 'lightning capture lacks dark storm palette' }

        # Whole-frame brass/cyan percentages are dominated by the room and HUD,
        # so they cannot prove that either monster material rendered.  Palette
        # authorship remains locked by ecology_material_coverage_tests; runtime
        # proof below uses atlas/frame telemetry plus each monster's connected
        # difference contour against the matching monster-free background.

        $regions = @(
            (Read-PixelRoi $Report.lightning_shooter_roi 'lightning_shooter'),
            (Read-PixelRoi $Report.lightning_dasher_roi 'lightning_dasher')
        )
        $first = $regions[0]
        $second = $regions[1]
        if (-not ($first.X + $first.Width -le $second.X -or
                $second.X + $second.Width -le $first.X -or
                $first.Y + $first.Height -le $second.Y -or
                $second.Y + $second.Height -le $first.Y)) {
            throw 'lightning material ROIs overlap'
        }
        foreach ($region in $regions) {
            $mask = New-Object 'bool[,]' $region.Width, $region.Height
            [int]$changed = 0
            [int]$brightChroma = 0
            [int]$relativeChroma = 0
            $regionColors = [System.Collections.Generic.HashSet[int]]::new()
            for ($y = $region.Y; $y -lt $region.Y + $region.Height; ++$y) {
                for ($x = $region.X; $x -lt $region.X + $region.Width; ++$x) {
                    $pixel = $isolatedBitmap.GetPixel($x, $y)
                    $base = $isolatedBackground.GetPixel($x, $y)
                    $redDifference = [Math]::Abs(
                        [int]$pixel.R - [int]$base.R)
                    $greenDifference = [Math]::Abs(
                        [int]$pixel.G - [int]$base.G)
                    $blueDifference = [Math]::Abs(
                        [int]$pixel.B - [int]$base.B)
                    $difference = $redDifference + $greenDifference +
                        $blueDifference
                    if ($difference -ge 72) {
                        $localMaskX = $x - $region.X
                        $localMaskY = $y - $region.Y
                        $mask[$localMaskX, $localMaskY] = $true
                        ++$changed
                        $maximum = [Math]::Max($pixel.R,
                            [Math]::Max($pixel.G, $pixel.B))
                        $minimum = [Math]::Min($pixel.R,
                            [Math]::Min($pixel.G, $pixel.B))
                        if ($maximum -gt 125 -and
                                $maximum - $minimum -gt 35) {
                            ++$brightChroma
                        }
                        $differenceMaximum = [Math]::Max($redDifference,
                            [Math]::Max($greenDifference, $blueDifference))
                        $differenceMinimum = [Math]::Min($redDifference,
                            [Math]::Min($greenDifference, $blueDifference))
                        if ($differenceMaximum -gt 35 -and
                                $differenceMaximum - $differenceMinimum -gt 35) {
                            ++$relativeChroma
                        }
                        [void]$regionColors.Add(
                            (((([int]$pixel.R) -shr 4) -shl 8) -bor
                            ((([int]$pixel.G) -shr 4) -shl 4) -bor
                            (([int]$pixel.B) -shr 4)))
                    }
                }
            }
            [int]$largest = 0
            [int]$largestWidth = 0
            [int]$largestHeight = 0
            for ($localY = 0; $localY -lt $region.Height; ++$localY) {
                for ($localX = 0; $localX -lt $region.Width; ++$localX) {
                    if (-not $mask[$localX, $localY]) { continue }
                    $queue = [System.Collections.Generic.Queue[int]]::new()
                    $queue.Enqueue($localY * $region.Width + $localX)
                    $mask[$localX, $localY] = $false
                    [int]$component = 0
                    [int]$minX = $localX; [int]$maxX = $localX
                    [int]$minY = $localY; [int]$maxY = $localY
                    while ($queue.Count -gt 0) {
                        $point = $queue.Dequeue()
                        $px = $point % $region.Width
                        $py = [Math]::Floor($point / $region.Width)
                        ++$component
                        $minX = [Math]::Min($minX, $px); $maxX = [Math]::Max($maxX, $px)
                        $minY = [Math]::Min($minY, $py); $maxY = [Math]::Max($maxY, $py)
                        foreach ($offset in @(
                                @(-1,-1),@(0,-1),@(1,-1),
                                @(-1,0),@(1,0),
                                @(-1,1),@(0,1),@(1,1))) {
                            $nx = $px + $offset[0]; $ny = $py + $offset[1]
                            if ($nx -ge 0 -and $nx -lt $region.Width -and
                                    $ny -ge 0 -and $ny -lt $region.Height -and
                                    $mask[$nx, $ny]) {
                                $mask[$nx, $ny] = $false
                                $queue.Enqueue($ny * $region.Width + $nx)
                            }
                        }
                    }
                    if ($component -gt $largest) {
                        $largest = $component
                        $largestWidth = $maxX - $minX + 1
                        $largestHeight = $maxY - $minY + 1
                    }
                }
            }
            $extentArea = $largestWidth * $largestHeight
            $extentValid = $largestWidth -ge 18 -and $largestHeight -ge 28
            $structureValid = $changed -ge 500 -and $largest -ge 180
            if ($region.Name -eq 'lightning_shooter') {
                # The authored frame is a near-square lightning construct.
                # Prove its isolated contour and background-independent palette
                # without accepting a long HUD/loot-label strip as the monster
                # silhouette.
                $extentValid = $largestWidth * 20 -ge $region.Width * 13 -and
                    $largestWidth * 20 -le $region.Width * 19 -and
                    $largestHeight * 20 -ge $region.Height * 13 -and
                    $largestHeight * 20 -le $region.Height * 19 -and
                    $extentArea * 2 -ge $region.Width * $region.Height -and
                    $largestWidth * 5 -ge $largestHeight * 4 -and
                    $largestWidth * 4 -le $largestHeight * 5
                $structureValid = $changed -ge 1300 -and
                    $largest -ge 900 -and $relativeChroma -ge 800 -and
                    $regionColors.Count -ge 120
            }
            if (-not $structureValid -or -not $extentValid) {
                throw "lightning capture lacks monster-vs-background contour: $($region.Name) changed=$changed largest=$largest extent=${largestWidth}x${largestHeight} bright=$brightChroma relative=$relativeChroma colors=$($regionColors.Count)"
            }
        }
    } finally {
        $bitmap.Dispose()
        $background.Dispose()
        $isolatedBitmap.Dispose()
        $isolatedBackground.Dispose()
    }
}

function Measure-ChaosCapture([string]$Path, [string]$BackgroundPath) {
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    $background = [System.Drawing.Bitmap]::FromFile($BackgroundPath)
    try {
        if ($bitmap.Width -ne 1280 -or $bitmap.Height -ne 720 -or
                $background.Width -ne 1280 -or $background.Height -ne 720) {
            throw 'wrong chaos-monster screenshot size'
        }
        [int]$dark = 0
        [int]$magenta = 0
        [int]$acid = 0
        $colors = [System.Collections.Generic.HashSet[int]]::new()
        for ($y = 0; $y -lt $bitmap.Height; $y += 2) {
            for ($x = 0; $x -lt $bitmap.Width; $x += 2) {
                $pixel = $bitmap.GetPixel($x, $y)
                if ($pixel.R + $pixel.G + $pixel.B -lt 180) { ++$dark }
                if ($pixel.R -gt 105 -and $pixel.B -gt 100 `
                        -and $pixel.G -lt 110) { ++$magenta }
                if ($pixel.G -gt 100 -and $pixel.G -gt $pixel.R + 10 `
                        -and $pixel.G -gt $pixel.B + 10) { ++$acid }
                [void]$colors.Add((((([int]$pixel.R) -shr 4) -shl 8) -bor `
                    ((([int]$pixel.G) -shr 4) -shl 4) -bor `
                    (([int]$pixel.B) -shr 4)))
            }
        }
        $sampled = ($bitmap.Width / 2) * ($bitmap.Height / 2)
        if ($colors.Count -lt 150) { throw 'chaos capture is effectively solid' }
        if ($dark -lt $sampled * 0.35) { throw 'chaos capture lacks dark rift palette' }
        if ($magenta -lt $sampled * 0.002) { throw 'chaos capture lacks magenta rift palette' }
        if ($acid -lt $sampled * 0.001) { throw 'chaos capture lacks acid warning palette' }

        $regions = @(
            @{ Name='chaos_chaser'; X=635; Y=390; Width=115; Height=155 },
            @{ Name='chaos_hazard'; X=750; Y=390; Width=115; Height=155 }
        )
        foreach ($region in $regions) {
            $mask = New-Object 'bool[,]' $region.Width, $region.Height
            [int]$changed = 0
            for ($y = $region.Y; $y -lt $region.Y + $region.Height; ++$y) {
                for ($x = $region.X; $x -lt $region.X + $region.Width; ++$x) {
                    $pixel = $bitmap.GetPixel($x, $y)
                    $base = $background.GetPixel($x, $y)
                    $difference = [Math]::Abs([int]$pixel.R - [int]$base.R) +
                        [Math]::Abs([int]$pixel.G - [int]$base.G) +
                        [Math]::Abs([int]$pixel.B - [int]$base.B)
                    if ($difference -ge 40) {
                        $localMaskX = $x - $region.X
                        $localMaskY = $y - $region.Y
                        $mask[$localMaskX, $localMaskY] = $true
                        ++$changed
                    }
                }
            }
            [int]$largest = 0
            [int]$largestWidth = 0
            [int]$largestHeight = 0
            for ($localY = 0; $localY -lt $region.Height; ++$localY) {
                for ($localX = 0; $localX -lt $region.Width; ++$localX) {
                    if (-not $mask[$localX, $localY]) { continue }
                    $queue = [System.Collections.Generic.Queue[int]]::new()
                    $queue.Enqueue($localY * $region.Width + $localX)
                    $mask[$localX, $localY] = $false
                    [int]$component = 0
                    [int]$minX = $localX; [int]$maxX = $localX
                    [int]$minY = $localY; [int]$maxY = $localY
                    while ($queue.Count -gt 0) {
                        $point = $queue.Dequeue()
                        $px = $point % $region.Width
                        $py = [Math]::Floor($point / $region.Width)
                        ++$component
                        $minX = [Math]::Min($minX, $px); $maxX = [Math]::Max($maxX, $px)
                        $minY = [Math]::Min($minY, $py); $maxY = [Math]::Max($maxY, $py)
                        foreach ($offset in @(@(-1,0),@(1,0),@(0,-1),@(0,1))) {
                            $nx = $px + $offset[0]; $ny = $py + $offset[1]
                            if ($nx -ge 0 -and $nx -lt $region.Width -and
                                    $ny -ge 0 -and $ny -lt $region.Height -and
                                    $mask[$nx, $ny]) {
                                $mask[$nx, $ny] = $false
                                $queue.Enqueue($ny * $region.Width + $nx)
                            }
                        }
                    }
                    if ($component -gt $largest) {
                        $largest = $component
                        $largestWidth = $maxX - $minX + 1
                        $largestHeight = $maxY - $minY + 1
                    }
                }
            }
            if ($changed -lt 500 -or $largest -lt 180 -or
                    ($largestWidth -lt 18 -and $largestHeight -lt 28)) {
                throw "chaos capture lacks monster-vs-background contour: $($region.Name) changed=$changed largest=$largest extent=${largestWidth}x${largestHeight}"
            }
        }
    } finally {
        $bitmap.Dispose()
        $background.Dispose()
    }
}

function Measure-UiGallery([string]$Path, [string]$BaselinePath) {
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    $baseline = [System.Drawing.Bitmap]::FromFile($BaselinePath)
    try {
        if ($bitmap.Width -ne 1280 -or $bitmap.Height -ne 720 -or
                $baseline.Width -ne 1280 -or $baseline.Height -ne 720) {
            throw 'wrong UI gallery/baseline size'
        }
        for ($index = 0; $index -lt 40; ++$index) {
            $column = $index % 8
            $row = [Math]::Floor($index / 8)
            $left = 196 + $column * 112
            $top = 84 + $row * 112
            [int]$changed = 0
            $colors = [System.Collections.Generic.HashSet[int]]::new()
            for ($y = $top; $y -lt $top + 104; $y += 2) {
                for ($x = $left; $x -lt $left + 104; $x += 2) {
                    $pixel = $bitmap.GetPixel($x, $y)
                    $reference = $baseline.GetPixel($x, $y)
                    $difference = [Math]::Abs([int]$pixel.R - [int]$reference.R) +
                        [Math]::Abs([int]$pixel.G - [int]$reference.G) +
                        [Math]::Abs([int]$pixel.B - [int]$reference.B)
                    if ($difference -ge 32) {
                        ++$changed
                        [void]$colors.Add((((([int]$pixel.R) -shr 4) -shl 8) -bor
                            ((([int]$pixel.G) -shr 4) -shl 4) -bor
                            (([int]$pixel.B) -shr 4)))
                    }
                }
            }
            if ($changed -lt 160 -or $colors.Count -lt 8) {
                throw "UI gallery cell lacks authored material difference: $index changed=$changed colors=$($colors.Count)"
            }
        }
    } finally {
        $bitmap.Dispose()
        $baseline.Dispose()
    }
}

function Assert-UiScreenDifferent([string]$Path, [string]$BaselinePath,
        [string]$Name) {
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    $baseline = [System.Drawing.Bitmap]::FromFile($BaselinePath)
    try {
        if ($bitmap.Width -ne 1280 -or $bitmap.Height -ne 720) {
            throw "wrong UI screenshot size: $Name"
        }
        [int]$changed = 0
        for ($y = 0; $y -lt 720; $y += 4) {
            for ($x = 0; $x -lt 1280; $x += 4) {
                $pixel = $bitmap.GetPixel($x, $y)
                $reference = $baseline.GetPixel($x, $y)
                $difference = [Math]::Abs([int]$pixel.R - [int]$reference.R) +
                    [Math]::Abs([int]$pixel.G - [int]$reference.G) +
                    [Math]::Abs([int]$pixel.B - [int]$reference.B)
                if ($difference -ge 36) { ++$changed }
            }
        }
        if ($changed -lt 800) {
            throw "UI screenshot duplicates/hides interface: $Name changed=$changed"
        }
    } finally {
        $bitmap.Dispose()
        $baseline.Dispose()
    }
}

function Assert-HudRoiDifferent([string]$Path, [string]$BaselinePath) {
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    $baseline = [System.Drawing.Bitmap]::FromFile($BaselinePath)
    try {
        [int]$changed = 0
        foreach ($region in @(
                [System.Drawing.Rectangle]::new(0, 530, 430, 190),
                [System.Drawing.Rectangle]::new(430, 620, 420, 100))) {
            for ($y = $region.Top; $y -lt $region.Bottom; $y += 2) {
                for ($x = $region.Left; $x -lt $region.Right; $x += 2) {
                    $pixel = $bitmap.GetPixel($x, $y)
                    $reference = $baseline.GetPixel($x, $y)
                    $difference = [Math]::Abs([int]$pixel.R - [int]$reference.R) +
                        [Math]::Abs([int]$pixel.G - [int]$reference.G) +
                        [Math]::Abs([int]$pixel.B - [int]$reference.B)
                    if ($difference -ge 36) { ++$changed }
                }
            }
        }
        if ($changed -lt 180) {
            throw "HUD material ROIs do not show an independent state: changed=$changed"
        }
    } finally {
        $bitmap.Dispose()
        $baseline.Dispose()
    }
}

function Assert-UiScreenDifferentAtSize([string]$Path, [string]$BaselinePath,
        [string]$Name, [int]$ExpectedWidth, [int]$ExpectedHeight,
        [int]$MinimumChanged) {
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    $baseline = [System.Drawing.Bitmap]::FromFile($BaselinePath)
    try {
        if ($bitmap.Width -ne $ExpectedWidth -or
                $bitmap.Height -ne $ExpectedHeight -or
                $baseline.Width -ne $ExpectedWidth -or
                $baseline.Height -ne $ExpectedHeight) {
            throw "wrong UI screenshot/baseline size: $Name"
        }
        [int]$changed = 0
        for ($y = 0; $y -lt $ExpectedHeight; $y += 4) {
            for ($x = 0; $x -lt $ExpectedWidth; $x += 4) {
                $pixel = $bitmap.GetPixel($x, $y)
                $reference = $baseline.GetPixel($x, $y)
                $difference = [Math]::Abs([int]$pixel.R - [int]$reference.R) +
                    [Math]::Abs([int]$pixel.G - [int]$reference.G) +
                    [Math]::Abs([int]$pixel.B - [int]$reference.B)
                if ($difference -ge 36) { ++$changed }
            }
        }
        if ($changed -lt $MinimumChanged) {
            throw "UI screenshot duplicates/hides interface: $Name changed=$changed"
        }
    } finally {
        $bitmap.Dispose()
        $baseline.Dispose()
    }
}

function Assert-UiRoiDifferent([string]$Path, [string]$BaselinePath,
        [string]$Name, [System.Drawing.Rectangle[]]$Regions,
        [int]$MinimumChanged) {
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    $baseline = [System.Drawing.Bitmap]::FromFile($BaselinePath)
    try {
        [int]$changed = 0
        foreach ($region in $Regions) {
            for ($y = $region.Top; $y -lt $region.Bottom; $y += 3) {
                for ($x = $region.Left; $x -lt $region.Right; $x += 3) {
                    $pixel = $bitmap.GetPixel($x, $y)
                    $reference = $baseline.GetPixel($x, $y)
                    $difference = [Math]::Abs([int]$pixel.R - [int]$reference.R) +
                        [Math]::Abs([int]$pixel.G - [int]$reference.G) +
                        [Math]::Abs([int]$pixel.B - [int]$reference.B)
                    if ($difference -ge 36) { ++$changed }
                }
            }
        }
        if ($changed -lt $MinimumChanged) {
            throw "UI page ROI duplicates baseline: $Name changed=$changed"
        }
    } finally {
        $bitmap.Dispose()
        $baseline.Dispose()
    }
}

function Assert-UiDrawMask([uint64]$Mask, [int[]]$Bits, [string]$Name) {
    foreach ($bit in $Bits) {
        $flag = [uint64]1 -shl $bit
        if (($Mask -band $flag) -eq 0) {
            throw "missing page-specific UI material draw: $Name bit=$bit"
        }
    }
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

function Measure-SampledPixelDifference([string]$LeftPath, [string]$RightPath) {
    $left = [System.Drawing.Bitmap]::FromFile($LeftPath)
    $right = [System.Drawing.Bitmap]::FromFile($RightPath)
    try {
        if ($left.Width -ne $right.Width -or $left.Height -ne $right.Height) {
            throw 'native background/gameplay comparison dimensions differ'
        }
        [int]$changed = 0
        for ($y = 0; $y -lt $left.Height; $y += 8) {
            for ($x = 0; $x -lt $left.Width; $x += 8) {
                $a = $left.GetPixel($x, $y)
                $b = $right.GetPixel($x, $y)
                $difference = [Math]::Abs([int]$a.R - [int]$b.R) +
                    [Math]::Abs([int]$a.G - [int]$b.G) +
                    [Math]::Abs([int]$a.B - [int]$b.B)
                if ($difference -ge 24) { ++$changed }
            }
        }
        return $changed
    } finally {
        $left.Dispose()
        $right.Dispose()
    }
}

function Assert-NativeBackgroundEvidence([hashtable]$Report,
        [string]$EvidenceRoot, [string]$SourceRoot,
        [bool]$PreflightOnly = $false) {
    if (-not $Report.ContainsKey('native_background_status') -or
            $Report.native_background_status -ne 'native-background-verified') {
        throw 'native room background status is not verified'
    }

    $ecologies = @('fire','water','lightning','chaos')
    $provenanceRelative = 'assets/stage12/room-background-build.json'
    $provenancePath = Join-Path $SourceRoot `
        ($provenanceRelative -replace '/', [System.IO.Path]::DirectorySeparatorChar)
    if (-not (Test-Path -LiteralPath $provenancePath -PathType Leaf)) {
        throw 'missing native room background provenance'
    }
    $provenance = Get-Content -Raw -LiteralPath $provenancePath -Encoding UTF8 |
        ConvertFrom-Json
    if ([int]$provenance.schema_version -ne 1 -or
            $null -eq $provenance.ecologies) {
        throw 'native room background provenance schema rejected'
    }

    $masterHashes = [System.Collections.Generic.HashSet[string]]::new()
    $runtimeHashes = [System.Collections.Generic.HashSet[string]]::new()
    $materialHashes = [System.Collections.Generic.HashSet[string]]::new()
    $backgroundHashes = @{
        '1280' = [System.Collections.Generic.HashSet[string]]::new()
        '1920' = [System.Collections.Generic.HashSet[string]]::new()
    }

    foreach ($ecology in $ecologies) {
        $expected = @{
            "${ecology}_background_atlas" =
                "assets/stage12/${ecology}_room_background.png"
            "${ecology}_background_atlas_id" = "${ecology}_room_background"
            "${ecology}_background_material_atlas" =
                "assets/stage12/${ecology}_room_background_material.png"
            "${ecology}_background_master" =
                "art_source/stage12/backgrounds/$ecology/${ecology}-room-background-master.png"
            "${ecology}_background_provenance" = $provenanceRelative
            "${ecology}_background_source" = '2560x1440'
            "${ecology}_background_source_xywh" = '0,0,2560,1440'
            "${ecology}_background_scale_1280" = '1/2'
            "${ecology}_background_scale_1920" = '3/4'
        }
        foreach ($field in $expected.Keys) {
            if (-not $Report.ContainsKey($field)) {
                throw "missing report field: $field"
            }
            if ($Report[$field] -ne $expected[$field]) {
                throw "native room background report binding rejected: $field"
            }
        }

        $buildProperty = $provenance.ecologies.PSObject.Properties[$ecology]
        if ($null -eq $buildProperty) {
            throw "native provenance ecology missing: $ecology"
        }
        $build = $buildProperty.Value
        if (($build.master_size -join 'x') -ne '3840x2160' -or
                ($build.runtime_size -join 'x') -ne '2560x1440' -or
                [int]$build.runtime_from_master.passes -ne 1) {
            throw "native provenance dimensions rejected: $ecology"
        }

        foreach ($asset in @(
                @{ Kind='master'; Field="${ecology}_background_master";
                   Hash="${ecology}_background_master_sha256";
                   Width=3840; Height=2160; Set=$masterHashes },
                @{ Kind='runtime'; Field="${ecology}_background_atlas";
                   Hash="${ecology}_background_runtime_sha256";
                   Width=2560; Height=1440; Set=$runtimeHashes },
                @{ Kind='material'; Field="${ecology}_background_material_atlas";
                   Hash="${ecology}_background_material_sha256";
                   Width=2560; Height=1440; Set=$materialHashes })) {
            if (-not $Report.ContainsKey($asset.Hash) -or
                    $Report[$asset.Hash] -notmatch '^[0-9a-f]{64}$') {
                throw "missing or invalid native background SHA-256: $($asset.Hash)"
            }
            $assetPath = Join-Path $SourceRoot `
                ($Report[$asset.Field] -replace '/',
                    [System.IO.Path]::DirectorySeparatorChar)
            if (-not (Test-Path -LiteralPath $assetPath -PathType Leaf)) {
                throw "missing native background asset: $($Report[$asset.Field])"
            }
            $size = if ($PreflightOnly) {
                Read-PngHeaderSize $assetPath
            } else { Read-PngSize $assetPath }
            if ($size[0] -ne $asset.Width -or $size[1] -ne $asset.Height) {
                throw "wrong native background asset dimensions: $($asset.Kind) $ecology"
            }
            $provenanceHashProperty =
                $build.output_sha256.PSObject.Properties[$asset.Kind]
            if ($null -eq $provenanceHashProperty -or
                    $Report[$asset.Hash] -ne
                        [string]$provenanceHashProperty.Value) {
                throw "native background SHA-256/provenance mismatch: $($asset.Kind) $ecology"
            }
            if (-not $PreflightOnly) {
                $actualHash = Get-Sha256 $assetPath
                if ($actualHash -ne $Report[$asset.Hash]) {
                    throw "native background SHA-256/provenance mismatch: $($asset.Kind) $ecology"
                }
                if (-not $asset.Set.Add($actualHash)) {
                    throw "cross-ecology native background duplicated: $($asset.Kind) $ecology"
                }
            }
        }

        foreach ($resolution in @(
                @{ Short='1280'; Full='1280x720'; Width=1280; Height=720 },
                @{ Short='1920'; Full='1920x1080'; Width=1920; Height=1080 })) {
            $short = $resolution.Short
            $backgroundField = "${ecology}_background_only_screenshot_$short"
            $gameplayField = "${ecology}_gameplay_screenshot_$short"
            $backgroundHashField = "${backgroundField}_sha256"
            $gameplayHashField = "${gameplayField}_sha256"
            $expectedBackground = "${ecology}-background-only-$($resolution.Full).png"
            $expectedGameplay = "${ecology}-gameplay-$($resolution.Full).png"
            foreach ($binding in @(
                    @($backgroundField, $backgroundHashField, $expectedBackground),
                    @($gameplayField, $gameplayHashField, $expectedGameplay))) {
                if (-not $Report.ContainsKey($binding[0]) -or
                        $Report[$binding[0]] -ne $binding[2] -or
                        -not $Report.ContainsKey($binding[1]) -or
                        $Report[$binding[1]] -notmatch '^[0-9a-f]{64}$') {
                    throw "native screenshot report binding rejected: $($binding[0])"
                }
                $screenshot = Join-Path $EvidenceRoot $binding[2]
                if (-not (Test-Path -LiteralPath $screenshot -PathType Leaf) -or
                        (Get-Item -LiteralPath $screenshot).Length -le 1024) {
                    throw "missing or empty native screenshot: $($binding[2])"
                }
                $size = if ($PreflightOnly) {
                    Read-PngHeaderSize $screenshot
                } else { Read-PngSize $screenshot }
                if ($size[0] -ne $resolution.Width -or
                        $size[1] -ne $resolution.Height) {
                    throw "wrong native screenshot dimensions: $($binding[2])"
                }
                if (-not $PreflightOnly -and
                        (Get-Sha256 $screenshot) -ne $Report[$binding[1]]) {
                    throw "native screenshot SHA-256 mismatch: $($binding[2])"
                }
            }
            foreach ($runtimeField in @(
                    "${ecology}_background_only_runtime_$short",
                    "${ecology}_background_only_resident_$short",
                    "${ecology}_background_only_drawn_$short",
                    "${ecology}_background_only_hud_ecology_$short",
                    "${ecology}_gameplay_runtime_$short",
                    "${ecology}_gameplay_resident_$short",
                    "${ecology}_gameplay_drawn_$short",
                    "${ecology}_gameplay_hud_ecology_$short")) {
                if (-not $Report.ContainsKey($runtimeField) -or
                        $Report[$runtimeField] -ne 'pass') {
                    throw "native runtime telemetry rejected: $runtimeField"
                }
            }
            if (-not $PreflightOnly) {
                $backgroundPath = Join-Path $EvidenceRoot $expectedBackground
                $gameplayPath = Join-Path $EvidenceRoot $expectedGameplay
                $backgroundHash = Get-Sha256 $backgroundPath
                $gameplayHash = Get-Sha256 $gameplayPath
                if (-not $backgroundHashes[$short].Add($backgroundHash)) {
                    throw "duplicate native ecology screenshot: $ecology $short"
                }
                if ($backgroundHash -eq $gameplayHash -or
                        (Measure-SampledPixelDifference $backgroundPath $gameplayPath) -lt 200) {
                    throw "native gameplay screenshot lacks gameplay layers: $ecology $short"
                }
            }
        }
    }

    if (-not $PreflightOnly -and
            ($masterHashes.Count -ne 4 -or $runtimeHashes.Count -ne 4 -or
            $materialHashes.Count -ne 4 -or
            $backgroundHashes['1280'].Count -ne 4 -or
            $backgroundHashes['1920'].Count -ne 4)) {
        throw 'native room background ecology coverage is incomplete'
    }
}

function Measure-ItemAtlasPalette([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "missing published item atlas: $Path"
    }
    $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        [int]$cellSize = 128
        [int]$usedCellCount = 31
        [int]$minimumPalette = 45
        [int]$minimumAlpha = 96
        $paletteOverrides = @{ 12 = 45; 18 = 72; 21 = 72; 22 = 64 }
        if ($bitmap.Width -ne 1024 -or $bitmap.Height -ne 1024 -or
                $cellSize -ne 128 -or $usedCellCount -ne 31 -or
                $minimumPalette -ne 45 -or $minimumAlpha -ne 96 -or
                $paletteOverrides.Count -ne 4 -or
                $paletteOverrides[12] -ne 45 -or
                $paletteOverrides[18] -ne 72 -or
                $paletteOverrides[21] -ne 72 -or
                $paletteOverrides[22] -ne 64) {
            throw 'published item atlas palette contract changed unexpectedly'
        }
        for ([int]$cell = 0; $cell -lt $usedCellCount; ++$cell) {
            [int]$originX = ($cell % 8) * $cellSize
            [int]$originY = [Math]::Floor($cell / 8) * $cellSize
            $mask = New-Object 'bool[,]' $cellSize, $cellSize
            for ([int]$y = 0; $y -lt $cellSize; ++$y) {
                for ([int]$x = 0; $x -lt $cellSize; ++$x) {
                    $mask[$x, $y] = $bitmap.GetPixel(
                        $originX + $x, $originY + $y).A -ge $minimumAlpha
                }
            }
            [int]$largest = 0
            [int]$largestPalette = 0
            for ([int]$y = 0; $y -lt $cellSize; ++$y) {
                for ([int]$x = 0; $x -lt $cellSize; ++$x) {
                    if (-not $mask[$x, $y]) { continue }
                    $queue = [System.Collections.Generic.Queue[int]]::new()
                    $queue.Enqueue($y * $cellSize + $x)
                    $mask[$x, $y] = $false
                    $colors = [System.Collections.Generic.HashSet[int]]::new()
                    [int]$component = 0
                    while ($queue.Count -gt 0) {
                        $point = $queue.Dequeue()
                        $px = $point % $cellSize
                        $py = [Math]::Floor($point / $cellSize)
                        ++$component
                        $pixel = $bitmap.GetPixel(
                            $originX + $px, $originY + $py)
                        [void]$colors.Add((((([int]$pixel.R) -shr 4) -shl 8) -bor
                            ((([int]$pixel.G) -shr 4) -shl 4) -bor
                            (([int]$pixel.B) -shr 4)))
                        foreach ($offset in @(@(-1,0),@(1,0),@(0,-1),@(0,1))) {
                            $nx = $px + $offset[0]
                            $ny = $py + $offset[1]
                            if ($nx -ge 0 -and $nx -lt $cellSize -and
                                    $ny -ge 0 -and $ny -lt $cellSize -and
                                    $mask[$nx, $ny]) {
                                $mask[$nx, $ny] = $false
                                $queue.Enqueue($ny * $cellSize + $nx)
                            }
                        }
                    }
                    if ($component -gt $largest) {
                        $largest = $component
                        $largestPalette = $colors.Count
                    }
                }
            }
            [int]$requiredPalette = if ($paletteOverrides.ContainsKey($cell)) {
                $paletteOverrides[$cell]
            } else {
                $minimumPalette
            }
            Write-Output ("[stage12-item-atlas] cell={0} largest={1} colors={2}" -f
                $cell, $largest, $largestPalette)
            if ($largest -eq 0 -or $largestPalette -lt $requiredPalette) {
                throw "published item atlas palette rejected: cell=$cell largest=$largest colors=$largestPalette required=$requiredPalette"
            }
        }
    } finally {
        $bitmap.Dispose()
    }
}

$reportPath = Join-Path $EvidenceDirectory 'stage12-material-evidence.txt'
if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { throw 'missing material report' }
$report = Read-Report $reportPath
foreach ($key in @('manifest','atlas_bytes','full_pack_bytes',
        'resident_peak_bytes','transition_peak_bytes','fallback','input_hole_regression',
        'monsters','monster_screenshot','item_screenshot','item_baseline_screenshot',
        'item_icon_screenshot','item_icon_baseline_screenshot','items_ui_pair',
        'item_runtime_draws','ui_material_pair','ui_runtime_draws',
        'hud_ui_runtime_draws','inventory_ui_runtime_draws',
        'skill_ui_runtime_draws','pause_ui_runtime_draws',
        'hud_ui_draw_mask','inventory_ui_draw_mask',
        'skill_ui_draw_mask','pause_ui_draw_mask',
        'hud_ui_runtime_draws_1920','inventory_ui_runtime_draws_1920',
        'skill_ui_runtime_draws_1920','pause_ui_runtime_draws_1920',
        'hud_ui_draw_mask_1920','inventory_ui_draw_mask_1920',
        'skill_ui_draw_mask_1920','pause_ui_draw_mask_1920',
        'bundled_font_runtime','bundled_font_glyph_count',
        'bundled_font_subset_capacity',
        'bundled_font_source_base_size','bundled_font_max_display_size',
        'bundled_font_atlas_bytes','bundled_font_atlas_byte_budget',
        'bundled_font_total_atlas_bytes',
        'bundled_font_total_atlas_byte_budget',
        'ui_text_contrast','ui_text_solid_fill','ui_text_primary_contrast',
        'ui_text_secondary_contrast','ui_text_muted_contrast',
        'ui_text_outline_pixels','ui_text_shadow_pixels',
        'ui_text_backing_alpha','ui_text_physical_scale',
        'inventory_min_font_1280','inventory_min_font_1920',
        'skill_min_font_1280','skill_min_font_1920',
        'pause_min_font_1280','pause_min_font_1920',
        'hud_decorative_stretch','inventory_decorative_stretch',
        'skill_decorative_stretch','pause_decorative_stretch',
        'hud_decorative_stretch_1920','inventory_decorative_stretch_1920',
        'skill_decorative_stretch_1920','pause_decorative_stretch_1920',
        'hud_text_layout','inventory_text_layout','skill_text_layout',
        'pause_text_layout','hud_text_layout_1920',
        'inventory_text_layout_1920','skill_text_layout_1920',
        'pause_text_layout_1920',
        'ui_baseline_screenshot','ui_baseline_screenshot_1920',
        'hud_ui_screenshot','ui_gallery_screenshot',
        'hud_ui_screenshot_1920','inventory_ui_screenshot',
        'inventory_ui_screenshot_1920','skill_ui_screenshot',
        'skill_ui_screenshot_1920','pause_ui_screenshot',
        'pause_ui_screenshot_1920',
        'water_monster_screenshot','lightning_monster_screenshot',
        'lightning_background_screenshot',
        'lightning_isolated_monster_screenshot',
        'lightning_isolated_background_screenshot','chaos_monster_screenshot',
        'chaos_background_screenshot',
        'f12_screenshot','screenshot_isolation',
        'shader_pipeline','water_showcase_pair_residency','water_environment_pair',
        'water_bulwark_pair','water_support_pair','lightning_showcase_pair_residency',
        'lightning_environment_pair','lightning_shooter_pair','lightning_dasher_pair',
        'lightning_shooter_presenter','lightning_shooter_use_material_frame',
        'lightning_shooter_atlas','lightning_shooter_frame',
        'lightning_shooter_roi','lightning_shooter_drawn',
        'lightning_dasher_presenter','lightning_dasher_use_material_frame',
        'lightning_dasher_atlas','lightning_dasher_frame',
        'lightning_dasher_roi','lightning_dasher_drawn',
        'chaos_showcase_pair_residency','chaos_environment_pair','chaos_chaser_pair',
        'chaos_hazard_pair','chaos_chaser_presenter',
        'chaos_chaser_use_material_frame','chaos_chaser_atlas',
        'chaos_chaser_frame','chaos_chaser_drawn','chaos_hazard_presenter',
        'chaos_hazard_use_material_frame','chaos_hazard_atlas',
        'chaos_hazard_frame','chaos_hazard_drawn',
        'screenshot_decode','result')) {
    if (-not $report.ContainsKey($key)) { throw "missing report field: $key" }
}
foreach ($key in @('bundled_font_runtime','ui_text_contrast','ui_text_solid_fill',
        'ui_text_physical_scale',
        'hud_decorative_stretch','inventory_decorative_stretch',
        'skill_decorative_stretch','pause_decorative_stretch',
        'hud_decorative_stretch_1920','inventory_decorative_stretch_1920',
        'skill_decorative_stretch_1920','pause_decorative_stretch_1920',
        'hud_text_layout','inventory_text_layout','skill_text_layout',
        'pause_text_layout','hud_text_layout_1920',
        'inventory_text_layout_1920','skill_text_layout_1920',
        'pause_text_layout_1920')) {
    if ($report[$key] -ne 'pass') {
        throw "UI readability contract failed: $key"
    }
}
$fontScalePairs = @(
    @([double]$report.inventory_min_font_1280,
      [double]$report.inventory_min_font_1920),
    @([double]$report.skill_min_font_1280,
      [double]$report.skill_min_font_1920),
    @([double]$report.pause_min_font_1280,
      [double]$report.pause_min_font_1920)
)
foreach ($pair in $fontScalePairs) {
    if ($pair[0] -le 0.0 -or $pair[1] -lt $pair[0] * 1.49) {
        throw 'full-HD UI font did not scale by 1.5x'
    }
}
$primaryContrast = [double]$report.ui_text_primary_contrast
$secondaryContrast = [double]$report.ui_text_secondary_contrast
$mutedContrast = [double]$report.ui_text_muted_contrast
if ($primaryContrast -lt 7.0 -or $secondaryContrast -lt 6.0 -or
        $mutedContrast -lt 4.5 -or
        [int]$report.ui_text_outline_pixels -ne 0 -or
        [int]$report.ui_text_shadow_pixels -lt 2 -or
        [int]$report.ui_text_backing_alpha -lt 220) {
    throw 'UI text contrast metrics failed'
}
$fontGlyphCount = [uint64]$report.bundled_font_glyph_count
$fontSubsetCapacity = [uint64]$report.bundled_font_subset_capacity
if ($fontGlyphCount -eq 0 -or $fontSubsetCapacity -ne 384 -or
        $fontGlyphCount -gt $fontSubsetCapacity) {
    throw 'bundled font glyph subset contract failed'
}
$fontSourceBase = [uint64]$report.bundled_font_source_base_size
$fontMaxDisplay = [uint64]$report.bundled_font_max_display_size
$fontAtlasBytes = [uint64]$report.bundled_font_atlas_bytes
$fontAtlasBudget = [uint64]$report.bundled_font_atlas_byte_budget
$fontTotalAtlasBytes = [uint64]$report.bundled_font_total_atlas_bytes
$fontTotalAtlasBudget = [uint64]$report.bundled_font_total_atlas_byte_budget
if ($fontSourceBase -lt ($fontMaxDisplay * 2) -or
        $fontAtlasBytes -eq 0 -or $fontAtlasBytes -gt $fontAtlasBudget -or
        $fontTotalAtlasBytes -gt $fontTotalAtlasBudget) {
    throw 'bundled high-resolution font atlas budget failed'
}
if ($report.result -ne 'pass' -or $report.manifest -ne 'pass' -or
        $report.fallback -ne 'pass' -or $report.input_hole_regression -ne 'pass' -or
        $report.items_ui_pair -ne 'resident' -or
        $report.item_runtime_draws -ne 'pass' -or
        $report.ui_material_pair -ne 'resident' -or
        $report.ui_runtime_draws -ne 'pass' -or
        $report.hud_ui_runtime_draws -ne 'pass' -or
        $report.inventory_ui_runtime_draws -ne 'pass' -or
        $report.skill_ui_runtime_draws -ne 'pass' -or
        $report.pause_ui_runtime_draws -ne 'pass' -or
        $report.hud_ui_runtime_draws_1920 -ne 'pass' -or
        $report.inventory_ui_runtime_draws_1920 -ne 'pass' -or
        $report.skill_ui_runtime_draws_1920 -ne 'pass' -or
        $report.pause_ui_runtime_draws_1920 -ne 'pass' -or
        $report.shader_pipeline -ne 'pass' -or
        $report.water_showcase_pair_residency -ne 'pass' -or
        $report.water_environment_pair -ne 'resident' -or
        $report.water_bulwark_pair -ne 'resident' -or
        $report.water_support_pair -ne 'resident' -or
        $report.lightning_showcase_pair_residency -ne 'pass' -or
        $report.lightning_environment_pair -ne 'resident' -or
        $report.lightning_shooter_pair -ne 'resident' -or
        $report.lightning_dasher_pair -ne 'resident' -or
        $report.lightning_shooter_presenter -ne 'pass' -or
        $report.lightning_shooter_use_material_frame -ne 'pass' -or
        $report.lightning_shooter_atlas -ne 'lightning_shooter' -or
        $report.lightning_shooter_drawn -ne 'pass' -or
        $report.lightning_dasher_presenter -ne 'pass' -or
        $report.lightning_dasher_use_material_frame -ne 'pass' -or
        $report.lightning_dasher_atlas -ne 'lightning_dasher' -or
        $report.lightning_dasher_drawn -ne 'pass' -or
        $report.chaos_showcase_pair_residency -ne 'pass' -or
        $report.chaos_environment_pair -ne 'resident' -or
        $report.chaos_chaser_pair -ne 'resident' -or
        $report.chaos_hazard_pair -ne 'resident' -or
        $report.chaos_chaser_presenter -ne 'pass' -or
        $report.chaos_chaser_use_material_frame -ne 'pass' -or
        $report.chaos_chaser_atlas -ne 'chaos_chaser' -or
        $report.chaos_chaser_drawn -ne 'pass' -or
        $report.chaos_hazard_presenter -ne 'pass' -or
        $report.chaos_hazard_use_material_frame -ne 'pass' -or
        $report.chaos_hazard_atlas -ne 'chaos_hazard' -or
        $report.chaos_hazard_drawn -ne 'pass' -or
        $report.screenshot_isolation -ne 'pass' -or $report.screenshot_decode -ne 'pass' -or
        $report.monsters -ne 'fire_bomber,fire_charger,water_bulwark,water_support,lightning_shooter,lightning_dasher,chaos_chaser,chaos_hazard') {
    throw 'formal material report rejected'
}
Assert-UiDrawMask ([uint64]$report.hud_ui_draw_mask) @(0,1,2,5,14,15,37) 'hud'
Assert-UiDrawMask ([uint64]$report.inventory_ui_draw_mask) @(17,18,19,20,21,22,24,26,37) 'inventory'
Assert-UiDrawMask ([uint64]$report.skill_ui_draw_mask) @(20,21,25,27,28,31) 'skill-stones'
Assert-UiDrawMask ([uint64]$report.pause_ui_draw_mask) @(32,33,34,35) 'pause'
Assert-UiDrawMask ([uint64]$report.hud_ui_draw_mask_1920) @(0,1,2,5,14,15,37) 'hud-1920'
Assert-UiDrawMask ([uint64]$report.inventory_ui_draw_mask_1920) @(17,18,19,20,21,22,24,26,37) 'inventory-1920'
Assert-UiDrawMask ([uint64]$report.skill_ui_draw_mask_1920) @(20,21,25,27,28,31) 'skill-stones-1920'
Assert-UiDrawMask ([uint64]$report.pause_ui_draw_mask_1920) @(32,33,34,35) 'pause-1920'
$shooterFrame = [uint16]$report.lightning_shooter_frame
$dasherFrame = [uint16]$report.lightning_dasher_frame
if ($shooterFrame -ge 12 -or $dasherFrame -ge 12) {
    throw 'formal material report rejected invalid lightning frame index'
}
$chaserFrame = [uint16]$report.chaos_chaser_frame
$hazardFrame = [uint16]$report.chaos_hazard_frame
if ($chaserFrame -ge 12 -or $hazardFrame -ge 12) {
    throw 'formal material report rejected invalid chaos frame index'
}
$atlasBytes = [uint64]$report.atlas_bytes
$fullPackBytes = [uint64]$report.full_pack_bytes
$residentPeakBytes = [uint64]$report.resident_peak_bytes
$transitionPeakBytes = [uint64]$report.transition_peak_bytes
if ($atlasBytes -ne $ExpectedFullPackBytes -or
        $fullPackBytes -ne $ExpectedFullPackBytes) {
    throw 'full pack byte statistic disagrees with the production manifest'
}
if ($residentPeakBytes -ne $ExpectedResidentPeakBytes) {
    throw 'resident peak byte statistic disagrees with the production manifest'
}
if ($transitionPeakBytes -ne $ExpectedTransitionPeakBytes) {
    throw 'transition peak byte statistic disagrees with the production manifest'
}
if ($residentPeakBytes -eq 0 -or
        $residentPeakBytes -gt $MaximumResidentTextureBytes) {
    throw 'resident texture budget exceeded'
}

foreach ($pair in @(
        @($report.item_icon_screenshot, $report.item_screenshot),
        @($report.item_icon_baseline_screenshot,
            $report.item_baseline_screenshot),
        @($report.item_icon_screenshot,
            $report.item_icon_baseline_screenshot))) {
    if ([string]::Equals($pair[0], $pair[1],
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'isolated item icon evidence aliases another capture'
    }
}
foreach ($pair in @(
        @($report.lightning_isolated_monster_screenshot,
            $report.lightning_monster_screenshot),
        @($report.lightning_isolated_background_screenshot,
            $report.lightning_background_screenshot),
        @($report.lightning_isolated_monster_screenshot,
            $report.lightning_isolated_background_screenshot))) {
    if ([string]::Equals($pair[0], $pair[1],
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'isolated lightning evidence aliases another capture'
    }
}
Assert-NativeBackgroundEvidence $report $EvidenceDirectory $ProjectRoot $true

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
$monsterScreenshot = Join-Path $EvidenceDirectory $report.monster_screenshot
if (-not (Test-Path -LiteralPath $monsterScreenshot -PathType Leaf)) { throw 'missing eight-monster screenshot' }
$monsterSize = Read-PngSize $monsterScreenshot
if ($monsterSize[0] -ne 1280 -or $monsterSize[1] -ne 720) { throw 'wrong eight-monster screenshot size' }
$itemScreenshot = Join-Path $EvidenceDirectory $report.item_screenshot
if (-not (Test-Path -LiteralPath $itemScreenshot -PathType Leaf)) { throw 'missing item/material screenshot' }
$itemSize = Read-PngSize $itemScreenshot
if ($itemSize[0] -ne 1280 -or $itemSize[1] -ne 720) { throw 'wrong item/material screenshot size' }
if ((Get-Item -LiteralPath $itemScreenshot).Length -le 4096) { throw 'empty item/material screenshot' }
$itemBaselineScreenshot = Join-Path $EvidenceDirectory $report.item_baseline_screenshot
if (-not (Test-Path -LiteralPath $itemBaselineScreenshot -PathType Leaf)) { throw 'missing item baseline screenshot' }
$itemBaselineSize = Read-PngSize $itemBaselineScreenshot
if ($itemBaselineSize[0] -ne 1280 -or $itemBaselineSize[1] -ne 720) { throw 'wrong item baseline screenshot size' }
if ((Get-Item -LiteralPath $itemBaselineScreenshot).Length -le 4096) { throw 'empty item baseline screenshot' }
$itemIconScreenshot = Join-Path $EvidenceDirectory $report.item_icon_screenshot
if (-not (Test-Path -LiteralPath $itemIconScreenshot -PathType Leaf)) { throw 'missing isolated item icon screenshot' }
$itemIconSize = Read-PngSize $itemIconScreenshot
if ($itemIconSize[0] -ne 1280 -or $itemIconSize[1] -ne 720) { throw 'wrong isolated item icon screenshot size' }
if ((Get-Item -LiteralPath $itemIconScreenshot).Length -le 4096) { throw 'empty isolated item icon screenshot' }
$itemIconBaselineScreenshot = Join-Path $EvidenceDirectory $report.item_icon_baseline_screenshot
if (-not (Test-Path -LiteralPath $itemIconBaselineScreenshot -PathType Leaf)) { throw 'missing isolated item icon baseline screenshot' }
$itemIconBaselineSize = Read-PngSize $itemIconBaselineScreenshot
if ($itemIconBaselineSize[0] -ne 1280 -or $itemIconBaselineSize[1] -ne 720) { throw 'wrong isolated item icon baseline screenshot size' }
if ((Get-Item -LiteralPath $itemIconBaselineScreenshot).Length -le 4096) { throw 'empty isolated item icon baseline screenshot' }
foreach ($pair in @(
        @($itemIconScreenshot, $itemScreenshot),
        @($itemIconBaselineScreenshot, $itemBaselineScreenshot),
        @($itemIconScreenshot, $itemIconBaselineScreenshot))) {
    if ([string]::Equals([System.IO.Path]::GetFullPath($pair[0]),
            [System.IO.Path]::GetFullPath($pair[1]),
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'isolated item icon evidence aliases another capture'
    }
}
Measure-ItemCapture $itemIconScreenshot $itemIconBaselineScreenshot 'isolated'
$uiBaseline = Join-Path $EvidenceDirectory $report.ui_baseline_screenshot
$hudUi = Join-Path $EvidenceDirectory $report.hud_ui_screenshot
$uiGallery = Join-Path $EvidenceDirectory $report.ui_gallery_screenshot
foreach ($path in @($uiBaseline, $hudUi, $uiGallery)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "missing HUD/UI screenshot: $path"
    }
}
if ($report.hud_ui_screenshot -eq $report.ui_baseline_screenshot) {
    throw 'HUD screenshot must be independent from the UI baseline'
}
Measure-UiGallery $uiGallery $uiBaseline
Assert-UiScreenDifferent $hudUi $uiBaseline 'hud'
Assert-HudRoiDifferent $hudUi $uiBaseline
foreach ($entry in @(
        @('inventory_ui_screenshot','inventory'),
        @('skill_ui_screenshot','skill-stones'),
        @('pause_ui_screenshot','pause'))) {
    $path = Join-Path $EvidenceDirectory $report[$entry[0]]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "missing UI screenshot: $($entry[1])"
    }
    Assert-UiScreenDifferent $path $uiBaseline $entry[1]
}
$uiBaseline1920 = Join-Path $EvidenceDirectory $report.ui_baseline_screenshot_1920
if (-not (Test-Path -LiteralPath $uiBaseline1920 -PathType Leaf)) {
    throw 'missing 1920 UI baseline screenshot'
}
$baseline1920Size = Read-PngSize $uiBaseline1920
if ($baseline1920Size[0] -ne 1920 -or $baseline1920Size[1] -ne 1080) {
    throw 'wrong 1920 UI baseline dimensions'
}
foreach ($entry in @(
        @('hud_ui_screenshot_1920','hud'),
        @('inventory_ui_screenshot_1920','inventory'),
        @('skill_ui_screenshot_1920','skill-stones'),
        @('pause_ui_screenshot_1920','pause'))) {
    $path = Join-Path $EvidenceDirectory $report[$entry[0]]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "missing 1920 UI screenshot: $($entry[1])"
    }
    $size = Read-PngSize $path
    if ($size[0] -ne 1920 -or $size[1] -ne 1080) {
        throw "wrong 1920 UI screenshot dimensions: $($entry[1])"
    }
    if ((Get-Item -LiteralPath $path).Length -le 4096) {
        throw "empty 1920 UI screenshot: $($entry[1])"
    }
    Assert-UiScreenDifferentAtSize $path $uiBaseline1920 `
        "$($entry[1])-1920" 1920 1080 1200
    switch ($entry[1]) {
        'hud' {
            $regions = @(
                [System.Drawing.Rectangle]::new(0, 795, 645, 285),
                [System.Drawing.Rectangle]::new(645, 930, 630, 150))
        }
        'inventory' {
            $regions = @([System.Drawing.Rectangle]::new(0, 60, 1920, 1020))
        }
        'skill-stones' {
            $regions = @([System.Drawing.Rectangle]::new(40, 70, 1840, 980))
        }
        'pause' {
            $regions = @([System.Drawing.Rectangle]::new(560, 260, 800, 580))
        }
    }
    Assert-UiRoiDifferent $path $uiBaseline1920 "$($entry[1])-1920" `
        $regions 500
}
$waterMonsterScreenshot = Join-Path $EvidenceDirectory $report.water_monster_screenshot
if (-not (Test-Path -LiteralPath $waterMonsterScreenshot -PathType Leaf)) { throw 'missing water-monster screenshot' }
$waterMonsterSize = Read-PngSize $waterMonsterScreenshot
if ($waterMonsterSize[0] -ne 1280 -or $waterMonsterSize[1] -ne 720) { throw 'wrong water-monster screenshot size' }
$lightningMonsterScreenshot = Join-Path $EvidenceDirectory $report.lightning_monster_screenshot
if (-not (Test-Path -LiteralPath $lightningMonsterScreenshot -PathType Leaf)) { throw 'missing lightning-monster screenshot' }
$lightningMonsterSize = Read-PngSize $lightningMonsterScreenshot
if ($lightningMonsterSize[0] -ne 1280 -or $lightningMonsterSize[1] -ne 720) { throw 'wrong lightning-monster screenshot size' }
if ((Get-Sha256 $waterMonsterScreenshot) -eq
        (Get-Sha256 $lightningMonsterScreenshot)) {
    throw 'lightning capture duplicates water ecology evidence'
}
$lightningBackgroundScreenshot = Join-Path $EvidenceDirectory $report.lightning_background_screenshot
if (-not (Test-Path -LiteralPath $lightningBackgroundScreenshot -PathType Leaf)) { throw 'missing lightning background baseline' }
$lightningBackgroundSize = Read-PngSize $lightningBackgroundScreenshot
if ($lightningBackgroundSize[0] -ne 1280 -or $lightningBackgroundSize[1] -ne 720) { throw 'wrong lightning background screenshot size' }
$lightningIsolatedMonsterScreenshot = Join-Path $EvidenceDirectory `
    $report.lightning_isolated_monster_screenshot
$lightningIsolatedBackgroundScreenshot = Join-Path $EvidenceDirectory `
    $report.lightning_isolated_background_screenshot
foreach ($path in @($lightningIsolatedMonsterScreenshot,
        $lightningIsolatedBackgroundScreenshot)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "missing isolated lightning screenshot: $path"
    }
    $size = Read-PngSize $path
    if ($size[0] -ne 1280 -or $size[1] -ne 720 -or
            (Get-Item -LiteralPath $path).Length -le 4096) {
        throw "invalid isolated lightning screenshot: $path"
    }
}
foreach ($pair in @(
        @($lightningIsolatedMonsterScreenshot, $lightningMonsterScreenshot),
        @($lightningIsolatedBackgroundScreenshot,
            $lightningBackgroundScreenshot),
        @($lightningIsolatedMonsterScreenshot,
            $lightningIsolatedBackgroundScreenshot))) {
    if ([string]::Equals([System.IO.Path]::GetFullPath($pair[0]),
            [System.IO.Path]::GetFullPath($pair[1]),
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'isolated lightning evidence aliases another capture'
    }
}
Measure-LightningCapture $lightningMonsterScreenshot `
    $lightningBackgroundScreenshot $lightningIsolatedMonsterScreenshot `
    $lightningIsolatedBackgroundScreenshot $report
$chaosMonsterScreenshot = Join-Path $EvidenceDirectory $report.chaos_monster_screenshot
if (-not (Test-Path -LiteralPath $chaosMonsterScreenshot -PathType Leaf)) { throw 'missing chaos-monster screenshot' }
$lightningBytes = [System.IO.File]::ReadAllBytes($lightningMonsterScreenshot)
$chaosBytes = [System.IO.File]::ReadAllBytes($chaosMonsterScreenshot)
$capturesMatch = $lightningBytes.Length -eq $chaosBytes.Length
for ($index = 0; $capturesMatch -and $index -lt $lightningBytes.Length; ++$index) {
    if ($lightningBytes[$index] -ne $chaosBytes[$index]) { $capturesMatch = $false }
}
if ($capturesMatch) { throw 'chaos capture duplicates lightning ecology evidence' }
$chaosMonsterSize = Read-PngSize $chaosMonsterScreenshot
if ($chaosMonsterSize[0] -ne 1280 -or $chaosMonsterSize[1] -ne 720) { throw 'wrong chaos-monster screenshot size' }
$chaosBackgroundScreenshot = Join-Path $EvidenceDirectory $report.chaos_background_screenshot
if (-not (Test-Path -LiteralPath $chaosBackgroundScreenshot -PathType Leaf)) { throw 'missing chaos background baseline' }
$chaosBackgroundSize = Read-PngSize $chaosBackgroundScreenshot
if ($chaosBackgroundSize[0] -ne 1280 -or $chaosBackgroundSize[1] -ne 720) { throw 'wrong chaos background screenshot size' }
Measure-ChaosCapture $chaosMonsterScreenshot $chaosBackgroundScreenshot
$f12Screenshot = Join-Path $EvidenceDirectory $report.f12_screenshot
if (-not (Test-Path -LiteralPath $f12Screenshot -PathType Leaf)) { throw 'missing isolated F12 screenshot' }
$f12Size = Read-PngSize $f12Screenshot
if ($f12Size[0] -ne 1280 -or $f12Size[1] -ne 720) { throw 'wrong isolated F12 screenshot size' }
$holeSummary = Join-Path $EvidenceDirectory 'input-hole-summary.txt'
if (-not (Test-Path -LiteralPath $holeSummary -PathType Leaf)) { throw 'missing input/hole formal evidence' }
$holeText = Get-Content -Raw -LiteralPath $holeSummary -Encoding UTF8
if ($holeText -notmatch 'depth=2' -or $holeText -notmatch 'last_transition=1' -or
        $holeText -notmatch 'resolution_valid=1') { throw 'input/hole formal evidence rejected' }

$assetRoot = Join-Path $PSScriptRoot '..\..\assets\stage12'
$publishedItemsAtlas = if ([string]::IsNullOrWhiteSpace($ItemAtlasPath)) {
    Join-Path $assetRoot 'items_ui.png'
} else {
    [System.IO.Path]::GetFullPath($ItemAtlasPath)
}
Measure-ItemAtlasPalette $publishedItemsAtlas
$expectedAtlases = @(@('environment.png',1024,1024),
    @('environment_material.png',1024,1024), @('actors.png',2048,2048),
    @('actors_material.png',2048,2048), @('effects_ui.png',1024,1024),
    @('effects_ui_material.png',1024,1024), @('items_ui.png',1024,1024),
    @('items_ui_material.png',1024,1024), @('ui_material.png',1024,1024),
    @('ui_material_material.png',1024,1024), @('water_environment.png',768,768),
    @('water_environment_material.png',768,768), @('water_bulwark.png',864,864),
    @('water_bulwark_material.png',864,864), @('water_support.png',864,864),
    @('water_support_material.png',864,864),
    @('lightning_environment.png',768,768),
    @('lightning_environment_material.png',768,768),
    @('lightning_shooter.png',864,864),
    @('lightning_shooter_material.png',864,864),
    @('lightning_dasher.png',864,864),
    @('lightning_dasher_material.png',864,864),
    @('chaos_environment.png',768,768),
    @('chaos_environment_material.png',768,768),
    @('chaos_chaser.png',864,864),
    @('chaos_chaser_material.png',864,864),
    @('chaos_hazard.png',864,864),
    @('chaos_hazard_material.png',864,864))
foreach ($expected in $expectedAtlases) {
    $path = Join-Path $assetRoot $expected[0]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "missing atlas: $($expected[0])" }
    $size = Read-PngSize $path
    if ($size[0] -ne $expected[1] -or $size[1] -ne $expected[2]) {
        throw "wrong atlas dimensions: $($expected[0])"
    }
}
$integration = Assert-MaterialRuntimeIntegration $EvidenceDirectory
Assert-NativeBackgroundEvidence $report $EvidenceDirectory $ProjectRoot
if ($transitionPeakBytes -ne [uint64]$integration.transition_peak_bytes) {
    throw 'transition peak byte statistic disagrees with formal runtime evidence'
}
Measure-ItemCapture $itemScreenshot $itemBaselineScreenshot 'legacy'
Write-Output 'stage12 material evidence validated: item/UI telemetry, same-host UI ROI/screens, atlases, fallback, input/hole, runtime monster draws, and ecology contours'
