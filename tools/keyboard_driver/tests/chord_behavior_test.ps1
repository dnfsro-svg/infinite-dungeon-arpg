param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('dry-run', 'key-order')]
    [string]$Case,

    [Parameter(Mandatory = $true)]
    [string]$Driver,

    [Parameter(Mandatory = $true)]
    [string]$Fixture
)

$ErrorActionPreference = 'Stop'
$fixtureName = [System.IO.Path]::GetFileName($Fixture)
$marker = Join-Path $env:TEMP `
    "keyboard_driver_chord_$([Guid]::NewGuid().ToString('N')).txt"
$fixtureProcess = $null

function Invoke-Driver {
    param([string[]]$Arguments)

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $script:driverLines = @(& $Driver @Arguments 2>&1 |
            ForEach-Object { $_.ToString() })
        $script:driverExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
}

try {
    if ($Case -eq 'dry-run') {
        Invoke-Driver -Arguments @(
            '--chord', 'd+j', '--hold-ms', '30', '--dry-run'
        )
        $output = $driverLines -join [Environment]::NewLine
        if ($driverExitCode -ne 0) {
            throw "Chord dry-run failed with $driverExitCode.`n$output"
        }
        $expected = 'Chord: press D + J, hold 30 ms, release J + D'
        if ($output -notlike "*$expected*") {
            throw "Expected explicit chord dry-run output: $expected`n$output"
        }
        return
    }

    $fixtureProcess = Start-Process -FilePath $Fixture `
        -ArgumentList $marker -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        $fixtureProcess.Refresh()
        $ready = $fixtureProcess.MainWindowHandle -ne 0
        if (-not $ready) {
            Start-Sleep -Milliseconds 50
        }
    } while (-not $ready -and [DateTime]::UtcNow -lt $deadline)
    if (-not $ready) {
        throw 'Window fixture did not expose a visible top-level window.'
    }

    Invoke-Driver -Arguments @(
        '--target', $fixtureName, '--pid', "$($fixtureProcess.Id)",
        '--chord', 'd+j', '--hold-ms', '30'
    )
    $output = $driverLines -join [Environment]::NewLine
    if ($driverExitCode -ne 0) {
        throw "Chord input failed with $driverExitCode.`n$output"
    }

    $expectedEvents = @('down:68', 'down:74', 'up:74', 'up:68')
    $deadline = [DateTime]::UtcNow.AddSeconds(2)
    do {
        $actualEvents = if (Test-Path -LiteralPath $marker) {
            @(Get-Content -LiteralPath $marker)
        } else {
            @()
        }
        if ($actualEvents.Count -lt $expectedEvents.Count) {
            Start-Sleep -Milliseconds 25
        }
    } while ($actualEvents.Count -lt $expectedEvents.Count `
        -and [DateTime]::UtcNow -lt $deadline)

    if ($actualEvents.Count -ne $expectedEvents.Count) {
        throw "Expected 4 key events, got $($actualEvents.Count): $actualEvents"
    }
    for ($index = 0; $index -lt $expectedEvents.Count; ++$index) {
        if ($actualEvents[$index] -ne $expectedEvents[$index]) {
            throw "Key event $index expected $($expectedEvents[$index]), got $($actualEvents[$index])."
        }
    }
} finally {
    if ($null -ne $fixtureProcess -and -not $fixtureProcess.HasExited) {
        Stop-Process -Id $fixtureProcess.Id -Force
    }
    Remove-Item -LiteralPath $marker -Force -ErrorAction SilentlyContinue
}
