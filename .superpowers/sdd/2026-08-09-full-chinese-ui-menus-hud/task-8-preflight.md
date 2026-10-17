# Task 8 read-only preflight

Status: **NOT READY TO IMPLEMENT**

Audit date: 2026-08-10

Audit snapshot:

- HEAD: `a12f569 feat: localize responsive debug overlay`
- Task 8's six declared target files are clean.
- The worktree is not clean: five tracked Task 7/review-fix files remain modified:
  `src/platform/raylib/actor_renderer.cpp`,
  `src/platform/raylib/ui_text_catalog.cpp`,
  `tests/platform/shared_ui_font_tests.cpp`,
  `tests/platform/task7_debug_overlay_source_test.cmake`, and
  `tests/platform/task7_debug_overlay_source_self_test.cmake`.
- Task 6 is committed (`825e6ed`) and its recovery screen is now catalogue-backed Chinese.
  Task 7 is committed (`a12f569`), but the five dirty files above show that its final guard/glyph
  fix round has not yet reached a clean hand-off.
- This audit changed no tracked file and ran no configure, build, test, game, launcher, deployment,
  or commit command. This ignored report is the only file created.

## Controller requirements and exact scope

Task 8 is a narrow production-title/launcher-copy audit after Tasks 6 and 7. The fixed outcomes are:

- `RaylibHostConfig{}.window_title` is exactly UTF-8 `无限地城`.
- The launcher window title remains exactly `无限地下城启动器`; do not normalize it to the game title.
- Existing Chinese launcher implementation is audited and tested in place. Do not rewrite already
  correct strings merely to manufacture a production diff.
- DirectWrite/Direct2D/Win32 API names and `HRESULT 0x...` may survive only as technical details
  following a Chinese error message and enclosed in parentheses.
- `arpg_game.exe`, exact asset paths, physical-key labels, technical logs, manifest identities,
  font-family/locale identifiers, environment-variable names, and internal class names are not
  translatable display copy. If any are shown, the human-facing prefix must remain Chinese and the
  technical token must be narrowly justified.
- `make_launch_request()` remains argument-free: exact application and working directory, plus only
  the quoted `arpg_game.exe` path in `command_line`; no `--...` option may be added.
- The default save directory remains `%LOCALAPPDATA%\InfiniteDungeon\save`.
- Task 8 does not modify Release packaging or deploy. Release build, deployment, hashes, shortcut and
  installed visual acceptance belong to the later release-validation plan.
- Do not enter inventory/passive-tree follow-up work.

Declared Task 8 files:

- `src/platform/raylib/raylib_host.hpp`
- `src/launcher/launcher_main.cpp`
- `src/launcher/launcher_renderer.cpp`
- `tests/platform/host_window_lifetime_tests.cpp`
- `tests/launcher/launcher_core_tests.cpp`
- `tests/launcher/launcher_window_smoke_test.ps1`

Read-only supporting contracts examined:

- `src/launcher/launcher_core.hpp/.cpp`
- `src/launcher/launcher_platform_win32.hpp/.cpp`
- `src/launcher/CMakeLists.txt`, launcher/app manifests and resource file
- `src/app/main.cpp`
- `src/platform/raylib/host_launch_options.hpp/.cpp`
- `scripts/DeployLauncher.ps1`
- launcher platform/layout/manifest/deploy tests and both Task 6/7 source guards
- `tests/platform/platform_test_main.cpp` and relevant CTest registration

## Current production audit

### Real player-visible English that still requires Task 8 work

1. **Game default window title**

   `src/platform/raylib/raylib_host.hpp:207` is still:

   ```cpp
   const char* window_title{"Infinite Dungeon - Stage 3 Dungeon Rules"};
   ```

   This is a real production default passed unchanged through `HostWindowLifetime` to raylib
   `InitWindow`. It must become exact UTF-8 `无限地城`. Validation executables which explicitly
   override `config.window_title` with an English scenario name are test/validation-only and are not
   production-default residue.

2. **Launcher renderer error stage is outside the technical-detail parentheses**

   `src/launcher/launcher_main.cpp:61` currently produces:

   ```cpp
   渲染失败：CreateTextLayout（HRESULT 0x........）
   ```

   The API stage follows the colon as if it were the primary message. This violates the brief's
   requirement that `CreateTextLayout`, all other renderer API stage names, and `HRESULT` appear only
   in the parenthesized technical detail. The minimum compliant shape is:

   ```text
   渲染失败（CreateTextLayout，HRESULT 0x........）
   ```

   The same formatted message feeds both the native fallback status card and the temporary launcher
   window title, so this is genuinely player-visible on renderer failure.

