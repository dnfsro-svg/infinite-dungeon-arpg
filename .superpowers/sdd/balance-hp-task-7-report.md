# Task 7 Report — High-resolution health potion presentation

Status: `DONE`

## Scope

- Worktree: `E:\game\.worktrees\balance-hp-potions`
- Branch: `codex/extreme-monster-nerf-hp-potions`
- Base HEAD: `6ffa3cf1108fcd1ff2501221d29c3f32d30a3a59`
- Tracked changes are limited to the 12 Task 7 allowlisted files.
- No gameplay, persistence, drop, healing, threshold, room, monster, or font-system code was changed.

## TDD RED

Baseline before test changes:

- `arpg_platform_tests.exe`: `443 cases, 0 failures`
- CTest `platform.units` and `stage12.item_material_asset_pipeline`: 2/2 passed.

After adding and registering the three brief-mandated C++ cases plus one Python asset case:

- C++ build failed for the expected missing Task 7 contracts only: `MaterialLootLabel::kind`, `SecondaryLootKind::health_potion`, `MaterialSpriteId::health_potion`, and `loot_label_rects_overlap`.
- Python ran 9 tests; the new case alone errored because `BUILDER.HEALTH_POTION_SOURCE` did not yet exist.

Review-driven TDD added two C++ regressions:

- Simultaneous new material and potion receipts first produced `447 cases, 1 failures`, failing at `material.ready`; the fixed pending material feedback now returns the potion immediately, the material once on the next observe, and no duplicate after that.
- Two same-position potion labels in a 40px-high viewport reproduced the top-clamp deadlock as a `platform.units` timeout at 20.06 seconds. The replacement uses bounded fixed-capacity placement: vertical steps first, then deterministic horizontal slots across fixed rows; an unplaceable label is removed and counted as saturated, so the returned view never retains an overlap.

## Image generation

- Mode: built-in `image_gen`, followed by the installed local chroma-key helper. No CLI/API fallback and no Pillow/geometric main-visual fabrication were used.
- Final built-in keyed output: `E:\codex1\.codex\generated_images\019f931a-d3c8-7af2-b037-bcf88ea4b4fc\exec-6b72c053-8e9e-456e-9102-f1813ef568e4.png`
- Ignored keyed workspace copy: `.superpowers/sdd/tmp/health-potion-keyed-final.png`
- Final project source: `art_source/stage12/items/health-potion-v1.png`

Final built-in edit prompt:

```text
Use case: precise-object-edit
Asset type: game UI inventory icon source
Primary request: Change only the broken disconnected bottle-cap area in the provided health potion image.
Replace all floating or separated cap pieces with one simple compact antique brushed-gold cylindrical stopper and neck band that is physically continuous with and firmly attached to the red glass neck. The stopper must be one connected silhouette with no finial, no floating parts, no gaps, no crest, no emblem, no rune, no symbol, no filigree, and no ornament.
Preserve the clean unadorned ruby-red transparent glass bottle body, visible red liquid, dark high-contrast outline, controlled white highlights, centered front three-quarter composition, symmetric readable silhouette, and generous padding exactly.
Scene/backdrop: perfectly flat solid exact #00ff00 chroma-key background; one uniform color with no shadow, gradient, texture, reflections, floor plane, vignette, or lighting variation.
Constraints: one connected bottle subject only; no platform, scenery, particles, outer glow, cast/contact shadow, letters, numbers, symbols, logo, UI frame, watermark, baked text, or additional objects. Do not use #00ff00 inside the subject.
```

Local removal:

```powershell
python 'E:\codex1\.codex\skills\.system\imagegen\scripts\remove_chroma_key.py' `
  --input '.superpowers\sdd\tmp\health-potion-keyed-final.png' `
  --out 'art_source\stage12\items\health-potion-v1.png' `
  --auto-key border --soft-matte --transparent-threshold 12 `
  --opaque-threshold 220 --despill --force
```

The helper sampled key color `#0ef217` and produced 1,164,948 fully transparent plus 2,999 partially transparent pixels out of 1,572,516. No `edge-contract` retry was needed.

Rejected candidates and their local keyed/transparent evidence remain under `.superpowers/sdd/tmp/`; they were not used in the atlas.

## Visual validation

`view_image` at original detail confirmed:

- 1254x1254 RGBA source with all four corner alpha values equal to zero;
- one connected centered bottle with generous padding;
- saturated ruby-red glass and visible red liquid;
- simple brushed-gold attached cap and neck band;
- dark readable silhouette and controlled white highlights;
- no green fringe, text, letters, numbers, symbols, watermark, scene, platform, shadow, or outer glow.

