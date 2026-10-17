# Full Chinese Menus/HUD — completion gate fix report

Date: 2026-08-10

## Scope

- Repair the stale `platform.pause_menu_state_boundary` allowlist only for the exact quoted `ui_text_catalog.hpp` dependency.
- Keep `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY` unchanged at 183 cases.
- Add a separate 86-case completion supplement for the four omitted suites: `host_input`, `host_settings_runtime`, `dungeon_runtime`, and `host_window_lifetime`.
- Add one executable Windows completion entry that also covers the Task 1/2/4/5/6/7 guards, mutation self-tests and parser; Task 8 launcher units, Chinese-copy guard and window smoke; and compile/link-only coverage of `arpg_stage11c_hud_formal`.
- No production source, release, deployment, shortcut, inventory/progression, or asset change belongs to this fix.

## TDD evidence

1. Added a legal exact-include self-test for `#include "ui_text_catalog.hpp"` before changing the allowlist.
2. RED command:

   ```powershell
   ctest --preset windows-msvc-debug --output-on-failure -R '^platform\.pause_menu_state_boundary$'
   ```

   Result: exit 1; the guard reported the expected `false-positive:UI text catalog include` failure.
3. Minimal GREEN: added only `"ui_text_catalog\.hpp"` to the exact quoted-header alternatives. No wildcard or production change.
4. GREEN command: same exact CTest command.

   Result: 1/1 passed, 0 failed, exit 0.

## Completion coverage

`ARPG_FULL_CHINESE_UI_MENU_HUD_COMPLETION_ONLY` runs exactly:

- `host_input_suite()` — 15 cases
- `host_settings_runtime_suite()` — 15 cases
- `dungeon_runtime_suite()` — 52 cases
- `host_window_lifetime_suite()` — 4 cases

Total: 86 cases. Fresh focused execution result: `86 cases, 0 failures`, exit 0.

The PowerShell gate builds with `--parallel 2` and names `arpg_stage11c_hud_formal` as a build target only. It does not run `stage11c.hud_formal`. Its exact guard regex contains 13 tests covering the pause boundary/layout, Task 4 structured HUD guards/parser/self-tests, Task 5 hole prompt guard/self-test, Task 6 recovery guard/self-test, and Task 7 overlay guard/self-test. It then runs `launcher.units`, the Task 8 Chinese-copy guard self-test, and `launcher.window_smoke`.

Fresh focused guard execution: 13/13 passed, 0 failed, exit 0, total 415.21 seconds.

## Static verification

- PowerShell AST parse: 0 errors.
- Scoped `git diff --check`: clean.
- Required selector, formal-target, guard, launcher-unit, Chinese-guard, and smoke tokens: present.

## Aggregate execution evidence

The complete aggregation script ran once at workspace HEAD `7b0e71e`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File \
    .\tests\platform\full_chinese_menu_hud_completion_gate.ps1
```

Result: exit 0.

- Configure: succeeded.
- Build: succeeded with `--parallel 2`; `arpg_platform_tests`, `arpg_launcher_tests`, `arpg_launcher`, and compile/link-only `arpg_stage11c_hud_formal` completed.
- MENU/HUD selector: 183 cases, 0 failures.
- Completion supplement: 86 cases, 0 failures.
- Exact guards/self-tests/parser: 13/13 passed, 0 failed, 411.56 seconds.
- `launcher.units`: 1/1 passed.
- Task 8 Chinese-copy guard: accepted pristine sources plus 3 lexical variants and rejected 18 mutations.
- `launcher.window_smoke`: 1/1 passed, 4.70 seconds.
- Final `git diff --check`: exit 0; only line-ending conversion warnings were printed.

Task 7 round 4 is expected to change HEAD after this run. Therefore this is valid aggregate evidence for the `7b0e71e` workspace, not the final completion claim. Re-run the same aggregate entry once after Task 7 round 4 is committed and the shared worktree is stable.

## Final aggregate execution at Task 7 round 4 HEAD

The same complete aggregation script was rerun after Task 7 round 4 committed at HEAD `8ec999b`. Result: exit 0.

- Configure: succeeded.
- Build: succeeded with `--parallel 2`; platform tests, launcher tests, launcher, and Stage11C formal compile/link-only targets completed.
- MENU/HUD selector: 183 cases, 0 failures.
- Completion supplement: 86 cases, 0 failures.
- Exact guards/self-tests/parser: 13/13 passed, 0 failed, 417.27 seconds.
- `launcher.units`: 1/1 passed.
- Task 8 Chinese-copy guard: accepted pristine sources plus 3 lexical variants and rejected 18 mutations.
- `launcher.window_smoke`: 1/1 passed, 4.65 seconds.
- Final `git diff --check`: exit 0; only line-ending conversion warnings were printed.

This is the submission evidence for the completion-gate fix on top of Task 7 round 4.

Scoped commit: `7288ec2 test: complete Chinese menus HUD gate` (exactly four files).

## Review fix round 1

Reviewer findings addressed:

- `Invoke-PlatformSelector` now snapshots and clears every `ARPG_*_ONLY` variable, restores exact original values after success or exception, and accepts only one exact `183 cases, 0 failures` or `86 cases, 0 failures` summary.
- The 13 guards and launcher unit/smoke tests now run one name per CTest invocation with an escaped `^...$` regex, `--no-tests=error`, and `--parallel 1`; any native failure stops the sequence.
- The gate checks both `git diff --check` and `git diff --cached --check`.

Strict TDD evidence:

- Old implementation RED: pure PowerShell self-test exit 1 with seven focused failures covering missing case-summary validation/API, absent exact CTest runner, absent cached check, and unproven failure propagation.
- GREEN: pure PowerShell self-test 7/7, exit 0.
- Final aggregate base: Task 7 final HEAD `d18ee04`.
- Configure and parallel-2 build: passed; Stage11C formal remained compile/link-only.
- Exact selector summaries: `183 cases, 0 failures`; `86 cases, 0 failures`.
- Thirteen exact fail-closed CTest invocations: all 1/1 passed. Task 7 guard self-test took 287.75 seconds.
- Launcher units: 1/1 passed.
- Chinese-copy guard: pristine plus 3 lexical variants accepted; 18 mutations rejected.
- Launcher window smoke: 1/1 passed, 4.85 seconds.
- Working-tree and cached diff checks: both passed; aggregate exit 0.

Scoped review-fix commit: `67e5b84 test: make Chinese UI completion gate fail closed` (exactly two files).
