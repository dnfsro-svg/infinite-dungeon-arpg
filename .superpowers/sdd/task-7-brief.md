# Task 7 Brief — High-resolution potion art, pure-red labels, and HUD feedback

## Scope

Implement only Task 7 in `E:\game\.worktrees\balance-hp-potions` after Task 6 review.

Allowed files:

- Create `art_source/stage12/items/health-potion-v1.png`
- Modify `tools/build_item_material_atlas.py`
- Regenerate `assets/stage12/items_ui.png`
- Regenerate `assets/stage12/items_ui_material.png`
- Modify `src/platform/raylib/material_asset_types.hpp`
- Modify `src/platform/raylib/material_manifest.hpp`
- Modify `src/platform/raylib/material_loot_view.hpp`
- Modify `src/platform/raylib/material_loot_view.cpp`
- Modify `src/platform/raylib/room_renderer.cpp`
- Modify `tests/platform/material_loot_view_tests.cpp`
- Modify `tests/platform/item_material_asset_pipeline_tests.py`
- Modify `tests/platform/platform_test_main.cpp`

Report: `.superpowers/sdd/balance-hp-task-7-report.md` (never modify tracked `.superpowers/sdd/task-7-report.md`).

## Required presentation

- A dedicated 1024x1024-or-larger source PNG, one centered compact bottle, transparent background after local key removal.
- Saturated ruby-red transparent glass, clearly visible red liquid, dark high-contrast silhouette, antique brushed-gold cap/neck band, controlled white highlights, premium hand-painted realistic material rendering.
- Front three-quarter view, readable symmetric silhouette, generous padding.
- Absolutely no baked text, letters, numbers, symbols, logo, UI frame, watermark, platform, scenery, particles, or outer glow.
- Atlas cell 30 at source `(768,384)` in the existing 8-column 128px cell layout.
- Ground label text exactly `生命药`, pure solid RGBA `{255,48,48,255}`, emphasized; dedicated red beam/sprite.
- Labels must be collision-resolved with fixed arrays and never overlap.
- HUD feedback only for a new exact committed valid receipt: `生命药 +N HP`; baseline generation and duplicate generation produce no message.

## TDD: tests first

Read and follow:
`E:\codex1\.codex\plugins\cache\openai-curated-remote\superpowers\6.1.1\skills\test-driven-development\SKILL.md`

Add these three C++ cases first and register them; platform total `443 -> 446` (or exactly +3 from current reviewed baseline):

```cpp
arpg::test::Failure health_potion_uses_dedicated_sprite_and_pure_red_label() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.ground_health_potion_count = 1U;
    snapshot.ground_health_potions[0] = {7U, 15U, {2.0F, 1.0F, 0.0F}};
    const platform::MaterialLootView view =
        platform::build_material_loot_view(snapshot, 1280.0F, 720.0F);
    ARPG_REQUIRE(view.count == 1U);
    ARPG_REQUIRE(view.labels[0].kind
        == platform::SecondaryLootKind::health_potion);
    ARPG_REQUIRE(view.labels[0].sprite
        == platform::MaterialSpriteId::health_potion);
    ARPG_REQUIRE(view.labels[0].text_color.r == 255U);
    ARPG_REQUIRE(view.labels[0].text_color.g == 48U);
    ARPG_REQUIRE(view.labels[0].text_color.b == 48U);
    ARPG_REQUIRE(view.labels[0].text_color.a == 255U);
    ARPG_REQUIRE(view.labels[0].emphasized);
    ARPG_REQUIRE(std::strcmp(view.labels[0].text.data(), "生命药") == 0);
    const auto manifest = platform::default_material_manifest();
    const auto* frame = platform::find_material_frame(
        manifest, platform::MaterialSpriteId::health_potion);
    ARPG_REQUIRE(frame != nullptr);
    ARPG_REQUIRE(frame->atlas == platform::MaterialAtlasId::items_ui);
    ARPG_REQUIRE(frame->source.x == 768.0F);
    ARPG_REQUIRE(frame->source.y == 384.0F);
    return {};
}

arpg::test::Failure overlapping_secondary_loot_labels_are_resolved() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.ground_health_potion_count = 2U;
    snapshot.ground_health_potions[0] = {2U, 5U, {0.0F, 0.0F, 0.0F}};
    snapshot.ground_health_potions[1] = {3U, 7U, {0.0F, 0.0F, 0.0F}};
    const auto view = platform::build_material_loot_view(snapshot, 1280.0F, 720.0F);
    ARPG_REQUIRE(view.count == 2U);
    ARPG_REQUIRE(!platform::loot_label_rects_overlap(
        view.labels[0].rect, view.labels[1].rect));
    return {};
}

arpg::test::Failure health_potion_feedback_only_observes_new_committed_receipts() noexcept {
    platform::MaterialPickupFeedbackState feedback{};
    dungeon::DungeonSnapshot snapshot{};
    snapshot.health_potion_pickup_receipt = {true, false, 1U, 10U, 250};
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);
    snapshot.health_potion_pickup_receipt.commit_generation = 11U;
    snapshot.health_potion_pickup_receipt.restored_hp = 251;
    const auto committed = feedback.observe(snapshot);
    ARPG_REQUIRE(committed.ready);
    ARPG_REQUIRE(committed.emphasized);
    ARPG_REQUIRE(std::strcmp(
        committed.text.bytes.data(), "生命药 +251 HP") == 0);
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);
    return {};
}
```

