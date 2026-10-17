param(
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDirectory,
    [Parameter(Mandatory = $true)]
    [string]$CommittedEvidenceDirectory
)

$ErrorActionPreference = 'Stop'
$scenarios = [ordered]@{
    'show-all' = 'show_all'
    'magic-plus' = 'magic_or_better'
    'rare-only' = 'rare_only'
    'rare-abyss' = 'rare_only_abyss'
    'preview-cancel' = 'preview_cancel'
    'pickup-feedback' = 'pickup_feedback'
}

function Read-Values([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "missing evidence: $Path" }
    $values = @{}
    Get-Content -LiteralPath $Path -Encoding UTF8 | ForEach-Object {
        $pair = $_ -split '=', 2
        if ($pair.Count -eq 2) { $values[$pair[0]] = $pair[1] }
    }
    return $values
}

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Csv([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) { return @() }
    return @($Value -split ',' | Where-Object { $_ -ne '' })
}

function Label-Rects([string]$Value, [string]$Scenario) {
    if ([string]::IsNullOrWhiteSpace($Value)) { return @() }
    $result = @()
    foreach ($entry in ($Value -split ';')) {
        $parts = @($entry -split ',')
        Require ($parts.Count -eq 5) "invalid label rect in $Scenario"
        $result += ,([pscustomobject]@{
            Ordinal = [int]$parts[0]
            X = [double]$parts[1]
            Y = [double]$parts[2]
            W = [double]$parts[3]
            H = [double]$parts[4]
        })
    }
    return $result
}

if (-not ('Stage11DLootFnv1a' -as [type])) {
    Add-Type -TypeDefinition @'
public static class Stage11DLootFnv1a {
    public static ulong Hash(byte[] bytes) {
        const ulong Offset = 1469598103934665603UL;
        const ulong Prime = 1099511628211UL;
        unchecked {
            ulong hash = Offset;
            foreach (byte value in bytes) { hash ^= value; hash *= Prime; }
            return hash;
        }
    }
}
'@
}

Add-Type -AssemblyName System.Drawing
function Measure-Region($Bitmap, [double]$X, [double]$Y,
        [double]$W, [double]$H) {
    $left = [Math]::Max(0, [int][Math]::Floor($X))
    $top = [Math]::Max(0, [int][Math]::Floor($Y))
    $right = [Math]::Min($Bitmap.Width, [int][Math]::Ceiling($X + $W))
    $bottom = [Math]::Min($Bitmap.Height, [int][Math]::Ceiling($Y + $H))
    Require ($right -gt $left -and $bottom -gt $top) 'empty screenshot region'
    $colors = [System.Collections.Generic.HashSet[int]]::new()
    $bright = 0
    $dark = 0
    $minimumLuma = 255
    $maximumLuma = 0
    for ($y = $top; $y -lt $bottom; $y += 2) {
        for ($x = $left; $x -lt $right; $x += 2) {
            $color = $Bitmap.GetPixel($x, $y)
            [void]$colors.Add($color.ToArgb())
            $luma = ([int]$color.R + [int]$color.G + [int]$color.B) / 3
            $minimumLuma = [Math]::Min($minimumLuma, $luma)
            $maximumLuma = [Math]::Max($maximumLuma, $luma)
            if ($luma -ge 105) { ++$bright }
            if ($luma -le 55) { ++$dark }
        }
    }
    return [pscustomobject]@{
        Colors = $colors.Count
        Bright = $bright
        Dark = $dark
        LumaSpan = $maximumLuma - $minimumLuma
    }
}

$manifestName = 'stage11d-loot-evidence.txt'
$manifestPath = Join-Path $EvidenceDirectory $manifestName
$committedManifest = Join-Path $CommittedEvidenceDirectory $manifestName
$manifest = Read-Values $manifestPath
Require ($manifest.result -eq 'pass') 'formal aggregate did not pass'
foreach ($key in @('ordinary_root','ordinary_room','ordinary_depth','abyss_root','abyss_room')) {
    Require ($manifest.ContainsKey($key) -and [uint64]$manifest[$key] -gt 0) "invalid manifest field: $key"
}
Require ((Get-Content -Raw -LiteralPath $manifestPath) -ceq
    (Get-Content -Raw -LiteralPath $committedManifest)) 'committed manifest differs'

