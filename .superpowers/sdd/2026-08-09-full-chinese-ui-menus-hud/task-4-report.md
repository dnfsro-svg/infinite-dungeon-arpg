# Task 4 implementation report

Status: DONE_WITH_CONCERNS

Commit: `461192e` (`feat: localize structured HUD controls`)

## Scope delivered

- Replaced English `ControlHints` display strings with a fixed ten-action `StableKey` array plus `revision` and `initialized`; revision zero initializes correctly and equal revisions leave every field unchanged.
- Added `control_hint_binding()` and `control_hint_key_label()` with safe invalid/uninitialized fallbacks.
- Built all three HUD control rows from `SettingAction` values and localized key labels in fixed UTF-8 buffers; the render/update path performs no per-frame allocation.
- Removed display-text parsing from `HudNoticeState`; interact, inventory, and passive-tree notices now consume structured actions directly.
- Localized player-facing HUD labels to `等级`, `生命`, `经验`, `经验已满`, and reward `经验`.
- Localized active-skill keycaps to `小键盘1` through `小键盘5` and cooldowns to `%.0f秒`, using UTF-8-safe fixed buffers.
- Appended `hud_experience_maxed` at catalog ordinal 43 and guarded every pre-existing ordinal 0 through 42.
- Added a narrow source guard plus reversible mutation self-test that prohibit notice-state string parsing and English action literals.

## TDD evidence

Observed RED rounds:

- Structured-binding compiler RED: `ControlHints` had no `bindings`, `initialized`, or query APIs.
- Render-model compiler RED: `HudBarPlan::label` and active-skill `cooldown_label` were missing, and the old key-label capacity rejected the Chinese numpad labels.
- Glyph runtime RED: CPLAY033 rejected the new Chinese HUD/control/keycap corpus.
- Cache runtime RED: an uninitialized hint cache returned a real key instead of the invalid fallback.
- MENU_HUD runtime REDs exposed the remaining English control-row fixtures before those real consumers were migrated.

Final GREEN:

- MSVC build of `arpg_platform_tests` and `arpg_stage11c_hud_stress` with `--parallel 2`: success, no OOM.
- `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1`: 130 cases, 0 failures.
- `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY=1`: 15 cases, 0 failures.
- HUD stress: 1/1 passed, including 100,000 allocation-free iterations.
- Structured notice source guard and mutation self-test: 2/2 passed.
- Full platform run: actual 660 versus expected 659, with exactly the two known unrelated failures: ecology alpha bounds and the preserved case-count +1 boundary. No Task 4 failure remains.
- Production-source forbidden-literal scan: no matches. `git diff --check`: clean apart from line-ending conversion warnings.

## Self-review

- MENU_HUD includes the real control-hint, notice, active-skill, render-plan, host-integration, and stress-adjacent fixtures; its exact count is derived as 130.
- Full expected count increased by exactly the one net-new Task 4 test case, from 658 to 659; the known actual +1 discrepancy remains visible.
- No world-hole host parameter or Task 5 hole-prompt path was changed.
- Build parallelism stayed at two; no memory exhaustion occurred. Peak memory was not instrumented.

## Concern

The branch-wide platform executable remains non-green only because of the two pre-existing failures listed above. They are outside Task 4 and were deliberately not masked.

## Review fix round 1

Status: DONE_WITH_CONCERNS

Commit: `91c641a` (`fix: initialize structured HUD consumers`)