Add the Python asset test:

```python
def test_health_potion_source_and_atlas_cell_are_high_resolution_red_glass(self) -> None:
    source = Image.open(BUILDER.HEALTH_POTION_SOURCE).convert("RGBA")
    self.assertGreaterEqual(source.width, 1024)
    self.assertGreaterEqual(source.height, 1024)
    icon = BUILDER.crop_cell(
        Image.open(ROOT / "assets/stage12/items_ui.png").convert("RGBA"),
        BUILDER.ICON_CELLS["health_potion"])
    visible = [pixel for pixel in icon.get_flattened_data() if pixel[3] >= 96]
    self.assertGreater(len(visible), 1500)
    self.assertGreater(sum(red > green * 1.35 and red > blue * 1.15
                           for red, green, blue, _ in visible),
                       len(visible) * 0.28)
    self.assertGreaterEqual(BUILDER.outline_contrast_score(icon), 0.18)
```

Run the Debug platform/asset target and capture valid RED: missing sprite/kind/source/cell.

## Image generation workflow (mandatory)

Use the built-in `image_gen` tool, not a drawing script and not CLI fallback. The main agent has already read the imagegen skill and selected the built-in chroma-key workflow. Do not use `gpt-image-1.5`/CLI without explicit new user confirmation; if key removal fails after one targeted retry, return `NEEDS_CONTEXT` with artifacts intact.

Generate a square source using this normalized prompt:

```text
Use case: stylized-concept
Asset type: game UI inventory icon source
Primary request: one premium health potion bottle for a dark-fantasy 2.5D action ARPG
Scene/backdrop: perfectly flat solid #00ff00 chroma-key background for local removal; one uniform color, no shadow, gradient, texture, reflections, floor plane, or lighting variation
Subject: a single compact centered health potion bottle, saturated ruby-red transparent glass, clearly visible red liquid, thick dark high-contrast silhouette, antique brushed-gold metal cap and neck band, subtle controlled white specular highlights
Style/medium: premium hand-painted realistic game material rendering
Composition/framing: front three-quarter view, symmetric readable silhouette, generous padding, subject fully separated from background
Materials/textures: physically coherent ruby glass and brushed gold; do not use #00ff00 in the bottle, liquid, highlights, or reflections
Constraints: no platform, scenery, particles, glow outside the bottle, cast/contact shadow, reflection outside the subject, letters, numbers, symbols, logo, UI frame, watermark, or baked text
```

Copy the generated source from the built-in output location into an ignored temporary workspace path, then run the installed helper:

```powershell
python 'E:\codex1\.codex\skills\.system\imagegen\scripts\remove_chroma_key.py' `
  --input <generated-keyed-source> `
  --out 'art_source\stage12\items\health-potion-v1.png' `
  --auto-key border --soft-matte --transparent-threshold 12 `
  --opaque-threshold 220 --despill
```

Use `view_image` with original detail to inspect the final file. Validate:

