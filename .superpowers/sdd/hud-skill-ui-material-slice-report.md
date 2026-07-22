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
- Added destination-rectangle material drawing to `MaterialPack`; successful
  UI draws use the paired material shader and increment per-sprite telemetry.
- HUD tracks/fills/panels/notices/statuses, skill-slot states, inventory tabs and
  grids, material bag, skill-stone grids, reinforcement dialog, and pause menu
  now select assets by semantic state. Legacy geometry is retained only as the
  explicit failed-load fallback.
- The formal host can expose actual inventory, skill-stone, and pause UI states
  without changing normal player input or transitions.

## RED / GREEN evidence

The initial RED checks failed because the deterministic builder and
`ui_material.hpp` did not exist. The completed implementation satisfies:

- forty unique IDs and manifest frames, valid anchors, distinct state frames,
  unique UV/hash assignments, and an 8 MiB decoded-byte contract;
- atlas-pair alpha parity, cell uniqueness/detail, safe anchors, authored
  connected contours, and the required steel/gold/energy palette;
- runtime residency plus non-zero telemetry for every one of the forty IDs.

## Formal evidence and anti-fallback validation

The same real Raylib host captured:

- `ui-baseline-1280x720.png`
- `ui-gallery-1280x720.png`
- `ui-inventory-1280x720.png`
- `ui-skill-stones-1280x720.png`
- `ui-pause-1280x720.png`

The validator compares all forty exact gallery ROIs with the baseline, checks
that the actual inventory/skill-stone/pause pages differ materially from the
baseline, verifies both atlas files and residency, and requires full runtime
draw telemetry. Its self-test proves rejection of a hidden gallery, missing UI
telemetry, and a baseline-substituted inventory page. Existing item/ecology
negative mutations remain covered.

Formal evidence reports `atlas_bytes=184205312`, below the 256 MiB ceiling.

## Fresh verification (2026-07-22)

- MSVC x64 build: `arpg_game`, `arpg_platform_tests`, and
  `arpg_stage12_material_formal` succeeded.
- `platform.units`: 419 cases, 0 failures.
- Asset pipelines: item material and UI material both passed.
- `stage12.material_formal`: passed.
- `stage12.material_evidence_validator`: passed.
- `stage12.material_evidence_validator_self_test`: passed.
- `stage12.material_root_safety`: passed.
- Combined CTest selection: 7/7 passed, 0 failed.
- `git diff --check`: passed.

## Residual concern

The atlas cells are fixed 128 x 128 ornaments rather than nine-slice panels, so
the largest destination rectangles intentionally stretch some surface detail.
This is visual-only and is covered by the real-host screenshots; no gameplay or
state-machine behavior changed in this slice.
