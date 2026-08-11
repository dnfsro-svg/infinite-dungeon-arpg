param(
    [string] $Launcher,
    [switch] $SelfTestStaleCleanup,
    [switch] $SelfTestChineseCopyGuard
)

$ErrorActionPreference = 'Stop'

function Assert-Task8SourcePattern {
    param(
        [Parameter(Mandatory = $true)][string] $Source,
        [Parameter(Mandatory = $true)][string] $Pattern,
        [Parameter(Mandatory = $true)][string] $Contract
    )

    if (-not [regex]::IsMatch(
            $Source,
            $Pattern,
            [Text.RegularExpressions.RegexOptions]::Singleline -bor
                [Text.RegularExpressions.RegexOptions]::CultureInvariant)) {
        throw "Task 8 Chinese-copy guard failed: $Contract"
    }
}

function Test-Task8CppCharacterLiteralStart {
    param(
        [Parameter(Mandatory = $true)][string] $Source,
        [Parameter(Mandatory = $true)][int] $Index
    )

    if ($Source[$Index] -ne "'") { return $false }
    if ($Index -eq 0 -or $Index + 1 -ge $Source.Length -or
        -not [char]::IsLetterOrDigit($Source[$Index - 1]) -or
        -not [char]::IsLetterOrDigit($Source[$Index + 1])) {
        return $true
    }

    $tokenStart = $Index - 1
    while ($tokenStart -gt 0 -and
        ([char]::IsLetterOrDigit($Source[$tokenStart - 1]) -or
            $Source[$tokenStart - 1] -in @('_', '.'))) {
        --$tokenStart
    }
    $numericToken = [char]::IsDigit($Source[$tokenStart]) -or
        ($Source[$tokenStart] -eq '.' -and
            $tokenStart + 1 -lt $Source.Length -and
            [char]::IsDigit($Source[$tokenStart + 1]))
    return -not $numericToken
}

function Remove-Task8CppComments {
    param([Parameter(Mandatory = $true)][string] $Source)

    $result = [System.Text.StringBuilder]::new($Source.Length)
    $state = 'code'
    $index = 0
    while ($index -lt $Source.Length) {
        $character = $Source[$index]
        $next = if ($index + 1 -lt $Source.Length) { $Source[$index + 1] } else { [char]0 }

        if ($state -eq 'code') {
            if ($character -eq '/' -and $next -eq '/') {
                [void] $result.Append('  ')
                $state = 'line_comment'
                $index += 2
                continue
            }
            if ($character -eq '/' -and $next -eq '*') {
                [void] $result.Append('  ')
                $state = 'block_comment'
                $index += 2
                continue
            }
            if ($character -eq '"') { $state = 'string' }
            elseif ($character -eq "'" -and
                (Test-Task8CppCharacterLiteralStart $Source $index)) {
                $state = 'character'
            }
            [void] $result.Append($character)
            ++$index
            continue
        }

        if ($state -eq 'string' -or $state -eq 'character') {
            [void] $result.Append($character)
            if ($character -eq '\' -and $index + 1 -lt $Source.Length) {
                [void] $result.Append($Source[$index + 1])
                $index += 2
                continue
            }
            if (($state -eq 'string' -and $character -eq '"') -or
                ($state -eq 'character' -and $character -eq "'")) {
                $state = 'code'
            }
            ++$index
            continue
        }

        if ($state -eq 'line_comment') {
            if ($character -eq '\' -and $next -eq "`r" -and
                $index + 2 -lt $Source.Length -and $Source[$index + 2] -eq "`n") {
                [void] $result.Append('   ')
                $index += 3
                continue
            }
            if ($character -eq '\' -and $next -eq "`n") {
                [void] $result.Append('  ')
                $index += 2
                continue
            }
            if ($character -eq "`r" -or $character -eq "`n") {
                [void] $result.Append($character)
                $state = 'code'
            }
            else {
                [void] $result.Append(' ')
            }
            ++$index
            continue
        }

        if ($character -eq '*' -and $next -eq '/') {
            [void] $result.Append('  ')
            $state = 'code'
            $index += 2
            continue
        }
        if ($character -eq "`r" -or $character -eq "`n") {
            [void] $result.Append($character)
        }
        else {
            [void] $result.Append(' ')
        }
        ++$index
    }

    if ($state -eq 'block_comment' -or $state -eq 'string' -or $state -eq 'character') {
        throw "Task 8 lexical audit found unterminated C++ $state"
    }
    return $result.ToString()
}

function Assert-Task8ActiveSource {
    param(
        [Parameter(Mandatory = $true)][string] $Source,
        [Parameter(Mandatory = $true)][string] $Name
    )

    if ([regex]::IsMatch(
            $Source,
            '(?m)^[ \t]*#[ \t]*(?:if|ifdef|ifndef|elif|else|endif|define|undef)\b')) {
        throw "Task 8 audited player-copy source cannot use conditional or macro preprocessing: $Name"
    }
    if ([regex]::IsMatch($Source, '(?<![A-Za-z0-9_])(?:u8|u|U|L)?R"')) {
        throw "Task 8 lexical audit does not allow raw string literals: $Name"
    }
    if ([regex]::IsMatch(
            $Source,
            '\b(?:if|while)\s*(?:constexpr\s*)?\(\s*(?:false|0)\s*\)',
            [Text.RegularExpressions.RegexOptions]::CultureInvariant) -or
        [regex]::IsMatch(
            $Source,
            '\bif\s*(?:constexpr\s*)?\([^\)]*(?:&&\s*(?:false|0)|(?:false|0)\s*&&)[^\)]*\)',
            [Text.RegularExpressions.RegexOptions]::CultureInvariant)) {
        throw "Task 8 audited player-copy source contains a constant-dead branch: $Name"
    }
}