Measured source evidence:

- alpha >= 24 coverage: 25.90%; bbox `(315, 144, 935, 1190)`;
- connected components at alpha >= 24: 1; largest-component ratio: 1.0;
- visible red-dominant ratio: 96.09%; green-dominant ratio: 0.0%.

Atlas cell 30 was inspected as an original 128x128 crop from `(768,384)`:

- visible pixels with alpha >= 96: 5,048;
- red-dominant ratio: 96.93%;
- outline contrast score: 0.883756;
- transparent background and recognizable red bottle/gold cap retained after downsampling.

## Implementation

- Preserved the original 30 board names as `BOARD_ICON_NAMES` and added `HEALTH_POTION_SOURCE` plus cell 30 to `ICON_CELLS`.
- Regenerated both `items_ui` atlases with the health potion at cell 30.
- Added `MaterialSpriteId::health_potion` and the manifest frame at `(768,384)`.
- Added fixed-capacity `SecondaryLootKind` labels, pure RGBA `{255,48,48,255}` runtime text `生命药`, emphasized sprite/beam presentation, ordinal sorting, and bounded deterministic collision resolution.
- Updated room rendering to resolve material and health-potion positions by kind rather than treating all secondary loot as material.
- Added an independent health-potion receipt generation baseline and exact one-shot `生命药 +N HP` feedback for newer valid committed receipts only; simultaneous material feedback is retained in a fixed pending slot and delivered once.

## GREEN verification

- `python .\tools\build_item_material_atlas.py`: built 31 icons.
- `python -m unittest tests.platform.item_material_asset_pipeline_tests -v`: 9/9 passed.
- `arpg_platform_tests.exe`: `448 cases, 0 failures`.
- CTest regex `^(platform\.units|stage12\.item_material_asset_pipeline)$`: 2/2 passed, 0 failures.
- `git diff --check`: passed.

## Artifact hashes

| File | Bytes | SHA-256 |
|---|---:|---|
| `art_source/stage12/items/health-potion-v1.png` | 826200 | `aff8f47b3af88265f1ccc3943c7e552d8cb901653ab8845f0fbb918a27e29017` |
| `assets/stage12/items_ui.png` | 706074 | `1e4123d31c8665e93e911feb830cf4f95b4b492287c193922ff2731060bb5b50` |
| `assets/stage12/items_ui_material.png` | 675012 | `537239648778ba89003a0aff62096280151b119e16764e561749b055567755bf` |

## Tracked files

1. `art_source/stage12/items/health-potion-v1.png`
2. `tools/build_item_material_atlas.py`
3. `assets/stage12/items_ui.png`
4. `assets/stage12/items_ui_material.png`
5. `src/platform/raylib/material_asset_types.hpp`
6. `src/platform/raylib/material_manifest.hpp`
7. `src/platform/raylib/material_loot_view.hpp`
8. `src/platform/raylib/material_loot_view.cpp`
9. `src/platform/raylib/room_renderer.cpp`
10. `tests/platform/material_loot_view_tests.cpp`
11. `tests/platform/item_material_asset_pipeline_tests.py`
12. `tests/platform/platform_test_main.cpp`

## Concerns

- The Debug build continues to emit the pre-existing `pause_menu_view_tests.cpp(377)` MSVC C4127 warning; Task 7 did not touch that file.
- The unique report and rejected image evidence are intentionally ignored and are not part of the commit.

## Independent review fixes — 2026-07-24

Two Important findings from the independent Task 7 review were fixed with new
test-first regressions. The tracked fix remains limited to four Task 7
allowlisted files: `material_loot_view.hpp/.cpp`,
`material_loot_view_tests.cpp`, and `platform_test_main.cpp`.

### Placement pressure RED/GREEN

- The regression fills all 400 material entries and all 192 health-potion
  entries, uses one invalid material to audit accounting, and runs both a
  same-position centered case and a top-clamped case on a saturated 1280x720
  screen.
- The first RED was the missing opt-in diagnostics contract. After attaching
  counters to the unchanged old algorithm, the centered case measured 65,104
  placement probes and 2,833,456 previous-label collision operations. The
  top-clamped case measured 4,118 probes and 13,514 collision operations.
- The replacement generates one deduplicated fixed slot grid per build and
  uses a fixed 592-bit occupancy map. Each ordinal scans each slot at most
  once, selects the closest free slot by squared distance, and breaks exact
  ties upward and then left. There is no dynamic allocation and no scan over
  previously retained labels.
- Both final pressure cases deterministically measure 92,196 probes and 92,196
  occupancy checks: 591 valid labels times 156 screen slots. This is below the
  asserted strict bound of 592 squared, or 350,464 operations.