- RGBA with fully transparent corners/background;
- one subject only; meaningful nontransparent coverage and generous padding;
- red glass/liquid and gold cap; dark readable outline;
- no green fringe, baked text, symbol, watermark, scene, platform, or outer glow.

If a thin fringe remains, retry once with `--edge-contract 1`; use feather only if visibly stair-stepped. Never fabricate the main visual with Pillow or geometric drawing.

## Atlas integration

Preserve the existing 30 board names as `BOARD_ICON_NAMES`, then add:

```python
HEALTH_POTION_SOURCE = (
    ROOT / "art_source" / "stage12" / "items" / "health-potion-v1.png")
ICON_CELLS = {name: index for index, name in enumerate(BOARD_ICON_NAMES)}
ICON_CELLS["health_potion"] = len(BOARD_ICON_NAMES)  # cell 30
```

After the board loop in `build_color_atlas()`:

```python
potion = contain_icon(Image.open(HEALTH_POTION_SOURCE).convert("RGBA"))
if outline_contrast_score(potion) < 0.18:
    raise RuntimeError("health_potion: authored outline contrast is too low")
cell = ICON_CELLS["health_potion"]
column = cell % (ATLAS_SIZE // CELL)
row = cell // (ATLAS_SIZE // CELL)
atlas.alpha_composite(potion, (column * CELL, row * CELL))
```

`main()` checks both authored sources. Regenerate both atlas outputs and run the Python tests.

## C++ integration

- Add `MaterialSpriteId::health_potion` in the material segment and manifest frame:

```cpp
ARPG_ITEM_FRAME(health_potion, 30,
    MaterialLayer::body, MaterialClass::loot),
```

- Add:

```cpp
enum class SecondaryLootKind : std::uint8_t {
    material,
    health_potion,
};
```

- Add `kind` to `MaterialLootLabel` and expose:

```cpp
[[nodiscard]] bool loot_label_rects_overlap(
    LootLabelRect left, LootLabelRect right) noexcept;
```

- Build material labels first, then potion labels using claim ordinal, pure red `{255,48,48,255}`, sprite `health_potion`, `emphasized=true`, text exactly `生命药`.
- Sort all labels by ordinal. Resolve overlap from front to back by moving the later label upward by `kLabelHeight + 3.0F`, re-clamping, until it no longer overlaps any prior label. Fixed arrays only.
- `room_renderer.cpp` must branch by `SecondaryLootKind`; find the correct ground material or ground potion position, then reuse emphasized red `DrawLineEx` beam and dedicated sprite. Never assume every label is a material.
- Add independent `health_potion_generation_` to `MaterialPickupFeedbackState`. First observe establishes both material and potion baselines. Only a newer valid potion receipt emits:

```cpp
std::snprintf(feedback.text.bytes.data(), feedback.text.bytes.size(),
    "生命药 +%d HP", receipt.restored_hp);
feedback.ready = receipt.restored_hp > 0;
feedback.emphasized = feedback.ready;
```

Same generation never repeats; failed transaction has no valid receipt and emits no success text.

## Verification

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
python .\tools\build_item_material_atlas.py
python -m unittest tests.platform.item_material_asset_pipeline_tests -v
ctest --preset windows-msvc-debug -R '^(platform\.units|stage12\.item_material_asset_pipeline)$' --output-on-failure
```

Expected: platform `446 cases, 0 failures`; Python asset tests all pass. Also inspect atlas cell 30 at original resolution/crop.

## Hard boundaries

- No gameplay, persistence, drop, heal, threshold, room, monster, or font-system changes.
- Text stays runtime-rendered via the existing high-resolution CJK path; never bake words into PNG.
- Pure color means no gradient/alpha variation in the label color value.
- No dynamic allocation in label building/collision resolution.
- No CLI image fallback without explicit confirmation.
- Do not touch `E:\game` or tracked historical report files.

## Completion

Commit only the 12 listed files with:
`feat: add high-resolution health potion presentation`

Write the unique report with image-generation mode/final prompt/final path, visual validation, RED/GREEN, hash/size evidence, files, and concerns. Return `DONE`, `DONE_WITH_CONCERNS`, `NEEDS_CONTEXT`, or `BLOCKED`.
