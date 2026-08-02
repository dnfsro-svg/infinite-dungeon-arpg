[CmdletBinding()]
param(
    [string]$OutputDirectory,
    [string]$BaselineWorktree,
    [ValidateRange(10000, 1000000)]
    [int]$Frames = 10240,
    [ValidateRange(1, 1000000)]
    [int]$WarmupFrames = 2048,
    [ValidateRange(0, 63)]
    [int]$LogicalCpu = 0
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Native {
    param(
        [Parameter(Mandatory)]
        [string]$FilePath,
        [Parameter(Mandatory)]
        [string[]]$Arguments,
        [Parameter(Mandatory)]
        [string]$WorkingDirectory
    )
    Push-Location $WorkingDirectory
    try {
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "native command failed ($LASTEXITCODE): $FilePath $($Arguments -join ' ')"
        }
    } finally {
        Pop-Location
    }
}

function Enter-ArpgMsvcEnvironment {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} `
        'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "vswhere.exe not found: $vswhere"
    }
    $installPath = & $vswhere -latest -version '[17.0,18.0)' -products '*' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        Microsoft.VisualStudio.Component.Windows11SDK.26100 `
        -property installationPath
    if ($LASTEXITCODE -ne 0 -or
        [string]::IsNullOrWhiteSpace($installPath)) {
        throw 'VS 2022 with MSVC x64 and SDK 26100 not found'
    }
    $installPath = ($installPath | Select-Object -First 1).Trim()
    $devShell = Join-Path $installPath `
        'Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
    Import-Module $devShell -Force
    Enter-VsDevShell -VsInstallPath $installPath -SkipAutomaticLocation `
        -DevCmdArguments '-arch=x64 -host_arch=x64 -winsdk=10.0.26100.0' |
        Out-Null
    $compiler = Get-Command cl.exe -ErrorAction Stop
    $version = [Version]$compiler.FileVersionInfo.FileVersion
    if ($version.Major -ne 19 -or $version.Minor -ne 44) {
        throw "MSVC 19.44 required; detected $version"
    }
    if ($env:WindowsSDKVersion.TrimEnd('\') -ne '10.0.26100.0') {
        throw "Windows SDK 10.0.26100.0 required; detected $env:WindowsSDKVersion"
    }
    Get-Command cmake.exe -ErrorAction Stop | Out-Null
    Get-Command ninja.exe -ErrorAction Stop | Out-Null
}

