# Task 8 execution report

## Baseline and scope

- Requirement source: `task-8-brief.md` (read in full); `task-8-preflight.md` used only as read-only audit support.
- Starting HEAD: `0179b69d324c93542b1a7d4b3f8ddd8035205a62`.
- Starting tracked worktree state: clean (`git status --short` produced no output).
- Process references read in full: `superpowers:test-driven-development`, its `writing-good-tests.md` reference, and `superpowers:verification-before-completion`.
- Production breaks the tests must catch before implementation:
  1. changing the untouched `RaylibHostConfig` default away from UTF-8 `无限地城`, or failing to forward it to `InitWindow`;
  2. authored launcher title/button/status/error copy reverting to English;
  3. renderer API stage or `HRESULT` escaping the parenthesized technical detail;
  4. launch request gaining arguments or the default save path changing;
  5. a broad-file English allowlist bypassing the contextual guard.
- Production audit at BASE confirms two real defects: the game default title is English, and renderer failures use `渲染失败：<stage>（HRESULT ...）` with the API stage outside the technical-detail parentheses.
- Existing launcher title/buttons/status strings are already Chinese; they will be behaviorally/architecturally guarded without churn to already-correct production copy.

## RED evidence

- Source/call-site guard RED, before any production edit:
  - Command: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\launcher\launcher_window_smoke_test.ps1 -SelfTestChineseCopyGuard`
  - Exit: 1.
  - Expected failure: `Task 8 Chinese-copy guard failed: untouched RaylibHostConfig must default to exact UTF-8 无限地城`.
- Test build attempt without the project MSVC environment failed before compilation with `fatal error C1083: ... cstdint: No such file or directory`. Investigation showed the tool shell had no `INCLUDE`/`VCToolsInstallDir`; this is not treated as a behavioral RED. `scripts/Configure.ps1` is the project boundary that imports MSVC 19.44 x64 and Windows SDK 10.0.26100.0.
- Test build after loading that environment in the same PowerShell process:
  - Command: `. .\scripts\Configure.ps1 -Preset windows-msvc-debug; cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_launcher_tests --parallel 2`
  - Exit: 0.
- Launcher behavior audit at RED:
  - Command: `& .\out\build\windows-msvc-debug\bin\arpg_launcher_tests.exe`
  - Exit: 0; output: `13 cases, 0 failures` (existing Chinese launcher core behavior and invariants were already correct).
- Full platform runtime RED, before production edit:
  - Command: `& .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe`
  - Exit: 1; output ends `671 cases, 3 failures`.
  - New expected Task 8 failure: `host_window_lifetime.failed initialization leaves window unowned` on exact `无限地城` comparison.
  - Preserved pre-existing baseline failures remain visible: `ecology_material_coverage.ecology props match emitted layout records` and `actual 671 != expected 670`.
- Targeted downstream RED probes, still before production edit:
  - renderer-error contract exited 1 with `RED: renderer API stage and HRESULT are not both inside Chinese parentheses`;
  - LOCALAPPDATA display contract exited 1 with `RED: LOCALAPPDATA is not a parenthesized technical suffix after the Chinese explanation`.

## GREEN evidence

- Minimum production changes only:
  - `RaylibHostConfig{}.window_title` -> `u8"无限地城"`;
  - renderer failure -> `渲染失败（<stage>，HRESULT 0x<code>）`;
  - save-path environment failure -> `无法解析本地应用数据目录（LOCALAPPDATA）`.
- Already-correct launcher title, product copy, buttons, installation states, launch request, save-path construction and renderer stage mapping were not rewritten.
- Source/call-site guard GREEN:
  - Command: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\launcher\launcher_window_smoke_test.ps1 -SelfTestChineseCopyGuard`
  - Exit: 0; output: `Task 8 Chinese-copy guard accepted pristine sources and rejected 7 mutations`.
- Debug builds, after loading the fixed project MSVC environment:
  - `cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_launcher_tests --parallel 2` -> exit 0.
  - `cmake --build --preset windows-msvc-debug --target arpg_launcher --parallel 2` -> exit 0; rebuilt `无限地下城启动器.exe` from the changed `launcher_main.cpp` before smoke.
