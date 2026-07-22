# Lightning material slice report

## Scope

Milestone 3 adds an original lightning ecology material slice without changing
gameplay, AI, damage, movement or monster statistics:

- independent 768 x 768 lightning environment color/material atlases;
- independent 864 x 864 color/material atlases for `lightning_shooter` and
  `lightning_dasher`;
- room background, wall, door, abyss hole, arc lamp, capacitor bank and
  grounding rod presentation;
- five complete authored animation states per monster: idle 12, move 16,
  special 20, hurt 8 and death 16;
- ecology-scoped loading and release, with only the active environment and two
  monster texture pairs resident.

## RED to GREEN

The initial x64 build failed because the lightning room slice and lightning
atlas identifiers did not exist. New tests then specified independent atlas
ownership, room prop coverage, high-contrast dark/yellow/cyan palette, all 72
frames per monster, presenter state mapping and the 256 MiB texture budget.

The complete source-board gate rejects duplicate frames, adjacent changes under
3 percent, a main connected silhouette below 94 percent and secondary
components above 2 percent. The builder normalizes every frame to one scale and
foot anchor; atlas tests additionally require stable shape and a fully collapsed
final death pose. All ten accepted boards pass. A bounded cleanup removes only
detached chroma-key antialias
islands of at most 47 pixels; no pose is synthesized or interpolated.

## Runtime and formal evidence

The formal host forces a lightning room, captures
`lightning-monsters-1280x720.png`, and reports hard runtime assertions for the
shader pipeline, lightning ecology readiness and all three lightning atlas
pairs. The validator also decodes the screenshot, checks all six PNG dimensions
and requires the new paired byte floor.

Visual inspection confirmed a readable dark storm-forge room, cyan electric
accents, brass warning structures, visible lightning shooter/dasher art and an
unobstructed central combat lane.

## Verification

- `python tools/build_lightning_material_slice.py` -- PASS
- MSVC x64 Ninja build of `arpg_platform_tests` and
  `arpg_stage12_material_formal` -- PASS
- `platform.units` -- PASS, 407 cases and 0 failures
- `stage12.material_formal` -- PASS
- `stage12.material_evidence_validator` -- PASS
- `stage12.material_root_safety` -- PASS
- formal paired texture total: 150,765,568 bytes, below 268,435,456 bytes

## Review hardening follow-up

Review found that the original fixed-grid crop could cut a complete source
subject at an invisible grid boundary and that foot anchoring happened before
the final alpha cleanup. RED evidence reproduced shooter foot bottoms spanning
76..92 and dasher special spanning 74..92. It also proved that the formal
validator accepted a solid gray image, a water-ecology substitution and a
capture with both lightning-monster regions erased.

The corrected source path detects complete connected subjects across the full
board, maps exactly one subject to every expected frame slot, requires
non-overlapping direct crop rectangles and preserves all subject pixels. It does
not move, copy, composite, scale or rewrite source character pixels. The
original complete poses remain the direct crop source. `shooter idle v4` was
regenerated because its earlier boards failed the new source-margin gate.

After atlas normalization and tiny-island cleanup, the builder now re-detects
the main component and aligns its bottom to baseline 92. Both Python and C++
tests hard-require every state to satisfy `max(bottom) - min(bottom) <= 1`.
Registered silhouette similarity rejects whole-frame translation or pose pairs
without enough authored change; a separate three-frame gate rejects exact
linear interpolation.

The formal PowerShell validator now decodes the actual lightning capture and
requires a complex dark-storm image with minimum brass-warning and cyan-electric
pixel ratios. It also verifies lightning-specific accent pixels in fixed shooter
and dasher regions. Its negative self-test must reject solid gray, water-room
substitution and erased-monster captures.

Fresh follow-up verification on MSVC x64:

- `python tests/platform/lightning_asset_pipeline_tests.py` -- PASS, 5 tests
- `platform.units` -- PASS, 407 cases and 0 failures
- `stage12.lightning_asset_pipeline` -- PASS
- `stage12.material_formal` -- PASS
- `stage12.material_evidence_validator` -- PASS
- `stage12.material_evidence_validator_self_test` -- PASS
- `stage12.material_root_safety` -- PASS
- combined CTest selection -- 6 of 6 passed

The regenerated formal lightning capture was visually re-inspected after the
atlas rebuild. Both lightning monsters remain visible in their expected combat
regions, the dark/brass/cyan ecology is intact and the central fight lane remains
readable.