$requiredSummary = @(
    'scenario','committed_mode','draft_mode','snapshot_count','visible_count',
    'inventory_count','normal_item_id','magic_item_id','rare_item_id',
    'abyss_item_id','abyss_rarity','preview_visible_count',
    'restored_visible_count','abyss_claimed','progress_phase',
    'progress_remaining','progress_inventory','progress_hp',
    'progress_rarities','max_ground_rarities','max_ground_count',
    'min_remaining','max_inventory','observed_normal','observed_magic',
    'observed_rare','monster_seen_bits','monster_defeated_bits',
    'monster_last_hp','monster_min_hp','defeat_distance_milli',
    'pickup_item_id','pickup_commit_generation','notice_text','snapshot_ids',
    'inventory_ids','label_rects','pickup_notice_rect','result'
)
$hashes = [System.Collections.Generic.HashSet[string]]::new()
$summaries = @{}
foreach ($name in $scenarios.Keys) {
    $image = Join-Path $EvidenceDirectory ($name + '.png')
    $summary = Join-Path $EvidenceDirectory ($name + '.txt')
    $committedImage = Join-Path $CommittedEvidenceDirectory ($name + '.png')
    $committedSummary = Join-Path $CommittedEvidenceDirectory ($name + '.txt')
    foreach ($path in @($image,$summary,$committedImage,$committedSummary)) {
        Require (Test-Path -LiteralPath $path) "missing evidence file: $path"
        Require ((Get-Item -LiteralPath $path).LastWriteTimeUtc -ge
            [DateTime]::UtcNow.AddMinutes(-20)) "stale evidence file: $path"
    }
    $bytes = [System.IO.File]::ReadAllBytes($image)
    $committedBytes = [System.IO.File]::ReadAllBytes($committedImage)
    Require ($bytes.Length -gt 1024) "empty PNG: $name"
    $signature = @(137,80,78,71,13,10,26,10)
    for ($index = 0; $index -lt $signature.Count; ++$index) {
        Require ($bytes[$index] -eq $signature[$index]) "invalid PNG: $name"
    }
    $width = [uint32]$bytes[16] * 16777216 + [uint32]$bytes[17] * 65536 +
        [uint32]$bytes[18] * 256 + [uint32]$bytes[19]
    $height = [uint32]$bytes[20] * 16777216 + [uint32]$bytes[21] * 65536 +
        [uint32]$bytes[22] * 256 + [uint32]$bytes[23]
    Require ($width -eq 1280 -and $height -eq 720) "wrong PNG dimensions: $name"
    [uint64]$hash = [Stage11DLootFnv1a]::Hash($bytes)
    [uint64]$committedHash = [Stage11DLootFnv1a]::Hash($committedBytes)
    Require ($hash -eq $committedHash) "committed PNG differs: $name"
    Require ($manifest[$name + '_hash'] -eq [string]$hash) "manifest hash mismatch: $name"
    Require ($hashes.Add([string]$hash)) "duplicate screenshot hash: $name"
    Require ((Get-Content -Raw -LiteralPath $summary) -ceq
        (Get-Content -Raw -LiteralPath $committedSummary)) "committed summary differs: $name"

    $values = Read-Values $summary
    foreach ($key in $requiredSummary) {
        Require ($values.ContainsKey($key)) "missing $name summary field: $key"
    }
    Require ($values.scenario -eq $scenarios[$name] -and
        $values.result -eq 'pass') "invalid scenario result: $name"
    $rects = @(Label-Rects $values.label_rects $name)
    Require ($rects.Count -eq [int]$values.visible_count) "label count mismatch: $name"
    $bitmap = [System.Drawing.Bitmap]::FromFile($image)
    try {
        foreach ($rect in $rects) {
            Require ($rect.W -gt 20 -and $rect.H -gt 10 -and $rect.X -ge 0 -and
                $rect.Y -ge 0 -and $rect.X + $rect.W -le 1280 -and
                $rect.Y + $rect.H -le 720) "invalid label bounds: $name"
            $feature = Measure-Region $bitmap $rect.X $rect.Y $rect.W $rect.H
            Require ($feature.Colors -ge 8 -and $feature.Bright -ge 8 -and
                $feature.LumaSpan -ge 40) "blank loot label region: $name"
        }
        if ($name -eq 'pickup-feedback') {
            $notice = @($values.pickup_notice_rect -split ',' | ForEach-Object { [double]$_ })
            Require ($notice.Count -eq 4 -and $notice[2] -gt 40 -and
                $notice[3] -gt 12) 'invalid pickup notice rect'
            $feature = Measure-Region $bitmap $notice[0] $notice[1] $notice[2] $notice[3]
            Require ($feature.Colors -ge 12 -and $feature.Bright -ge 20 -and
                $feature.Dark -ge 20) 'pickup notice region is blank'
        }
    } finally {
        $bitmap.Dispose()
    }
    $summaries[$name] = $values
}

$show = $summaries['show-all']
$magic = $summaries['magic-plus']
$rare = $summaries['rare-only']
$abyss = $summaries['rare-abyss']
$preview = $summaries['preview-cancel']
$pickup = $summaries['pickup-feedback']
$ordinaryIds = @(Csv $show.snapshot_ids)
Require ($ordinaryIds.Count -eq 3 -and
    ($ordinaryIds | Select-Object -Unique).Count -eq 3) 'show-all must contain three exact production items'