- Each case also verifies strict ordinal order, all-pairs non-overlap for every
  retained label, nonzero saturation, and exact accounting:
  `count + capacity_saturation_count + invalid_material_count == 592`.

### Receipt queue RED/GREEN

- RED reproduced the lost generation-11 potion receipt: after generation 10
  produced material plus `生命药 +250 HP`, the old early return published the
  pending material before consuming generation 11, so the assertion for
  `生命药 +251 HP` failed.
- `observe()` now consumes and advances both receipt generations before it
  publishes one feedback item. Its fixed two-entry queue inserts potion
  feedback ahead of the single coalesced cumulative material entry. This
  capacity is sufficient for the reachable per-observe input of one potion
  and one material receipt; a 64-generation continuous dual-receipt stress
  test reaches the fixed capacity repeatedly without dropping a potion.
- The exact minimal output order is `+250`, `+251`, `+252`, retained material
  `混沌石 x1`, then no duplicate. The stress test sees every potion exactly
  once and eventually publishes cumulative material `混沌石 x64`.
- Invalid receipts do not advance a generation. A valid zero-restore potion
  receipt advances its generation but emits no success, and a later positive
  value at that same generation does not emit. Zero-count material receipts
  follow the same consume-without-success rule.

### Review-fix verification

- Independent fix commit: `ca854bd85049a302919423c8bf17c653be5c9d55`
  (`fix: bound loot labels and queue pickup feedback`).
- `arpg_platform_tests.exe`: `450 cases, 0 failures`.
- `python .\tools\build_item_material_atlas.py`: built 31 icons.
- Python asset tests: 9/9 passed.
- CTest regex `^(platform.units|stage12.item_material_asset_pipeline)$`: 2/2
  passed.
- `git diff --check`: passed.
- Tracked diff whitelist: four Task 7 files, zero violations.
- Tracked `.superpowers/sdd/task-7-report.md`: unchanged.

Remaining Minor: receipt generation rollback/reset is still handled by the
existing strict-greater comparison. The independent review explicitly marked
generation rollback as out of scope, so this fix does not broaden Task 7.

## Second independent review fix — original anchors and full-screen candidates

The second Important review finding was fixed test-first on top of
`ca854bd85049a302919423c8bf17c653be5c9d55`. The tracked change remains limited
to the same four Task 7 allowlisted files: `material_loot_view.hpp/.cpp`,
`material_loot_view_tests.cpp`, and `platform_test_main.cpp`.

### RED evidence

- Three platform cases were added first and the exact platform total was
  changed from 450 to 453.
- Fresh `platform.units` produced `453 cases, 3 failures`:
  `single secondary label preserves original clamped rect`,
  `identical anchors keep first and prefer upward`, and
  `large viewport candidates reach bottom near anchor`.
- The old global-grid implementation moved a lone 1280x720 bottom label from
  original rect `(545,597.6)` to approximately `(439.2,601.2)`, a
  `(-105.8,+3.6)` offset. The top-clamped case also moved x by `-105.8`.
  At 3840x2160, row-major truncation of the first 592 natural slots moved the
  bottom rect from y `1864.8` to approximately `805.5`, over 1059 pixels away.
- A second test-first diagnostic compile RED then failed only because the old
  `placement_probe_count/collision_operation_count` contract lacked the new
  direct-collision, candidate-probe, and occupancy-mark counters.

### GREEN implementation

- Every label first checks its already-clamped original rect against retained
  earlier labels. A non-conflicting rect is retained byte-for-byte; its anchor
  is never rewritten or grid-snapped.
- A conflicting later label first attempts an exact same-x upward move of
  `rect.height + 3`. If a complete upward step is unavailable at the top, it
  falls back to the fixed candidate grid.
- Remaining candidates use the strict direction priority
  `upward < side < downward`, then squared distance, y, and x for deterministic
  nearest selection.
- Natural grids up to 592 slots remain unchanged. Larger grids are compressed
  proportionally across both axes and interpolated over both complete screen
  extents. The 3440x1440 grid is 13x45 (585 slots); the 3840x2160 grid is
  11x53 (583 slots). Both retain top/bottom and left/right endpoints, with no
  row-major prefix truncation. Generated slots are deduplicated in the fixed
  592-entry array.
- Every exact retained rect scans the fixed slot set once and marks every
  intersecting slot occupied. Original and preferred-up collision checks scan
  only the fixed retained rect array. No dynamic allocation or new container
  was introduced; inability to place still increments saturation and removes
  the later label.

### Bounded pressure evidence