function Get-Task8CppStringLiterals {
    param([Parameter(Mandatory = $true)][string] $Source)

    $literals = [System.Collections.Generic.List[string]]::new()
    $index = 0
    while ($index -lt $Source.Length) {
        if ($Source[$index] -eq "'" -and
            (Test-Task8CppCharacterLiteralStart $Source $index)) {
            ++$index
            while ($index -lt $Source.Length) {
                if ($Source[$index] -eq '\' -and $index + 1 -lt $Source.Length) {
                    $index += 2
                    continue
                }
                $closed = $Source[$index] -eq "'"
                ++$index
                if ($closed) { break }
            }
            continue
        }
        if ($Source[$index] -ne '"') {
            ++$index
            continue
        }

        $prefix = ''
        $prefixStart = $index
        if ($index -ge 2 -and $Source.Substring($index - 2, 2) -ceq 'u8' -and
            ($index -eq 2 -or
                (-not [char]::IsLetterOrDigit($Source[$index - 3]) -and
                    $Source[$index - 3] -ne '_'))) {
            $prefix = 'u8'
            $prefixStart = $index - 2
        }
        elseif ($index -ge 1 -and $Source[$index - 1] -in @('L', 'u', 'U') -and
            ($index -eq 1 -or
                (-not [char]::IsLetterOrDigit($Source[$index - 2]) -and
                    $Source[$index - 2] -ne '_'))) {
            $prefix = [string] $Source[$index - 1]
            $prefixStart = $index - 1
        }

        $end = $index + 1
        while ($end -lt $Source.Length) {
            if ($Source[$end] -eq '\' -and $end + 1 -lt $Source.Length) {
                $end += 2
                continue
            }
            if ($Source[$end] -eq '"') { break }
            ++$end
        }
        if ($end -ge $Source.Length) {
            throw 'Task 8 lexical audit found an unterminated C++ string literal'
        }
        $literals.Add($Source.Substring($prefixStart, $end - $prefixStart + 1))
        $index = $end + 1
    }
    return $literals
}

function Get-Task8ChineseCopySources {
    $sourceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    return [ordered]@{
        Host = [System.IO.File]::ReadAllText(
            (Join-Path $sourceRoot 'src\platform\raylib\raylib_host.hpp'),
            [System.Text.Encoding]::UTF8)
        Main = [System.IO.File]::ReadAllText(
            (Join-Path $sourceRoot 'src\launcher\launcher_main.cpp'),
            [System.Text.Encoding]::UTF8)
        Renderer = [System.IO.File]::ReadAllText(
            (Join-Path $sourceRoot 'src\launcher\launcher_renderer.cpp'),
            [System.Text.Encoding]::UTF8)
        Core = [System.IO.File]::ReadAllText(
            (Join-Path $sourceRoot 'src\launcher\launcher_core.cpp'),
            [System.Text.Encoding]::UTF8)
        Platform = [System.IO.File]::ReadAllText(
            (Join-Path $sourceRoot 'src\launcher\launcher_platform_win32.cpp'),
            [System.Text.Encoding]::UTF8)
    }
}

