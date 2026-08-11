# Active Skill Text Plates Design

**Date:** 2026-08-11

**Status:** Approved design, pending written-spec review

## Problem

The combat HUD draws each active-skill key label and skill name directly over
transparent slot artwork, the skill icon, and the moving world. A cooldown
label also relies on a translucent cooldown overlay. The resulting foreground
contrast changes with the scene and cannot be guaranteed to reach 4.5:1.

The current isolated Release play session reproduces the problem at
1280x720. Ready slots show `小键盘1` / `小键盘2` and `拔刀斩` / `暴风式`
without an opaque local backing. A successful `Numpad_2` cast shows `21秒`,
confirming that the cooldown path is reachable and uses the same uncentered
text path.

## Scope

This change affects only the five-slot active-skill combat HUD.

It does not change:

- active-skill input or cooldown rules;
- skill icons, cooldown fill, or slot materials;
- HUD status tags;
- the global HUD text backing alpha;
- inventory skill-stone layouts;
- any Chinese terminology or shared-font glyph inventory.

## Layout

Each 76x76 reference slot receives three independent text regions. Every
reference-space value is multiplied by the existing viewport scale.

- Key region: `{slot.x + 4, slot.y - 20, slot.width - 8, 18}`.
- Cooldown region: `{slot.x + 16, slot.y + 28, slot.width - 32, 20}`.
- Name region: `{slot.x + 7, slot.y + 77, slot.width - 14, 18}`.

The key region sits above the slot, the name region sits below it, and only the
cooldown region covers the icon. At 800x450 the existing scale floor is 1.0,
so the five slots remain 76x76. The complete visual extent is y=334..449 and
stays inside the viewport. The cooldown plate covers no more than 25 percent
of the 60x60 icon region.

`ActiveSkillHudLayout` keeps its existing slot-strip `bounds` contract and adds
per-slot key, cooldown, and name regions plus a visual extent. Layout remains a
pure, allocation-free calculation in `active_skill_view.cpp`.

## State Rules

- Empty slot: draw the key plate and key label only.
- Equipped and ready slot: draw key and name plates and their labels.
- Equipped slot on cooldown: draw all three plates and labels.
- The cooldown plate appears and disappears in the same frame as the existing
  non-empty `cooldown_label`; it has no independent animation or retained
  state.
- If the shared UI font is unavailable, the renderer continues to fail closed
  and draws no text plates or default-font text.

## Rendering

The draw order is:

1. slot material or the existing procedural fallback;
2. skill icon;
3. the existing translucent cooldown fill and its edge;
4. visible local text plates;
5. fitted, centered shared-font text.

Every text plate uses the opaque color `{5, 9, 16, 255}`. Text uses the
existing primary contrast color at alpha 255. The foreground-to-plate luma
ratio must be at least 4.5. Text is rendered without the existing two-pixel
shadow because the opaque plate supplies the contrast boundary.

All three labels use `fit_centered_ui_text` with caller-owned fixed buffers,
16px preferred size, 9px minimum size, and 0.5px preferred spacing in reference
space. Known labels (`小键盘1` through `小键盘5`, `拔刀斩`, `暴风式`, and
`30秒`) must fit without truncation. Extreme valid UTF-8 input may shrink and
then elide only at complete code-point boundaries through the existing helper.

The renderer records separate HUD audit roles for key, skill name, and
cooldown text against the exact plate rectangle used for drawing.

## Failure Handling

Invalid viewport or rectangle geometry returns an empty layout and draws
nothing. A failed text fit draws neither that plate nor its label. Empty and
invalid UTF-8 strings also fail closed. No default font, heap allocation, or
per-frame string construction is introduced.

## Verification

The change uses an existing active-skill TestCase so platform case counts do
not change.

Focused evidence will cover:

- 800x450, 1280x720, and 1920x1080;
- all regions inside the viewport and pairwise separated;
- key/name/cooldown visibility for empty, ready, and cooling slots;
- no more than 25 percent icon overlap from the cooldown plate;
- alpha 255 and foreground contrast of at least 4.5;
- centered fitted text with no truncation for all approved labels;
- 10,000 layout iterations with no allocation;
- the existing structured HUD source guard updated to require the real backing,
  fit, audit, and model-text draw path;
- only the focused MENU/HUD selector and affected source guard/self-test;
- a Release `arpg_game` target build followed by isolated manual screenshots of
  both ready and cooling skill slots.

No full platform suite, formal executable, deployment, or unrelated visual
test is required for this change.