3. **Strict-audit concern: `LOCALAPPDATA` appears in the body of a visible error**

   `无法解析 LOCALAPPDATA` has a Chinese action/error prefix and an exact technical environment token.
   It can be narrowly allowed as an environment-variable name. For the cleanest interpretation of
   “technical details after a Chinese main message”, prefer `无法解析本地应用数据目录（LOCALAPPDATA）`.
   Do not translate or rename the actual environment variable or directory contract.

### Player-visible launcher copy already correct; audit/test only

The launcher normal and native-fallback paths already agree on these Chinese strings:

- window title: `无限地下城启动器`
- product title: `无限地下城`
- subtitle: `版本 0.1.0 · 单人无限地下城`
- buttons: `开始游戏`, `检查游戏文件`, `打开存档目录`, `退出`
- path copy: `安装目录：不可用`, `安装目录：<dynamic path>`
- installation states: `游戏已就绪`, `启动器路径无效`, `缺少 arpg_game.exe`,
  `缺少资源：<exact relative asset path>`
- local actions/errors: `无法创建游戏启动请求`, `无法解析存档目录`, `无法启动游戏`,
  `无法创建存档目录`, `无法打开存档目录`, `无法获取系统错误消息`
- Win32 failure formatting begins with a Chinese operation, then `（错误代码 N）：`.
  `FormatMessageW` may append an OS-language technical description; the brief only requires the
  error prefix to be Chinese.

Task 6's former recovery-page English is no longer present. The current view uses exact catalogue
text `需要恢复存档`, `两个存档槽均无效。按 N 将其归档并创建新游戏。`, and dynamic
`存档错误 N`, rendered through the shared CJK font. Task 8 must not reopen its layout/state work.

### Exact narrow technical allowlist

The Task 8 English-residue guard must be contextual and must not exempt an entire file.

| File/context | Exact allowed token/category | Reason |
|---|---|---|
| `launcher_main.cpp` | `InfiniteDungeonLauncherWindow` | internal Win32 class identity, never display copy |
| `launcher_main.cpp` | `LOCALAPPDATA` | exact Windows environment-variable name; if displayed, keep after Chinese explanation |
| `launcher_main.cpp`, `launcher_renderer.cpp` | `Microsoft YaHei UI` | installed font-family identifier, not copy |
| `launcher_renderer.cpp` | `zh-CN` | DirectWrite locale identifier |
| `launcher_renderer.cpp` | `none`, `unknown`, `CreateHwndRenderTarget`, `CreateGradientStopCollection`, `CreateLinearGradientBrush`, `CreateSolidColorBrush`, `ID2D1HwndRenderTarget::Resize`, `CreateTextLayout`, `IDWriteTextLayout::SetWordWrapping`, `IDWriteTextLayout::SetTrimming`, `ID2D1RenderTarget::EndDraw` | renderer diagnostic stage; allowed only through the parenthesized Chinese error formatter |
| `launcher_main.cpp` | `HRESULT`, `%08X`, `0x` | numeric technical renderer detail, parenthesized only |
| `launcher_core.cpp` | `arpg_game.exe` | immutable physical executable name used for inspection and launch |
| `launcher_core.cpp` | `assets/fonts`, `assets/player`, `assets/skills`, `assets/stage12`, `assets/stage14/audio`, `assets/stage15/audio` | immutable physical package paths; may follow `缺少资源：` |
| `launcher_core.cpp` | `InfiniteDungeon`, `save` | immutable default save-path components |
| `launcher_platform_win32.cpp` | `open` | `ShellExecuteW` verb, not displayed |
| manifests/resources | `InfiniteDungeon.Launcher`, `OpenRealm.ARPG`, icon path, XML/DPI tokens | non-display technical identities; explicitly out of rename scope |
| host/launcher | includes, API symbols, enum names, `TraceLog`, test assertions and automation diagnostics | code/log/test context, not player display copy |

Dynamic install/save paths are user/system data and may contain ASCII; the guard must not treat their
contents as authored copy. It must instead prove the authored prefix is Chinese.

## Launch, save and deploy invariants

Current `launcher_core.cpp` and its tests already lock:

- `request.application == status.game_executable`
- `request.working_directory == status.application_directory`
- `request.command_line == L"\"" + game_executable + L"\""`
- `request.command_line` contains no `--`
- long Chinese/space-containing paths are preserved verbatim
- the default save path is `<LOCALAPPDATA>/InfiniteDungeon/save`