function Assert-Task8ChineseCopyGuard {
    param(
        [Parameter(Mandatory = $true)] $Sources,
        [string[]] $WholeFileAllowlist = @()
    )

    if ($WholeFileAllowlist.Count -ne 0) {
        throw 'Task 8 forbids whole-file English allowlists'
    }

    $activeSources = [ordered]@{}
    foreach ($name in $Sources.Keys) {
        $commentFreeSource = Remove-Task8CppComments $Sources[$name]
        Assert-Task8ActiveSource $commentFreeSource $name
        $activeSources[$name] = $commentFreeSource
    }
    $Sources = $activeSources

    Assert-Task8SourcePattern $Sources.Host 'const\s+char\*\s+window_title\{u8"无限地城"\};' 'untouched RaylibHostConfig must default to exact UTF-8 无限地城'
    Assert-Task8SourcePattern $Sources.Main 'constexpr\s+wchar_t\s+kWindowTitle\[\]\s*=\s*L"无限地下城启动器";' 'launcher window title must remain exact 无限地下城启动器'
    Assert-Task8SourcePattern $Sources.Main 'constexpr\s+wchar_t\s+kWindowClassName\[\]\s*=\s*L"InfiniteDungeonLauncherWindow";' 'internal Win32 class identity is the only allowed launcher class token'
    Assert-Task8SourcePattern $Sources.Main 'GetEnvironmentVariableW\(\s*L"LOCALAPPDATA"' 'LOCALAPPDATA is allowed only as the exact environment-variable lookup token'
    Assert-Task8SourcePattern $Sources.Main 'CreateFontW\([^;]+L"Microsoft YaHei UI"\s*\)' 'native fallback font-family token must stay in the font API call'
    Assert-Task8SourcePattern $Sources.Renderer 'constexpr\s+wchar_t\s+kFontFamily\[\]\s*=\s*L"Microsoft YaHei UI";' 'DirectWrite font-family token must stay an internal identifier'
    Assert-Task8SourcePattern $Sources.Renderer 'CreateTextFormat\([^;]+L"zh-CN"' 'zh-CN is allowed only as the DirectWrite locale identifier'

    $buttons = @(
        @{ Layout = 'start'; Label = '开始游戏'; Button = 'start' },
        @{ Layout = 'verify'; Label = '检查游戏文件'; Button = 'verify' },
        @{ Layout = 'save'; Label = '打开存档目录'; Button = 'save' },
        @{ Layout = 'exit'; Label = '退出'; Button = 'exit' }
    )
    foreach ($button in $buttons) {
        $fallbackPattern = 'draw_fallback_button\(\s*device_context,\s*view\.layout\.' +
            $button.Layout + ',\s*L"' + [regex]::Escape($button.Label) +
            '",\s*LauncherButton::' + $button.Button + ',\s*view,\s*scale\);'
        Assert-Task8SourcePattern $Sources.Main $fallbackPattern "native fallback button must be exact Chinese: $($button.Label)"

        $directWritePattern = 'draw_button\(\s*view\.layout\.' + $button.Layout +
            ',\s*L"' + [regex]::Escape($button.Label) +
            '",\s*LauncherButton::' + $button.Button + ',\s*view\);'
        Assert-Task8SourcePattern $Sources.Renderer $directWritePattern "DirectWrite button must be exact Chinese: $($button.Label)"
    }

    foreach ($source in @($Sources.Main, $Sources.Renderer)) {
        Assert-Task8SourcePattern $source 'L"无限地下城"' 'launcher product title must be Chinese in both rendering paths'
        Assert-Task8SourcePattern $source 'L"版本 0\.1\.0 · 单人无限地下城"' 'launcher subtitle must be Chinese in both rendering paths'
    }

    foreach ($contract in @(
        @{ Pattern = 'case\s+InstallationState::ready:\s*return\s+L"游戏已就绪";'; Name = 'ready status' },
        @{ Pattern = 'case\s+InstallationState::invalid_launcher_path:\s*return\s+L"启动器路径无效";'; Name = 'invalid path status' },
        @{ Pattern = 'case\s+InstallationState::missing_game_executable:\s*return\s+L"缺少 arpg_game\.exe";'; Name = 'missing executable status' },
        @{ Pattern = 'case\s+InstallationState::missing_asset_directory:\s*return\s+L"缺少资源："\s*\+'; Name = 'missing asset prefix' }
    )) {
        Assert-Task8SourcePattern $Sources.Core $contract.Pattern "installation copy must be exact Chinese: $($contract.Name)"
    }
    foreach ($pathToken in @(
        'arpg_game.exe',
        'assets/fonts',
        'assets/player',
        'assets/skills',
        'assets/stage12',
        'assets/stage14/audio',
        'assets/stage15/audio')) {
        Assert-Task8SourcePattern $Sources.Core ('L"' + [regex]::Escape($pathToken) + '"') "immutable package token must remain exact: $pathToken"
    }
    Assert-Task8SourcePattern $Sources.Core 'L"InfiniteDungeon"\s*/\s*L"save"' 'default save path must remain InfiniteDungeon/save'

    foreach ($contract in @(
        @{ Pattern = 'path_text_\s*=\s*L"安装目录：不可用";'; Name = 'unavailable install path' },
        @{ Pattern = 'path_text_\s*=\s*L"安装目录："\s*\+\s*full_path_;'; Name = 'dynamic install path prefix' },
        @{ Pattern = 'set_error\(L"无法创建游戏启动请求"\);'; Name = 'launch request failure' },
        @{ Pattern = 'set_error\(L"无法解析本地应用数据目录（LOCALAPPDATA）"\);'; Name = 'LOCALAPPDATA failure with technical suffix' },
        @{ Pattern = 'set_error\(L"无法解析存档目录"\);'; Name = 'save path failure' }
    )) {
        Assert-Task8SourcePattern $Sources.Main $contract.Pattern "launcher error/status copy must be exact Chinese: $($contract.Name)"
    }

    $renderFailureMatches = [regex]::Matches(
        $Sources.Main,
        'std::wstring\s+render_failure_message\s*\(\s*const\s+RendererResult&\s+result\s*\)\s*\{(?<Body>.*?)\r?\n\}\s*\r?\n\s*RECT\s+fallback_rectangle',
        [Text.RegularExpressions.RegexOptions]::Singleline -bor
            [Text.RegularExpressions.RegexOptions]::CultureInvariant)
    if ($renderFailureMatches.Count -ne 1) {
        throw 'Task 8 must isolate exactly one render_failure_message function body'
    }
    $renderFailureBody = $renderFailureMatches[0].Groups['Body'].Value
    if ([regex]::Matches($renderFailureBody, '\breturn\b').Count -ne 1) {
        throw 'Task 8 render_failure_message must contain exactly one return statement'
    }
    if ([regex]::IsMatch($renderFailureBody, '\[[^\]\r\n]*\]\s*\(')) {
        throw 'Task 8 render_failure_message cannot hide its formatter in a lambda'
    }
    Assert-Task8SourcePattern $renderFailureBody 'return\s+L"渲染失败（"\s*\+\s*std::wstring\{stage\}\s*\+\s*L"，HRESULT 0x"\s*\+\s*hresult\s*\+\s*L"）";\s*$' 'render_failure_message actual return must keep API stage and HRESULT inside Chinese parentheses'
    if ([regex]::IsMatch($Sources.Main, '渲染失败：[^\r\n]*HRESULT')) {
        throw 'Task 8 Chinese-copy guard rejected an unparenthesized renderer API stage'
    }
    if ([regex]::Matches($Sources.Main, 'renderer_stage_name\s*\(').Count -ne 1) {
        throw 'Task 8 renderer stage names must flow only through render_failure_message'
    }

    $stageBody = [regex]::Match(
        $Sources.Renderer,
        'std::wstring_view\s+renderer_stage_name\([^\{]+\{(?<Body>.*?)\n\}\s*\n\s*bool\s+LauncherRenderer::initialize',
        [Text.RegularExpressions.RegexOptions]::Singleline).Groups['Body'].Value
    if ([string]::IsNullOrEmpty($stageBody)) {
        throw 'Task 8 could not isolate renderer_stage_name for contextual audit'
    }
    $actualStages = @([regex]::Matches($stageBody, 'return\s+L"([^"]+)";') |
        ForEach-Object { $_.Groups[1].Value } | Sort-Object)
    $allowedStages = @(
        'CreateGradientStopCollection',
        'CreateHwndRenderTarget',
        'CreateLinearGradientBrush',
        'CreateSolidColorBrush',
        'CreateTextLayout',
        'ID2D1HwndRenderTarget::Resize',
        'ID2D1RenderTarget::EndDraw',
        'IDWriteTextLayout::SetTrimming',
        'IDWriteTextLayout::SetWordWrapping',
        'none',
        'unknown'
    ) | Sort-Object
    if (($actualStages -join "`n") -cne ($allowedStages -join "`n")) {
        throw 'Task 8 renderer-stage technical allowlist changed'
    }

    foreach ($contract in @(
        @{ Pattern = 'message\s*\+=\s*L"（错误代码 ";'; Name = 'numeric error prefix' },
        @{ Pattern = 'message\s*\+=\s*L"）：";'; Name = 'numeric error suffix' },
        @{ Pattern = 'failure\(L"无法启动游戏",\s*error_code\)'; Name = 'game launch failure' },
        @{ Pattern = 'failure\(\s*L"无法创建存档目录",'; Name = 'save create failure' },
        @{ Pattern = 'failure\(\s*L"无法打开存档目录",'; Name = 'save open failure' },
        @{ Pattern = 'message\s*\+=\s*L"无法获取系统错误消息";'; Name = 'system message fallback' }
    )) {
        Assert-Task8SourcePattern $Sources.Platform $contract.Pattern "Win32 error path must keep a Chinese main message: $($contract.Name)"
    }
    Assert-Task8SourcePattern $Sources.Platform 'ShellExecuteW\(\s*nullptr,\s*L"open"' 'open is allowed only as the ShellExecuteW verb'

    $playerLiteralContracts = @(
        @{ File = 'Host'; Literal = 'u8"无限地城"'; Count = 1; Reason = 'exact game window title' },
        @{ File = 'Main'; Literal = 'L"安装目录："'; Count = 1; Reason = 'dynamic install-path Chinese prefix' },
        @{ File = 'Main'; Literal = 'L"安装目录：不可用"'; Count = 1; Reason = 'unavailable install-path state' },
        @{ File = 'Main'; Literal = 'L"开始游戏"'; Count = 1; Reason = 'native fallback Start button' },
        @{ File = 'Main'; Literal = 'L"打开存档目录"'; Count = 1; Reason = 'native fallback Save button' },
        @{ File = 'Main'; Literal = 'L"无法创建游戏启动请求"'; Count = 1; Reason = 'launch-request failure' },
        @{ File = 'Main'; Literal = 'L"无法解析存档目录"'; Count = 1; Reason = 'save-path failure' },
        @{ File = 'Main'; Literal = 'L"无法解析本地应用数据目录（LOCALAPPDATA）"'; Count = 1; Reason = 'Chinese error with exact parenthesized environment token' },
        @{ File = 'Main'; Literal = 'L"无限地下城"'; Count = 1; Reason = 'native fallback product title' },
        @{ File = 'Main'; Literal = 'L"无限地下城启动器"'; Count = 1; Reason = 'launcher window title' },
        @{ File = 'Main'; Literal = 'L"检查游戏文件"'; Count = 1; Reason = 'native fallback Verify button' },
        @{ File = 'Main'; Literal = 'L"渲染失败（"'; Count = 1; Reason = 'renderer failure Chinese main message' },
        @{ File = 'Main'; Literal = 'L"版本 0.1.0 · 单人无限地下城"'; Count = 1; Reason = 'native fallback subtitle' },
        @{ File = 'Main'; Literal = 'L"退出"'; Count = 1; Reason = 'native fallback Exit button' },
        @{ File = 'Renderer'; Literal = 'L"开始游戏"'; Count = 1; Reason = 'DirectWrite Start button' },
        @{ File = 'Renderer'; Literal = 'L"打开存档目录"'; Count = 1; Reason = 'DirectWrite Save button' },
        @{ File = 'Renderer'; Literal = 'L"无限地下城"'; Count = 1; Reason = 'DirectWrite product title' },
        @{ File = 'Renderer'; Literal = 'L"检查游戏文件"'; Count = 1; Reason = 'DirectWrite Verify button' },
        @{ File = 'Renderer'; Literal = 'L"版本 0.1.0 · 单人无限地下城"'; Count = 1; Reason = 'DirectWrite subtitle' },
        @{ File = 'Renderer'; Literal = 'L"退出"'; Count = 1; Reason = 'DirectWrite Exit button' },
        @{ File = 'Core'; Literal = 'L"启动器路径无效"'; Count = 2; Reason = 'invalid installation state and defensive fallback' },
        @{ File = 'Core'; Literal = 'L"游戏已就绪"'; Count = 1; Reason = 'ready installation state' },
        @{ File = 'Core'; Literal = 'L"缺少 arpg_game.exe"'; Count = 1; Reason = 'Chinese missing-game state with immutable executable suffix' },
        @{ File = 'Core'; Literal = 'L"缺少资源："'; Count = 1; Reason = 'missing-asset Chinese prefix' },
        @{ File = 'Platform'; Literal = 'L"无法创建存档目录"'; Count = 1; Reason = 'save-directory creation operation' },
        @{ File = 'Platform'; Literal = 'L"无法启动游戏"'; Count = 2; Reason = 'game launch operation and catch fallback' },
        @{ File = 'Platform'; Literal = 'L"无法打开存档目录"'; Count = 2; Reason = 'save-directory open operation and catch fallback' },
        @{ File = 'Platform'; Literal = 'L"无法获取系统错误消息"'; Count = 1; Reason = 'FormatMessage fallback' }
    )
    $technicalAllowlist = @(
        @{ File = 'Host'; Literal = '"combat/room_bounds.hpp"'; Count = 1; Reason = 'compile-time include path' },
        @{ File = 'Host'; Literal = '"core/fixed_step.hpp"'; Count = 1; Reason = 'compile-time include path' },
        @{ File = 'Host'; Literal = '"dungeon_runtime.hpp"'; Count = 1; Reason = 'compile-time include path' },
        @{ File = 'Host'; Literal = '"dungeon/dungeon_types.hpp"'; Count = 1; Reason = 'compile-time include path' },
        @{ File = 'Host'; Literal = '"material_asset_types.hpp"'; Count = 1; Reason = 'compile-time include path' },
        @{ File = 'Main'; Literal = '"launcher_core.hpp"'; Count = 1; Reason = 'compile-time include path' },
        @{ File = 'Main'; Literal = '"launcher_layout.hpp"'; Count = 1; Reason = 'compile-time include path' },
        @{ File = 'Main'; Literal = '"launcher_platform_win32.hpp"'; Count = 1; Reason = 'compile-time include path' },
        @{ File = 'Main'; Literal = '"launcher_renderer.hpp"'; Count = 1; Reason = 'compile-time include path' },
        @{ File = 'Main'; Literal = 'L" - "'; Count = 1; Reason = 'window-title diagnostic separator' },
        @{ File = 'Main'; Literal = 'L"，HRESULT 0x"'; Count = 1; Reason = 'parenthesized renderer numeric-detail prefix' },
        @{ File = 'Main'; Literal = 'L"）"'; Count = 1; Reason = 'renderer technical-detail closing punctuation' },
        @{ File = 'Main'; Literal = 'L"%08X"'; Count = 1; Reason = 'fixed-width hexadecimal formatter' },
        @{ File = 'Main'; Literal = 'L"InfiniteDungeonLauncherWindow"'; Count = 1; Reason = 'internal Win32 class identity' },
        @{ File = 'Main'; Literal = 'L"LOCALAPPDATA"'; Count = 2; Reason = 'exact environment-variable lookup token' },
        @{ File = 'Main'; Literal = 'L"Microsoft YaHei UI"'; Count = 1; Reason = 'native font-family identifier' },
        @{ File = 'Renderer'; Literal = '"launcher_renderer.hpp"'; Count = 1; Reason = 'compile-time include path' },
        @{ File = 'Renderer'; Literal = 'L""'; Count = 1; Reason = 'empty DirectWrite buffer fallback' },
        @{ File = 'Renderer'; Literal = 'L"CreateGradientStopCollection"'; Count = 1; Reason = 'parenthesized renderer diagnostic stage' },
        @{ File = 'Renderer'; Literal = 'L"CreateHwndRenderTarget"'; Count = 1; Reason = 'parenthesized renderer diagnostic stage' },
        @{ File = 'Renderer'; Literal = 'L"CreateLinearGradientBrush"'; Count = 1; Reason = 'parenthesized renderer diagnostic stage' },
        @{ File = 'Renderer'; Literal = 'L"CreateSolidColorBrush"'; Count = 1; Reason = 'parenthesized renderer diagnostic stage' },
        @{ File = 'Renderer'; Literal = 'L"CreateTextLayout"'; Count = 1; Reason = 'parenthesized renderer diagnostic stage' },
        @{ File = 'Renderer'; Literal = 'L"ID2D1HwndRenderTarget::Resize"'; Count = 1; Reason = 'parenthesized renderer diagnostic stage' },
        @{ File = 'Renderer'; Literal = 'L"ID2D1RenderTarget::EndDraw"'; Count = 1; Reason = 'parenthesized renderer diagnostic stage' },
        @{ File = 'Renderer'; Literal = 'L"IDWriteTextLayout::SetTrimming"'; Count = 1; Reason = 'parenthesized renderer diagnostic stage' },
        @{ File = 'Renderer'; Literal = 'L"IDWriteTextLayout::SetWordWrapping"'; Count = 1; Reason = 'parenthesized renderer diagnostic stage' },
        @{ File = 'Renderer'; Literal = 'L"Microsoft YaHei UI"'; Count = 1; Reason = 'DirectWrite font-family identifier' },
        @{ File = 'Renderer'; Literal = 'L"none"'; Count = 1; Reason = 'parenthesized renderer diagnostic stage' },
        @{ File = 'Renderer'; Literal = 'L"unknown"'; Count = 1; Reason = 'parenthesized renderer diagnostic stage' },
        @{ File = 'Renderer'; Literal = 'L"zh-CN"'; Count = 1; Reason = 'DirectWrite locale identifier' },
        @{ File = 'Core'; Literal = '"launcher_core.hpp"'; Count = 1; Reason = 'compile-time include path' },
        @{ File = 'Core'; Literal = 'L"\""'; Count = 2; Reason = 'quoted argument-free command-line delimiter' },
        @{ File = 'Core'; Literal = 'L"arpg_game.exe"'; Count = 1; Reason = 'immutable executable name' },
        @{ File = 'Core'; Literal = 'L"assets/fonts"'; Count = 1; Reason = 'immutable package path' },
        @{ File = 'Core'; Literal = 'L"assets/player"'; Count = 1; Reason = 'immutable package path' },
        @{ File = 'Core'; Literal = 'L"assets/skills"'; Count = 1; Reason = 'immutable package path' },
        @{ File = 'Core'; Literal = 'L"assets/stage12"'; Count = 1; Reason = 'immutable package path' },
        @{ File = 'Core'; Literal = 'L"assets/stage14/audio"'; Count = 1; Reason = 'immutable package path' },
        @{ File = 'Core'; Literal = 'L"assets/stage15/audio"'; Count = 1; Reason = 'immutable package path' },
        @{ File = 'Core'; Literal = 'L"InfiniteDungeon"'; Count = 1; Reason = 'immutable default save-path component' },
        @{ File = 'Core'; Literal = 'L"save"'; Count = 1; Reason = 'immutable default save-path component' },
        @{ File = 'Platform'; Literal = '"launcher_platform_win32.hpp"'; Count = 1; Reason = 'compile-time include path' },
        @{ File = 'Platform'; Literal = 'L"（错误代码 "'; Count = 1; Reason = 'Win32 numeric-detail prefix' },
        @{ File = 'Platform'; Literal = 'L"）："'; Count = 1; Reason = 'Win32 numeric-detail separator' },
        @{ File = 'Platform'; Literal = 'L"open"'; Count = 1; Reason = 'ShellExecuteW verb' }
    )

    $literalContracts = @($playerLiteralContracts) + @($technicalAllowlist)
    $contractByKey = [System.Collections.Generic.Dictionary[string, object]]::new(
        [StringComparer]::Ordinal)
    foreach ($entry in $literalContracts) {
        if (-not $Sources.Contains($entry.File) -or
            [string]::IsNullOrWhiteSpace($entry.Literal) -or
            $entry.Count -le 0 -or
            [string]::IsNullOrWhiteSpace($entry.Reason)) {
            throw 'Task 8 literal allowlist entries require source key, exact literal, positive count, and reason'
        }
        $key = $entry.File + "`0" + $entry.Literal
        if ($contractByKey.ContainsKey($key)) {
            throw "Task 8 literal allowlist has a duplicate entry: $($entry.File) $($entry.Literal)"
        }
        $contractByKey.Add($key, $entry)
    }

    foreach ($file in $Sources.Keys) {
        $actualCounts = [System.Collections.Generic.Dictionary[string, int]]::new(
            [StringComparer]::Ordinal)
        foreach ($literal in @(Get-Task8CppStringLiterals $Sources[$file])) {
            if ($actualCounts.ContainsKey($literal)) { ++$actualCounts[$literal] }
            else { $actualCounts.Add($literal, 1) }
        }
        foreach ($literal in $actualCounts.Keys) {
            $key = $file + "`0" + $literal
            if (-not $contractByKey.ContainsKey($key)) {
                throw "Task 8 found an unaudited authored string literal in ${file}: $literal"
            }
            if ($actualCounts[$literal] -ne $contractByKey[$key].Count) {
                throw "Task 8 authored literal count changed in ${file}: $literal actual $($actualCounts[$literal]) expected $($contractByKey[$key].Count)"
            }
        }
        foreach ($entry in @($literalContracts | Where-Object { $_.File -ceq $file })) {
            if (-not $actualCounts.ContainsKey($entry.Literal)) {
                throw "Task 8 required authored literal is missing from ${file}: $($entry.Literal)"
            }
        }
    }
}