- Launcher units: `arpg_launcher_tests.exe` -> `13 cases, 0 failures`.
- Serial Windows smoke: `ctest --preset windows-msvc-debug --output-on-failure -R 'launcher.window_smoke'` -> 1/1 passed in 4.28 sec.
- Existing smoke cleanup self-test after the parameter/source-guard integration: `launcher_window_smoke_test.ps1 -SelfTestStaleCleanup` -> exit 0.

## Mutation sensitivity

- The reversible in-memory self-test rejects:
  1. English game default title;
  2. English launcher window title;
  3. English native-fallback Start button;
  4. English DirectWrite Start button;
  5. English ready status;
  6. the old `渲染失败：<stage>（HRESULT ...）` shape;
  7. an attempted whole-file allowlist for `launcher_main.cpp`.
- Guard call-site coverage separately requires all four buttons in both rendering paths, both launcher heading strings, four installation states, install-path/error prefixes, Win32 numeric error structure, and the exact set of renderer stages.
- Technical allowlist is contextual and reasoned per token/category: internal class identity, `LOCALAPPDATA`, font/locale identifiers, the exact renderer-stage set, `HRESULT 0x`, `arpg_game.exe`, each exact required asset path, `InfiniteDungeon/save`, and the `ShellExecuteW` verb `open`. No source file is exempted wholesale.

## Full verification

- Full platform runtime after GREEN:
  - Command: `arpg_platform_tests.exe`.
  - Exit: 1; `671 cases, 2 failures`.
  - Task 8 host-window failure is gone. The only failures are the pre-existing ecology mismatch and the deliberate `actual 671 != expected 670` baseline; neither was absorbed or hidden.
- Menu/HUD focused runtime:
  - Command: set `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1`, run `arpg_platform_tests.exe`, remove the variable in `finally`.
  - Exit: 0; `183 cases, 0 failures`.
- Completion-gate source guards:
  - `platform.pause_menu_host_layout_source` -> passed, 1/1.
  - `platform.hole_prompt_binding_source` -> passed, 1/1.
  - `platform.pause_menu_state_boundary` -> failed before the other tests under the combined stop-on-failure preset because BASE `pause_menu_state.cpp` includes `ui_text_catalog.hpp`, while BASE `pause_menu_state_boundary_test.cmake` does not allow it.
- `git diff 0179b69... -- src/platform/raylib/pause_menu_state.cpp tests/platform/pause_menu_state_boundary_test.cmake` exits 0 with no diff, proving that source-boundary failure is unchanged from Task 8 BASE and outside Task 8 ownership.
- `git diff --check` -> exit 0.

## Self-review and concerns

- Commit: `9e4215b` (`feat: localize game and launcher titles`), one Task 8 commit from BASE `0179b69`.
- Changed tracked files are restricted to five declared Task 8 files: two production files and three tests. `launcher_renderer.cpp` was audited and guarded but deliberately left unchanged because its production copy was already correct.
- No Release build, deployment, installation, package, shortcut, manifest, executable/asset rename, transaction, default directory, argument parser, inventory, or passive-tree work was performed.
- `make_launch_request()` and `default_save_directory()` production code remain byte-for-byte unchanged; launcher unit behavior still proves quoted argument-free `arpg_game.exe`, no `--`, and `%LOCALAPPDATA%\InfiniteDungeon\save`.
- Concern: the plan-completion command is not fully green because `platform.pause_menu_state_boundary` is already stale at BASE. Fixing that test or refactoring `pause_menu_state.cpp` is outside Task 8's declared files and must be handled separately; it is not silently waived here.
- Known baseline concerns remain visible: ecology material coverage mismatch and full-platform case count actual 671 vs expected 670.

## Fix round 1 - reviewer findings

### Technical verification

- I1 confirmed: the original `$technicalAllowlist` only checked that its descriptive fields were non-empty. It was not consulted when enumerating or filtering authored player-visible literals, so preserving the expected Chinese token while adding a second English literal could pass.
- I2 confirmed: all structural regexes and `renderer_stage_name` extraction operated on raw source. Expected calls inside comments, `#if 0`, or a constant-false branch could satisfy the old existence checks.
- Production title and existing Chinese copy remained untouched in this round.

