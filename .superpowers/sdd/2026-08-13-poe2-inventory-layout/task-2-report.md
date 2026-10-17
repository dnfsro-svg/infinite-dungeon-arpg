# Task 2 report: skill stones consume InventoryShellLayout

## Scope

- Task 2 changes are limited to the six files named in `task-2-brief.md`.
- Added `active_skill_loadout_layout(InventoryShellLayout)`; the retained
  `active_skill_loadout_layout(int, int)` wrapper only constructs the shared
  shell and delegates.
- The shell is the overload's coordinate source: `panel = shell.content`,
  `remove_button = shell.action_bounds`, and the two temporary page-button
  fields only mirror the corresponding `shell.tabs` entries.
- Five main slots remain interactive in the left panel, five support slots
  remain read-only in its lower area, and unequipped skill stones are laid out
  in the right panel.
- Stage17 builds one shell/layout pair and both skill-page clicks address
  `shell.tabs[InventoryPage::skill_stones]` directly.
- The temporary `InventoryPage::equipment_materials = equipment` alias and
  both temporary `ActiveSkillLoadoutLayout` tab fields were retained for Task 4.
- No Task 3 or Task 4 work was performed.

## TDD RED

Tests and exact source expectations were changed before production code.

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

Expected RED: MSVC C2660 at
`tests/platform/active_skill_loadout_view_tests.cpp:74` because
`active_skill_loadout_layout` had no one-argument
`InventoryShellLayout` overload. Configure succeeded, so the first cause was
the missing production API rather than environment or test syntax.

## Exact-guard baseline repair

The Task 2 Stage17 exact guard initially reached an unrelated shared
window-lifecycle validator and exposed two deterministic baseline defects:

1. `evidence_cpp_normalize_token_whitespace` produced
   `void*detach_ime_context_noexcept(` while the boundary signature still
   expected a space after `*` (first late failure at
   `input_latency_source_test.cmake:1576`).
2. After normalizing that signature, exhaustive lifecycle diagnostics found
   stale ownership expectations: death overlay `0` vs old `2`, HUD `3` vs old
   `4`, pause menu `0` vs old `1`, and shared UI font `1` vs old `0`
   (`input_latency_source_test.cmake:1596` named death overlay first).

An earlier implementation of the new exact-two-click guard used
`string(REGEX MATCHALL ...)` over the full normalized function and consumed a
CPU core without completing. The authorized stalled processes were
terminated. A controlled linear-search diagnostic proved the two-hit/one-hit
semantics, and the final guard uses bounded `string(FIND)`/`string(SUBSTRING)`
searches without weakening the hash or mutations.

The baseline fix changed exactly three separate files and was committed as:

```text
108c5d6 fix(test): align raylib lifecycle ownership
```

Before the formal rerun, the ignored exhaustive production scan completed:

```text
-- lifecycle invalid_count=0
-- lifecycle invalid_list=
```

The sole final exact-guard run was:

```powershell
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error --parallel 1 -R '^platform\.input_latency_source$'
```

Exact result:

```text
1/1 Test #102: platform.input_latency_source .... Passed 618.11 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) = 618.12 sec
```

The input guard was not run again after the baseline commit.

## Focused GREEN

Because the baseline commit changed the raylib CMake allowlist, Configure and
build were deliberately run in the same PowerShell process so the MSVC
environment remained active:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_platform_tests arpg_stage12_material_formal `
    arpg_stage17_skill_stones_game_validation `
  --parallel 2
```

Result: exit 0; all three requested targets compiled/linked. A preceding split
process invocation failed before compilation with raylib.h C1083
`stdarg.h: No such file or directory` because the dot-sourced VS environment
did not persist into the next PowerShell process. No code was changed for that
environment-only failure; the authorized same-process invocation above is the
final result.

```powershell
powershell.exe -NoProfile -File tests/platform/full_chinese_inventory_selector.ps1 `
  -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
```

Exact result: `120 cases, 0 failures`.

## Self-review

- `git diff --check` passed.
- The Task 2 working diff contains exactly the six brief-listed files; the
  baseline repair is isolated in commit `108c5d6`.
- `remove_button` is assigned directly from `shell.action_bounds`; both
  temporary tab fields are direct shell-tab copies; the integer wrapper has
  one delegation return statement.
- All five main slots remain in command hit testing. Support slots are only
  rendered/layout-tested and are absent from click-command handling; existing
  select/equip/swap/remove and accepted-authority tests remain GREEN.
- Both Stage17 skill-page clicks use the shared shell tab directly, and the
  source guard requires exactly two occurrences plus the exact function hash
  and reversible mutations.
- Independent read-only Task 2 review returned `C=0 / I=0 / M=0`, Ready.
- No window executable, Stage12/Stage17 executable, full `Test.ps1`, Release,
  or formal-game execution was run.

## Status

Task 2 focused validation is GREEN and committed as:

```text
f436a62 refactor(ui): place skill stones in inventory shell
```