function Copy-Task8ChineseCopySources {
    param([Parameter(Mandatory = $true)] $Sources)
    $copy = [ordered]@{}
    foreach ($key in $Sources.Keys) { $copy[$key] = $Sources[$key] }
    return $copy
}

function Invoke-Task8ChineseCopyGuardSelfTest {
    param([Parameter(Mandatory = $true)] $Sources)

    Assert-Task8ChineseCopyGuard $Sources
    $newline = [Environment]::NewLine
    $mutations = @(
        @{ Name = 'English game title'; File = 'Host'; Needle = 'u8"无限地城"'; Replacement = '"Infinite Dungeon"' },
        @{ Name = 'English launcher title'; File = 'Main'; Needle = 'L"无限地下城启动器"'; Replacement = 'L"Infinite Dungeon Launcher"' },
        @{ Name = 'English fallback button'; File = 'Main'; Needle = 'L"开始游戏"'; Replacement = 'L"Start Game"' },
        @{ Name = 'English DirectWrite button'; File = 'Renderer'; Needle = 'L"开始游戏"'; Replacement = 'L"Start Game"' },
        @{ Name = 'English status prefix'; File = 'Core'; Needle = 'L"游戏已就绪"'; Replacement = 'L"Ready"' },
        @{ Name = 'unparenthesized renderer stage'; File = 'Main'; Needle = 'return L"渲染失败（" + std::wstring{stage} + L"，HRESULT 0x" + hresult + L"）";'; Replacement = 'return L"渲染失败：" + std::wstring{stage} + L"（HRESULT 0x" + hresult + L"）";' },
        @{
            Name = 'added English error while preserving Chinese error'
            File = 'Main'
            Needle = 'set_error(L"无法解析存档目录");'
            Replacement = 'set_error(L"无法解析存档目录");' + $newline +
                '            set_error(L"Launch failed");'
        },
        @{
            Name = 'comment decoy preserves Chinese DirectWrite button'
            File = 'Renderer'
            Needle = 'result = draw_button(view.layout.exit, L"退出", LauncherButton::exit, view);'
            Replacement = 'result = draw_button(view.layout.exit, L"Exit", LauncherButton::exit, view);' + $newline +
                '        /* result = draw_button(view.layout.exit, L"退出", LauncherButton::exit, view); */'
        },
        @{
            Name = 'inactive preprocessor branch preserves Chinese fallback button'
            File = 'Main'
            Needle = 'draw_fallback_button(' + $newline +
                '        device_context, view.layout.verify, L"检查游戏文件", LauncherButton::verify, view, scale);'
            Replacement = '#if 0' + $newline +
                '    draw_fallback_button(' + $newline +
                '        device_context, view.layout.verify, L"检查游戏文件", LauncherButton::verify, view, scale);' + $newline +
                '#endif' + $newline +
                '    draw_fallback_button(' + $newline +
                '        device_context, view.layout.verify, L"Verify", LauncherButton::verify, view, scale);'
        },
        @{
            Name = 'constant-false duplicate preserves Chinese fallback button'
            File = 'Main'
            Needle = 'draw_fallback_button(' + $newline +
                '        device_context, view.layout.exit, L"退出", LauncherButton::exit, view, scale);'
            Replacement = 'if (false) {' + $newline +
                '        draw_fallback_button(' + $newline +
                '            device_context, view.layout.exit, L"退出", LauncherButton::exit, view, scale);' + $newline +
                '    }' + $newline +
                '    draw_fallback_button(' + $newline +
                '        device_context, view.layout.exit, L"Exit", LauncherButton::exit, view, scale);'
        },
        @{
            Name = 'ordinary English macro with noncanonical dead Chinese decoy'
            File = 'Main'
            Needle = 'draw_fallback_button(' + $newline +
                '        device_context, view.layout.start, L"开始游戏", LauncherButton::start, view, scale);'
            Replacement = '#define TASK8_WIDEN_IMPL(value) L##value' + $newline +
                '#define TASK8_WIDEN(value) TASK8_WIDEN_IMPL(value)' + $newline +
                '    if (1 == 0) {' + $newline +
                '        draw_fallback_button(' + $newline +
                '            device_context, view.layout.start, L"开始游戏", LauncherButton::start, view, scale);' + $newline +
                '    }' + $newline +
                '    draw_fallback_button(' + $newline +
                '        device_context, view.layout.start, TASK8_WIDEN("Start Game"), LauncherButton::start, view, scale);' + $newline +
                '#undef TASK8_WIDEN' + $newline +
                '#undef TASK8_WIDEN_IMPL'
        },
        @{
            Name = 'ordinary English runtime error while preserving Chinese error'
            File = 'Main'
            Needle = 'set_error(L"无法解析本地应用数据目录（LOCALAPPDATA）");'
            Replacement = 'set_error(L"无法解析本地应用数据目录（LOCALAPPDATA）");' + $newline +
                '            set_error(std::filesystem::path{"Launch failed"}.wstring());'
        },
        @{
            Name = 'uninvoked lambda decoys renderer failure return'
            File = 'Main'
            Needle = '    return L"渲染失败（" + std::wstring{stage} + L"，HRESULT 0x" + hresult + L"）";'
            Replacement = '    [[maybe_unused]] const auto task8_decoy = [&]() -> std::wstring {' + $newline +
                '        return L"渲染失败（" + std::wstring{stage} + L"，HRESULT 0x" + hresult + L"）";' + $newline +
                '    };' + $newline +
                '    return std::wstring{stage};'
        }
    )
    $acceptedMutations = @()
    foreach ($mutation in $mutations) {
        $mutated = Copy-Task8ChineseCopySources $Sources
        if (-not $mutated[$mutation.File].Contains($mutation.Needle)) {
            throw "Task 8 mutation anchor is missing: $($mutation.Name)"
        }
        $mutated[$mutation.File] = $mutated[$mutation.File].Replace(
            $mutation.Needle, $mutation.Replacement)
        $accepted = $true
        try { Assert-Task8ChineseCopyGuard $mutated }
        catch { $accepted = $false }
        if ($accepted) {
            $acceptedMutations += $mutation.Name
        }
    }

    $acceptedBroadAllowlist = $true
    try { Assert-Task8ChineseCopyGuard $Sources -WholeFileAllowlist @('launcher_main.cpp') }
    catch { $acceptedBroadAllowlist = $false }
    if ($acceptedBroadAllowlist) {
        $acceptedMutations += 'broad-file allowlist attempt'
    }

    $validVariants = @(
        @{
            Name = 'C++17 digit separator'
            File = 'Main'
            Needle = 'constexpr int kLauncherIconId = 101;'
            Replacement = 'constexpr int kLauncherIconId = 101;' + $newline +
                "constexpr unsigned int kTask8DigitSeparatorProbe = 1'000U;"
        },
        @{
            Name = 'continued line comment'
            File = 'Main'
            Needle = 'constexpr int kLauncherIconId = 101;'
            Replacement = 'constexpr int kLauncherIconId = 101;' + $newline +
                '// Task 8 lexical probe continues \' + $newline +
                'L"Comment-only English"'
        },
        @{
            Name = 'character literal after return without whitespace'
            File = 'Main'
            Needle = 'constexpr int kLauncherIconId = 101;'
            Replacement = 'constexpr int kLauncherIconId = 101;' + $newline +
                'constexpr char task8_quote_probe() { return''"''; }'
        }
    )
    $rejectedValidVariants = @()
    foreach ($variant in $validVariants) {
        $mutated = Copy-Task8ChineseCopySources $Sources
        if (-not $mutated[$variant.File].Contains($variant.Needle)) {
            throw "Task 8 valid-variant anchor is missing: $($variant.Name)"
        }
        $mutated[$variant.File] = $mutated[$variant.File].Replace(
            $variant.Needle, $variant.Replacement)
        try { Assert-Task8ChineseCopyGuard $mutated }
        catch { $rejectedValidVariants += $variant.Name }
    }

    if ($acceptedMutations.Count -ne 0 -or $rejectedValidVariants.Count -ne 0) {
        throw "Task 8 Chinese-copy guard self-test mismatch; accepted mutations: $($acceptedMutations -join '; '); rejected valid variants: $($rejectedValidVariants -join '; ')"
    }
    return [pscustomobject]@{
        MutationCount = $mutations.Count + 1
        ValidVariantCount = $validVariants.Count
    }
}

