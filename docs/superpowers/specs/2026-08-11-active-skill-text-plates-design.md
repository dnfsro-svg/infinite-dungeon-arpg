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

Two shared helpers receive backward-compatible corrections needed by this HUD:
`draw_crisp_ui_text` must make an explicitly requested zero shadow truly
shadow-free, and `record_ui_text_bounds` must accept the actual shadow size.
All callers that use their defaults retain current behavior.

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
per-slot key, cooldown, and name regions, a visual extent, and a `valid` flag.
"Pairwise separated" refers to the three text-plate regions; the cooldown plate
is the only plate intentionally allowed to intersect the icon. Invalid viewport
or derived geometry produces `valid=false`. Layout remains a pure,
allocation-free calculation in `active_skill_view.cpp`.

The view layer also exposes a fixed-buffer `ActiveSkillTextPlatePlan` for each
of the three regions and an aggregate `ActiveSkillHudTextPlan`. A pure plan
builder consumes one `ActiveSkillHudSlot`, its regions, font readiness, and an
`UiTextMeasureFn` together with its caller-owned `void* measure_context`. The
production context points to the real shared `Font`; tests may supply a pure
deterministic context. Each plate plan contains its backing rectangle, fitted
text bytes, foreground position and extent, final font size and spacing,
truncation state, visibility, and its distinct `UiTextAuditRole`. The returned
plan owns all text bytes and contains no self-referential pointers.

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

All three labels use `fit_centered_ui_text` with plan-owned fixed buffers,
16px preferred size, 9px minimum size, and 0.5px preferred spacing in reference
space. Known labels (`小键盘1` through `小键盘5`, `拔刀斩`, `暴风式`, and
`30秒`) must fit without truncation. Extreme valid UTF-8 input may shrink and
then elide only at complete code-point boundaries through the existing helper.

The renderer records separate HUD audit roles for key, skill name, and
cooldown text against the exact plate rectangle used for drawing. The bounds
audit API gains a backward-compatible final `shadow_pixels` argument: omitted
uses the existing two-pixel default, while zero or a negative value measures
only the foreground. Active-skill text explicitly passes zero.

`draw_crisp_ui_text` must honor its existing `shadow_pixels` parameter by
skipping the shadow draw entirely when the value is zero or negative. Existing
callers that omit the argument retain the current two-pixel shadow. Both draw
and audit helpers therefore use the same rule: default argument 2, and
`shadow_pixels <= 0` means no shadow.

## Failure Handling

Invalid viewport or derived rectangle geometry returns an invalid layout. The
renderer checks `layout.valid` before the first slot/material/icon draw and
returns immediately when false. A false font-readiness input returns an empty
aggregate text plan but does not suppress valid slot artwork. A failed text fit
draws neither that plate nor its label. Empty and invalid UTF-8 strings also
fail closed. No default font, heap allocation, self-referential pointer, or
per-frame string construction is introduced.

## Verification

The change uses an existing active-skill TestCase so platform case counts do
not change.

Focused evidence will cover:

- 800x450, 1280x720, and 1920x1080;
- all three text plates inside the viewport and pairwise separated from one
  another;
- key/name/cooldown visibility for empty, ready, and cooling slots through the
  pure aggregate plan;
- no more than 25 percent icon overlap from the cooldown plate;
- alpha 255 and foreground contrast of at least 4.5;
- centered fitted text with no truncation for all approved labels;
- empty plans for unavailable font, invalid UTF-8, invalid geometry, and failed
  measurement;
- invalid `ActiveSkillHudLayout` rejected before any slot, material, icon,
  cooldown, plate, or text draw, enforced by the source guard and a reversible
  mutation that moves the check after the first draw;
- zero-shadow rendering and bounds auditing that excludes shadow expansion,
  while default callers retain the existing two-pixel behavior;
- three distinct audit roles carried by the pure plan, with the source guard
  requiring the real renderer to submit each visible plan to
  `record_ui_text_bounds` before drawing it;
- 10,000 combined layout and aggregate text-plan iterations with no allocation;
- the existing structured HUD source guard updated to require the real backing,
  fit, audit, and model-text draw path;
- only the focused MENU/HUD selector and affected source guard/self-test;
- a Release `arpg_game` target build followed by isolated manual screenshots of
  both ready and cooling skill slots.

No full platform suite, formal executable, deployment, or unrelated visual
test is required for this change.
