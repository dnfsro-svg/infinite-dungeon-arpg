# Task 5 brief: rebuild HP-potion presentation on current material runtime

## Preconditions

- Start only after Task 4 external independent approval.
- Worktree/branch remain
  `E:/game/.worktrees/integration-large-square-rooms` /
  `codex/integration-large-square-rooms`.
- Serial builds/tests only; never overlap GPU/game validators.
- Do not push.

## Binding output

- High-resolution source and published color/material atlases include health
  potion at atlas cell 30 with correct keyed transparency/material channels.
- Ground label is pure opaque Chinese `生命药`; feedback is pure opaque
  `生命药恢复 +N HP`.
- Text bounds never overlap at 1024x576, 1280x720, or 1920x1080; anchors and
  queue order remain stable; full-capacity rendering cannot overflow stack.
- Font production codepoint list explicitly contains U+836F (`药`), proven by
  a test that is RED before the production addition.
- Current material residency synchronization, active-skill atlas validation,
  native room backgrounds, and actual-capture completion semantics stay intact.

## Execution constraints

1. Follow Task 5 in the plan exactly. Process source commits `7b0731e`,
   `ca854bd`, `bfafd1e`, `a3b30ff` one at a time and do not proceed until each
   current-API resolution is clean and its focused platform executable passes.
2. Resolve current platform totals by execution: 482 -> 484 -> 487 -> 488.
   Never choose an old registry side wholesale.
3. Add the U+836F test first, record RED, then manually transfer only the
   `hud_font.cpp` glyph addition and exact test coverage.
4. Rebuild only potion cell / loot-label / feedback paths against the current
   material APIs. Never copy old `CombatRenderer`, `RoomRenderer`,
   `MaterialPack`, Stage 12 validator, or `raylib_host.cpp` wholesale.
5. Manually port from `742ce70` only the approved Stage 17 lower-HP timing:
   valid Storm lane before activation, move player after first hit, require
   exactly three hits in C++ and PowerShell validation. Never cherry-pick it.
   Apply neither `b57930e` nor `2422d13` unless the explicit plan condition is
   proved; never wholesale.
6. Run the exact platform/asset, Stage 12 truth, and Stage 17 gates from the
   plan. Actual image export must succeed before a frame is complete.
7. Require the Step 9 guard against `d710e01` to be empty for all six protected
   current-runtime files. If not empty, stop before commit and report.
8. Commit the exact reviewed presentation slice as
   `feat: present high-resolution health potions`.

## Review and report

Create `task-5-report.md` with per-source conflict decisions and test totals,
U+836F RED/GREEN evidence, atlas source/published dimensions and hashes,
pure-color/text-bound evidence, Stage 12/17 command totals and actual captures,
the six-file guard output, exact changed files, final SHA/status/process state,
self-review, and concerns. Update the ignored progress ledger.