$LauncherSmokeDirectoryPrefix = 'arpg-launcher-window-smoke-'
$LauncherSmokeDirectoryPattern = '^arpg-launcher-window-smoke-[0-9a-f]{32}$'
$LauncherSmokeMarkerName = '.launcher-window-smoke-owner.marker'
$LauncherSmokeMarkerMagic = 'ARPG_LAUNCHER_WINDOW_SMOKE_OWNER_V1'
$LauncherSmokeStaleAgeSeconds = 15

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

function Write-LauncherSmokeMarker {
    param(
        [System.IO.DirectoryInfo] $Directory,
        [int] $OwnerPid = $PID,
        [int64] $OwnerStartTimeUtcTicks = 0
    )

    if ($OwnerStartTimeUtcTicks -eq 0) {
        $OwnerStartTimeUtcTicks = (Get-Process -Id $OwnerPid -ErrorAction Stop).StartTime.ToUniversalTime().Ticks
    }
    [System.IO.File]::WriteAllText(
        (Join-Path $Directory.FullName $LauncherSmokeMarkerName),
        "$LauncherSmokeMarkerMagic`n$OwnerPid`n$OwnerStartTimeUtcTicks`n",
        [System.Text.UTF8Encoding]::new($false))
}

function Remove-StaleLauncherSmokeDirectories {
    param([string] $TemporaryRoot, [DateTime] $NowUtc)

    try {
        $directories = @(Get-ChildItem -Path $TemporaryRoot -Directory -Force -ErrorAction Stop)
    }
    catch {
        return
    }
    $cutoff = $NowUtc.AddSeconds(-$LauncherSmokeStaleAgeSeconds)
    $expectedRoot = [System.IO.Path]::GetFullPath($TemporaryRoot).TrimEnd('\')
    foreach ($directory in $directories) {
        try {
            if ($directory.Name -cnotmatch $LauncherSmokeDirectoryPattern -or
                ($directory.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0 -or
                $directory.CreationTimeUtc -gt $cutoff -or
                -not [string]::Equals([System.IO.Path]::GetFullPath($directory.Parent.FullName).TrimEnd('\'), $expectedRoot, [StringComparison]::OrdinalIgnoreCase)) {
                continue
            }
            $marker = Get-Item -LiteralPath (Join-Path $directory.FullName $LauncherSmokeMarkerName) -Force -ErrorAction Stop
            if ($marker.PSIsContainer -or ($marker.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0 -or $marker.Length -gt 128) { continue }
            $lines = [System.IO.File]::ReadAllLines($marker.FullName, [System.Text.Encoding]::UTF8)
            [int] $ownerPid = 0
            [int64] $ownerStartTicks = 0
            if ($lines.Count -ne 3 -or $lines[0] -cne $LauncherSmokeMarkerMagic -or
                -not [int]::TryParse($lines[1], [ref] $ownerPid) -or $ownerPid -le 0 -or
                -not [int64]::TryParse($lines[2], [ref] $ownerStartTicks) -or $ownerStartTicks -le 0) {
                continue
            }
            $owner = $null
            try {
                $owner = [System.Diagnostics.Process]::GetProcessById($ownerPid)
            }
            catch [System.ArgumentException] {
                $owner = $null
            }
            catch {
                continue
            }
            if ($null -ne $owner) {
                try {
                    if ($owner.StartTime.ToUniversalTime().Ticks -eq $ownerStartTicks) { continue }
                }
                catch {
                    continue
                }
                finally {
                    $owner.Dispose()
                }
            }
            Remove-Item -LiteralPath $directory.FullName -Recurse -Force -ErrorAction Stop
        }
        catch {
            continue
        }
    }
}

function Invoke-LauncherSmokeStaleCleanupSelfTest {
    param([string] $TemporaryRoot)

    $oldTime = [DateTime]::UtcNow.AddSeconds(-($LauncherSmokeStaleAgeSeconds + 5))
    $names = @([guid]::NewGuid().ToString('N'), [guid]::NewGuid().ToString('N'), [guid]::NewGuid().ToString('N')) |
        ForEach-Object { Join-Path $TemporaryRoot ($LauncherSmokeDirectoryPrefix + $_) }
    $unmarkedPath, $liveOwnerPath, $deadOwnerPath = $names
    try {
        $unmarked = [System.IO.Directory]::CreateDirectory($unmarkedPath)
        $liveOwner = [System.IO.Directory]::CreateDirectory($liveOwnerPath)
        $deadOwner = [System.IO.Directory]::CreateDirectory($deadOwnerPath)
        Write-LauncherSmokeMarker $liveOwner
        Write-LauncherSmokeMarker $deadOwner 999999 1
        foreach ($directory in @($unmarked, $liveOwner, $deadOwner)) {
            [System.IO.Directory]::SetCreationTimeUtc($directory.FullName, $oldTime)
        }
        $cleanupDefinition = (Get-Command Remove-StaleLauncherSmokeDirectories).Definition
        if ($cleanupDefinition -notmatch '(?s)\$directory\.Attributes.*?ReparsePoint') {
            throw 'Stale cleanup must skip reparse-point directories'
        }
        $reparseAttributes = [System.IO.FileAttributes]::Directory -bor [System.IO.FileAttributes]::ReparsePoint
        if ($deadOwner.Name -cmatch $LauncherSmokeDirectoryPattern -and ($reparseAttributes -band [System.IO.FileAttributes]::ReparsePoint) -eq 0) {
            throw 'Reparse-point attributes must not be cleanup candidates'
        }
        Remove-StaleLauncherSmokeDirectories $TemporaryRoot ([DateTime]::UtcNow)
        if (-not (Test-Path -LiteralPath $unmarkedPath)) { throw 'Cleanup removed an unmarked directory' }
        if (-not (Test-Path -LiteralPath $liveOwnerPath)) { throw 'Cleanup removed a live-owner directory' }
        if (Test-Path -LiteralPath $deadOwnerPath) { throw 'Cleanup did not remove an old dead-owner directory' }
    }
    finally {
        try {
            foreach ($path in @($unmarkedPath, $liveOwnerPath)) {
                if (-not (Test-Path -LiteralPath $path)) { continue }
                $directory = Get-Item -LiteralPath $path -Force
                Write-LauncherSmokeMarker $directory 999999 1
                [System.IO.Directory]::SetCreationTimeUtc($directory.FullName, $oldTime)
            }
            Remove-StaleLauncherSmokeDirectories $TemporaryRoot ([DateTime]::UtcNow)
        }
        finally {
            foreach ($path in $names) {
                if (Test-Path -LiteralPath $path) {
                    Remove-Item -LiteralPath $path -Recurse -Force -ErrorAction Stop
                }
            }
        }
    }
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

$task8Sources = Get-Task8ChineseCopySources
$task8SelfTestResult = Invoke-Task8ChineseCopyGuardSelfTest $task8Sources
if ($SelfTestChineseCopyGuard) {
    Write-Host "Task 8 Chinese-copy guard accepted pristine sources and $($task8SelfTestResult.ValidVariantCount) lexical variants, and rejected $($task8SelfTestResult.MutationCount) mutations"
    exit 0
}

$temporaryRoot = [System.IO.Path]::GetTempPath()
if ($SelfTestStaleCleanup) {
    Invoke-LauncherSmokeStaleCleanupSelfTest $temporaryRoot
    exit 0
}
if ([string]::IsNullOrWhiteSpace($Launcher)) {
    throw 'Launcher is required unless only a self-test is requested'
}
Remove-StaleLauncherSmokeDirectories $temporaryRoot ([DateTime]::UtcNow)
$source = (Resolve-Path -LiteralPath $Launcher).Path
$temporaryDirectory = Join-Path $temporaryRoot (
    $LauncherSmokeDirectoryPrefix + [guid]::NewGuid().ToString('N'))
$process = $null
$jobHandle = [IntPtr]::Zero
$originalCursor = [LauncherPixelSmokeNative+POINT]::new()
[void][LauncherPixelSmokeNative]::GetCursorPos([ref] $originalCursor)

try {
    $fixtureDirectory = [System.IO.Directory]::CreateDirectory($temporaryDirectory)
    Write-LauncherSmokeMarker $fixtureDirectory
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