### RED

- Test-only change first: added four reversible in-memory mutants while leaving the old guard implementation intact.
- Command: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\launcher\launcher_window_smoke_test.ps1 -SelfTestChineseCopyGuard`.
- Exit: 1.
- Exact accepted-mutant summary from the old guard:
  - `added English error while preserving Chinese error`;
  - `comment decoy preserves Chinese DirectWrite button`;
  - `inactive preprocessor branch preserves Chinese fallback button`;
  - `constant-false duplicate preserves Chinese fallback button`.

### GREEN implementation

- Added a C++ lexical pass that removes line/block comments while preserving strings, character literals, escapes and line structure; unterminated lexical states fail closed.
- All existing structural patterns and renderer-stage extraction now consume comment-free source.
- The audited player-copy sources fail closed on conditional preprocessing, raw strings unsupported by the narrow lexer, and constant-false branches.
- Added an active prefixed-string enumerator for `L`, `u8`, `u`, and `U` literals.
- Replaced the decorative allowlist with executable contracts. Every player or technical literal is keyed by source file, exact source spelling, exact active occurrence count and reason. Unknown literals, duplicate/missing contracts and count drift all fail. The technical allowlist is now part of the actual filter, not documentation-only data.
- GREEN command: same `-SelfTestChineseCopyGuard` command.
- Exit: 0; output: `Task 8 Chinese-copy guard accepted pristine sources and rejected 11 mutations`.

### Fresh verification

- Debug configure/build with fixed MSVC environment:
  - `cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_launcher_tests --parallel 2` -> exit 0;
  - `cmake --build --preset windows-msvc-debug --target arpg_launcher --parallel 2` -> exit 0.
- Launcher source guard + stale-cleanup self-test -> exit 0; guard rejected 11 mutations.
- Launcher units -> `13 cases, 0 failures`.
- Serial `launcher.window_smoke` -> 1/1 passed in 4.27 sec.
- MENU/HUD focused platform -> `183 cases, 0 failures`.
- Full platform -> `671 cases, 2 failures`; only the known ecology material mismatch and deliberate `actual 671 != expected 670` remain.
- `git diff --check` -> exit 0.

### Fix-round concerns

- Fix commit: `86ebaa7` (`test: harden launcher Chinese copy guard`).
- The source audit intentionally fails closed if these five narrow audited files later introduce conditional compilation or raw string literals; such a change requires an explicit guard design update rather than an implicit bypass.
- The pre-existing `platform.pause_menu_state_boundary` completion-gate concern from the original Task 8 report is unchanged and outside this fix round.

## Fix round 2 - all-authored-string audit

### Technical verification

- I1 confirmed: the round-1 lexer deliberately collected only `L`, `u8`, `u`, and `U` string literals. An ordinary literal such as `"Launch failed"` could therefore reach a widening macro without participating in the exact literal contract.
- I2 confirmed: recognizing selected spellings of constant-false statements is not a complete activity proof. `if (1 == 0)`, `if (!true)`, an uncalled lambda, or another dead construct could retain the expected Chinese call as a decoy.
- Minor lexer findings confirmed: a quote in the valid C++17 number `1'000U` entered character-literal mode, and a backslash-continued `//` comment incorrectly ended at its first newline.
- Root approach: every string literal authored in each of the five narrow audited sources is now inventoried, including ordinary include-path literals and strings in dead code. Conditional/macro preprocessing remains fail-closed. This removes the need to guess every possible constant-false syntax.
- Production titles and existing Chinese production copy were not changed.

### RED

