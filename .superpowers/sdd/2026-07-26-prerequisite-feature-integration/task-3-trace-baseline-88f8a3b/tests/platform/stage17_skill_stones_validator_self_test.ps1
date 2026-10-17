param(
    [Parameter(Mandatory = $true)][string]$EvidenceDirectory,
    [Parameter(Mandatory = $true)][string]$Validator,
    [Parameter(Mandatory = $true)][string]$MutationRoot
)

$ErrorActionPreference = 'Stop'

function Test-PathWithin([string]$Child, [string]$Parent) {
    $prefix = $Parent.TrimEnd([System.IO.Path]::DirectorySeparatorChar) +
        [System.IO.Path]::DirectorySeparatorChar
    return $Child.StartsWith($prefix,
        [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-NotReparsePoint([System.IO.FileSystemInfo]$Item) {
    if (($Item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "reparse point is forbidden in controlled cleanup path: $($Item.FullName)"
    }
}

function Resolve-ControlledMutationRoot(
    [string]$EvidencePath, [string]$MutationPath) {
    $evidenceFull = [System.IO.Path]::GetFullPath($EvidencePath)
    if (-not (Test-Path -LiteralPath $evidenceFull -PathType Container)) {
        throw "missing evidence root: $evidenceFull"
    }
    $evidenceItem = Get-Item -LiteralPath $evidenceFull -Force
    Assert-NotReparsePoint $evidenceItem
    $evidenceReal = [System.IO.Path]::GetFullPath(
        (Resolve-Path -LiteralPath $evidenceFull).ProviderPath)
    $mutationFull = [System.IO.Path]::GetFullPath($MutationPath)
    if (-not (Test-PathWithin $mutationFull $evidenceReal)) {
        throw "mutation root escapes evidence root: $mutationFull"
    }

    $relative = $mutationFull.Substring(
        $evidenceReal.TrimEnd([System.IO.Path]::DirectorySeparatorChar).Length)
    $parts = $relative.TrimStart(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar).Split(
            [char[]]@([System.IO.Path]::DirectorySeparatorChar,
                [System.IO.Path]::AltDirectorySeparatorChar),
            [System.StringSplitOptions]::RemoveEmptyEntries)
    $cursor = $evidenceReal
    foreach ($part in $parts) {
        $cursor = Join-Path $cursor $part
        if (-not (Test-Path -LiteralPath $cursor)) { break }
        $item = Get-Item -LiteralPath $cursor -Force
        Assert-NotReparsePoint $item
        if (-not $item.PSIsContainer) {
            throw "controlled cleanup path component is not a directory: $cursor"
        }
    }

    if (Test-Path -LiteralPath $mutationFull) {
        $pending = [System.Collections.Generic.Stack[string]]::new()
        $pending.Push($mutationFull)
        while ($pending.Count -gt 0) {
            $directory = $pending.Pop()
            foreach ($child in Get-ChildItem -LiteralPath $directory -Force) {
                Assert-NotReparsePoint $child
                if ($child.PSIsContainer) { $pending.Push($child.FullName) }
            }
        }
        $mutationReal = [System.IO.Path]::GetFullPath(
            (Resolve-Path -LiteralPath $mutationFull).ProviderPath)
        if (-not (Test-PathWithin $mutationReal $evidenceReal)) {
            throw "resolved mutation root escapes evidence root: $mutationReal"
        }
    }
    return $mutationFull
}

$evidence = [System.IO.Path]::GetFullPath(
    (Resolve-Path -LiteralPath $EvidenceDirectory).ProviderPath)
$mutation = Resolve-ControlledMutationRoot $evidence $MutationRoot
& $Validator -EvidenceDirectory $evidence | Out-Null

if (Test-Path -LiteralPath $mutation) {
    Remove-Item -LiteralPath $mutation -Recurse -Force
}
New-Item -ItemType Directory -Path $mutation | Out-Null

function Assert-ValidatorRejects([string]$Name, [scriptblock]$Mutate) {
    $caseRoot = Join-Path $mutation $Name
    $caseRun = Join-Path $caseRoot 'stage17-run'
    New-Item -ItemType Directory -Path $caseRun -Force | Out-Null
    Get-ChildItem -LiteralPath $evidence | Where-Object {
        $_.FullName -ne $mutation
    } | Copy-Item -Destination $caseRun -Recurse
    & $Mutate $caseRun
    $rejected = $false
    try {
        & $Validator -EvidenceDirectory $caseRun 2>$null | Out-Null
    } catch {
        $rejected = $true
    }
    if (-not $rejected) { throw "validator accepted mutation: $Name" }
}

Assert-ValidatorRejects 'broken-png' {
    param($caseRun)
    [System.IO.File]::WriteAllBytes(
        (Join-Path $caseRun '03-draw-slash-hit-1280x720.png'),
        [byte[]](0x89, 0x50, 0x4E, 0x47))
}
Assert-ValidatorRejects 'forged-state' {
    param($caseRun)
    $path = Join-Path $caseRun 'stage17-skill-stones-state.txt'
    $text = [System.IO.File]::ReadAllText($path).Replace(
        'save_version=8', 'save_version=7')
    [System.IO.File]::WriteAllText(
        $path, $text, [System.Text.UTF8Encoding]::new($false))
}
Assert-ValidatorRejects 'stale-png' {
    param($caseRun)
    $marker = Get-Item -LiteralPath (Join-Path $caseRun 'run.marker')
    (Get-Item -LiteralPath (Join-Path $caseRun '04-storm-ground-array-1280x720.png')).LastWriteTimeUtc =
        $marker.LastWriteTimeUtc.AddSeconds(-1)
}

$junctionTarget = Join-Path $evidence 'validator-self-test-junction-target'
$junction = Join-Path $mutation 'existing-junction'
New-Item -ItemType Directory -Path $junctionTarget -Force | Out-Null
$sentinel = Join-Path $junctionTarget 'sentinel.keep'
New-Item -ItemType File -Path $sentinel -Force | Out-Null
try {
    New-Item -ItemType Junction -Path $junction -Target $junctionTarget | Out-Null
    $junctionRejected = $false
    try {
        Resolve-ControlledMutationRoot $evidence $junction | Out-Null
    } catch {
        $junctionRejected = $true
    }
    if (-not $junctionRejected) {
        throw 'controlled cleanup accepted an existing junction MutationRoot'
    }
    $nestedJunctionRejected = $false
    try {
        Resolve-ControlledMutationRoot $evidence $mutation | Out-Null
    } catch {
        $nestedJunctionRejected = $true
    }
    if (-not $nestedJunctionRejected) {
        throw 'controlled cleanup accepted a MutationRoot containing a junction'
    }
    if (-not (Test-Path -LiteralPath $sentinel -PathType Leaf)) {
        throw 'junction rejection did not preserve the target sentinel'
    }
} finally {
    if (Test-Path -LiteralPath $junction) {
        $junctionItem = Get-Item -LiteralPath $junction -Force
        if (($junctionItem.Attributes -band
                [System.IO.FileAttributes]::ReparsePoint) -eq 0) {
            throw "junction self-test path lost reparse identity: $junction"
        }
        [System.IO.Directory]::Delete($junction)
    }
    $safeTarget = Resolve-ControlledMutationRoot $evidence $junctionTarget
    if (Test-Path -LiteralPath $safeTarget) {
        Remove-Item -LiteralPath $safeTarget -Recurse -Force
    }
}

Write-Output '[stage17-skill-stones-evidence-self-test] PASS'