Task 8 must not change host argument parsing (`--seed`, `--save-dir`, `--settings-dir`,
`--screenshot-dir`) or inject any of those options into the launcher request.

Deployment remains completely out of scope. The current protected transaction is:

1. validate explicit Release `bin` and required executables/assets;
2. copy into a sibling `*.staging.*` directory;
3. validate staging;
4. rename the old app to `*.previous.*`;
5. rename staging to the fixed app leaf;
6. validate published layout;
7. on failure remove only the just-published app and restore the previous directory;
8. after success remove the previous backup;
9. create `无限地下城.lnk` targeting `无限地下城启动器.exe`, with the install root as working
   directory and `<launcher>,0` as icon.

Do not edit `scripts/DeployLauncher.ps1`, executable names, assets, manifests, `%LOCALAPPDATA%\InfiniteDungeon\app`,
the shortcut, its target/working directory/icon, or the save directory located beside `app`.

## Minimum TDD interface and RED/GREEN path

### Host title

The smallest production change is one literal in `raylib_host.hpp`. The smallest test change which
does not manufacture a new case is to extend an existing `host_window_lifetime_tests.cpp` case:

- construct an untouched `RaylibHostConfig{}`;
- assert `std::strcmp(config.window_title, u8"无限地城") == 0`;
- initialize the fake backend with that default config and assert the backend receives the same exact
  title pointer/text;
- keep the existing explicit `"lifetime test"` override test, which proves caller overrides are
  preserved and is not player copy.

This gives a legitimate RED before the literal change and GREEN after it without changing the case
count. It also preserves the deliberate full-platform baseline: actual remains expected + 1 and the
known ecology failure remains visible.

**Execution gap in the written plan:** Step 2 only builds `arpg_platform_tests`; it never executes the
host-title case. The completion gate runs `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1`, but that focused
array does not include `host_window_lifetime_suite()`. Therefore the declared commands cannot provide
runtime GREEN for the game title. Within the current file scope, the honest verification is a full
platform run and confirmation that only the two known failures remain (ecology plus deliberate
actual=expected+1), with no host-window-lifetime failure. A zero-exit focused run would require an
authorized `platform_test_main.cpp` selector/suite change, which is outside Task 8's declared files.

### Existing launcher copy and invariants

Avoid a production rewrite. Extend existing launcher-core cases in place so no launcher case-count
baseline update is needed:

- all four `InstallationState` messages have the exact Chinese prefix;
- `arpg_game.exe` and an exact relative asset path are preserved only as technical suffixes;
- the argument-free quoted launch request remains exact;
- the default save path remains exact;
- platform failure text starts with `无法启动游戏（错误代码 ` and includes the numeric code.

The current launcher test target links `launcher_core` and `launcher_platform_win32.cpp`, but it does
**not** compile/link `launcher_main.cpp` or `launcher_renderer.cpp`. Consequently
`render_failure_message()` is not callable from `launcher_core_tests.cpp`; pretending it is covered
there would be false evidence.

### Narrow source guard for launcher rendering/error copy

The practical RED/GREEN seam inside the declared files is a static contract in
`launcher_window_smoke_test.ps1` (or an explicitly authorized new source-guard file):

- read only the relevant C++ sources relative to `$PSScriptRoot`;
- require the exact game and launcher titles;
- require each exact Chinese button in both DirectWrite and native-fallback draw paths;
- require exact Chinese normal status/error prefixes;
- require renderer failure shape `渲染失败（<stage>，HRESULT 0x<code>）` and reject the current
  `渲染失败：<stage>（HRESULT ...）` shape;
- inspect actual call sites so API-stage strings are only fed to this parenthesized formatter;
- apply the file + exact token + reason allowlist above; never skip a whole source file;
- provide reversible in-memory mutants for at least: English game title, English launcher title,
  English button, English status prefix, current unparenthesized renderer stage, and a broad-file
  allowlist attempt.

The pristine guard should RED now on the English host title and unparenthesized renderer-stage format,
then GREEN after the two minimum production changes. The mutation self-test must reject each mutant.

If no source guard is added, Task 8 cannot honestly claim that all launcher buttons/status/error paths
were audited by automation: the current window smoke is pixel-based and does not OCR or expose custom
DirectWrite text through an accessibility API.

## Launcher smoke audit and risks

Existing strengths:

- checks PE GUI subsystem 2;
- copies the launcher into an isolated exact-pattern temp directory;
- supplies a dummy `arpg_game.exe` and all asset directories, then removes only `assets/skills` to
  exercise the missing-resource/disabled-start presentation;
