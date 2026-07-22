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
Save-MutatedBitmap (Join-Path $missingMonsters 'lightning-monsters-1280x720.png') {
    param($bitmap)
    $floor = $bitmap.Clone(
        [System.Drawing.Rectangle]::new(250, 220, 230, 155),
        $bitmap.PixelFormat)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.DrawImageUnscaled($floor, 520, 390)
    } finally {
        $graphics.Dispose()
        $floor.Dispose()
    }
}
$mutations += @{ Name='missing-lightning-monsters'; Path=$missingMonsters }

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
Write-Output 'stage12 material validator rejected solid, wrong-ecology and missing-monster captures'