- Test-only change first, before the guard implementation: added a reversible mutant that keeps the expected Chinese fallback call under `if (1 == 0)` but displays `TASK8_WIDEN("Start Game")`; also added valid lexical probes for `1'000U` and a backslash-continued line comment.
- Command: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\launcher\launcher_window_smoke_test.ps1 -SelfTestChineseCopyGuard`.
- Exit: 1.
- Exact mismatch from the round-1 guard: `accepted mutations: ordinary English macro with noncanonical dead Chinese decoy; rejected valid variants: C++17 digit separator; continued line comment`.
- This is a real false GREEN of the old guard: the live English ordinary string was skipped, while the dead Chinese call preserved the old structural count.

### GREEN implementation and mutation sensitivity

- Replaced the prefixed-only inventory with `Get-Task8CppStringLiterals`, which enumerates all authored ordinary, `L`, `u8`, `u`, and `U` string literals after comment removal.
- Added exact per-file contracts for every ordinary include-path literal. File, exact source spelling, positive occurrence count, and non-empty reason are all mandatory and are used by the executable filter; unknown literals, count drift, duplicate contracts, and missing literals fail.
- Added a second reversible mutant that preserves the Chinese LOCALAPPDATA error and appends a direct ordinary `"Launch failed"` runtime error, proving the ordinary-string inventory independently of macro fail-closed behavior.
- The narrow audited sources now fail closed on `#define` and `#undef` as well as conditional preprocessing. Macro-based player copy cannot bypass the literal inventory.
- Character-literal recognition now distinguishes C++17 numeric separators, and line-comment removal remains in comment state across both LF and CRLF backslash continuations.
- GREEN command: the same `-SelfTestChineseCopyGuard` command.
- Exit: 0; output: `Task 8 Chinese-copy guard accepted pristine sources and 2 lexical variants, and rejected 13 mutations`.
- The 13 rejected mutations include all round-1 comment/inactive/dead-call cases, the broad-file allowlist attempt, the macro + noncanonical-dead-decoy bypass, and the direct ordinary English runtime error. The two positive variants prove the digit separator and continued-comment fixes do not reject valid audited C++17 text.

### Fresh verification

- Configure: `. .\scripts\Configure.ps1 -Preset windows-msvc-debug` -> exit 0; MSVC Debug generation completed.
- Debug builds, bounded to `--parallel 2`:
  - `cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_launcher_tests --parallel 2` -> exit 0 (`ninja: no work to do` after fresh configure);
  - `cmake --build --preset windows-msvc-debug --target arpg_launcher --parallel 2` -> exit 0 (`ninja: no work to do`).
- Launcher units: `arpg_launcher_tests.exe` -> exit 0, `13 cases, 0 failures`.
- Guard/self-test: exit 0, pristine sources plus 2 lexical variants accepted and all 13 mutations rejected.
- Serial Windows smoke: `ctest --preset windows-msvc-debug -R '^launcher\.window_smoke$' --output-on-failure -j 1` -> 1/1 passed (final rerun 4.41 sec).
- Menu/HUD focused platform: with `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1`, `arpg_platform_tests.exe` -> exit 0, `183 cases, 0 failures`.
- Full platform: executable exit 1, exactly two `[FAIL]` records and `671 cases, 2 failures`. They are still only ecology material coverage and `host settings runtime contract case count: actual 671 != expected 670`.
- The first full-output wrapper incorrectly required a cross-stream line order and exited 9 even though it captured the expected two failures. It was immediately replaced and rerun with an order-independent exact-set assertion; that wrapper exited 0 and confirmed executable exit 1, one ecology failure, one case-count failure, one exact summary, and no additional `[FAIL]` records.
- `git diff --check` -> exit 0. Diff from fix-round-1 commit `86ebaa7` contains only `tests/launcher/launcher_window_smoke_test.ps1`; `git diff 86ebaa7 -- src` is empty.

### Self-review and concerns

- Fix-round-2 commit: `08d1e91` (`test: audit all launcher source strings`).
- This round changes only the narrow source guard and its reversible self-test; production source is byte-for-byte unchanged from fix round 1.
- The literal contract is intentionally strict: adding any authored string, including a compile-time include path or a string in dead code, requires an explicit per-file count and reason. Conditional or macro preprocessing and raw literals in these five audited sources remain fail-closed and require an explicit guard design update.
- No Release build, deployment, installation, package, shortcut, manifest, executable/asset, transaction, launch-argument, default-save-path, inventory, or passive-tree work was performed.
- The pre-existing `platform.pause_menu_state_boundary` concern and the two known full-platform baseline failures remain visible and unchanged.

## Fix round 3 - formatter control-flow binding

### Technical verification