- Added `ControlHints::initialized` to the HUD projector cache key. The real `CombatRenderer` regression first observes uninitialized revision zero (`未知`), then initialized default revision zero and requires one control-fragment rebuild plus the localized W/S/A/D row.
- Strengthened the notice source guard by stripping line/block comments, anchoring required structured lookups to real formatting/action calls, and prohibiting `strstr`, `strchr`, `memcmp`, `string_view`, `.find`, and legacy hint fields inside `hud_notice_state.cpp`. Mutation tests retain required tokens while attempting Chinese display-text parsing and are rejected.
- Migrated dungeon runtime, loot suction, Stage 12, and both Stage 16 renderer fixtures to `refresh_control_hints(... default_settings())`; the old revision-only fake is prohibited.
- Expanded the existing host integration case (no case-count increase) to verify Q/O/T rebinding through `CombatRenderer`, the Chinese Q notice, all five numpad keycaps, and 2/30-second cooldown labels.
- Added function-body renderer guards and mutation self-tests for `HudBarPlan::label`, `slot.key_label`, and `slot.cooldown_label`; function-external decoy tokens cannot satisfy the guard.
- Replaced whole-object `memcmp` in the same-revision test with explicit bindings/revision/initialized comparisons.

Review RED evidence:

- I1: MENU_HUD was 130 cases / 1 failure because initialized revision zero did not invalidate the projector cache.
- I2: the guard first rejected forbidden tokens that appeared only in comments; after comment stripping it then missed a required query placed only in dead code. Both failures were observed before the corresponding guard changes.
- I3: the new renderer/fixture guard rejected the existing dungeon renderer fixture because it supplied an uninitialized `ControlHints` object.

Review GREEN evidence:

- `arpg_platform_tests`, `arpg_stage11c_hud_stress`, `arpg_stage12_material_formal`, and `arpg_stage16_loot_reinforcement_game_validation` compiled with `--parallel 2`; no formal executable was run.
- MENU_HUD 130/130, foundation 15/15, CPLAY033 15/15.
- HUD stress plus structured notice/renderer guards: 5/5 passed, including 100,000 allocation-free iterations and all reversible mutants.
- Full platform run remained actual 660 / expected 659 with exactly the known ecology alpha and case-count +1 failures; this fix adds no platform test case.
- No Task 5 production path changed; no memory exhaustion occurred. Peak memory was not instrumented.

## Review fix round 2

Status: DONE_WITH_CONCERNS

Commit: `30fffc8` (`test: harden structured HUD architecture guards`)

- Replaced the notice guard's whole-file token blacklist with comment-stripped, brace-depth function extraction. `add_action_notice` must match the single structured formatting/enqueue body exactly, while the interact, inventory, and passive-tree observe blocks must make their exact `SettingAction` calls.
- Added notice mutants whose dead `if(false)` branches retain the full required call while the live paths perform a handwritten loop or `std::search` display-text lookup. The old guard missed the loop mutant before the new contract rejected both.
- Upgraded the HUD and active-skill renderer guards to extract the real overload bodies with brace-depth scanning that ignores quoted braces. The real draw paths must consume `bar.label`, `slot.key_label`, and `slot.cooldown_label`; in-body legacy formatting and English label recomputation are forbidden.
- Added renderer mutants with complete correct calls in dead branches but hardcoded live draws. The old token-count guard missed both before the function-body contract rejected them.
- Tightened fixture checks so every `ControlHints` owner must be refreshed adjacent to declaration and the same named owner must reach the production consumer. A mutant that refreshed an unused owner and passed a second uninitialized object was observed escaping the old guard and is now rejected.
- Added no platform case and changed no production source, so the full expected count remains 659.

Review RED evidence:

- Notice self-test: old guard failed with `HUD notice guard missed mutation: dead_required_call_with_loop_lookup`.
- Renderer self-test: old guard missed `hud_dead_correct_decoy_with_real_hardcode`, `active_dead_correct_decoy_with_real_legacy_text`, and `refreshes_unused_hints_but_passes_uninitialized`.

Review GREEN evidence:

- Notice and renderer source guards plus their self-tests: 4/4 passed after the final test-script cleanup.
- MENU_HUD 130/130, foundation 15/15, CPLAY033 15/15.
- HUD stress 1/1 passed, including 100,000 allocation-free iterations.
- Formal targets remained compiled from the earlier round-2 build check; no formal executable was run.
- Full platform boundary remained actual 660 / expected 659 with only the known ecology alpha and case-count +1 failures; round 2 adds no case and does not alter this boundary.
- No memory exhaustion occurred. Peak memory was not instrumented.

