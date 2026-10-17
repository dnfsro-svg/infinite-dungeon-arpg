# Task 6 implementation report

Status: DONE_WITH_CONCERNS

Commit: `feat: localize save recovery screen`

## Scope delivered

- Added the appended `UiTextId::recovery_dual_slot_instruction` at ordinal 44; ordinals 0–43 remain asserted stable.
- Added `make_recovery_screen_view()` with a fixed UTF-8 error buffer, centred 640×260 bounded panel, catalogue-backed Chinese title/message, dynamic `存档错误 N`, and contrast-safe panel/text colors.
- Replaced recovery-screen default `DrawText`/`TextFormat` drawing with the host's existing `SharedUiFont`, panel backing, measured centred CJK text, and `draw_crisp_ui_text`.
- Preserved recovery state, N-key handling, archive/new-run behavior, and capture flow. The real branch remains `observe recovery -> SharedUiFont draw -> present/capture`.
- Updated the existing host presentation guard and added a narrow Task 6 source guard plus reversible self-test for unshared-font, present-before-draw, and default-text mutations.

## TDD evidence

- RED: the new model/catalogue tests failed to compile because `RecoveryScreenView`, `make_recovery_screen_view`, and `recovery_dual_slot_instruction` did not exist.
- GREEN: after the minimum model, catalogue, and borrowed-font draw path, the targeted suites passed.
- The pure model verifies 800×450, 1280×720, and 1920×1080 panel/text bounds, exact Chinese strings, error text, and at-least-4.5 contrast. Its 10,000-iteration allocation probe reports zero allocations.

## Verification

- `cmake --build --preset windows-msvc-debug --parallel 2 --target arpg_platform_tests` — passed.
- `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY=1 arpg_platform_tests.exe` — 15 cases, 0 failures.
- `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1 arpg_platform_tests.exe` — 178 cases, 0 failures.
- `ctest -R ^platform.task6_recovery_screen_source_guard` — 2/2 passed, including the reversible self-test.
- `ctest -R ^platform.units$` — 666 cases, 1 external failure: `ecology_material_coverage.ecology props match emitted layout records` at `ecology_material_coverage_tests.cpp:1163`. The former global case-count mismatch was corrected to 666.
- The existing broad `platform.host_input_source` guard still stops at its unrelated `death_overlay_renderer.cpp` lifecycle check; it was not changed. The Task 6 narrow guard above provides the exact recovery ordering evidence.
- No formal game executable or Windows visual acceptance was run.

## Self-review

- No recovery/N-key/archive/save/state-machine semantics changed.
- No new font ownership/load site was introduced.
- `git diff --check` is clean apart from Git's CRLF conversion warnings.

## Review fix round 1

- Restored the full-suite expected count from 666 to 665. Task 6 contributes one net test, while the pre-existing actual +1 drift remains intentionally visible.
- Fresh MSVC Debug verification: Foundation 15/0; MENU_HUD 178/0; full platform reports exactly 666 cases and two failures — the existing ecology alpha-bounds failure and `actual 666 != expected 665`.
