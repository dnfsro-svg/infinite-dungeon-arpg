param(
    [Parameter(Mandatory = $true)]
    [string]$Driver,

    [Parameter(Mandatory = $true)]
    [string]$Fixture
)

$ErrorActionPreference = 'Stop'
$fixtureName = [System.IO.Path]::GetFileName($Fixture)
$testId = [Guid]::NewGuid().ToString('N')
$selectedMarker = Join-Path $env:TEMP "keyboard_driver_selected_$testId.txt"
$otherMarker = Join-Path $env:TEMP "keyboard_driver_other_$testId.txt"
$processes = @()

try {
    $processes = @(
        Start-Process -FilePath $Fixture -ArgumentList $selectedMarker -PassThru
        Start-Process -FilePath $Fixture -ArgumentList $otherMarker -PassThru
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
        $lines = @(& $Driver --target $fixtureName --pid $processes[0].Id `
            --key d --hold-ms 1 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $output = $lines -join [Environment]::NewLine

    if ($exitCode -ne 0) {
        throw "Expected explicit PID targeting to succeed, got $exitCode.`n$output"
    }

    $deadline = [DateTime]::UtcNow.AddSeconds(2)
    while (-not (Test-Path -LiteralPath $selectedMarker) `
        -and [DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 25
    }
    if (-not (Test-Path -LiteralPath $selectedMarker)) {
        throw "Requested PID $($processes[0].Id) did not receive the key.`n$output"
    }
    if (Test-Path -LiteralPath $otherMarker) {
        throw "Non-requested PID $($processes[1].Id) received the key."
    }
} finally {
    foreach ($process in $processes) {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
        }
    }
    Remove-Item -LiteralPath $selectedMarker, $otherMarker -Force `
        -ErrorAction SilentlyContinue
}
