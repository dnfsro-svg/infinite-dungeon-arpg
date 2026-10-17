param(
    [Parameter(Mandatory = $true)][string] $Launcher,
    [Parameter(Mandatory = $true)][string] $Output,
    [Parameter(Mandatory = $true)][ValidateSet('ready', 'missing-assets')][string] $State
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class Task2LauncherCaptureNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }

    [DllImport("dwmapi.dll")] public static extern int DwmFlush();
    [DllImport("user32.dll", SetLastError=true)] [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetClientRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll", SetLastError=true)] [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll", SetLastError=true)] [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ClientToScreen(IntPtr hwnd, ref POINT point);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT point);
    [DllImport("user32.dll", SetLastError=true)] [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll", SetLastError=true)] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr value);
}
'@

$launcherPath = (Resolve-Path -LiteralPath $Launcher).Path
$outputPath = [IO.Path]::GetFullPath($Output)
$outputParent = Split-Path -Parent $outputPath
if (-not (Test-Path -LiteralPath $outputParent -PathType Container)) {
    throw "Output parent does not exist: $outputParent"
}

$process = $null
$bitmap = $null
$previousContext = [IntPtr]::Zero
try {
    $process = Start-Process -FilePath $launcherPath -WorkingDirectory (Split-Path -Parent $launcherPath) -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    do {
        Start-Sleep -Milliseconds 50
        $process.Refresh()
    } while (-not $process.HasExited -and $process.MainWindowHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
    if ($process.HasExited -or $process.MainWindowHandle -eq [IntPtr]::Zero) {
        throw "Launcher PID $($process.Id) did not present a window"
    }
    if ($process.MainWindowTitle -ne '无限地下城启动器') {
        throw "Unexpected title for PID $($process.Id): $($process.MainWindowTitle)"
    }

    $hwnd = $process.MainWindowHandle
    [uint32] $ownerPid = 0
    [void][Task2LauncherCaptureNative]::GetWindowThreadProcessId($hwnd, [ref]$ownerPid)
    if ($ownerPid -ne $process.Id) { throw "HWND owner $ownerPid did not match launcher PID $($process.Id)" }

    $previousContext = [Task2LauncherCaptureNative]::SetThreadDpiAwarenessContext([IntPtr](-4))
    if ($previousContext -eq [IntPtr]::Zero) { throw 'Could not enter Per-Monitor-V2 capture context' }
    $window = [Task2LauncherCaptureNative+RECT]::new()
    if (-not [Task2LauncherCaptureNative]::GetWindowRect($hwnd, [ref]$window)) {
        throw 'Could not read initial launcher geometry'
    }
    $windowWidth = $window.Right - $window.Left
    $windowHeight = $window.Bottom - $window.Top
    if (-not [Task2LauncherCaptureNative]::SetWindowPos($hwnd, [IntPtr](-1), 40, 40, $windowWidth, $windowHeight, 0x0041)) {
        throw 'Could not place launcher at the VDD capture origin'
    }
    [void][Task2LauncherCaptureNative]::SetForegroundWindow($hwnd)
    $geometryDeadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        Start-Sleep -Milliseconds 100
        $dpi = [Task2LauncherCaptureNative]::GetDpiForWindow($hwnd)
        $client = [Task2LauncherCaptureNative+RECT]::new()
        if (-not [Task2LauncherCaptureNative]::GetClientRect($hwnd, [ref]$client)) {
            throw 'Could not read launcher client geometry'
        }
        $width = $client.Right - $client.Left
        $height = $client.Bottom - $client.Top
    } while (($dpi -ne 192 -or $width -ne 1840 -or $height -ne 1120) -and [DateTime]::UtcNow -lt $geometryDeadline)
    if ($dpi -ne 192) { throw "GetDpiForWindow=$dpi after VDD placement, expected 192" }
    if ($width -ne 1840 -or $height -ne 1120) { throw "GetClientRect=${width}x${height} after VDD placement, expected 1840x1120" }

    Start-Sleep -Milliseconds 250
    if ([Task2LauncherCaptureNative]::DwmFlush() -ne 0) { throw 'DwmFlush failed' }

    $origin = [Task2LauncherCaptureNative+POINT]@{ X = 0; Y = 0 }
    if (-not [Task2LauncherCaptureNative]::ClientToScreen($hwnd, [ref]$origin)) { throw 'ClientToScreen failed' }
    $points = @(
        @(4, 4), @(($width - 5), 4), @(4, ($height - 5)), @(($width - 5), ($height - 5)),
        @([int]($width * 0.50), [int]($height * 0.22)),
        @([int]($width * 0.50), [int]($height * 0.54)),
        @([int]($width * 0.50), [int]($height * 0.88))
    )
    foreach ($point in $points) {
        $screenPoint = [Task2LauncherCaptureNative+POINT]@{ X = $origin.X + $point[0]; Y = $origin.Y + $point[1] }
        [uint32] $pointPid = 0
        [void][Task2LauncherCaptureNative]::GetWindowThreadProcessId(
            [Task2LauncherCaptureNative]::WindowFromPoint($screenPoint), [ref]$pointPid)
        if ($pointPid -ne $process.Id) { throw "Client capture point was owned by PID $pointPid instead of $($process.Id)" }
    }

    $bitmap = [System.Drawing.Bitmap]::new($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try { $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0, $bitmap.Size, [System.Drawing.CopyPixelOperation]::SourceCopy) }
    finally { $graphics.Dispose() }

    $dark = 0; $white = 0; $pureGreen = 0; $pureRed = 0; $samples = 0
    for ($y = 0; $y -lt $height; $y += 4) {
        for ($x = 0; $x -lt $width; $x += 4) {
            $pixel = $bitmap.GetPixel($x, $y); ++$samples
            if ((($pixel.R * 30) + ($pixel.G * 59) + ($pixel.B * 11)) -lt 24000) { ++$dark }
            if ($pixel.R -ge 248 -and $pixel.G -ge 248 -and $pixel.B -ge 248) { ++$white }
            if ($pixel.R -eq 0 -and $pixel.G -eq 255 -and $pixel.B -eq 0) { ++$pureGreen }
            if ($pixel.R -eq 255 -and $pixel.G -eq 0 -and $pixel.B -eq 0) { ++$pureRed }
        }
    }
    if ($dark -lt [int]($samples * 0.70)) { throw "Client was not predominantly dark: $dark/$samples" }
    if ($State -eq 'ready' -and $pureGreen -lt 5) { throw "Ready capture lacked pure green status pixels: $pureGreen" }
    if ($State -eq 'missing-assets' -and $pureRed -lt 5) { throw "Missing capture lacked pure red status pixels: $pureRed" }

    $bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputPath).Hash
    [pscustomobject]@{
        State = $State; PID = $process.Id; HWND = ('0x{0:X}' -f $hwnd.ToInt64())
        Dpi = $dpi; ClientWidth = $width; ClientHeight = $height
        ClientOrigin = "$($origin.X),$($origin.Y)"; DarkSamples = $dark
        WhiteSamples = $white; PureGreen = $pureGreen; PureRed = $pureRed
        Output = $outputPath; Sha256 = $hash
    } | Format-List
}
finally {
    if ($previousContext -ne [IntPtr]::Zero) {
        [void][Task2LauncherCaptureNative]::SetThreadDpiAwarenessContext($previousContext)
    }
    if ($null -ne $bitmap) { $bitmap.Dispose() }
    if ($null -ne $process -and -not $process.HasExited) {
        [void]$process.CloseMainWindow()
        if (-not $process.WaitForExit(2500)) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
    }
}