- Important finding confirmed: the round-2 formatter assertion searched all of comment-free `launcher_main.cpp` for the canonical return text. A syntactically valid uninvoked lambda inside `render_failure_message` could retain that exact return expression while the function's real return became `std::wstring{stage}`. This preserved every string literal/count and the single `renderer_stage_name` call, so the old guard accepted it.
- Minor lexer finding confirmed: `Test-Task8CppCharacterLiteralStart` used the character before an apostrophe as a delimiter heuristic. In valid `return'"';`, the preceding `n` caused it to skip the opening apostrophe, misread the double quote as a string start, and reject otherwise valid C++17 source.
- Production source and all production copy remained untouched.

### RED

- Test-only changes first: added the complete in-memory mutant `uninvoked lambda decoys renderer failure return`, which reuses all existing literals and returns only `stage` on the real path; added a positive lexical variant containing exact valid syntax `return'"';`.
- Command: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\launcher\launcher_window_smoke_test.ps1 -SelfTestChineseCopyGuard`.
- Exit: 1.
- Exact self-test mismatch: `accepted mutations: uninvoked lambda decoys renderer failure return; rejected valid variants: character literal after return without whitespace`.
- The failure directly demonstrates both reported defects against the round-2 implementation, before the guard fix.

### GREEN implementation and mutation sensitivity

- `render_failure_message` is now isolated exactly once between its real signature and the following `fallback_rectangle` definition. Checks operate only on that extracted, comment-free function body.
- The isolated body must contain exactly one `return`, must not contain a lambda capture/call introducer, and its final real return statement must be the canonical `渲染失败（<stage>，HRESULT 0x<code>）` expression. A decoy elsewhere in the file or inside a local lambda can no longer satisfy the formatter contract.
- Character-literal recognition now treats an apostrophe as a numeric separator only when both adjacent characters are alphanumeric and the backward-scanned token begins as a numeric token (digit, or dot followed by digit). `1'000U` remains accepted while `return'"';` enters and exits character-literal state correctly.
- GREEN command: the same `-SelfTestChineseCopyGuard` command.
- Exit: 0; output: `Task 8 Chinese-copy guard accepted pristine sources and 3 lexical variants, and rejected 14 mutations`.
- All 13 prior mutations remain rejected; the new uninvoked-lambda control-flow mutant is the fourteenth. All two prior lexical variants remain accepted; the no-whitespace character literal is the third.

### Fresh verification

- Configure: `. .\scripts\Configure.ps1 -Preset windows-msvc-debug` -> exit 0; Debug generation completed.
- Bounded build gate: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_launcher_tests arpg_launcher --parallel 2` -> exit 0 (`ninja: no work to do`).
- Launcher units -> exit 0, `13 cases, 0 failures`.
- Guard/self-test -> exit 0, 3 lexical variants accepted and all 14 mutations rejected.
- Serial Windows smoke: `ctest --preset windows-msvc-debug -R '^launcher\.window_smoke$' --output-on-failure -j 1` -> 1/1 passed in 4.34 sec.
- Menu/HUD focused platform: `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1` plus `arpg_platform_tests.exe` -> exit 0, `183 cases, 0 failures`.
- Full platform: executable exit 1; order-independent exact-set wrapper exits 0 and records exactly two failures: ecology material coverage and `host settings runtime contract case count: actual 671 != expected 670`; exact summary `671 cases, 2 failures`.
- `git diff --check` -> exit 0. Diff from round-2 commit `08d1e91` contains only `tests/launcher/launcher_window_smoke_test.ps1`; `git diff 08d1e91 -- src` is empty.

### Self-review and concerns

- Fix-round-3 commit: `d14f61b` (`test: bind launcher formatter return path`).
- Round 3 modifies only the narrow source guard and its reversible pressure tests. The full per-file authored-string contracts and the 13 prior mutations are retained.
- The formatter-body audit intentionally fails closed if the formatter gains lambdas, multiple return statements, or changes its neighboring function boundary; such a production redesign requires an explicit audit update.
- No Release build, deployment, installation, package, shortcut, manifest, executable/asset, transaction, launch-argument, default-save-path, inventory, or passive-tree work was performed.
- The pre-existing `platform.pause_menu_state_boundary` concern and the two known full-platform baseline failures remain visible and unchanged.

## Fix round 4 - lexer-aware formatter body boundary

### Technical verification

- The Important finding was confirmed: the round-3 formatter-body regex ended at a column-zero `}` followed by the token sequence `RECT fallback_rectangle`, without proving either token belonged to the next function definition.
- A syntactically valid nested block can therefore close at column zero, retain the canonical formatter return inside that block, declare a local `RECT fallback_rectangle{}`, and place `return std::wstring{stage};` later in the real function body. The old lazy regex stopped at the nested close, saw one canonical return, and accepted the broken formatter.
- Production source and production copy remain byte-for-byte unchanged in this round.

### RED

- Added the reversible in-memory mutant `local fallback rectangle creates a false formatter boundary` first. It reuses every existing authored string, introduces no preprocessor macro, and keeps a complete C++17 function definition.
- Command: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\launcher\launcher_window_smoke_test.ps1 -SelfTestChineseCopyGuard`.
- Exit: 1.
- Exact mismatch: `accepted mutations: local fallback rectangle creates a false formatter boundary; rejected valid variants:`. This proves the round-3 guard was false GREEN before the extractor change.
- Added direct parser behavior probes before implementation; their first run exited 1 because `Get-Task8CppFunctionBody` did not yet exist.

