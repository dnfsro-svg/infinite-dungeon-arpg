# Task 8 review round 1

Verdict: Changes requested.

## Important

- `actor_renderer.cpp` chooses the combat font with both `hud_font_ready` and `IsFontValid(hud_font)`. A valid passed font falls back when readiness is false, contrary to the explicit contract that fallback occurs only when the passed font is invalid. Decide only from `IsFontValid(hud_font)` and strengthen the existing combat-text case without increasing the 469-case total.

## Other verified points

- Exactly four new cases, 465 to 469.
- Hidden death mode returns before material draws; visible mode uses the warning-modal nine-slice at 32 px and the label-plate region.
- Panel and title have independent fallbacks; dimmer and text remain visible.
- Damage and `DEFEATED` use `MeasureTextEx`, `draw_crisp_ui_text`, opaque solid foregrounds and a physical 1 px dark shadow.
- Text preserves lifetime-driven motion/removal; no gameplay change.
- Font sizes scale monotonically, required glyphs are covered, and old crisp-text callers retain the 2 px default.
- Serial `platform.units` passed with 469 cases and no residual process.
