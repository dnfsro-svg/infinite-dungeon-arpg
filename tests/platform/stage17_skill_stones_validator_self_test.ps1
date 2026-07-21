param(
    [Parameter(Mandatory = $true)][string]$EvidenceDirectory,
    [Parameter(Mandatory = $true)][string]$Validator,
    [Parameter(Mandatory = $true)][string]$MutationRoot
)

$ErrorActionPreference = 'Stop'
$evidence = [System.IO.Path]::GetFullPath($EvidenceDirectory)
$mutation = [System.IO.Path]::GetFullPath($MutationRoot)
$prefix = $evidence.TrimEnd([System.IO.Path]::DirectorySeparatorChar) +
    [System.IO.Path]::DirectorySeparatorChar
if (-not $mutation.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "mutation root escapes evidence root: $mutation"
}
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
        (Join-Path $caseRun '02-draw-slash-hit-1280x720.png'),
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
    (Get-Item -LiteralPath (Join-Path $caseRun '03-storm-array-1280x720.png')).LastWriteTimeUtc =
        $marker.LastWriteTimeUtc.AddSeconds(-1)
}

Write-Output '[stage17-skill-stones-evidence-self-test] PASS'
