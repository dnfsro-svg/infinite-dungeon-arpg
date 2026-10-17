# Task 5 implementation report

Status: DONE_WITH_CONCERNS

Commit: `110a284` (`feat: localize dungeon interaction views`)

## Scope delivered

- Localized all room-facing display mappings: four ecologies plus unknown, three abyss danger levels, nine abyss rules and nine effect explanations, four hole states, four save states, every room phase, and every exit direction.
- Added fixed-buffer UTF-8 interaction prompts. The prompt consumes the structured host binding and produces `Q：下潜` or `Q：进入下一层`; null/empty bindings stay empty and no `E` fallback exists.
- Routed door, abyss, and hole text through the `CombatRenderer`-owned borrowed shared CJK `Font`. Real rendering uses `MeasureTextEx`, then an opaque `#0B1019` backing, then `draw_crisp_ui_text`; there is no default-font or `DrawText` fallback.
- Added viewport-clamped prompt layout for 800/1280/1920 and high-contrast text correction against the real backing.
- Extended the shared dynamic glyph corpus without adding or reordering `UiTextId`; ordinals 0-43 remain unchanged.
- Hardened the host/renderer source guard with a reversible seven-mutant self-test, including hardcoded binding, missing backing, default drawing/font, legacy measuring, dead-branch decoy, and missing contrast correction.

## TDD evidence

Observed RED rounds:

- Mapping RED: the expanded exhaustive case rejected the former `FIRE`, `ABYSS HIGH`, `ABYSS FURY`, `SEALED`, `SAVING`, room-phase, direction, and `E: DESCEND` outputs.
- Interface RED: tests failed to compile before `HoleInteractionAction`, `hole_visual_label`, `WorldPromptLayout`, and `world_prompt_style` existed.
- Renderer/source RED: the strengthened guard rejected the former room renderer's default `DrawText`/`MeasureText` path and English hole literals.
- Glyph RED: CPLAY033 rejected the new dungeon interaction corpus before it was appended to the shared visitor.
- Contrast RED: the chaos door color failed the 4.5 display-luma boundary against `#0B1019`; the test then failed to compile until the explicit correction API existed.

Final GREEN:

- `arpg_platform_tests` built with MSVC debug and `--parallel 2`; no OOM.
- `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1`: 177 cases, 0 failures.
- `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY=1`: 15 cases, 0 failures.
- `ARPG_CPLAY030_HOLE_PROMPT_ONLY=1`: 22 cases, 0 failures.
- `ARPG_CPLAY033_PASSIVE_CJK_ONLY=1`: 16 cases, 0 failures.
- `ARPG_TASK9_ENVIRONMENT_RENDER_ONLY=1`: 57 cases, 0 failures.
- Hole binding/renderer source guard plus seven-mutant self-test: 2/2 passed.
- `arpg_stage11c_hud_formal` compiled and linked with `--parallel 2`; the formal executable was not run.
- Full platform run: actual 665 versus expected 664, with exactly the two known unrelated failures: ecology alpha bounds and the preserved case-count +1 boundary. No Task 5 failure remains.
- The dynamic prompt allocation test completed 10,000 iterations with zero allocations. Peak process memory was not instrumented.

## Self-review

- MENU_HUD now includes Task 5 mapping, font, layout, contrast, and render-plan coverage; the exact total is 177.
- Full expected increased only by the five net-new Task 5 cases, from 659 to 664; the known actual +1 discrepancy remains visible.
- The unified glyph catalog covers all new Chinese strings and `UiTextId` ordinals 0-43 are preserved.
- No Task 6 recovery flow or Task 7 F1 work was implemented.
- `git diff --check` is clean apart from Windows line-ending conversion warnings.

## Concern

The branch-wide platform executable remains non-green only because of the two pre-existing failures listed above. They are outside Task 5 and were deliberately not masked. Windows visual acceptance was not performed in this headless validation pass.

## Review fix round 1

Status: DONE_WITH_CONCERNS

Commit: `59661f2` (`fix: preserve dungeon prompt layout contracts`)

- Strengthened the real abyss-overlay guard to extract `CombatRenderer::draw_abyss_overlay`, require exactly one `draw_abyss` call using `world_ui_font_` and `world_ui_font_ready_`, and reject default-font or dead-decoy paths. The self-test now covers nine reversible mutants.
- Added `WorldPromptLayout::text_scale`. Oversized measured text is uniformly scaled into the viewport, final measured/backing/text bounds use the same scale, normal 800/1280/1920 prompts remain at scale 1, and text below the 8-pixel readability floor is hidden.
- Wired the real renderer to apply the layout scale to both font size and glyph spacing.
- Tightened color semantics: fire, water, lightning, and all high-contrast semantic colors remain byte-for-byte unchanged; only low-contrast colors, including chaos, fall back to primary.

Review RED evidence:

- The original source guard missed both `abyss_overlay_default_font` and `abyss_overlay_dead_shared_font_decoy`.
- The layout test failed to compile because `WorldPromptLayout::text_scale` did not exist; the real-renderer guard separately rejected the unscaled `draw_crisp_ui_text` call.
- Reversible color mutants proved both directions: always returning primary failed the preserved fire-color channel assertion, while never falling back failed the 4.5 contrast assertion.

Review GREEN evidence:

- MSVC debug `arpg_platform_tests` build completed with `--parallel 2`; no OOM and peak memory was not instrumented.
- MENU_HUD 177/177, foundation 15/15, CPLAY030 22/22, CPLAY033 16/16, and Task9 57/57.
- Hole source guard plus nine-mutant self-test: 2/2 passed (18.66 s and 80.66 s).
- `arpg_stage11c_hud_formal` compiled and linked with `--parallel 2`; the formal executable was not run.
- Full platform remained actual 665 / expected 664 with exactly the known ecology alpha-bounds and count +1 failures; no Task 5 failure.
- `git diff --check` passed apart from line-ending conversion warnings. No Task 6 production or test path changed.
