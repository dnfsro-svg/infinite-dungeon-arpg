[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $ExecutablePath
)

$ErrorActionPreference = 'Stop'

function Invoke-InventorySelectorProcess {
    param(
        [Parameter(Mandatory)]
        [string] $ExecutablePath
    )

    $output = @(& $ExecutablePath 2>&1)
    $exitCode = if ($null -eq $LASTEXITCODE) {
        1
    }
    else {
        [int] $LASTEXITCODE
    }
    return [pscustomobject]@{
        Output = @($output)
        ExitCode = $exitCode
    }
}

function Invoke-FullChineseInventorySelector {
    param(
        [Parameter(Mandatory)]
        [string] $ExecutablePath
    )

    $selector = 'ARPG_FULL_CHINESE_UI_INVENTORY_ONLY'
    $expectedSummary = '120 cases, 0 failures'
    $saved = @{}
    Get-ChildItem Env: | Where-Object {
        $_.Name -match '^ARPG_.*_ONLY$'
    } | ForEach-Object {
        $saved[$_.Name] = $_.Value
    }

    try {
        Get-ChildItem Env: | Where-Object {
            $_.Name -match '^ARPG_.*_ONLY$'
        } | ForEach-Object {
            Remove-Item -LiteralPath "Env:$($_.Name)"
        }
        Set-Item -LiteralPath "Env:$selector" -Value '1'
        $result = Invoke-InventorySelectorProcess `
            -ExecutablePath $ExecutablePath
        $lines = @($result.Output | ForEach-Object { $_.ToString() })
        if ([int] $result.ExitCode -ne 0) {
            throw "Inventory selector process failed with exit code $($result.ExitCode): $($lines -join '; ')"
        }

        $summaryLines = @($lines | Where-Object {
            $_ -match '^[0-9]+ cases, [0-9]+ failures$'
        })
        if ($summaryLines.Count -ne 1 `
                -or $summaryLines[0] -cne $expectedSummary) {
            $observed = $summaryLines -join '; '
            throw "Inventory selector expected one exact summary '$expectedSummary'; observed '$observed'"
        }
        $lines | Write-Output
    }
    finally {
        Get-ChildItem Env: | Where-Object {
            $_.Name -match '^ARPG_.*_ONLY$'
        } | ForEach-Object {
            Remove-Item -LiteralPath "Env:$($_.Name)"
        }
        foreach ($entry in $saved.GetEnumerator()) {
            Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value
        }
    }
}

$resolvedExecutable = (Resolve-Path -LiteralPath $ExecutablePath).Path
Invoke-FullChineseInventorySelector -ExecutablePath $resolvedExecutable
