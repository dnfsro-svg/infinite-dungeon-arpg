param(
    [Parameter(Mandatory = $true)][string]$EvidenceDirectory,
    [Parameter(Mandatory = $true)][string]$Validator,
    [Parameter(Mandatory = $true)][string]$MutationRoot
)
$ErrorActionPreference = 'Stop'
& $Validator -EvidenceDirectory $EvidenceDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $MutationRoot | Out-Null
$source = Join-Path $EvidenceDirectory 'stage15-audio-evidence.txt'
$target = Join-Path $MutationRoot 'stage15-audio-evidence.txt'
$text = [System.IO.File]::ReadAllText($source).Replace('result=PASS', 'result=FAIL')
[System.IO.File]::WriteAllText($target, $text, [System.Text.UTF8Encoding]::new($false))
$failed = $false
try { & $Validator -EvidenceDirectory $MutationRoot 2>$null | Out-Null } catch { $failed = $true }
if (-not $failed) { throw 'validator accepted forged result' }
Write-Output '[stage15-audio-evidence-self-test] PASS'