- asserts exact normal launcher window title `无限地下城启动器`;
- checks 920×560 logical client size at the actual DPI and one-monitor placement;
- checks disabled-start, status-error color, hover, press and full-path tooltip pixels;
- uses a Job Object with kill-on-close, restores the cursor, closes the launcher, and deletes its own
  exact temp fixture;
- stale cleanup requires exact directory-name pattern, no reparse point, old creation time, a small
  valid owner marker, and a dead/mismatched owner before deletion. Do not weaken these conditions.

Coverage limitations/risks:

- the smoke proves spatially distributed bright pixels in the Start rectangle, not the exact
  `开始游戏` text;
- it proves a red status region, not exact `缺少资源：assets/skills`;
- it does not prove the Verify/Save/Exit glyph strings; only hover/press geometry and colors;
- it normally exercises successful DirectWrite initialization, so it does not force or inspect the
  native fallback renderer-error message;
- if rendering fails, the window title is temporarily suffixed with the error, so the early exact-title
  assertion fails before fallback-copy semantics can be inspected;
- screen-copy capture can fail when another window/overlay occludes one of six probe points; this is
  intentionally treated as a failure, not silently accepted;
- it moves the pointer and launcher window on the user's desktop. Cursor restoration exists, but the
  test must remain serial and should not run alongside other UI automation;
- three DWM/capture retries, desktop contention and cold COM/DirectWrite initialization can consume a
  meaningful fraction of the 20-second CTest timeout on a loaded machine;
- plan line 456 says the smoke's window title is `无限地城`; that is incorrect for this executable.
  The smoke must continue to assert `无限地下城启动器`. The game default `无限地城` belongs to the
  headless host-config test, not to launcher smoke.

Do not broaden cleanup, remove owner checks, start the dummy game, deploy Release, or turn the smoke
into installed-app acceptance during Task 8.

## Implementation hand-off concerns

1. **Wait for a clean Task 7 hand-off.** The five dirty tracked changes are not Task 8's property.
   They affect actor UTF-8 formatting, catalogue glyph coverage and the Task 7 source guard/self-test.
   Starting Task 8 now would violate the brief's “after Task6/7” gate and make the eventual English
   guard/glyph baseline ambiguous.
2. **Two real Task 8 REDs exist now:** the production game title and the unparenthesized API stage in
   renderer errors. Existing Chinese launcher copy should remain untouched except where the error
   structure itself is wrong.
3. **The written test commands omit host-title execution.** Build-only is not GREEN. Either accept the
   known nonzero full-suite baseline while proving no new failure, or explicitly authorize a narrow
   host-title selector outside the declared file list.
4. **Current launcher smoke is not a text oracle.** Exact button/status/error-copy claims require the
   narrow source guard and reversible self-test described above.
5. **Do not update expected counts to actual.** If an implementation adds no new case, retain MENU_HUD
   expected 183 and full expected 670. If a genuinely new case is authorized, increase only the
   corresponding expected baseline by the exact net case count so the pre-existing actual=expected+1
   mismatch remains visible; do not absorb the known ecology failure.
6. **Release/deploy/installed screenshots remain deferred.** Task 8 may run Debug launcher units and
   `launcher.window_smoke` when authorized, but must not build Release, deploy, compare installation
   hashes, alter shortcuts, or claim installed visual acceptance.

## Audit file hashes

These SHA-256 values identify the exact clean Task 8 inputs inspected:

```text
4F9B8FF7BC952D2C32CFB635314D1125F3639B16B0796E08D724F126462059EC  src/platform/raylib/raylib_host.hpp
F511F20D96E876999F1E26AE2E32302DE795A1EED7E4F92E3DCA64C7A337D604  src/launcher/launcher_main.cpp
05DBF0D1F50E2AE6D562E3101CB8F96D2EF7C4B381AEC61DA0A0FC7AEBCC0CCB  src/launcher/launcher_renderer.cpp
934FA078C5152BDB5F0E7076CE55E781609726CDAC2258D9A203030CDFA4E87A  src/launcher/launcher_core.cpp
CC264CB30CAAC0C2B0B36EE34D9EDD6E12AD86B2812FB29A624A378EAF96173E  tests/platform/host_window_lifetime_tests.cpp
1FEF3155A92E289D83AF0BA537023BE73065D59DAA916823BD7CFACC3B6928B5  tests/launcher/launcher_core_tests.cpp
F89AA9E274BF173D4E724BC6A5F3FEFC29494D2CDCE28F1E79A5EB862BF8913A  tests/launcher/launcher_window_smoke_test.ps1
```
