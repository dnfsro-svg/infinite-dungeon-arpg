[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$AssetDirectory,
    [Parameter(Mandatory=$true)][string]$Validator,
    [Parameter(Mandatory=$true)][string]$MutationRoot
)
$ErrorActionPreference='Stop'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Validator -AssetDirectory $AssetDirectory
if($LASTEXITCODE-ne0){throw 'validator rejected pristine Stage 15 assets'}
function Invoke-Rejected([string]$Name,[scriptblock]$Mutate){
    $root=Join-Path $MutationRoot $Name
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
    New-Item -ItemType Directory -Force -Path $root|Out-Null
    Copy-Item -Path (Join-Path $AssetDirectory '*') -Destination $root -Recurse
    & $Mutate $root
    $oldPreference=$ErrorActionPreference
    $ErrorActionPreference='Continue'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Validator -AssetDirectory $root 1>$null 2>$null
    $exitCode=$LASTEXITCODE
    $ErrorActionPreference=$oldPreference
    if($exitCode-eq0){throw "validator accepted mutation: $Name"}
}
Invoke-Rejected 'hash' { param($root) $p=Join-Path $root 'music-explore.ogg'; $b=[IO.File]::ReadAllBytes($p); $b[$b.Length-17]=$b[$b.Length-17]-bxor1; [IO.File]::WriteAllBytes($p,$b) }
Invoke-Rejected 'extra' { param($root) [IO.File]::WriteAllText((Join-Path $root 'unexpected.tmp'),'x') }
Write-Host 'Stage 15 audio source validator rejects hash mutation and unexpected files.'
