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

## Second review hardening

The interpolation gate no longer depends on an exact 50 percent arithmetic
average. After silhouette registration it estimates the interpolation amount
from all visible RGBA differences, verifies correlated before-to-middle and
middle-to-after deltas, checks normalized fit residuals at 4, 8, 16 and 32
channel-value tolerances, and requires alpha-weighted contour/keypoint moments
to follow a linear trajectory. It evaluates both centroid-registered and
foot/body-anchor-preserving candidates so articulated limbs cannot disguise a
tween or make a real redrawn pose fail merely because its centroid moved.

RED tests demonstrated that the previous gate accepted a bilinear-resampled
tween, a 0.32 eased tween and a midpoint tween with small color perturbation and
six-level compression. Those three samples are now rejected end to end by
`validate_frames`; a deliberately nonlinear intermediate pose remains accepted.
The full generator also accepts every real shooter and dasher source sequence,
providing positive coverage against false rejection.

The formal host now records what happened in the production actor renderer for
both lightning monsters, rather than treating atlas residency as draw proof.
For every presented frame it reports presenter visibility,
`use_material_frame`, selected atlas, selected frame index and the actual
`MaterialPack::draw_frame` return value. The formal result hard-requires the
shooter and dasher to select their respective atlases and complete a material
frame draw.

A second deterministic lightning capture uses the same ecology and camera but
removes all showcase monsters only from the copied formal presentation
snapshot. The validator compares each lightning-monster region against this
background baseline, thresholds material pixel differences, and requires a
large connected contour with minimum width and height. Global lightning palette
checks remain ecology evidence only; they are no longer accepted as evidence
that a monster was drawn. Negative self-tests replace both monster regions with
the exact baseline and separately falsify the runtime `drawn` field; both are
rejected. An out-of-range runtime frame is rejected as well.

Fresh MSVC x64 verification:

- `python tests/platform/lightning_asset_pipeline_tests.py` -- PASS, 9 tests
- `python tools/build_lightning_material_slice.py` -- PASS, every real source board
- build targets `arpg_platform_tests` and `arpg_stage12_material_formal` -- PASS
- `platform.units` -- PASS, 407 cases and 0 failures
- `stage12.lightning_asset_pipeline` -- PASS
- `stage12.material_formal` -- PASS
- `stage12.material_evidence_validator` -- PASS
- `stage12.material_evidence_validator_self_test` -- PASS
- `stage12.material_root_safety` -- PASS
- combined CTest selection -- 6 of 6 passed

The fresh preset configuration confirmed MSVC 19.44 x64 and Windows SDK 26100.
The repository-wide all-target build later stopped in the unrelated existing
`stage11d_loot_formal` target because Win32 `CloseWindow`/`ShowCursor`
declarations conflict with raylib. The two targets in this slice were then
explicitly built in that same fresh tree and the complete six-test selection
above passed; no Stage 11D source was changed here.
