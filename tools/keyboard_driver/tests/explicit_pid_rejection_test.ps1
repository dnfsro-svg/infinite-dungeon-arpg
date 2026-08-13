param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('executable-mismatch', 'multiple-windows')]
    [string]$Case,

    [Parameter(Mandatory = $true)]
    [string]$Driver,

    [Parameter(Mandatory = $true)]
    [string]$Fixture
)

$ErrorActionPreference = 'Stop'
$fixtureName = [System.IO.Path]::GetFileName($Fixture)
$marker = Join-Path $env:TEMP `
    "keyboard_driver_rejection_$([Guid]::NewGuid().ToString('N')).txt"
$readyMarker = Join-Path $env:TEMP `
    "keyboard_driver_ready_$([Guid]::NewGuid().ToString('N')).txt"
$fixtureArgument = if ($Case -eq 'multiple-windows') {
    "two-windows:$readyMarker"
} else {
    $marker
}
$fixtureProcess = $null

try {
    $fixtureProcess = Start-Process -FilePath $Fixture `
        -ArgumentList $fixtureArgument -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        $fixtureProcess.Refresh()
        $ready = if ($Case -eq 'multiple-windows') {
            Test-Path -LiteralPath $readyMarker
        } else {
            $fixtureProcess.MainWindowHandle -ne 0
        }
        if (-not $ready) {
            Start-Sleep -Milliseconds 50
        }
    } while (-not $ready -and [DateTime]::UtcNow -lt $deadline)

    if (-not $ready) {
        throw 'Window fixture did not expose a visible top-level window.'
    }

    $requestedPid = if ($Case -eq 'executable-mismatch') {
        $PID
    } else {
        $fixtureProcess.Id
    }
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $Driver --target $fixtureName --pid $requestedPid `
            --key d --hold-ms 1 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $output = $lines -join [Environment]::NewLine

    if ($exitCode -ne 3) {
        throw "Expected exit code 3 for $Case, got $exitCode.`n$output"
    }
    if ($output -notmatch "\b$requestedPid\b") {
        throw "Expected requested PID $requestedPid in the diagnostic.`n$output"
    }
    if ($Case -eq 'executable-mismatch') {
        if ($output -notmatch '(?i)does not match target executable') {
            throw "Expected an executable mismatch diagnostic.`n$output"
        }
        if (Test-Path -LiteralPath $marker) {
            throw 'A key was sent despite the executable mismatch.'
        }
    } elseif ($output -notmatch '(?i)multiple visible top-level windows') {
        throw "Expected a multiple-window diagnostic.`n$output"
    }
} finally {
    if ($null -ne $fixtureProcess -and -not $fixtureProcess.HasExited) {
        Stop-Process -Id $fixtureProcess.Id -Force
    }
    Remove-Item -LiteralPath $marker, $readyMarker -Force `
        -ErrorAction SilentlyContinue
}
