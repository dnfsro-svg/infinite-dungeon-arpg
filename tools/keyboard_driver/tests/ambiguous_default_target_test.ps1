param(
    [Parameter(Mandatory = $true)]
    [string]$Driver,

    [Parameter(Mandatory = $true)]
    [string]$Fixture
)

$ErrorActionPreference = 'Stop'
$fixtureName = [System.IO.Path]::GetFileName($Fixture)
$processes = @()

try {
    $processes = @(
        Start-Process -FilePath $Fixture -PassThru
        Start-Process -FilePath $Fixture -PassThru
    )

    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        foreach ($process in $processes) {
            $process.Refresh()
        }
        $ready = @($processes | Where-Object { $_.MainWindowHandle -ne 0 }).Count -eq 2
        if (-not $ready) {
            Start-Sleep -Milliseconds 50
        }
    } while (-not $ready -and [DateTime]::UtcNow -lt $deadline)

    if (-not $ready) {
        throw 'Window fixtures did not expose two visible top-level windows.'
    }

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $Driver --target $fixtureName --key d --hold-ms 1 2>&1 |
            ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $output = $lines -join [Environment]::NewLine

    if ($exitCode -ne 3) {
        throw "Expected exit code 3 for an ambiguous target, got $exitCode.`n$output"
    }
    if ($output -notmatch '(?i)multiple visible top-level windows') {
        throw "Expected an explicit ambiguity diagnostic.`n$output"
    }
    foreach ($process in $processes) {
        if ($output -notmatch "\b$($process.Id)\b") {
            throw "Expected candidate PID $($process.Id) in the diagnostic.`n$output"
        }
    }
} finally {
    foreach ($process in $processes) {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
        }
    }
}
