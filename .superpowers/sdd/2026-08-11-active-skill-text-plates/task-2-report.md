# Task 2 Report — Opaque Active-Skill Text Plates

## Scope

Modified only the seven tracked files named in the Task 2 brief. The report is
an ignored SDD artifact and is not staged.

## RED then GREEN

- RED: `ctest --preset windows-msvc-debug --output-on-failure --no-tests=error
  --parallel 1 -R '^platform\.hud_renderer_structured_model_source$'` failed
  directly with `ActiveSkillRenderer::draw_hud must reject invalid layout before
  drawing`. This was the missing production contract, not a CMake argument or
  script syntax issue.
- GREEN implementation: `draw_hud` now returns before its first draw when the
  layout is invalid, uses the Task 1 aggregate plan with a shared-font
  `MeasureTextEx` adapter/context, and draws visible backing -> audit ->
  zero-shadow foreground after existing slot/icon/cooldown artwork. The shared
  draw and audit helpers now honor an explicit non-positive shadow request.

## Focused verification

- `cmake --build --preset windows-msvc-debug --target arpg_platform_tests
  --parallel 2` exited 0. The configured MSVC DevShell was required because a
  standalone shell does not inherit `WindowsSDKVersion`.
- Isolated `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1`: `183 cases, 0 failures`.
- Direct source guard: 1/1 passed in 53.73 seconds.
- Reversible source-guard self-test: 1/1 passed in 98.14 seconds. It rejects
  moved layout guard, removed key/cooldown/name plans, fixed text position,
  either changed zero-shadow argument, unconditional shadow, ignored audit
  shadow, constant-false decoy, and uncalled-lambda decoy.
- `cmake --build --preset windows-msvc-release --target arpg_game --parallel 2`
  exited 0 and linked `arpg_game.exe`.
- `git diff --check` passed before staging.

No full platform suite, formal executable, deployment, or `scripts/Test.ps1`
was run.

## Isolated manual proof

- Closed the pre-existing isolated game with Alt+F4, confirmed it had no
  remaining app/window, copied only the rebuilt Release executable, then
  launched it with:
  `--save-dir .../runtime/save --settings-dir .../runtime/settings
  --screenshot-dir .../runtime/screenshots`.
- Release and isolated executable SHA-256 matched:
  `E5F8EB4EE07663D68BA2AD46750E9C6F3E62D9E2E11D607E5823E5A98EE20F51`.
- The launched isolated process was PID 2528 and was closed with Alt+F4;
  PID absence was confirmed afterward. Default save/settings were not used.
- F12 screenshot evidence:
  - `C:/Users/dnfsr/AppData/Local/InfiniteDungeon/material-multiplier-20260811-0945/runtime/screenshots/task2-ready-empty.png`
    shows opaque key plates above, opaque name plates below the equipped first
    two slots, and key-only plates for empty slots.
  - `C:/Users/dnfsr/AppData/Local/InfiniteDungeon/material-multiplier-20260811-0945/runtime/screenshots/task2-numpad2-retry.png`
    shows the real `Numpad_2` cooling state: a centered opaque `23秒` plate
    over the second icon while the icon remains mostly visible.
- Both F12 images show no viewport clipping. Window-capture images were white
  for OpenGL, so only the in-game F12 PNGs were used as visual evidence.

## Static contracts checked

- The guarded real renderer path contains no `GetFontDefault`, `DrawText`,
  `TextFormat`, `snprintf`, or `std::string`; the plan owns fixed buffers and
  Task 1 retains UTF-8 validation/fail-closed fitting.
- An unavailable/invalid shared font yields an empty text plan while the
  existing slot artwork path remains available.
- No heap allocation or per-frame string construction was added.

## Concern

The CMake source guard and self-test are intentionally string/body based and
take about 54 s and 98 s respectively. This task ran only the brief-authorized
focused verification.

## Round 1 Guard Final Review

- Added syntax-valid, single-cause self-test mutations for
  `if constexpr(false)` hidden key draw, `auto&&` uncalled-lambda hidden key
  draw, and moving the complete plan/key/cooldown/name block before icon and
  cooldown artwork while leaving the first material draw in front.
- The old guard self-test was RED in 113.95 seconds and named exactly those
  three missed mutations. The strengthened guard now operates on extracted
  parser `CODE` bodies, rejects `if constexpr(false)` and uncalled declared
  `const auto`/`auto&`/`auto&&` lambda forms, and requires every slot/fallback,
  icon, cooldown-fill, and cooldown-edge artwork call to precede the plan.
- Updated direct guard passed 1/1 in 52.66 seconds. Updated exact self-test
  passed 1/1 in 108.15 seconds. No build, selector, Release, formal, full, or
  deployment command was run in this production-free round.

## M1 Water Prompt Review

- `dungeon_view_math.cpp` maps the downward exit to `水流`; `room_renderer.cpp`
  sends that door label through `draw_world_prompt`, so it is world text rather
  than the third active-skill name.
- The prior conclusion for `task2-m1-after-w.png` was incorrect: the blue
  `水流` text remains visibly below the third-slot name region. That image is
  not clean acceptance evidence.
- Follow-up used the same hash-verified isolated Release executable and
  existing runtime directories. F1 first enabled the full debug overlay; its
  second press returned to the normal presentation but did not remove `水流`,
  so F1 is not the cause.
- The follow-up desktop controller then reported user input while attempting a
  safe isolated movement-only viewpoint change. Per Computer Use safety rules,
  no additional game input, F12 capture, or close action was issued. No clean
  screenshot path is claimed. The current isolated process is intentionally
  left untouched pending a resumed manual session; default save/settings were
  not used.