### GREEN implementation and mutation sensitivity

- Removed the neighboring-`fallback_rectangle` formatter regex completely.
- Added a same-length structural C++ source mask. It removes line and block comments, masks escaped ordinary string and character literals, preserves code-token indexes, and fails closed on unsupported raw strings or unterminated lexical states.
- `Get-Task8CppFunctionBody` locates exactly one real `render_failure_message` signature in the structural source, requires its next non-whitespace token to be `{`, and walks brace depth to that opening brace's matching `}`. Duplicate signatures, declarations without a body, and unpaired braces fail closed.
- Direct extractor probes cover a signature decoy in a string, a signature decoy in a block comment, braces in strings, character literals, line comments and block comments, nested code braces, duplicate real signatures, and an unterminated definition.
- GREEN command: the same `-SelfTestChineseCopyGuard` command.
- Exit: 0; output: `Task 8 Chinese-copy guard accepted pristine sources and 3 lexical variants, and rejected 15 mutations`.
- All 14 prior pressure cases remain rejected, the new false-boundary mutant is the fifteenth, and all three prior valid lexical variants remain accepted.

### Fresh verification

- Configure plus bounded Debug build: `. .\scripts\Configure.ps1 -Preset windows-msvc-debug; cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_launcher_tests arpg_launcher --parallel 2` -> exit 0; generation completed and Ninja reported no work to do.
- Launcher units -> exit 0, `13 cases, 0 failures`.
- Guard/self-test -> exit 0, pristine sources plus 3 lexical variants accepted and all 15 mutations rejected.
- Existing stale-cleanup self-test -> exit 0.
- Serial Windows smoke: `ctest --preset windows-msvc-debug -R '^launcher\.window_smoke$' --output-on-failure -j 1` -> 1/1 passed in 4.56 sec.
- Menu/HUD focused platform -> exit 0, `183 cases, 0 failures`.
- Full platform executable -> exit 1; an order-independent exact-set wrapper exited 0 and confirmed exactly the existing ecology material mismatch plus `host settings runtime contract case count: actual 671 != expected 670`, with summary `671 cases, 2 failures`.
- Final diff audit -> exit 0: `git diff --check` is clean, the tracked round-4 diff contains only `tests/launcher/launcher_window_smoke_test.ps1`, and `git diff d14f61b -- src` is empty.

### Self-review and concerns

- Fix-round-4 commit: `be0cb64` (`test: parse launcher formatter body structurally`).
- Round 4 changes only `tests/launcher/launcher_window_smoke_test.ps1` plus this ignored cumulative report; no production file changed.
- No production functionality, Release build, deployment, installation, package, shortcut, manifest, executable/asset, transaction, launch argument, default save path, inventory, or passive-tree work changed.
- The source audit still intentionally fails closed on conditional or macro preprocessing and raw literals in the five narrow audited production sources.
- The pre-existing `platform.pause_menu_state_boundary` concern and the two known full-platform baseline failures remain visible and unchanged.

## Fix round 5 - player-safe platform errors and opaque status palette