The existing all-400-material plus all-192-potion pressure fixture remains
GREEN, including one invalid material, ordinal ordering, all-pairs non-overlap,
exact accounting, and fixed saturation. Each diagnostic category is asserted
independently at or below `592^2 = 350,464`:

- top-clamped z=100: retained 155, saturated 436, direct collision checks 590,
  candidate probes 92,040, occupancy marks 24,180;
- centered z=0: retained 154, saturated 437, direct collision checks 1,769,
  candidate probes 91,884, occupancy marks 24,024.

The continuous 64-generation dual material/potion receipt queue regression is
unchanged and remains in the same passing platform executable.

### Second-fix verification

- Debug platform executable: `453 cases, 0 failures`.
- `python .\tools\build_item_material_atlas.py`: built 31 icons.
- Python asset tests: 9/9 passed.
- CTest regex `^(platform.units|stage12.item_material_asset_pipeline)$`: 2/2
  passed, 0 failures; latest `platform.units` duration 9.40 seconds.
- Existing ground-loot allocation stress: 100,000 builds, 50,000 unchanged,
  50,000 alternating, 0 allocations.
- `git diff --check`: passed. Tracked whitelist: exactly four Task 7 files,
  zero violations. Tracked `.superpowers/sdd/task-7-report.md`: unchanged.
  Unique report ignore rule: `E:/game/.git/info/exclude:7:.superpowers/`.
- Independent fix commit: `bfafd1e` (`fix: preserve loot label anchors across
  viewports`).

## Material loot render-stack regression fix — 2026-07-24

### Root cause and RED

- The Debug formal host reproducibly exited with Windows stack-overflow code
  `0xC00000FD`. The dump/PDB chain was `_alloca_probe ->
  build_material_loot_view -> make_combat_render_plan -> CombatRenderer::draw
  -> run_raylib_host`.
- `MaterialLootLabel` embedded a 64-byte text buffer in each of the unchanged
  592 fixed slots. The measured sizes were `MaterialLootLabel=100`,
  `MaterialLootView=59216`, and `CombatRenderPlan=79992` bytes with the formal
  PE's unchanged 1 MiB stack reserve.
- A new fixed-stack budget regression was added before the implementation and
  the platform total was raised from 453 to 454. Fresh Debug output was
  `454 cases, 1 failures`; the only failure was
  `sizeof(MaterialLootView) <= 40 KiB`, with the measured 59,216-byte view.

### Minimal GREEN

- `MaterialLootLabel::text` is now a `std::string_view`. Material names still
  come exclusively from `material_label()` string literals and the potion
  label still points to the static literal `生命药`, so every reference has
  static lifetime and remains NUL-terminated for the HUD renderer.
- The builder assigns those views directly and no longer formats/copies label
  text per slot. Tests use length-aware `string_view` equality.
- No capacity, placement, color, sort order, layout, atlas, gameplay, dynamic
  allocation, or linker stack setting changed.
- Fresh sizes are `MaterialLootLabel=56`, `MaterialLootView=33168`, and
  `CombatRenderPlan=53944` bytes. The view is below 40 KiB and below the old
  400-slot pre-Task7 view size.

### Verification

- Debug platform executable: `454 cases, 0 failures`.
- Python item material asset tests: 9/9 passed.
- CTest `platform.units|stage12.item_material_asset_pipeline`: 2/2 passed.
- Focused `stage9.validation_game.capture_after_present`: 1/1 passed in 3.72s.
- Direct Debug formal host: 3.825s, exit 0, fresh screenshot generated.
- `stage9.formal_game.capture_after_present`: 1/1 passed in 4.36s. Screenshot
  was 1,818,980 bytes with SHA-256
  `EA2DD9964136BA53B4F0700F163F61E804557FB22954182063FF8E669BECA100`.
- Independent validation of that screenshot reported
  `formal_capture=PASS`, 1,149 colors, 3,433 non-background samples, 3,351
  transitions, luminance range 253,875, 2,954 dark samples, and 612 mid-tone
  samples.
- Later repetition attempts on the heavily loaded host timed out at changing
  resource-load points, with no stack-overflow exit. Fresh system samples
  showed available memory as low as 66 MiB and physical-disk traffic up to
  344.5 MB/s. Repeated graphics stability remains part of Task 8's full gate;
  no timeout, user process, stack reserve, or resource-loading behavior was
  changed here.
- Independent code review was clean and independently reran platform 454/0.
- `git diff --check` passed. Only the four Task 7 platform allowlist files are
  committed by this fix; the three concurrent Task 8 dungeon test changes
  remain unstaged and untouched.
