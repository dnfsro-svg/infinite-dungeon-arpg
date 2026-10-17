### Task 4: 异生态怪物与火怪显式姿势

**Files:**
- Modify: `src/platform/raylib/material_animation.hpp`
- Modify: `src/platform/raylib/material_animation.cpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/monster_material_presenter.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `tests/platform/material_animation_tests.cpp`
- Modify: `tests/platform/stage12_actor_render_tests.cpp`
- Modify: `tests/platform/fire_room_material_slice_tests.cpp`
- Modify: `tests/platform/ecology_material_coverage_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes dynamic monster-atlas residency from Task 1.
- Produces phase-aware `MonsterAnimationState` and optional explicit frame sequences for nonuniform fire atlases.

- [ ] **Step 1: Write RED coverage for every monster and fire pose.**

Expand `MonsterAnimationState` test expectations to `idle`, `move`, `telegraph`, `active`, `recovery`, `cooldown`, `hurt`, `death`. For all eight monster IDs, require a non-null clip and in-bounds frame for each drawable phase. Assert fire-bomber frame cells `{0,1,2,3,4,5,6,7,8,9,10,11}` are reachable and fire-charger cells `{0,1,2,3,4,5,6,8,9,10,11}` are reachable; charger cell 7 is transparent and must never be returned.

Assert an active chaos chaser in a fire snapshot produces `use_material_frame == true` when its atlas is available and does not reach `draw_monster_silhouette` on the healthy path.

- [ ] **Step 2: Run RED.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: fire clips are absent because `has_complete_material_animation` excludes both fire monsters.

- [ ] **Step 3: Add an explicit-frame clip representation.**

Extend the clip definition without changing the existing 96×96 ecology sheets:

```cpp
struct ExplicitMonsterAnimationFrame final {
    Rectangle source{};
    Vector2 foot_anchor{};
    std::uint8_t duration_ticks{1U};
    std::uint8_t key_pose_index{};
};

struct MonsterAnimationClipDefinition final {
    combat::MonsterId monster{};
    MonsterAnimationState state{};
    MaterialAtlasId atlas{};
    std::uint16_t first_cell{};
    std::uint16_t frame_count{};
    std::uint8_t frames_per_second{18U};
    std::uint8_t key_pose_count{4U};
    const ExplicitMonsterAnimationFrame* explicit_frames{};
};
```

When `explicit_frames != nullptr`, `monster_animation_frame()` uses the exact rectangle, anchor and nonzero duration; `monster_animation_frame_index()` walks the fixed frame array’s cumulative tick durations and either loops or clamps. Otherwise both functions retain the 96-pixel/9-column path.

- [ ] **Step 4: Register exact fire sequences and phase mapping.**

Use 256×256 source cells and per-frame foot anchors derived from the committed alpha bounds. The phase sequences are fixed as follows:

```text
fire_bomber (cell@duration_ticks):
idle [0@6]; move [1@4]; telegraph [2@6,7@6];
active [8@3,3@3,9@4]; recovery [4@4,10@6];
cooldown [5@6]; hurt [4@5]; death [6@6,11@12]

fire_charger (cell@duration_ticks):
idle [0@6]; move [1@4]; telegraph [2@5,8@5];
active [9@3,3@3,10@4]; recovery [4@6];
cooldown [5@6]; hurt [4@5]; death [6@6,11@12]
```

For bomber cells 0..11 use foot anchors `{125,234}`, `{111,235}`, `{141,235}`, `{112,228}`, `{121,221}`, `{110,222}`, `{130,225}`, `{113,229}`, `{121,222}`, `{109,224}`, `{128,220}`, `{116,220}`. For charger cells 0..6 and 8..11 use `{125,250}`, `{119,249}`, `{132,253}`, `{123,250}`, `{132,249}`, `{155,252}`, `{114,249}`, `{144,227}`, `{128,231}`, `{128,229}`, `{109,229}`.

Map AI telegraph/active/recovery/cooldown to their namesake animation states so each transition resets its clip clock. Water/lightning/chaos may point all four phase states to their authored 20-frame special range. Remove the fire exclusion from `MonsterMaterialPresenter`; keep its fixed slot array and generation reset behavior.

Remove the five generic `kDefaultAnimationClips` monster rows that claim unavailable 12/16/20/8/16 fire frames. `material_animation_clip(AnimationClipId::monster_*)` must return `nullptr`; all monster presentation and validation use the phase-aware monster clip table above.

Register exactly three new C++ cases from Step 1 and change the platform expected case count from 452 to 455.

- [ ] **Step 5: Run GREEN and verify no false completeness claim.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: every existing fire pose is reachable, all six off-ecology full sequences still pass, and tests assert actual fire counts 12/11 rather than the unavailable 12/16/20/8/16 action totals.

- [ ] **Step 6: Commit.**

```powershell
git add src/platform/raylib/material_animation.* src/platform/raylib/material_manifest.hpp src/platform/raylib/monster_material_presenter.cpp src/platform/raylib/actor_renderer.cpp tests/platform/material_animation_tests.cpp tests/platform/stage12_actor_render_tests.cpp tests/platform/fire_room_material_slice_tests.cpp tests/platform/ecology_material_coverage_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "fix: present all available monster material poses"
```