foreach ($id in @($show.normal_item_id,$show.magic_item_id,$show.rare_item_id)) {
    Require ([uint64]$id -gt 0 -and $ordinaryIds -contains $id) 'invalid ordinary rarity item id'
}
foreach ($name in @('show-all','magic-plus','rare-only','preview-cancel')) {
    $values = $summaries[$name]
    Require ((Compare-Object $ordinaryIds @(Csv $values.snapshot_ids)).Count -eq 0) "production item IDs drifted: $name"
    Require ([int]$values.snapshot_count -eq 3 -and
        [int]$values.inventory_count -eq 0) "filtered item left snapshot or entered inventory: $name"
    Require ($values.monster_seen_bits -eq '7' -and
        $values.monster_defeated_bits -eq '7' -and
        $values.progress_remaining -eq '0' -and
        [int]$values.progress_hp -gt 0) "ordinary combat did not finish legitimately: $name"
    $distances = @(Csv $values.defeat_distance_milli | ForEach-Object { [int]$_ })
    Require ($distances.Count -eq 3 -and
        ($distances | Measure-Object -Minimum).Minimum -gt 1500) "ordinary drop was not kept outside pickup radius: $name"
}
Require ($show.committed_mode -eq '0' -and
    $show.visible_count -eq '3') 'show-all presentation mismatch'
Require ($magic.committed_mode -eq '1' -and $magic.visible_count -eq '2' -and
    @(Csv $magic.inventory_ids).Count -eq 0) 'magic+ hidden-item semantics mismatch'
Require ($rare.committed_mode -eq '2' -and $rare.visible_count -eq '1' -and
    @(Csv $rare.inventory_ids).Count -eq 0) 'rare-only hidden-item semantics mismatch'
$magicOrdinals = @(Label-Rects $magic.label_rects 'magic-plus' |
    ForEach-Object { [string]$_.Ordinal })
$normalOrdinal = ($magic.observed_normal -split ',')[1]
$magicOrdinal = ($magic.observed_magic -split ',')[1]
$rareOrdinal = ($magic.observed_rare -split ',')[1]
Require ($magicOrdinals -contains $magicOrdinal -and
    $magicOrdinals -contains $rareOrdinal -and
    -not ($magicOrdinals -contains $normalOrdinal)) 'magic+ rendered wrong production ordinals'
$rareOrdinals = @(Label-Rects $rare.label_rects 'rare-only' |
    ForEach-Object { [string]$_.Ordinal })
Require ($rareOrdinals.Count -eq 1 -and
    $rareOrdinals[0] -eq (($rare.observed_rare -split ',')[1])) 'rare-only rendered wrong production ordinal'

Require ($preview.committed_mode -eq '0' -and $preview.draft_mode -eq '0' -and
    [int]$preview.preview_visible_count -lt [int]$preview.restored_visible_count -and
    $preview.restored_visible_count -eq '3' -and
    $preview.visible_count -eq '3') 'settings Cancel did not restore committed loot presentation'

$abyssIds = @(Csv $abyss.snapshot_ids)
Require ($abyss.committed_mode -eq '2' -and $abyss.snapshot_count -eq '1' -and
    $abyss.visible_count -eq '1' -and [uint64]$abyss.abyss_item_id -gt 0 -and
    $abyssIds -contains $abyss.abyss_item_id -and [int]$abyss.abyss_rarity -lt 2 -and
    $abyss.abyss_claimed -eq '1' -and
    $abyss.pickup_item_id -eq $abyss.abyss_item_id -and
    [uint64]$abyss.pickup_commit_generation -gt 0) 'rare-only abyss exception was not real and claimable'

$pickupInventory = @(Csv $pickup.inventory_ids)
$pickupSnapshot = @(Csv $pickup.snapshot_ids)
$pickupNoticePrefix = -join @([char]0x5DF2,[char]0x62FE,[char]0x53D6,[char]0xFF1A)
Require -Condition ($pickup.inventory_count -eq '1') -Message 'pickup inventory count mismatch'
Require -Condition ($pickupInventory.Count -eq 1) -Message 'pickup ownership list mismatch'
Require -Condition ($pickup.pickup_item_id -eq $pickupInventory[0]) -Message 'pickup receipt item is not owned'
Require -Condition ($ordinaryIds -contains $pickup.pickup_item_id) -Message 'pickup receipt is not an expected production item'
Require -Condition ($pickupSnapshot.Count -eq 2) -Message 'pickup did not remove exactly one ground item'
Require -Condition (-not ($pickupSnapshot -contains $pickup.pickup_item_id)) -Message 'picked item remained in the ground snapshot'
Require -Condition ([uint64]$pickup.pickup_commit_generation -gt 0) -Message 'pickup receipt lacks a committed generation'
Require -Condition ($pickup.notice_text -match
    ('^' + [regex]::Escape($pickupNoticePrefix))) -Message 'confirmed automatic pickup notice mismatch'

Write-Output 'stage11d loot formal evidence validated: six fresh unique production screenshots'
