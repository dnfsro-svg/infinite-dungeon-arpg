# Stage 11-C Task 5 Report — Player Panel and Monster Combat Bars

## Status

Complete. Commit: `b7f08d4fede26e2c4d526e68d75c5b4f3bc218a2`
(`feat: render player and monster combat bars`).

## RED

Added and registered seven headless cases before production implementation:

- health-first player bar;
- conditional barrier visibility;
- `XP MAX` presentation;
- low-health presentation pulse boundary;
- three fixed status tags;
- clamped ratios with time-stable bounds;
- monster HP/shield/break shared palette IDs and world-space plan.

After loading the required MSVC/Windows SDK environment through
`scripts/Configure.ps1`, `arpg_platform_tests` failed in
`hud_render_plan_tests.cpp` because `PlayerPanelPlan`, `HudBarKind`, and
`make_player_panel_plan` did not exist. This was the expected interface RED.

## Changes

- Exposed the fixed-capacity pure player-panel plan in `hud_renderer.hpp`.
- Implemented clamped health, conditional barrier, XP/MAX, fixed three-tag,
  and <=2Hz presentation-time low-health plan behavior.
- Drew Chinese player labels and status tags with `DrawTextEx`, using the
  shared font and palette, and constrained bars to `HudLayout::player_panel`.
- Added a world-space monster break bar from the existing
  `break_value/max_break` snapshot fields, with HP/shield/break routed through
  shared health/barrier/experience palette IDs.
- Did not change combat snapshots, locking, simulation, session/store access,
  input, RNG, or fixed ticks.

## GREEN

```text
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
  passed
ctest -R "^(platform.units|combat.units|architecture.combat_no_raylib)$"
  3/3 passed
out\\build\\windows-msvc-debug\\bin\\arpg_platform_tests.exe
  217 cases, 0 failures
git diff --check
  passed
```

## Self-review

- Pure plan uses only fixed arrays and scalar math; no dynamic containers or
  side effects.
- Low-health pulse is derived solely from the `presentation_seconds` argument
  in the pure plan and is bounded to a two-Hz cycle.
- Actor resource bars still use projected actor coordinates, so they remain
  world-space.

## Concerns

`HudRenderer` remains deliberately unintegrated with the production host; that
is Task 7. Objective, navigation, notice, formal-screenshot, and allocation
guard work remains outside Task 5.

## Review follow-up — `796693cec9b075b85658c858520cecf94f775fcc`

Two Important review findings were repaired with test-first coverage:

- `NaN`, `+infinity`, and `-infinity` for all player bar ratios first failed
  the new headless regression. `clamped_ratio` now explicitly uses
  `std::isfinite` and returns `0.0F` for every non-finite input; low-health
  emphasis additionally requires a finite source ratio.
- Replaced the self-describing monster palette constant with
  `make_monster_bar_visual_plan(const combat::MonsterSnapshot&)`. Its three
  fixed entries expose visibility, bounded ratio, palette ID, and world-space
  meaning. The actor renderer now draws only by iterating that plan. Tests use
  resource-free and distinct resource-bearing snapshots to verify HP always,
  shield/break conditional visibility, and ratios.

Follow-up GREEN: Debug targets built; `platform.units`, `combat.units`, and
`architecture.combat_no_raylib` passed 3/3; direct platform run passed
`218 cases, 0 failures`; `git diff --check` passed.
