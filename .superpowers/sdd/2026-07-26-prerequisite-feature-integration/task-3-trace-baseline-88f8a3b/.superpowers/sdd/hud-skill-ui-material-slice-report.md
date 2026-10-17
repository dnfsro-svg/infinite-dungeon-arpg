# HUD / Skill UI Material Slice Report

## Scope

This slice replaces the flat-color HUD, active-skill, inventory, material-bag,
skill-stone, reinforcement-confirmation, and pause surfaces with one original
dark-steel / ancient-gold / blue-white-energy material family. Gameplay rules,
input bindings, inventory semantics, and pause behavior are unchanged.

## Original assets and deterministic build

- Original ImageGen source board:
  `art_source/stage12/ui/dark-steel-ancient-gold-ui-board-v1.png`.
- Authored atlas pair: `assets/stage12/ui_material.png` and
  `assets/stage12/ui_material_material.png`, both RGBA 1024 x 1024.
- Provenance and prompt constraints are recorded in
  `assets/stage12/ui-material-source.md`; the prompt forbids text, logos,
  watermarks, existing-game references, and copied UI.
- `tools/build_ui_material_atlas.py` deterministically removes the chroma key,
  finds the forty authored ornaments, fits them into an 8 x 5 grid, and emits
  matching visible/material atlases.

## Runtime contract and integration

- Added a `ui_material` atlas and forty unique `ui_*` sprite IDs, each with a
  128 x 128 source cell, explicit anchor, UI class, and unique manifest hash.
- Added `UiMaterialElement`, the forty-entry sprite table, and the 8 MiB decoded
  atlas-pair budget contract in `ui_material.hpp`.
- Added destination-rectangle, ornament-preserving panel composition, and
  aspect-fit region drawing to `MaterialPack`; successful logical UI draws use
  the paired material shader and increment per-sprite telemetry once. Large
  panels expand only a low-feature 1 x 1 background sample, repeat undecorated
  8 px border segments without changing their aspect ratio, then overlay the
  four corners, four edge ornaments, and 64 x 64 center emblem at 1:1 scale.
- HUD tracks/fills/panels/notices/statuses, skill-slot states, inventory tabs and
  grids, material bag, skill-stone grids, reinforcement dialog, and pause menu
  now select assets by semantic state. Legacy geometry is retained only as the
  explicit failed-load fallback.
- The formal host can expose actual inventory, skill-stone, and pause UI states
  without changing normal player input or transitions.
- Inventory, skill-stone, and pause large panels use the ornament-preserving
  composition. The authored `ui_label_plate` is trimmed to its 120 x 67 alpha
  content bounds and aspect-fitted in the real HUD and inventory title paths.
- Active-skill material frames retain a continuous bottom-up cooldown overlay
  and moving boundary line derived from the clamped `cooldown_ratio`.
- Decorated horizontal controls use a three-part horizontal slice: authored
  caps preserve aspect ratio while only an undecorated 8 px center strip
  expands. Formal telemetry rejects direct-stretch draws for all four UI pages
  at both resolutions.

## Bundled font, readability, and resource contract

- Runtime requires the bundled Google Fonts Noto Sans SC variable TTF and does
  not probe Windows fonts. Source URL, retrieval date, SHA-256, and SIL OFL 1.1
  metadata are in `assets/fonts/README.md`; `OFL.txt` is shipped verbatim.
- The fixed production corpus preheats 346 unique Simplified Chinese and ASCII
  glyphs under a hard 384-entry capacity. Raylib rasterizes at a 64 px source
  size, at least 2x the largest 26 px UI size, and uses bilinear downsampling.
- Formal telemetry measured one gray-alpha atlas at 4,194,304 bytes against an
  8,388,608-byte budget. The HUD, pause, and death instances total 12,582,912
  bytes against a 25,165,824-byte budget; shutdown unloads owned fonts.
- Glyph fills are solid RGBA: warm white `(248,246,238,255)`, interaction ice
  blue `(194,229,255,255)`, and warning `(255,210,118,255)`. A separate 1 px
  outline, 2 px shadow, and optional `(5,9,16,232)` backing do not modulate fill.
- Display-luma ratios against the backing are 12.08 primary, 11.03 interaction,
  and 9.98 muted. The contract covers HUD, inventory, material bag, skill page,
  pause menu, and death overlay copy.

