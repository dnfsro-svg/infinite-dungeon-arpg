# Chaos material slice report

## Scope and art

- Added an original chaos rift-forge environment with independent floor, wall,
  door, abyss hole, rift lantern, anomaly condenser and warning obelisk imagery.
  The dark indigo/obsidian base uses restrained magenta rifts and small
  acid-green warning surfaces, keeping the central combat lane readable.
- Added independent 864 x 864 color/material atlases for `chaos_chaser` and
  `chaos_hazard`. Each monster contains idle 12, move 16, special 20, hurt 8
  and death 16 authored poses.
- The accepted built-in ImageGen chroma-key boards are documented in
  `assets/stage12/chaos-source.md`. Three rejected candidates remain untracked:
  chaser special v1, hazard idle v1 and hazard move v1.

## Source-board production gates

- The builder detects complete connected subjects over each full source board,
  maps exactly one subject to each target slot and requires non-overlapping
  direct crop rectangles, complete pixel conservation and at least
  `max(8 px, 2.5%)` source margin.
- Direct-crop validation does not move, copy, composite, scale or rewrite
  character pixels. Normalization begins only after the direct source crop has
  passed. No `Image.blend`, grid-cut character assembly or synthetic tween is
  used.
- Every state rejects duplicate whole frames, adjacent visible change below
  3%, registered translation/silhouette similarity at or above 97%, correlated
  exact/eased/bilinear/color-quantized three-frame interpolation, main
  connectivity below 94%, secondary connectivity above 2% and final foot
  anchor drift above one pixel. Real nonlinear authored poses remain accepted.

## Runtime and evidence

- Added chaos atlas and sprite manifest ownership, 72-frame animation profiles,
  presenter support, independent room material slice and room/prop rendering.
- `MaterialPack::load(MaterialEcology::chaos)` keeps common/player/UI plus only
  the active chaos environment and two monster pairs; switching ecology unloads
  the previous ecology immediately. Full paired manifest size is 167,428,096
  bytes and each active ecology remains below 256 MiB.
- The production host records presenter visibility, `use_material_frame`,
  selected atlas, frame index and actual `draw_frame` result for both chaos
  monsters. Formal success hard-requires shader readiness, chaos ecology
  readiness, all three atlas pairs, correct in-range atlas/frame selection and
  successful draws.
- Formal capture produces `chaos-monsters-1280x720.png` plus the same-ecology
  `chaos-background-1280x720.png`. The validator checks dark/magenta/acid ecology
  evidence and monster-vs-baseline changed connected contours in fixed chaser
  and hazard regions. Negative self-tests reject solid gray, wrong ecology,
  baseline-only monster regions, false runtime `drawn` and out-of-range frames.

## RED to GREEN

- Initial RED: `python tests/platform/chaos_asset_pipeline_tests.py` failed with
  `missing independent chaos material builder` before production code existed.
- Source-board RED candidates were preserved when direct-crop gates rejected
  chaser special v1 for outer safe margin and hazard idle/move v1 for overlapping
  crop rectangles. Their regenerated successors passed unchanged strict gates.
- First x64 platform execution built successfully and reported 413 cases with
  one expected bookkeeping failure because the old contract still required 407;
  the chaos cases then replaced that stale total and passed.
- First formal validator run exposed a pre-existing PowerShell byte-shift color
  bucket overflow; explicit integer shifts restored real complexity evidence.
  The first chaos ROI was then corrected from observed baseline diffs before the
  validator and all negative mutations passed.

## Verification

- `python tools/build_chaos_material_slice.py` -- PASS.
- `python tests/platform/chaos_asset_pipeline_tests.py` -- PASS, 9 tests.
- x64 MSVC build targets `arpg_platform_tests` and
  `arpg_stage12_material_formal` -- PASS.
- `platform.units` -- PASS, 413 cases and zero failures.
- `stage12.chaos_asset_pipeline` -- PASS.
- `stage12.material_formal` -- PASS with real raylib/OpenGL capture.
- `stage12.material_evidence_validator` -- PASS.
- `stage12.material_evidence_validator_self_test` -- PASS.
- `stage12.material_root_safety` -- PASS.

All C++ build and CTest commands use `VsDevCmd.bat -arch=x64 -host_arch=x64`.

## Commit

- `feat: add chaos ecology material slice` (single semantic commit; hash is
  recorded after commit creation). No push is performed.
