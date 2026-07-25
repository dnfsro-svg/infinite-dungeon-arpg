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

    [DllImport("user32.dll")]
    public static extern uint GetDpiForWindow(IntPtr window);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetWindowRect(IntPtr window, out RECT rectangle);

    [DllImport("user32.dll")]
    public static extern IntPtr MonitorFromWindow(IntPtr window, uint flags);

    [StructLayout(LayoutKind.Sequential)]
    public struct MONITORINFO {
        public uint Size;
        public RECT Monitor;
        public RECT Work;
        public uint Flags;
    }

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetMonitorInfo(IntPtr monitor, ref MONITORINFO monitorInfo);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetCursorPos(out POINT point);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool RedrawWindow(IntPtr window, IntPtr rectangle, IntPtr region, uint flags);

    [StructLayout(LayoutKind.Sequential)]
    public struct JOBOBJECT_BASIC_LIMIT_INFORMATION {
        public long PerProcessUserTimeLimit;
        public long PerJobUserTimeLimit;
        public uint LimitFlags;
        public UIntPtr MinimumWorkingSetSize;
        public UIntPtr MaximumWorkingSetSize;
        public uint ActiveProcessLimit;
        public UIntPtr Affinity;
        public uint PriorityClass;
        public uint SchedulingClass;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct IO_COUNTERS {
        public ulong ReadOperationCount;
        public ulong WriteOperationCount;
        public ulong OtherOperationCount;
        public ulong ReadTransferCount;
        public ulong WriteTransferCount;
        public ulong OtherTransferCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION {
        public JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
        public IO_COUNTERS IoInfo;
        public UIntPtr ProcessMemoryLimit;
        public UIntPtr JobMemoryLimit;
        public UIntPtr PeakProcessMemoryUsed;
        public UIntPtr PeakJobMemoryUsed;
    }

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr CreateJobObject(IntPtr jobAttributes, string name);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetInformationJobObject(
        IntPtr job,
        int jobObjectInfoClass,
        ref JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobObjectInfo,
        uint jobObjectInfoLength);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool CloseHandle(IntPtr handle);

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

function New-LauncherSmokeJob {
    $job = [LauncherPixelSmokeNative]::CreateJobObject([IntPtr]::Zero, $null)
    if ($job -eq [IntPtr]::Zero) {
        throw "Could not create launcher smoke Job Object: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }

    $limits = [LauncherPixelSmokeNative+JOBOBJECT_EXTENDED_LIMIT_INFORMATION]::new()
    $limits.BasicLimitInformation.LimitFlags = 0x00002000 # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
    $limitSize = [uint32][Runtime.InteropServices.Marshal]::SizeOf($limits)
    if (-not [LauncherPixelSmokeNative]::SetInformationJobObject($job, 9, [ref] $limits, $limitSize)) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        [void][LauncherPixelSmokeNative]::CloseHandle($job)
        throw "Could not configure launcher smoke Job Object: $errorCode"
    }
    return $job
}

function Convert-DipToPixels {
    param([float] $Dip, [uint32] $Dpi)
    return [int][math]::Floor((($Dip * $Dpi) + 95.0) / 96.0)
}

function Assert-ColorNear {
    param($Pixel, [int] $Red, [int] $Green, [int] $Blue, [string] $Name)
    if ([math]::Abs($Pixel.R - $Red) -gt 6 -or
        [math]::Abs($Pixel.G - $Green) -gt 6 -or
        [math]::Abs($Pixel.B - $Blue) -gt 6) {
        throw "$Name expected RGB($Red,$Green,$Blue), found RGB($($Pixel.R),$($Pixel.G),$($Pixel.B))"
    }
}

function Assert-LauncherOwnsPoints {
    param($Process, $Origin, [int] $Width, [int] $Height)
    $points = @(
        @{ X = $Origin.X + 4; Y = $Origin.Y + 4 },
        @{ X = $Origin.X + $Width - 5; Y = $Origin.Y + 4 },
        @{ X = $Origin.X + 4; Y = $Origin.Y + $Height - 5 },
        @{ X = $Origin.X + $Width - 5; Y = $Origin.Y + $Height - 5 },
        @{ X = [int]($Origin.X + ($Width * 0.50)); Y = [int]($Origin.Y + ($Height * 0.54)) },
        @{ X = [int]($Origin.X + ($Width * 0.50)); Y = [int]($Origin.Y + ($Height * 0.88)) }
    )
    foreach ($point in $points) {
        $screenPoint = [LauncherPixelSmokeNative+POINT]@{ X = $point.X; Y = $point.Y }
        [uint32] $ownerProcessId = 0
        [void][LauncherPixelSmokeNative]::GetWindowThreadProcessId(
            [LauncherPixelSmokeNative]::WindowFromPoint($screenPoint),
            [ref] $ownerProcessId)
        if ($ownerProcessId -ne $Process.Id) {
            throw "Launcher client was occluded by process $ownerProcessId during pixel capture"
        }
    }
}

function Get-VerifiedClientBitmap {
    param($Process, $Origin, [int] $Width, [int] $Height, [uint32] $ReadinessDpi = 0)
    $lastFailure = 'no client bitmap was captured'
    $lastReadiness = $null
    for ($attempt = 1; $attempt -le 3; ++$attempt) {
        $bitmap = $null
        try {
            if ([LauncherPixelSmokeNative]::DwmFlush() -ne 0) {
                throw 'DwmFlush failed before launcher pixel capture'
            }
            Assert-LauncherOwnsPoints $Process $Origin $Width $Height
            $bitmap = [System.Drawing.Bitmap]::new($Width, $Height)
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.CopyFromScreen($Origin.X, $Origin.Y, 0, 0, $bitmap.Size)
            }
            finally {
                $graphics.Dispose()
            }
            Assert-LauncherOwnsPoints $Process $Origin $Width $Height
            if ($ReadinessDpi -eq 0) { return $bitmap }
            $lastReadiness = Get-InitialPixelReadiness $bitmap $ReadinessDpi
            if ($lastReadiness.Ready) { return $bitmap }
            $lastFailure = 'launcher key pixels were not ready'
        }
        catch {
            $lastFailure = $_.Exception.Message
        }

        if ($null -ne $bitmap) { $bitmap.Dispose() }
        if ($attempt -lt 3) {
            Start-Sleep -Milliseconds 80
        }
    }

    if ($null -ne $lastReadiness) {
        $start = $lastReadiness.StartPixel
        $verify = $lastReadiness.VerifyPixel
        throw "Launcher client was not ready after 3 captures ($lastFailure): dark=$($lastReadiness.DarkPixels)/$($lastReadiness.SampleCount), disabledStart=RGB($($start.R),$($start.G),$($start.B)), verify=RGB($($verify.R),$($verify.G),$($verify.B)), pureRed=$($lastReadiness.PureRed)"
    }
    throw "Launcher client was not ready after 3 captures: $lastFailure; pixel diagnostics unavailable"
}

function Get-InitialPixelReadiness {
    param($Bitmap, [uint32] $Dpi)

    $darkPixels = 0
    for ($y = 0; $y -lt $Bitmap.Height; $y += 4) {
        for ($x = 0; $x -lt $Bitmap.Width; $x += 4) {
            $pixel = $Bitmap.GetPixel($x, $y)
            if ((($pixel.R * 30) + ($pixel.G * 59) + ($pixel.B * 11)) -lt 24000) { ++$darkPixels }
        }
    }
    $sampleCount = [math]::Ceiling($Bitmap.Width / 4) * [math]::Ceiling($Bitmap.Height / 4)
    $scale = $Dpi / 96.0
    $startX = Convert-DipToPixels 58 $Dpi
    $startY = Convert-DipToPixels 260 $Dpi
    $verifyY = Convert-DipToPixels 370 $Dpi
    $startPixel = $Bitmap.GetPixel($startX + [int](20 * $scale), $startY + [int](20 * $scale))
    $verifyPixel = $Bitmap.GetPixel($startX + [int](20 * $scale), $verifyY + [int](20 * $scale))
    $pureRed = 0
    for ($y = (Convert-DipToPixels 174 $Dpi); $y -lt (Convert-DipToPixels 226 $Dpi); ++$y) {
        for ($x = (Convert-DipToPixels 58 $Dpi); $x -lt (Convert-DipToPixels 862 $Dpi); ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            if ($pixel.R -ge 220 -and $pixel.G -le 20 -and $pixel.B -le 20) { ++$pureRed }
        }
    }
    [pscustomobject]@{
        Ready = ($darkPixels -ge ($sampleCount * 0.55)) -and
            ([math]::Abs($startPixel.R - 85) -le 6) -and ([math]::Abs($startPixel.G - 91) -le 6) -and ([math]::Abs($startPixel.B - 99) -le 6) -and
            ([math]::Abs($verifyPixel.R - 36) -le 6) -and ([math]::Abs($verifyPixel.G - 69) -le 6) -and ([math]::Abs($verifyPixel.B - 94) -le 6) -and
            ($pureRed -ge 8)
        DarkPixels = $darkPixels
        SampleCount = $sampleCount
        StartPixel = $startPixel
        VerifyPixel = $verifyPixel
        PureRed = $pureRed
    }
}

function Send-LauncherMouse {
    param($Process, $Origin, [uint32] $Message, [uint32] $Buttons, [int] $X, [int] $Y)
    if (-not [LauncherPixelSmokeNative]::SetCursorPos($Origin.X + $X, $Origin.Y + $Y)) {
        throw 'Could not move cursor into launcher client'
    }
    $packed = [IntPtr](($Y -shl 16) -bor ($X -band 0xffff))
    [void][LauncherPixelSmokeNative]::SendMessage(
        $Process.MainWindowHandle, $Message, [IntPtr]$Buttons, $packed)
    if (-not [LauncherPixelSmokeNative]::RedrawWindow(
        $Process.MainWindowHandle, [IntPtr]::Zero, [IntPtr]::Zero, 0x0101)) {
        throw "Could not redraw launcher after mouse message: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    Start-Sleep -Milliseconds 80
}

function Assert-RenderedClientPixels {
    param([Parameter(Mandatory = $true)][System.Diagnostics.Process] $Process)

    $MONITOR_DEFAULTTONEAREST = 2
    $SWP_NOSIZE = 0x0001
    $SWP_SHOWWINDOW = 0x0040
    $HWND_TOP = [IntPtr]::Zero
    $dpi = [LauncherPixelSmokeNative]::GetDpiForWindow($Process.MainWindowHandle)
    if ($dpi -eq 0) { throw 'GetDpiForWindow returned zero' }
    $monitor = [LauncherPixelSmokeNative]::MonitorFromWindow($Process.MainWindowHandle, $MONITOR_DEFAULTTONEAREST)
    $monitorInfo = [LauncherPixelSmokeNative+MONITORINFO]::new()
    $monitorInfo.Size = [Runtime.InteropServices.Marshal]::SizeOf($monitorInfo)
    if ($monitor -eq [IntPtr]::Zero -or -not [LauncherPixelSmokeNative]::GetMonitorInfo($monitor, [ref] $monitorInfo)) {
        throw "Could not read launcher monitor work area: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    $expectedWidth = Convert-DipToPixels 920 $dpi
    $expectedHeight = Convert-DipToPixels 560 $dpi
    $existingWindow = [LauncherPixelSmokeNative+RECT]::new()
    if (-not [LauncherPixelSmokeNative]::GetWindowRect($Process.MainWindowHandle, [ref] $existingWindow)) {
        throw "Could not read launcher window size: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    $windowWidth = $existingWindow.Right - $existingWindow.Left
    $windowHeight = $existingWindow.Bottom - $existingWindow.Top
    $targetX = [math]::Max($monitorInfo.Work.Left, $monitorInfo.Work.Right - $windowWidth - 16)
    $targetY = [math]::Max($monitorInfo.Work.Top, $monitorInfo.Work.Bottom - $windowHeight - 16)
    if (-not [LauncherPixelSmokeNative]::SetWindowPos(
        $Process.MainWindowHandle, $HWND_TOP, $targetX, $targetY, 0, 0,
        $SWP_NOSIZE -bor $SWP_SHOWWINDOW)) {
        throw "Could not place launcher for pixel capture: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    Start-Sleep -Milliseconds 80

    $client = [LauncherPixelSmokeNative+RECT]::new()
    $window = [LauncherPixelSmokeNative+RECT]::new()
    if (-not [LauncherPixelSmokeNative]::GetClientRect($Process.MainWindowHandle, [ref] $client) -or
        -not [LauncherPixelSmokeNative]::GetWindowRect($Process.MainWindowHandle, [ref] $window)) {
        throw "Could not read launcher geometry: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    $width = $client.Right - $client.Left
    $height = $client.Bottom - $client.Top
    if ($width -ne $expectedWidth -or $height -ne $expectedHeight) {
        throw "Unexpected launcher client size: ${width}x${height}, expected ${expectedWidth}x${expectedHeight} at $dpi DPI"
    }
    if ($window.Left -lt $monitorInfo.Work.Left -or $window.Top -lt $monitorInfo.Work.Top -or
        $window.Right -gt $monitorInfo.Work.Right -or $window.Bottom -gt $monitorInfo.Work.Bottom) {
        throw 'Launcher window did not fit within one monitor work area'
    }

    $origin = [LauncherPixelSmokeNative+POINT]@{ X = 0; Y = 0 }
    if (-not [LauncherPixelSmokeNative]::ClientToScreen($Process.MainWindowHandle, [ref] $origin)) {
        throw "Could not resolve launcher client origin: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    $scale = $dpi / 96.0
    $bitmap = Get-VerifiedClientBitmap $Process $origin $width $height $dpi
    try {
        $startX = Convert-DipToPixels 58 $dpi
        $startY = Convert-DipToPixels 260 $dpi
        $verifyY = Convert-DipToPixels 370 $dpi

        $brightXs = @()
        for ($y = $startY; $y -lt ($startY + (Convert-DipToPixels 82 $dpi)); ++$y) {
            for ($x = $startX; $x -lt (Convert-DipToPixels 862 $dpi); ++$x) {
                $pixel = $bitmap.GetPixel($x, $y)
                if ($pixel.R -ge 210 -and $pixel.G -ge 210 -and $pixel.B -ge 210) { $brightXs += $x }
            }
        }
        if ($brightXs.Count -lt 40 -or (($brightXs | Measure-Object -Maximum).Maximum - ($brightXs | Measure-Object -Minimum).Minimum) -lt (60 * $scale)) {
            throw 'Launcher Start text was not high-contrast and spatially distributed'
        }
    }
    finally {
        $bitmap.Dispose()
    }

    $verifyX = Convert-DipToPixels 58 $dpi
    Send-LauncherMouse $Process $origin 0x0200 0 ($verifyX + [int](125 * $scale)) ($verifyY + [int](28 * $scale))
    $hover = Get-VerifiedClientBitmap $Process $origin $width $height
    try { Assert-ColorNear ($hover.GetPixel($verifyX + 2, $verifyY + 2)) 0 217 255 'Hovered Verify border' }
    finally { $hover.Dispose() }

    Send-LauncherMouse $Process $origin 0x0201 1 ($verifyX + [int](125 * $scale)) ($verifyY + [int](28 * $scale))
    $pressed = Get-VerifiedClientBitmap $Process $origin $width $height
    try { Assert-ColorNear ($pressed.GetPixel($verifyX + [int](20 * $scale), $verifyY + [int](20 * $scale))) 21 54 79 'Pressed Verify fill' }
    finally { $pressed.Dispose() }
    Send-LauncherMouse $Process $origin 0x0202 0 ($verifyX + [int](125 * $scale)) ($verifyY + [int](28 * $scale))

    $pathX = Convert-DipToPixels 58 $dpi
    $pathY = Convert-DipToPixels 474 $dpi
    Send-LauncherMouse $Process $origin 0x0200 0 ($pathX + [int](30 * $scale)) ($pathY + [int](20 * $scale))
    $tooltip = Get-VerifiedClientBitmap $Process $origin $width $height
    try { Assert-ColorNear ($tooltip.GetPixel((Convert-DipToPixels 800 $dpi), (Convert-DipToPixels 445 $dpi))) 10 28 43 'Full-path tooltip card' }
    finally { $tooltip.Dispose() }
}

$source = (Resolve-Path -LiteralPath $Launcher).Path
$temporaryDirectoryPrefix = 'arpg-launcher-window-smoke-'
$temporaryRoot = [System.IO.Path]::GetTempPath()
Get-ChildItem -LiteralPath $temporaryRoot -Directory -Filter "${temporaryDirectoryPrefix}*" | ForEach-Object {
    Remove-Item -LiteralPath $_.FullName -Recurse -Force
}
$temporaryDirectory = Join-Path $temporaryRoot (
    $temporaryDirectoryPrefix + [guid]::NewGuid().ToString('N'))
$process = $null
$jobHandle = [IntPtr]::Zero
$originalCursor = [LauncherPixelSmokeNative+POINT]::new()
[void][LauncherPixelSmokeNative]::GetCursorPos([ref] $originalCursor)

try {
    [System.IO.Directory]::CreateDirectory($temporaryDirectory) | Out-Null
    $copiedLauncher = Join-Path $temporaryDirectory ([System.IO.Path]::GetFileName($source))
    Copy-Item -LiteralPath $source -Destination $copiedLauncher
    [System.IO.File]::WriteAllBytes((Join-Path $temporaryDirectory 'arpg_game.exe'), [byte[]]@())
    foreach ($relativeDirectory in @(
        'assets/fonts', 'assets/player', 'assets/skills', 'assets/stage12',
        'assets/stage14/audio', 'assets/stage15/audio')) {
        [System.IO.Directory]::CreateDirectory((Join-Path $temporaryDirectory $relativeDirectory)) | Out-Null
    }
    Remove-Item -LiteralPath (Join-Path $temporaryDirectory 'assets/skills') -Recurse -Force

    $subsystem = Get-PeSubsystem -Path $copiedLauncher
    if ($subsystem -ne 2) {
        throw "Expected Windows GUI subsystem 2, found $subsystem"
    }

    $jobHandle = New-LauncherSmokeJob
    $process = Start-Process -FilePath $copiedLauncher -WorkingDirectory $temporaryDirectory -PassThru
    if (-not [LauncherPixelSmokeNative]::AssignProcessToJobObject($jobHandle, $process.Handle)) {
        throw "Could not assign launcher process $($process.Id) to smoke Job Object: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
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
    $closeJobError = $null
    try {
        [void][LauncherPixelSmokeNative]::SetCursorPos($originalCursor.X, $originalCursor.Y)
        if ($null -ne $process -and -not $process.HasExited) {
            [void] $process.CloseMainWindow()
            if (-not $process.WaitForExit(2000)) {
                Stop-Process -Id $process.Id -Force
                $process.WaitForExit()
            }
        }
    }
    finally {
        try {
            if ($jobHandle -ne [IntPtr]::Zero) {
                if (-not [LauncherPixelSmokeNative]::CloseHandle($jobHandle)) {
                    $closeJobError = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
                }
                $jobHandle = [IntPtr]::Zero
            }
        }
        finally {
            if (Test-Path -LiteralPath $temporaryDirectory) {
                Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
            }
        }
    }
    if ($null -ne $closeJobError) {
        throw "Could not close launcher smoke Job Object: $closeJobError"
    }
}
