### Task 5: 技能颜色/材质图配对与按装备常驻

**Files:**
- Create: `tools/build_active_skill_material_maps.py`
- Create: `tests/platform/active_skill_material_asset_pipeline_tests.py`
- Create: `assets/skills/draw_slash_atlas_material.png`
- Create: `assets/skills/storm_swords_atlas_material.png`
- Modify: `src/platform/raylib/material_asset_types.hpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/material_residency.cpp`
- Modify: `src/platform/raylib/active_skill_assets.hpp`
- Modify: `src/platform/raylib/active_skill_assets.cpp`
- Modify: `src/platform/raylib/active_skill_renderer.hpp`
- Modify: `src/platform/raylib/active_skill_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `tests/platform/active_skill_asset_tests.cpp`
- Modify: `tests/platform/material_asset_validation_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes `MaterialPack::draw_frame` and the request from Task 1.
- Produces `MaterialAtlasId::skill_draw_slash`, `MaterialAtlasId::skill_storm_swords`, `active_skill_material_atlas(...)`, and metadata-only `active_skill_atlas_frame(...)`.

- [ ] **Step 1: Write RED asset and metadata tests.**

The Python test requires exact dimensions and alpha parity:

```python
CASES = {
    "draw_slash": ((1254, 1254), 36),
    "storm_swords": ((1024, 1536), 24),
}
self.assertEqual(color.size, expected_size)
self.assertEqual(material.size, expected_size)
self.assertEqual(color.getchannel("A").tobytes(),
                 material.getchannel("A").tobytes())
```

Run the generator twice into temporary directories and require equal SHA-256 hashes. In C++, require the two atlas definitions, all 36/24 source cells, and that a loadout containing only draw slash requests only `skill_draw_slash`; both equipped skills request both atlases and remain under 256 MB.

- [ ] **Step 2: Run RED.**

```powershell
python -m unittest tests.platform.active_skill_material_asset_pipeline_tests -v
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
```

Expected: the material PNGs and material atlas IDs do not exist.

- [ ] **Step 3: Implement deterministic material-map generation.**

For each nontransparent pixel, compute luminance `Y=(54R+183G+19B)/256`, energy `max(B-R, G-R, 0)`, ancient-gold mask where `R>G> B` and `R-B>=24`, and dark-metal mask where `Y<118` and channel spread `<42`. Emit:

```text
R roughness = clamp(220 - Y/2 - 70*dark_metal, 40, 235)
G emissive  = clamp(energy*3 + (Y>210 ? 48 : 0), 0, 255)
B metalness = clamp(32 + 180*dark_metal + 145*ancient_gold, 0, 255)
A           = source alpha
```

The script writes temporary PNGs, reopens them, verifies dimensions/alpha, then replaces the output. Runtime code never invokes Pillow or parses pixels.

- [ ] **Step 4: Move skill atlases into the material manifest.**

Append both atlas IDs before `count`, with dimensions `1254×1254` and `1024×1536`; register `rgba_bytes = width * height * 4U`. Keep the grid definitions:

```cpp
draw_slash: columns=6, rows=6, frame_count=36, cell=209x209
storm_swords: columns=4, rows=6, frame_count=24, cell=256x256
```

Replace `ActiveSkillAssets` with metadata-only lookup and expose:

```cpp
struct ActiveSkillAtlasFrame final {
    MaterialAtlasId atlas{MaterialAtlasId::count};
    Rectangle source{};
    Vector2 foot_anchor{};
    Vector2 weapon_anchor{};
};

[[nodiscard]] MaterialAtlasId active_skill_material_atlas(
    skills::ActiveSkillId id) noexcept;
[[nodiscard]] std::optional<ActiveSkillAtlasFrame>
active_skill_atlas_frame(skills::ActiveSkillId id,
    std::size_t frame_index) noexcept;
```

Add an equipped non-`none` skill’s atlas to `make_material_residency_request`. Do not mark skill atlases common.

Atomically adapt `ActiveSkillRenderer::draw_world` to accept `const MaterialPack&` and draw its current selected frame through `MaterialPack::draw_frame`; remove the class-owned `Texture2D` array and every direct `LoadTexture`, `UnloadTexture` and `DrawTexturePro` call. Remove the corresponding initialize/shutdown calls and startup warning from `CombatRenderer`. Keep `active_skill_assets_ready()` as a compatibility query for manifest/frame-metadata validity; actual per-skill GPU residency is queried with `material_atlas_available(active_skill_material_atlas(id))` after the first synchronized draw and is captured that way in Task 10. Task 6 changes frame selection and player suppression, not texture ownership.

Register exactly three new C++ cases from Step 1 and change the platform expected case count from 455 to 458.

Register the Python test as CTest `stage17.active_skill_material_asset_pipeline` with labels `headless;platform;assets;stage17` and timeout 120 seconds.

- [ ] **Step 5: Run GREEN tests.**

```powershell
python tools/build_active_skill_material_maps.py --root .
python -m unittest tests.platform.active_skill_material_asset_pipeline_tests -v
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: deterministic maps, alpha parity, frame bounds, loadout-driven residency and final legal peak budget pass.

- [ ] **Step 6: Commit.**

```powershell
git add tools/build_active_skill_material_maps.py tests/platform/active_skill_material_asset_pipeline_tests.py tests/platform/active_skill_asset_tests.cpp tests/platform/material_asset_validation_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp assets/skills/draw_slash_atlas_material.png assets/skills/storm_swords_atlas_material.png src/platform/raylib/material_asset_types.hpp src/platform/raylib/material_manifest.hpp src/platform/raylib/material_residency.cpp src/platform/raylib/active_skill_assets.* src/platform/raylib/active_skill_renderer.* src/platform/raylib/combat_renderer.*
git commit -m "feat: add material-backed active skill atlases"
```

