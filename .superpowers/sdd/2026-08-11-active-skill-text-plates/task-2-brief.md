# Task 2 Brief — Opaque Renderer Plates, Accurate Audit, Guard, and Manual Proof

## Workspace and baseline

- Work only in `E:/game/.worktrees/loot-suction-animation`.
- Branch: `codex/full-chinese-ui`.
- BASE: `34eed403e459e490110f200fb2886d9a9c3bffdc`.
- Read completely before editing:
  - `docs/superpowers/specs/2026-08-11-active-skill-text-plates-design.md`
  - `docs/superpowers/plans/2026-08-11-active-skill-text-plates.md`
  - this brief
- Write the final task report with `apply_patch` to:
  `.superpowers/sdd/2026-08-11-active-skill-text-plates/task-2-report.md`.

## Scope

Modify only:

- `src/platform/raylib/ui_text_renderer.cpp`
- `src/platform/raylib/ui_text_bounds_audit.hpp`
- `src/platform/raylib/ui_text_bounds_audit.cpp`
- `src/platform/raylib/active_skill_renderer.cpp`
- `tests/platform/hud_renderer_structured_model_source_test.cmake`
- `tests/platform/hud_renderer_structured_model_source_self_test.cmake`
- `tests/platform/CMakeLists.txt`

Do not change input, cooldown rules, slot/icon/material geometry, catalogs,
terminology, glyphs, inventory UI, other HUD backings, case counts, or Task 1
pure-layout contracts. Preserve all unrelated work; never reset Git or delete
files.

## Required behavior

1. `draw_crisp_ui_text` draws its shadow only when `shadow_pixels > 0`.
   Foreground behavior and the default two-pixel behavior stay unchanged.
2. `record_ui_text_bounds` gains final `int shadow_pixels = 2`; actual bounds
   use `max(shadow_pixels, 0)`. Existing callers retain two pixels; active
   skill text passes zero.
3. `ActiveSkillRenderer::draw_hud` returns before its first draw when
   `!layout.valid`.
4. It consumes Task 1's aggregate plan using the real shared font and a
   `MeasureTextEx` adapter/context. No default font, heap allocation,
   `TextFormat`, `snprintf`, `DrawText`, per-frame string construction, or
   fixed-position text recomputation.
5. Draw order per slot remains artwork first: slot/fallback, icon, cooldown
   fill/edge, then visible plates and fitted text. For each visible plate:
   - opaque `DrawRectangleRec(plan.backing, Color{5,9,16,255})` through
     `active_skill_text_plate_backing()`;
   - `record_ui_text_bounds` with exact role/backing/position/font size/spacing
     and final `shadow_pixels=0`;
   - `draw_crisp_ui_text` with primary text and `shadow_pixels=0`.
6. Empty = key only; ready = key+name; cooling = all three, as carried by the
   Task 1 plan. If font unavailable, only text plates fail closed; valid slot
   artwork still draws.

## TDD and source guard

Before production changes:

- Pass `UI_TEXT_RENDERER_SOURCE` and `UI_TEXT_BOUNDS_AUDIT_SOURCE` from both
  existing structured-HUD CTest registrations.
- Strengthen the real-function-body guard and reversible self-test. The guard
  must prove the early `layout.valid` return occurs before the first draw,
  creation of the real plan from `slot`, `layout.text_regions[index]`,
  `layout.scale`, real font readiness and the MeasureTextEx adapter, then draw
  key/cooldown/name plans.
- The plate helper must prove backing -> audit -> text order, exact plan fields,
  exact opaque backing accessor, and zero shadow for audit and draw.
- Shared helper guards must prove conditional shadow draw and explicit audit
  shadow handling.
- Add reversible mutants for: guard moved after first material draw; each of
  key/cooldown/name draws removed; position replaced by fixed offset; either
  zero-shadow changed to 2; unconditional shadow draw; audit ignores explicit
  shadow; required calls hidden in constant-false code or uncalled lambda.
- Obtain a valid direct-guard RED caused by the missing production contract,
  not by missing CMake arguments or syntax errors. Then implement the minimum
  GREEN.

## Verification boundary

The user explicitly said `尽量不测试`. Run only:

1. MSVC debug target build:
   `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2`
2. Selector-isolated `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1`, with all other
   `ARPG_*_ONLY` variables snapshotted/removed/restored. Exact unique summary:
   `183 cases, 0 failures`.
3. Exact direct guard and exact self-test, each with
   `--no-tests=error --parallel 1`.
4. Release game target only:
   `cmake --build --preset windows-msvc-release --target arpg_game --parallel 2`.

Do not run full platform, formal executables, deployment, `scripts/Test.ps1`,
or unrelated suites.

## Isolated manual proof

- Use the `computer-use:computer-use` skill faithfully for desktop actions.
- Existing isolated root:
  `C:/Users/dnfsr/AppData/Local/InfiniteDungeon/material-multiplier-20260811-0945`
- Safely close the currently paused exact isolated game with `Alt+F4` and wait
  for that exact process to exit. Never use `taskkill` and never touch the
  default save/settings.
- Copy only the rebuilt Release `arpg_game.exe` into the isolated `app` root.
- Launch that exact executable with the existing isolated `--save-dir`,
  `--settings-dir`, and `--screenshot-dir` arguments.
- Use in-game F12 screenshots (OS screenshots of OpenGL may be white). Capture
  and inspect at least ready/empty slots and a real `Numpad_2` cooling state.
- Prove: opaque key plate above, opaque name plate below equipped slot, empty
  slot key-only, centered opaque `N秒` cooldown plate, no viewport clipping,
  and icon art remains mostly visible. Report exact screenshot paths.
- Exit safely with `Alt+F4` after evidence unless continuing the already
  authorized play/fix loop is clearly useful; do not leave an unknown process.

## Commit and report

- Self-review exact scope, run `git diff --check` and staged check.
- Commit only the seven scoped files with:
  `fix(ui): back active skill labels with contrast`.
- Do not stage ignored SDD files.
- Report RED, GREEN, exact commands/results, manual screenshot evidence,
  allocation/UTF-8/default-font/fail-closed checks, commit hash, final status,
  and concerns. Do not claim broader testing than actually run.
