param(
    [Parameter(Mandatory = $true)]
    [string]$ValidatorScript,
    [Parameter(Mandatory = $true)]
    [string]$ScratchDirectory
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Path $ScratchDirectory -Force | Out-Null
$validPath = Join-Path $ScratchDirectory 'valid-low-color.png'
$blankPath = Join-Path $ScratchDirectory 'blank.png'

function New-TestCapture {
    param([string]$Path, [bool]$Blank)
    $image = [System.Drawing.Bitmap]::new(1280, 720)
    $graphics = [System.Drawing.Graphics]::FromImage($image)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(13, 17, 27))
        if (-not $Blank) {
            $palette = 0..18 | ForEach-Object {
                [System.Drawing.Color]::FromArgb(
                    28 + $_ * 3,
                    38 + ($_ * 7) % 52,
                    58 + ($_ * 11) % 64)
            }
            for ($row = 0; $row -lt 45; ++$row) {
                for ($column = 0; $column -lt 80; ++$column) {
                    $brush = [System.Drawing.SolidBrush]::new(
                        $palette[($row + $column) % $palette.Count])
                    try {
                        $graphics.FillRectangle($brush,
                            $column * 16, $row * 16, 16, 16)
                    } finally {
                        $brush.Dispose()
                    }
                }
            }
        }
        $image.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $image.Dispose()
    }
}

New-TestCapture -Path $validPath -Blank $false
New-TestCapture -Path $blankPath -Blank $true

$powershell = (Get-Command powershell.exe -ErrorAction Stop).Source
$validArguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass',
    '-File', $ValidatorScript, '-ValidateOnlyPath', $validPath)
$validOutput = & $powershell @validArguments 2>&1
$validExitCode = $LASTEXITCODE
if ($validExitCode -ne 0 -or
        ($validOutput -join "`n") -notmatch 'stage10_capture_content=PASS') {
    throw "Low-color submitted scene was rejected or lacked PASS marker: exit=$validExitCode output=$($validOutput -join ' | ')"
}
Write-Output "stage10_capture_valid_accepted=PASS exit=$validExitCode"

$blankArguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass',
    '-File', $ValidatorScript, '-ValidateOnlyPath', $blankPath)
try {
    $ErrorActionPreference = 'Continue'
    $blankOutput = & $powershell @blankArguments 2>&1
} finally {
    $ErrorActionPreference = 'Stop'
}
$blankExitCode = $LASTEXITCODE
if ($blankExitCode -eq 0 -or
        ($blankOutput -join "`n") -notmatch 'Formal capture is blank') {
    throw "True blank rejection was not observed: exit=$blankExitCode output=$($blankOutput -join ' | ')"
}
Write-Output "stage10_capture_blank_rejected=PASS exit=$blankExitCode"
Write-Output 'stage10_capture_content_regression=PASS'