### Technical verification

- The Win32 platform formatter appended `FormatMessageW` output directly to the player-visible message. On an English Windows installation this leaked English after an otherwise Chinese operation and numeric error code.
- The Direct2D status rectangle was outline-only. Error text used pure red directly over the blue gradient, whose worst sampled contrast was below the required 4.5:1. The native fallback used a dark card, but it did not share a tested palette with Direct2D.
- The round-4 guard proved the canonical `render_failure_message` body and globally unique `renderer_stage_name` call independently, but did not prove that the stage call was inside that formatter or that `paint()` and `handle_renderer_failure()` actually consumed it.

### RED

- Added a syntactically valid in-memory forge that leaves a canonical but disconnected `render_failure_message`, moves the only `renderer_stage_name(result.stage)` call into an external English helper, and redirects both real consumers to that helper. The round-4 guard exited 1 with exact mismatch `accepted mutations: disconnected renderer formatter consumers`.
- Added pure behavior tests before production interfaces. The first `arpg_launcher_tests` build exited 1 because `format_platform_failure_message`, `LauncherColor`, and `launcher_status_style` did not exist.
- Added the opaque-backing renderer source seam before implementation. The guard exited 1 with `DirectWrite status must fill an opaque palette backing before its border and text`.
- The first real window smoke after the production palette change exited 1 with otherwise-correct button pixels and `pureRed=0`. Investigation showed the readiness probe was coupled to the removed pure-red foreground, not a rendering failure.

### GREEN implementation and mutation sensitivity

- Added injectable pure `format_platform_failure_message(operation, error_code, system_detail)`. It returns only `<中文操作>（错误代码 N）`; the dynamic system-language detail is explicitly excluded from the player value and sent only to `OutputDebugStringW` diagnostics.
- The real failed-process-launch test now asserts the complete exact player message. A deterministic injected detail `The system cannot find the file specified.` is also asserted absent.
- Added one shared ready/error `LauncherStatusStyle` consumed by both Direct2D and GDI fallback. Both backings and foregrounds are fully opaque. Measured WCAG ratios are approximately 13.48:1 for ready and 12.10:1 for error.
- Direct2D now sets the shared palette, fills the rounded status backing, draws its border, then draws the text. The fallback uses the same backing and foreground colors.
- The smoke readiness probe now samples a stable interior status pixel and requires the opaque error backing `RGB(55,16,24)`; the formal smoke repeats that exact assertion. It no longer depends on a specific foreground hue.
- The guard now isolates both real renderer-failure consumer bodies, requires their exact `render_failure_message(result)` calls, requires the only stage lookup inside the formatter, and rejects dynamic system-detail rejoining plus outline-only status rendering.
- Final guard/self-test output: pristine sources and 3 lexical variants accepted; all 18 mutations rejected.

### Fresh verification

- Bounded Debug build: `cmake --build --preset windows-msvc-debug --target arpg_launcher_tests arpg_launcher --parallel 2` -> exit 0.
- Launcher units after the final test tightening -> `15 cases, 0 failures`.
- Task 8 guard/self-test -> exit 0; 3 valid lexical variants accepted and 18 mutants rejected.
- Serial Windows window smoke -> 1/1 passed in 4.56 sec after replacing the stale pure-red readiness probe with the opaque-backing sample.
- Menu/HUD focused platform -> `183 cases, 0 failures`.
- Full platform -> executable exit 1 with exactly the two pre-existing failures: ecology material alpha bounds and `actual 671 != expected 670`; summary `671 cases, 2 failures`.
- Scoped `git diff --check` -> exit 0. Forbidden-scope diff for `launcher_core.cpp`, `raylib_host.hpp`, scripts, manifests, resources and assets is empty.

### Self-review and scope

- Fix-round-5 commit: `94c27d6` (`fix: harden launcher player feedback`).
- No default title, launcher title, `make_launch_request`, save directory, manifest, resource, shortcut, asset, deployment, Release, inventory, passive-tree or transaction behavior changed.
- Unrelated concurrent Task 7 and pause-boundary edits were left unstaged and are not part of this commit.
- This is fix round 5/5 and now awaits the controller's independent final review; it is not self-declared review-clean.
