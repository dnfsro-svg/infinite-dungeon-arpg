# Stage 10 Task 11 Report

## Outcome

- Added the four directional abyss-door markers as a boolean-only preview. The
  existing locked/open door frame, arrow, label, and locked interior remain the
  original rendering path.
- Added snapshot-driven warning/active ground visuals for thunderstorm, hunting
  flames, and chaos expansion. Center, radius, kind, and phase come only from
  `CombatSnapshot::hazards`; the renderer contains no random or rule simulation.
- Added current-room abyss danger, rule name, exact effect text, pending/unpicked
  reward counts, and door/descent abandonment confirmation prompts.
- Added `DungeonSnapshot::abyss_rule` and `abyss_danger` as a necessary contract
  extension because neither existing snapshot exposed non-environment rule
  metadata. They are copied only from the committed current-room abyss state;
  door targets remain `std::array<bool, 4>` and reveal no target rule details.

## TDD Evidence

- RED: the initial platform build failed because
  `EnvironmentHazardVisualMode`, `abyss_door_marker`,
  `environment_hazard_visual`, `abyss_hud_values`, and the two snapshot fields
  did not exist.
- RED: exact-effect coverage then failed specifically because
  `AbyssHudValues::effect_label` did not exist.
- GREEN: `arpg_platform_tests` reports `84 cases, 0 failures`.
- Contract regression: `arpg_dungeon_tests` reports `197 cases, 0 failures`,
  including the 1,000-room deterministic stress path.

## Verification

- `cmake --build --preset windows-msvc-debug --target arpg_platform_tests`:
  passed.
- `ctest --preset windows-msvc-debug -R "platform.units|architecture"
  --output-on-failure`: 19/19 passed, 0 failed, 66.95 seconds.
- Final post-effect-label `platform.units`: 1/1 passed, 2.65 seconds.
- `git diff --check`: no errors (only the repository's existing LF-to-CRLF
  checkout warnings).
- Search for `GetRandomValue`, `select_abyss_rule`, `is_abyss_roll`,
  `combat_config_for`, and `derive_abyss` under `src/platform/raylib`: no
  matches.

## Font / raylib 6.0 Note

The bundled/default raylib font does not guarantee Chinese glyph coverage.
Prompts therefore use concise equivalent ASCII English instead of silently
rendering missing-glyph boxes. The Chinese semantics are preserved: same-door
second touch abandons all remaining rewards; descent requires pressing E again
and abandons remaining rewards before descending.

The code builds against the repository's pinned raylib 6.0 API, including the
rounded panel and polygon marker calls.

## Independent Review Fix

Review found that the legacy actor-layer generic hazard pass still rendered
abyss-environment hazards after the new room-layer pass. That second rendering
could cover the rule-specific color and put environment geometry above room
items/doors/hole.

- RED: `generic_hazard_pass_excludes_abyss_environment` failed to compile
  because `uses_generic_hazard_pass` did not exist.
- GREEN: the routing function returns true only for active monster hazards,
  and the real `actor_renderer.cpp::draw_hazards` loop invokes it before the
  generic visual path. Abyss-environment hazards now render exclusively in the
  room-layer snapshot-driven pass.
- Updated platform result: `85 cases, 0 failures`.
- Updated platform/architecture result: 19/19 passed, 0 failed, 65.59 seconds.
- Post-fix dungeon snapshot/stress regression: `197 cases, 0 failures`.