function Get-ProbeExecutable {
    param([Parameter(Mandatory)][string]$BuildRoot)
    $matches = @(Get-ChildItem -LiteralPath $buildRoot -Recurse -File `
        -Filter 'arpg_room_render_prep_probe.exe')
    if ($matches.Count -ne 1) {
        throw "expected exactly one room render probe below $buildRoot; found $($matches.Count)"
    }
    return $matches[0].FullName
}

function Invoke-ProbeRun {
    param(
        [Parameter(Mandatory)][string]$Executable,
        [Parameter(Mandatory)][string]$RunDirectory,
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][string]$Revision
    )
    New-Item -ItemType Directory -Path $RunDirectory -Force | Out-Null
    Invoke-Native -FilePath $Executable -WorkingDirectory (Split-Path $Executable) `
        -Arguments @('--output', $RunDirectory, '--label', $Label,
            '--revision', $Revision, '--frames', "$Frames",
            '--warmup', "$WarmupFrames", '--cpu', "$LogicalCpu")
    $summaryPath = Join-Path $RunDirectory 'summary.json'
    $rawPath = Join-Path $RunDirectory 'samples.csv'
    if (-not (Test-Path -LiteralPath $summaryPath -PathType Leaf) -or
        -not (Test-Path -LiteralPath $rawPath -PathType Leaf)) {
        throw "probe did not produce summary.json and samples.csv in $RunDirectory"
    }
    return Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
}

$scriptPath = $MyInvocation.MyCommand.Path
$currentRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$commonGit = (& git -C $currentRoot rev-parse --path-format=absolute --git-common-dir).Trim()
if ($LASTEXITCODE -ne 0) { throw 'unable to locate common Git directory' }
$repositoryRoot = Split-Path $commonGit -Parent
$baselineRevision = 'b91cce1d352f26fc91f08e96e55ca19bacfbf1e3'
$currentRevision = (& git -C $currentRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'unable to resolve current revision' }

if ([string]::IsNullOrWhiteSpace($BaselineWorktree)) {
    $BaselineWorktree = Join-Path $repositoryRoot '.worktrees/task11-release-b91cce1'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $stamp = (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ')
    $OutputDirectory = Join-Path $currentRoot `
        "docs/validation/evidence/square-hundredfold-room/release-$stamp"
}
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$BaselineWorktree = [System.IO.Path]::GetFullPath($BaselineWorktree)
$currentStatusBefore = @(& git -C $currentRoot status --porcelain=v1 `
    --untracked-files=all)
if ($LASTEXITCODE -ne 0) { throw 'unable to inspect current worktree' }
if ($currentStatusBefore.Count -ne 0) {
    throw "formal evidence requires a clean current worktree: $($currentStatusBefore -join '; ')"
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
Enter-ArpgMsvcEnvironment

if (-not (Test-Path -LiteralPath (Join-Path $BaselineWorktree '.git'))) {
    Invoke-Native -FilePath 'git' -WorkingDirectory $repositoryRoot `
        -Arguments @('worktree', 'add', '--detach', $BaselineWorktree,
            $baselineRevision)
}
$actualBaseline = (& git -C $BaselineWorktree rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualBaseline -ne $baselineRevision) {
    throw "baseline worktree must be exactly $baselineRevision; got $actualBaseline"
}

$probeSource = Join-Path $currentRoot 'tools/performance/room_render_prep_probe.cpp'
$probeCMake = Join-Path $currentRoot 'tools/performance/CMakeLists.txt'
$baselineProbeDirectory = Join-Path $BaselineWorktree 'tools/performance'
$baselineProbeSource = Join-Path $baselineProbeDirectory 'room_render_prep_probe.cpp'
$baselineProbeCMake = Join-Path $baselineProbeDirectory 'CMakeLists.txt'

$baselineStatus = @(& git -C $BaselineWorktree status --porcelain `
    --untracked-files=all)
$unexpectedBaselineChanges = @($baselineStatus | Where-Object {
    $_ -notmatch 'tools/performance/(CMakeLists\.txt|room_render_prep_probe\.cpp)$'
})
if ($unexpectedBaselineChanges.Count -ne 0) {
    throw "baseline worktree has unrelated changes: $($unexpectedBaselineChanges -join '; ')"
}
if ((Test-Path -LiteralPath $baselineProbeSource) -and
    (Get-FileHash -Algorithm SHA256 $baselineProbeSource).Hash -ne
        (Get-FileHash -Algorithm SHA256 $probeSource).Hash) {
    throw "existing baseline probe differs from current harness; preserve it and choose a new worktree"
}
if ($baselineStatus.Count -ne 0 -and
    (Get-FileHash -Algorithm SHA256 $baselineProbeCMake).Hash -ne
        (Get-FileHash -Algorithm SHA256 $probeCMake).Hash) {
    throw "existing baseline CMake overlay differs from current harness"
}
Copy-Item -LiteralPath $probeSource -Destination $baselineProbeSource
Copy-Item -LiteralPath $probeCMake -Destination $baselineProbeCMake

$currentBuildRoot = Join-Path $currentRoot `
    'out/build/task11-release-benchmark-current'
$baselineBuildRoot = Join-Path $BaselineWorktree `
    'out/build/task11-release-benchmark-baseline'
$currentConfigure = @('-S', $currentRoot, '-B', $currentBuildRoot,
    '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
    '-DARPG_BUILD_PERFORMANCE_PROBES=ON',
    '-DARPG_TASK11_RENDER_BENCHMARK_VARIANT=current',
    "-DARPG_TASK11_BENCHMARK_REVISION=$currentRevision",
    '-DBUILD_TESTING=OFF')
$baselineConfigure = @('-S', $BaselineWorktree, '-B', $baselineBuildRoot,
    '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
    '-DARPG_BUILD_PERFORMANCE_PROBES=ON',
    '-DARPG_TASK11_RENDER_BENCHMARK_VARIANT=baseline',
    "-DARPG_TASK11_BENCHMARK_REVISION=$baselineRevision",
    '-DBUILD_TESTING=OFF')
Invoke-Native -FilePath 'cmake' -Arguments $baselineConfigure `
    -WorkingDirectory $BaselineWorktree
Invoke-Native -FilePath 'cmake' -Arguments @('--build', $baselineBuildRoot,
    '--target', 'arpg_room_render_prep_probe', '--', '-j1') `
    -WorkingDirectory $BaselineWorktree
Invoke-Native -FilePath 'cmake' -Arguments $currentConfigure `
    -WorkingDirectory $currentRoot
Invoke-Native -FilePath 'cmake' -Arguments @('--build', $currentBuildRoot,
    '--target', 'arpg_room_render_prep_probe', '--', '-j1') `
    -WorkingDirectory $currentRoot

$baselineExecutable = Get-ProbeExecutable -BuildRoot $baselineBuildRoot
$currentExecutable = Get-ProbeExecutable -BuildRoot $currentBuildRoot
$runs = [ordered]@{ baseline = @(); current = @() }
$sequence = @(
    @{ Variant = 'baseline'; Round = 0 },
    @{ Variant = 'current'; Round = 0 },
    @{ Variant = 'current'; Round = 1 },
    @{ Variant = 'baseline'; Round = 1 },
    @{ Variant = 'baseline'; Round = 2 },
    @{ Variant = 'current'; Round = 2 }
)
foreach ($entry in $sequence) {
    $variant = $entry.Variant
    $round = [int]$entry.Round
    $executable = if ($variant -eq 'baseline') {
        $baselineExecutable
    } else {
        $currentExecutable
    }
    $revision = if ($variant -eq 'baseline') {
        $baselineRevision
    } else {
        $currentRevision
    }
    $runDirectory = Join-Path $OutputDirectory "$variant-round-$round"
    $summary = Invoke-ProbeRun -Executable $executable `
        -RunDirectory $runDirectory -Label "$variant-round-$round" `
        -Revision $revision
    $runs[$variant] += $summary
}

$ratios = @()
for ($round = 0; $round -lt 3; ++$round) {
    $baselineP99 = [double]$runs.baseline[$round].thread_cycles_p99
    $currentP99 = [double]$runs.current[$round].thread_cycles_p99
    if ($baselineP99 -le 0) { throw 'baseline P99 must be positive' }
    $ratios += $currentP99 / $baselineP99
}
$sortedRatios = @($ratios | Sort-Object)
$medianRatio = [double]$sortedRatios[1]
$maximumRatio = [double](($ratios | Measure-Object -Maximum).Maximum)
$ratioSpread = [double]$sortedRatios[2] - [double]$sortedRatios[0]
$visibleSignatures = @(
    $runs.baseline.visible_signature
    $runs.current.visible_signature
) | Select-Object -Unique
$visibleDensitySignatures = @(
    $runs.baseline.visible_density_signature
    $runs.current.visible_density_signature
) | Select-Object -Unique
$validFlags = @($runs.baseline.valid) + @($runs.current.valid)
$allValid = $validFlags -notcontains $false
$currentProductionPopulations = @(
    $runs.current.production_population | Select-Object -Unique)
$currentProductionPaths = @(
    $runs.current.production_path | Select-Object -Unique)
$productionCoverageValid = $currentProductionPopulations.Count -eq 1 -and
    [int]$currentProductionPopulations[0] -eq 1125 -and
    $currentProductionPaths.Count -eq 1 -and
    $currentProductionPaths[0] -eq 'DungeonSession::write_render_snapshot' -and
    @($runs.current.production_visible_monsters |
        Where-Object { [int]$_ -le 0 }).Count -eq 0 -and
    @($runs.current.production_environment_candidates |
        Where-Object { [int]$_ -le 0 }).Count -eq 0 -and
    @($runs.current.production_drop_candidates |
        Where-Object { [int]$_ -le 0 }).Count -eq 0
$stable = $ratioSpread -le 0.05
$allRoundsWithinThreshold = @($ratios | Where-Object { $_ -gt 1.10 }).Count -eq 0
$passed = $allValid -and $stable -and
    $visibleSignatures.Count -eq 1 -and
    $visibleDensitySignatures.Count -eq 1 -and
    $productionCoverageValid -and $allRoundsWithinThreshold

$currentStatus = @(& git -C $currentRoot status --short `
    --untracked-files=all)
$baselineStatusAfter = @(& git -C $BaselineWorktree status --short `
    --untracked-files=all)
$unexpectedBaselineChangesAfter = @($baselineStatusAfter | Where-Object {
    $_ -notmatch 'tools/performance/(CMakeLists\.txt|room_render_prep_probe\.cpp)$'
})
$baselineOverlayValid = $unexpectedBaselineChangesAfter.Count -eq 0 -and
    (Test-Path -LiteralPath $baselineProbeSource -PathType Leaf) -and
    (Test-Path -LiteralPath $baselineProbeCMake -PathType Leaf) -and
    (Get-FileHash -Algorithm SHA256 $baselineProbeSource).Hash -eq
        (Get-FileHash -Algorithm SHA256 $probeSource).Hash -and
    (Get-FileHash -Algorithm SHA256 $baselineProbeCMake).Hash -eq
        (Get-FileHash -Algorithm SHA256 $probeCMake).Hash
$passed = $passed -and $baselineOverlayValid
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1 `
    Name, Manufacturer, NumberOfCores, NumberOfLogicalProcessors,
    MaxClockSpeed
$os = Get-CimInstance Win32_OperatingSystem | Select-Object Caption,
    Version, BuildNumber
$powerScheme = (& powercfg /getactivescheme) -join "`n"
$comparison = [ordered]@{
    schema = 1
    generated_utc = (Get-Date).ToUniversalTime().ToString('o')
    baseline_revision = $baselineRevision
    current_revision = $currentRevision
    frames_per_path_per_round = $Frames
    warmup_frames_per_process = $WarmupFrames
    rounds = 3
    process_order = @($sequence | ForEach-Object {
        "$($_.Variant)-$($_.Round)"
    })
    timer = 'QueryThreadCycleTime'
    logical_cpu_requested = $LogicalCpu
    visible_signature = if ($visibleSignatures.Count -eq 1) {
        [string]$visibleSignatures[0]
    } else { $null }
    visible_density_signature = if ($visibleDensitySignatures.Count -eq 1) {
        [string]$visibleDensitySignatures[0]
    } else { $null }
    current_maximum_population_path_valid = $productionCoverageValid
    baseline_overlay_valid = $baselineOverlayValid
    paired_p99_ratios = $ratios
    median_p99_ratio = $medianRatio
    p99_regression_percent = ($medianRatio - 1.0) * 100.0
    maximum_p99_ratio = $maximumRatio
    maximum_p99_regression_percent = ($maximumRatio - 1.0) * 100.0
    ratio_spread = $ratioSpread
    stability_limit = 0.05
    threshold_ratio = 1.10
    all_rounds_within_threshold = $allRoundsWithinThreshold
    stable = $stable
    valid = $allValid -and $visibleSignatures.Count -eq 1 -and
        $visibleDensitySignatures.Count -eq 1 -and $productionCoverageValid -and
        $baselineOverlayValid
    passed = $passed
    cpu = $cpu
    os = $os
    power_scheme = $powerScheme
    baseline_executable = $baselineExecutable
    baseline_executable_sha256 = (Get-FileHash -Algorithm SHA256 `
        $baselineExecutable).Hash
    current_executable = $currentExecutable
    current_executable_sha256 = (Get-FileHash -Algorithm SHA256 `
        $currentExecutable).Hash
    harness_sha256 = (Get-FileHash -Algorithm SHA256 $probeSource).Hash
    launcher_sha256 = (Get-FileHash -Algorithm SHA256 $scriptPath).Hash
    baseline_runs = $runs.baseline
    current_runs = $runs.current
    current_worktree_status = $currentStatus
    baseline_overlay_status = $baselineStatusAfter
    limitations = @(
        'The two revisions are separate executables, so samples cannot be interleaved inside one process.',
        'ABBA process ordering, identical affinity, Release builds, warmup, and thread-cycle timing reduce but do not eliminate cross-process thermal and cache drift.',
        'Both variants time their production snapshot path before equal-density render preparation; current uses a real 1125-population DungeonSession and DungeonSession::write_render_snapshot.',
        'Cross-revision visible_signature hashes stable monster, render-stage, equipment, material, and potion identities plus category counts. variant_output_signature separately records topology-specific prop identities.'
    )
}
$comparisonPath = Join-Path $OutputDirectory 'comparison-summary.json'
$comparison | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath `
    $comparisonPath -Encoding utf8

$currentTrackedStatusAfter = @(& git -C $currentRoot status `
    --porcelain=v1 --untracked-files=no)
if ($currentTrackedStatusAfter.Count -ne 0) {
    throw "benchmark changed tracked current files: $($currentTrackedStatusAfter -join '; ')"
}
$currentPatchPath = Join-Path $OutputDirectory 'current-revision.patch'
$currentRevisionDiff = & git -C $currentRoot diff --binary `
    "$baselineRevision..$currentRevision" -- .
if ($LASTEXITCODE -ne 0) { throw 'unable to capture revision patch' }
$currentRevisionDiff | Set-Content -LiteralPath $currentPatchPath `
    -Encoding utf8
$manifestPath = Join-Path $OutputDirectory 'evidence-hashes.csv'
$evidenceFiles = @(Get-ChildItem -LiteralPath $OutputDirectory -Recurse -File |
    Where-Object { $_.FullName -ne $manifestPath })
@('relative_path,sha256') + @($evidenceFiles | ForEach-Object {
    $relative = [System.IO.Path]::GetRelativePath(
        $OutputDirectory, $_.FullName).Replace('\', '/')
    "$relative,$((Get-FileHash -Algorithm SHA256 $_.FullName).Hash)"
}) | Set-Content -LiteralPath $manifestPath -Encoding utf8

Write-Host (("Task11 Release render preparation: median P99 ratio={0:N4}, " +
    "regression={1:N2}%, stable={2}, passed={3}") -f $medianRatio,
    (($medianRatio - 1.0) * 100.0), $stable, $passed)
Write-Host "Evidence: $OutputDirectory"
if (-not $passed) { exit 1 }
