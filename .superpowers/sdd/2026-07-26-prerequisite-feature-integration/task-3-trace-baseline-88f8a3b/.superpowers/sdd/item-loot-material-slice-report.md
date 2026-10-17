# Item, Loot, Material and Bag Resource Slice

## Scope

- Replaced the four programmatic ground-loot placeholders with a real paired
  `items_ui` material atlas and unique equipment-slot plus rarity resources.
- Added unique resources for all 14 materials, active/support skill stones,
  and the four material-bag corners.
- Wired the resources into ground loot, material drops, equipment/inventory,
  skill-stone slots, and the material bag. Programmatic shapes remain only as
  load/draw failure fallbacks.
- Gameplay generation, rarity, pickup, crafting, reinforcement, bag selection,
  and skill-loadout rules are unchanged.

## Authored resources and provenance

- Original source board:
  `art_source/stage12/items/dark-steel-ancient-gold-item-board-v1.png`.
- Generated with OpenAI's built-in ImageGen tool on 2026-07-22. The prompt
  requested an original 6 x 5 board of dark-steel, ancient-gold and blue-white
  energy ARPG icons on removable flat magenta, with unique silhouettes and no
  text, logos, watermarks, or references to existing game assets.
- Deterministic builder: `tools/build_item_material_atlas.py`.
- Runtime pair: `assets/stage12/items_ui.png` and
  `assets/stage12/items_ui_material.png`, both RGBA 1024 x 1024.
- Resource inventory: six equipment silhouettes, four geometrically distinct
  rarity sigils, active/support stones, 14 material icons, and four bag-frame
  corners (30 authored cells total).
- Pair cost is 8,388,608 decoded bytes. The complete Stage12 paired-texture
  budget is 175,816,704 bytes and remains below the 256 MiB validator ceiling.

## TDD evidence

- Asset RED: the new item pipeline test failed before the atlas builder/assets
  existed; GREEN is now 6/6 for dimensions/alpha parity, cell uniqueness and
  safe margins, outline contrast, rarity-mask distinction, background matte,
  subject connectivity, clean borders, and noise-independent anchoring.
- Platform RED: compilation failed while the new slot/rarity/material mappings
  and sprite-draw telemetry were absent; GREEN is 417 cases, 0 failures.
- Formal RED: compilation failed before `Stage12MaterialRuntimeStatus` exposed
  item-atlas residency and per-resource draw counts; GREEN now requires all six
  equipment, four rarity, and 14 material resources to record successful real
  draws.
- Validator negative coverage rejects solid or no-item evidence, missing item
  draw telemetry, duplicated ecology evidence, baseline-only monster regions,
  and invalid runtime draw/frame proof.

## Fresh verification

- x64 Debug build: `arpg_game`, `arpg_platform_tests`,
  `arpg_stage12_material_formal`, Stage16 and Stage17 validation executables:
  PASS.
- `arpg_platform_tests.exe`: 417 cases, 0 failures.
- `stage12.item_material_asset_pipeline`: PASS.
- `stage12.material_formal`: PASS using real raylib 6.0/OpenGL presentation.
- `stage12.material_evidence_validator`: PASS.
- `stage12.material_evidence_validator_self_test`: PASS.
- `stage12.material_root_safety`: PASS.
- Formal screenshot:
  `out/build/windows-msvc-debug/tests/platform/stage12 material evidence/stage12-run/items-materials-1280x720.png`.
- Old `loot_icon_*` references: zero under `src`, `tests`, `tools`, and `assets`.

## Concerns outside this slice

- The broader Stage16 real-raylib scenario still exits before rendering because
  its deterministic gameplay driver does not obtain the expected material drop.
  Unit/simulation Stage16 tests pass; the failure occurs before the new material
  bag/icon rendering path.
- The broader Stage17 real-raylib scenario reaches `approach_draw` but does not
  accept the skill within its frame budget. The failure occurs before its
  inventory/skill-stone UI path; active/support resource mapping and manifest
  coverage pass in platform tests.

## Important review fixes

- Replaced the broad item-screenshot palette scan with a same-host baseline
  capture and 20 fixed-position checks: six equipment positions and all 14
  material positions must each have a substantial connected difference contour,
  authored chroma, and local color diversity. The negative test replaces the
  item capture with the no-item baseline and must be rejected.
- Replaced per-pixel distance keying with a border-derived, connected magenta
  background matte. Significant components near the largest authored subject
  are retained; detached background fragments are excluded before the subject
  bounding box is centered. Real purple foreground detail is preserved.
- Asset quality coverage is now 8/8 and explicitly checks exact-transparent
  background ratios, the dominant connected subject, five-pixel clean cell
  borders, and a synthetic purple subject whose anchor must not be shifted by
  dark-magenta edge noise.
- Added a localized alpha-edge despill pass after connected-background removal.
  It is restricted to key-mixed pixels within two pixels of the transparent
  background, so genuine purple interiors are not recolored. A synthetic
  key-blended edge sample now proves the edge is made translucent and
  decontaminated while its inner purple sample remains byte-identical.
- Added a per-cell outer-halo quality gate for all non-purple resources. Under
  that metric, the rebuilt atlas reduced the affected outer-magenta ratios from
  approximately 26.2-38.2% to 1.65-6.01% (3.81% mean), with every checked cell
  below the 12% ceiling.
- The fixed-position validator no longer treats the removed magenta fringe as
  authored weapon chroma. The intentionally near-neutral steel weapon uses a
  one-pixel accent floor while retaining the same substantial-difference,
  connected-contour, extent, and 45-color-diversity requirements; the
  baseline-replacement negative test still has to fail.
