# T0 implementation report

## Scope

Only T0 from `docs/superpowers/plans/2026-08-11-full-chinese-ui-inventory-progression-revised.md` on base `2b6e4dd`.

## Preflight

- Linked worktree: `E:/game/.git/worktrees/loot-suction-animation`
- Branch: `codex/full-chinese-ui`
- Clean baseline: yes
- TDD status: tests not written yet

## RED evidence

- Selector self-test before implementation:
  `pwsh -NoProfile -File tests/platform/full_chinese_inventory_selector_self_test.ps1`
  exited 1 because `full_chinese_inventory_selector.ps1` did not exist.
- Real selector before C++ selector wiring:
  `pwsh -NoProfile -File tests/platform/full_chinese_inventory_selector.ps1 -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe`
  exited 1 after the unrecognized selector fell through to the preserved
  baseline `671 cases, 2 failures`.
- C++ test-first build:
  configure plus `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2`
  failed with MSVC C1083 for missing `ui_text_layout.hpp`.
- In-place fixed-buffer regression mutation:
  the focused selector reported `120 cases, 1 failures` at
  `in_place_fit.drawable` before the implementation stopped clearing the
  source/output alias.

## GREEN evidence

- `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2`: exit 0, no compiler warnings.
- `pwsh -NoProfile -File tests/platform/full_chinese_inventory_selector_self_test.ps1`: seven fake-runner branches passed.
- `pwsh -NoProfile -File tests/platform/full_chinese_inventory_selector.ps1 -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe`: unique exact `120 cases, 0 failures`.
- Allocation line: `[ui-text-layout] iterations=100000 allocations=0`.

## Verification

- Fresh full platform run with every `ARPG_*_ONLY` cleared produced exactly
  `676 cases, 2 failures`.
- The only failures remained the pre-existing ecology alpha-bounds mismatch
  and expected 675 versus actual 676 count drift.
- `git diff --check`: exit 0.
- `git diff --cached --check`: exit 0 before staging.
- Scope inventory: eight T0 files only; zero changes outside T0.

## Self-review

- Five cases map one-to-one to centering/containment, UTF-8 ellipsis,
  label/value separation, 100,000 zero-allocation calls, and fail-closed
  invalid/impossible inputs.
- Selector accepts exactly one `120 cases, 0 failures` summary, rejects
  nonzero/error/wrong/duplicate/mixed summaries, and restores all selector
  environment variables on success and exceptions.
- `ui_text_layout_suite` is present in both the dedicated selector and the
  default full suite; default expected changed only from 670 to 675.
- No player-visible production string, T1 file, game rule, or save format was
  changed.
- Independent read-only review: spec compliance PASS; code/test quality
  APPROVED; zero Critical, Important, or Minor findings.

## Commit

- `6110945 feat(ui): add inventory layout baseline`
- Exactly eight T0 files; no T1 or unrelated file included.