## Review fix round 3

Status: DONE_WITH_CONCERNS

Commit: `19a6ddc` (`test: close structured HUD guard bypasses`)

- Replaced regex comment stripping with a single-pass CMake C++ lexical state machine covering normal code, CRLF line comments, block comments, escaped ordinary strings, character literals, numeric separators, raw-string delimiters/bodies, and nested brace depth. It emits both a literal-preserving normalized body and a literal/comment-free code projection.
- Added an independent parser CTest with positive coverage for `http://x`, `/*x*/`, escaped quotes, `'}'`, `'\''`, raw strings containing braces/comment markers, real line/block comments, nested braces, and CRLF. Negative subprocess fixtures reject missing functions and unclosed braces, block comments, strings, and raw strings.
- Notice guards now scan code-only tokens across the real source for `strstr`, `strchr`, `memcmp`, `std::search`, `string_view`, and `.find`; `observe` additionally rejects handwritten loop/direct-hint display scans while retaining its exact structured action blocks.
- HUD and active-skill draw guards require exactly one real model audit/draw sequence, prohibit `if(false)`/`if(0)` decoys, and reject legacy formatting from code-only bodies. Complete correct calls hidden in dead branches no longer satisfy the guard.
- Fixture guards follow each same-name `ControlHints` owner from adjacent refresh to its production consumer and reject assignment, typed reconstruction, `reset`/`clear`, or `memset` invalidation in between.
- Added one CTest only; the platform executable case count and expected full count remain unchanged.

Review RED evidence:

- The old parser corrupted `"http://x"` and truncated the extracted function body in the new CRLF/literal/raw positive fixture.
- The old notice guard missed `observe_live_handwritten_display_lookup`.
- The old renderer guard missed `hud_complete_dead_calls_with_live_legacy_path` and `active_complete_dead_calls_with_live_legacy_path`.
- The old fixture guard missed `refresh_then_brace_assignment`, `refresh_then_typed_reassignment`, and `refresh_then_memset_clear`.

Review GREEN evidence:

- MENU_HUD 130/130, foundation 15/15, CPLAY033 15/15.
- HUD 100,000-iteration allocation-free stress plus parser/notice/renderer guards and self-tests: 6/6 passed.
- `arpg_platform_tests`, HUD stress, Stage 12 formal, and Stage 16 formal targets compiled with `--parallel 2`; formal executables were not run.
- Full platform boundary remains actual 660 / expected 659 with only the known ecology alpha and case-count +1 failures.
- No production source changed and no memory exhaustion occurred. Peak memory was not instrumented.

## Review fix round 4

Status: DONE_WITH_CONCERNS

Commit: `9f3370c` (`test: parse C++ digit separators safely`)

- Added literal expected-output coverage proving the parser preserves `1'000`, `10'000`, and `0xFF'00` in both the extracted body and code-only projection.
- Replaced CMake truthiness on regex match values with explicit non-empty comparisons, so a matched character `0` is not treated as false.

Review RED evidence:

- The expanded parser positive fixture failed with `Unterminated C++ lexical construct: character` because the old condition rejected a matched adjacent `0`.

Review GREEN evidence:

- Parser self-test passed after the two explicit non-empty checks.
- MENU_HUD 130/130, foundation 15/15, CPLAY033 15/15.
- HUD stress plus parser/notice/renderer guards and self-tests: 6/6 passed.
- Platform, HUD stress, Stage 12 formal, and Stage 16 formal targets compiled with `--parallel 2` as no-work; formal executables were not run.
- Full platform remained actual 660 / expected 659 with only the known ecology alpha and case-count +1 failures.
- Only parser guard/tests changed; no production source, platform case, or memory behavior changed. No OOM occurred.
