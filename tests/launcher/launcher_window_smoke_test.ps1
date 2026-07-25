param(
    [Parameter(Mandatory = $true)]
    [string] $Launcher
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class LauncherPixelSmokeNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT {
        public int X;
        public int Y;
    }

    [DllImport("dwmapi.dll")]
    public static extern int DwmFlush();

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetClientRect(IntPtr window, out RECT rectangle);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ClientToScreen(IntPtr window, ref POINT point);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetWindowPos(
        IntPtr window,
        IntPtr insertAfter,
        int x,
        int y,
        int width,
        int height,
        uint flags);

    [DllImport("user32.dll")]
    public static extern IntPtr WindowFromPoint(POINT point);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr dpiContext);

}
'@

function Get-PeSubsystem {
    param([Parameter(Mandatory = $true)][string] $Path)

    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::Read)
    try {
        $reader = [System.IO.BinaryReader]::new($stream)
        try {
            $stream.Position = 0x3c
            $peOffset = $reader.ReadInt32()
            $stream.Position = $peOffset
            if ($reader.ReadUInt32() -ne 0x00004550) {
                throw "Invalid PE signature: $Path"
            }

            $stream.Position = $peOffset + 24
            $magic = $reader.ReadUInt16()
            if ($magic -ne 0x010b -and $magic -ne 0x020b) {
                throw "Unsupported PE optional-header magic: $magic"
            }

            $stream.Position = $peOffset + 24 + 68
            return $reader.ReadUInt16()
        }
        finally {
            $reader.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

function Assert-RenderedClientPixels {
    param([Parameter(Mandatory = $true)][System.Diagnostics.Process] $Process)

    $SWP_NOMOVE = 0x0002
    $SWP_NOSIZE = 0x0001
    $SWP_SHOWWINDOW = 0x0040
    if (-not [LauncherPixelSmokeNative]::SetWindowPos(
        $Process.MainWindowHandle,
        [IntPtr](-1),
        0,
        0,
        0,
        0,
        $SWP_NOMOVE -bor $SWP_NOSIZE -bor $SWP_SHOWWINDOW)) {
        throw "Could not place launcher above the test runner: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    if ([LauncherPixelSmokeNative]::DwmFlush() -ne 0) {
        throw 'DwmFlush failed before launcher pixel capture'
    }
    $client = [LauncherPixelSmokeNative+RECT]::new()
    if (-not [LauncherPixelSmokeNative]::GetClientRect($Process.MainWindowHandle, [ref] $client)) {
        throw "Could not read launcher client rectangle: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    $width = $client.Right - $client.Left
    $height = $client.Bottom - $client.Top
    if ($width -lt 800 -or $height -lt 450) {
        throw "Unexpected launcher client size: ${width}x${height}"
    }

    $origin = [LauncherPixelSmokeNative+POINT]@{ X = 0; Y = 0 }
    if (-not [LauncherPixelSmokeNative]::ClientToScreen($Process.MainWindowHandle, [ref] $origin)) {
        throw "Could not resolve launcher client origin: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    $capturePoints = @(
        @{ X = $origin.X + 4; Y = $origin.Y + 4 },
        @{ X = $origin.X + $width - 5; Y = $origin.Y + 4 },
        @{ X = $origin.X + 4; Y = $origin.Y + $height - 5 },
        @{ X = $origin.X + $width - 5; Y = $origin.Y + $height - 5 },
        @{ X = [int]($origin.X + ($width * 0.50)); Y = [int]($origin.Y + ($height * 0.50)) },
        @{ X = [int]($origin.X + ($width * 0.50)); Y = [int]($origin.Y + ($height * 0.54)) },
        @{ X = [int]($origin.X + ($width * 0.50)); Y = [int]($origin.Y + ($height * 0.88)) }
    )
    foreach ($capturePoint in $capturePoints) {
        $screenPoint = [LauncherPixelSmokeNative+POINT]@{ X = $capturePoint.X; Y = $capturePoint.Y }
        $windowAtPoint = [LauncherPixelSmokeNative]::WindowFromPoint($screenPoint)
        [uint32] $windowProcessId = 0
        [void][LauncherPixelSmokeNative]::GetWindowThreadProcessId($windowAtPoint, [ref] $windowProcessId)
        if ($windowProcessId -ne $Process.Id) {
            throw "Launcher client was occluded by process $windowProcessId during pixel capture"
        }
    }

    $bitmap = [System.Drawing.Bitmap]::new($width, $height)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0, $bitmap.Size)
        }
        finally {
            $graphics.Dispose()
        }
        $darkPixels = 0
        for ($y = 0; $y -lt $height; $y += 4) {
            for ($x = 0; $x -lt $width; $x += 4) {
                $pixel = $bitmap.GetPixel($x, $y)
                if ((($pixel.R * 30) + ($pixel.G * 59) + ($pixel.B * 11)) -lt 24000) {
                    ++$darkPixels
                }
            }
        }
        $sampleCount = [math]::Ceiling($width / 4) * [math]::Ceiling($height / 4)
        if ($darkPixels -lt ($sampleCount * 0.55)) {
            $Process.Refresh()
            throw "Launcher client did not contain substantial dark rendering: $darkPixels of $sampleCount sampled pixels; window title '$($Process.MainWindowTitle)'"
        }

        $darkButtonPixels = 0
        $brightButtonPixels = 0
        $buttonLeft = [int][math]::Floor($width * (58.0 / 920.0))
        $buttonRight = [int][math]::Ceiling($width * (862.0 / 920.0))
        $buttonTop = [int][math]::Floor($height * (260.0 / 560.0))
        $buttonBottom = [int][math]::Ceiling($height * (342.0 / 560.0))
        for ($y = $buttonTop; $y -lt $buttonBottom; ++$y) {
            for ($x = $buttonLeft; $x -lt $buttonRight; ++$x) {
                $pixel = $bitmap.GetPixel($x, $y)
                if ((($pixel.R * 30) + ($pixel.G * 59) + ($pixel.B * 11)) -lt 24000) {
                    ++$darkButtonPixels
                }
                if ($pixel.R -ge 210 -and $pixel.G -ge 210 -and $pixel.B -ge 210) {
                    ++$brightButtonPixels
                }
            }
        }
        $buttonPixelCount = ($buttonRight - $buttonLeft) * ($buttonBottom - $buttonTop)
        if ($darkButtonPixels -lt ($buttonPixelCount * 0.70)) {
            throw "Launcher start-button region did not contain the expected dark fill: $darkButtonPixels of $buttonPixelCount pixels"
        }
        if ($brightButtonPixels -lt 40) {
            throw "Launcher start-button region did not contain readable text pixels: $brightButtonPixels"
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

$source = (Resolve-Path -LiteralPath $Launcher).Path
$temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) (
    'arpg-launcher-smoke-' + [guid]::NewGuid().ToString('N'))
$process = $null

try {
    [System.IO.Directory]::CreateDirectory($temporaryDirectory) | Out-Null
    $copiedLauncher = Join-Path $temporaryDirectory ([System.IO.Path]::GetFileName($source))
    Copy-Item -LiteralPath $source -Destination $copiedLauncher

    $subsystem = Get-PeSubsystem -Path $copiedLauncher
    if ($subsystem -ne 2) {
        throw "Expected Windows GUI subsystem 2, found $subsystem"
    }

    $process = Start-Process -FilePath $copiedLauncher -WorkingDirectory $temporaryDirectory -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        Start-Sleep -Milliseconds 50
        $process.Refresh()
    } while (-not $process.HasExited -and
        $process.MainWindowHandle -eq [IntPtr]::Zero -and
        [DateTime]::UtcNow -lt $deadline)

    if ($process.HasExited) {
        throw "Launcher exited before presenting a main window"
    }
    if ($process.MainWindowHandle -eq [IntPtr]::Zero) {
        throw "Launcher did not present a main window within five seconds"
    }
    if ($process.MainWindowTitle -ne '无限地下城启动器') {
        throw "Unexpected launcher title: '$($process.MainWindowTitle)'"
    }
    $perMonitorV2 = [IntPtr](-4)
    $previousDpiContext = [LauncherPixelSmokeNative]::SetThreadDpiAwarenessContext($perMonitorV2)
    if ($previousDpiContext -eq [IntPtr]::Zero) {
        throw "Could not enable Per-Monitor-V2 pixel capture: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    try {
        Assert-RenderedClientPixels -Process $process
    }
    finally {
        [void][LauncherPixelSmokeNative]::SetThreadDpiAwarenessContext($previousDpiContext)
    }
}
finally {
    if ($null -ne $process -and -not $process.HasExited) {
        [void] $process.CloseMainWindow()
        if (-not $process.WaitForExit(2000)) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        }
    }
    if (Test-Path -LiteralPath $temporaryDirectory) {
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
}