## RED / GREEN evidence

The initial RED checks failed because the deterministic builder and
`ui_material.hpp` did not exist. The completed implementation satisfies:

- forty unique IDs and manifest frames, valid anchors, distinct state frames,
  unique UV/hash assignments, and an 8 MiB decoded-byte contract;
- atlas-pair alpha parity, cell uniqueness/detail, safe anchors, authored
  connected contours, and the required steel/gold/energy palette;
- runtime residency plus non-zero telemetry for every one of the forty IDs.

The review-fix RED checks then reproduced four regressions: a missing
`draw_nine_slice` API, a cooldown overlay coupled to the failed-load fallback,
an unused label plate, and gallery-only formal telemetry. The platform build
failed on the missing nine-slice/cooldown contracts; the first formal run then
reported `hud_ui_runtime_draws=fail` and `skill_ui_runtime_draws=fail`. Per-page
draw masks identified the exact state-dependent resources and the final
contracts use only resources guaranteed by each captured production state.

The second review-fix RED tightened this further: every non-background panel
part must preserve source/destination aspect ratio, the 64 x 64 center emblem
must be drawn at its authored size, and label-plate source/destination ratios
must match. It also made the validator reject the old evidence with
`missing report field: hud_ui_runtime_draws_1920` before 1920 page-local
telemetry was implemented.

The typography RED sequence then failed on the missing bundled font,
horizontal-slice API, safe-layout boxes, direct-stretch telemetry, contrast
style, and high-resolution resource fields. The high-resolution RED failed on
the absent `kUiFontSourceBaseSize`, `kUiFontMaximumDisplaySize`, and
`kUiFontAtlasByteBudget`; the solid-fill RED failed on the absent interaction
color role. All now have unit, source, formal, validator, and negative coverage.

## Formal evidence and anti-fallback validation

The same real Raylib host captured:

- `ui-baseline-1280x720.png`
- `ui-hud-1280x720.png`
- `ui-hud-1920x1080.png`
- `ui-gallery-1280x720.png`
- `ui-inventory-1280x720.png`
- `ui-inventory-1920x1080.png`
- `ui-skill-stones-1280x720.png`
- `ui-skill-stones-1920x1080.png`
- `ui-pause-1280x720.png`
- `ui-pause-1920x1080.png`

The validator compares all forty exact gallery ROIs with the baseline, requires
an independently captured HUD file plus changed HUD ROIs, checks that the actual
inventory/skill-stone/pause pages differ materially from the baseline, validates
all four 1920 x 1080 captures against `ui-baseline-1920x1080.png` with both
whole-frame and page ROI differences, and verifies both atlas files and
residency. At both resolutions each page has a separate runtime status object,
pass field, and exact resource draw mask; 1280 or gallery telemetry cannot
satisfy a 1920 page contract. The self-test rejects hidden or
baseline-substituted HUD/inventory/skill-stone/pause images and rejects missing
telemetry independently for every page at both resolutions. Existing
item/ecology negative mutations remain covered. The self-test also rejects an
empty font subset, low-resolution source, over-budget atlas, low contrast,
non-solid fill, direct-stretched decorations, and page text-layout failures.

Formal evidence reports `atlas_bytes=184205312`, below the 256 MiB ceiling.

## Fresh verification (2026-07-22)

- MSVC x64 build: `arpg_game`, `arpg_platform_tests`, and
  `arpg_stage12_material_formal` succeeded.
- `platform.units`: 428 cases, 0 failures.
- UI material/font source pipeline: 7 tests, 0 failures.
- `stage12.material_formal`: passed.
- `stage12.material_evidence_validator`: passed.
- `stage12.material_evidence_validator_self_test`: passed.
- `stage12.material_root_safety`: passed.
- Combined formal/validator selection: 4/4 passed, 0 failed.
- `git diff --check`: passed.

## Residual concern

The ornament-preserving renderer intentionally trades several small border
draws for undistorted art; the formal host covers this path at both required
resolutions. The reviewed captures show 1:1 center and edge emblems, undistorted
corner gems, continuous repeated borders, uncropped titles, and readable solid
warm-white/ice-blue text at 1280 x 720 and 1920 x 1080. The atlas is deliberately
corpus-specific; new UI copy must update the fixed plan and coverage test. No
gameplay, input, or state-machine behavior changed in this slice.
