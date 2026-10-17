### Task 5: Fixed-Capacity Ground Loot View

**Files:**
- Create: `src/platform/raylib/ground_loot_view.hpp`
- Create: `src/platform/raylib/ground_loot_view.cpp`
- Create: `tests/platform/ground_loot_view_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces: `LootLabelRect`, `GroundLootLabel`, and `GroundLootView` with capacity `dungeon::kGroundDropCapacity`.
- Produces: `build_ground_loot_view(const dungeon::DungeonSnapshot&, settings::LootFilterMode, float width, float height) noexcept`.
- Produces: `bool ground_loot_visible(const GroundItemSnapshot&, LootFilterMode) noexcept`.

- [ ] **Step 1: Write failing visibility, text, and layout tests**

Cover all three modes, abyss bypass, invalid base fallback, rarity palette, NUL termination, deterministic ordinal order, overlapping anchors, and 1024x576/1280x720/1920x1080 safety bounds. Assert the snapshot is not mutated.

```cpp
const GroundLootView view = build_ground_loot_view(
    snapshot, settings::LootFilterMode::magic_or_better, 1280.0F, 720.0F);
ARPG_REQUIRE(view.count == 2U);
ARPG_REQUIRE(view.labels[0].ordinal == magic_ordinal);
ARPG_REQUIRE(view.labels[1].ordinal == rare_ordinal);
ARPG_REQUIRE(view.labels[0].text.back() == '\0');
```

- [ ] **Step 2: Register/build and verify RED**

Register a new `ground_loot_view_suite()` in the platform runner. Build; expected missing-header failure.

- [ ] **Step 3: Implement fixed-capacity builder**

Use `project_combat_position`, `std::snprintf`, catalog base names, and stable insertion by ordinal. Resolve overlaps by moving each later label upward in a bounded loop, then clamp every rectangle to the screen safety inset. Return diagnostics counters for invalid base, truncation, overlap adjustment, and capacity saturation.

- [ ] **Step 4: Run platform tests and allocation probe GREEN**

Add a 100,000-iteration allocation probe around unchanged and alternating filter builds. Expected zero allocations and all view tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib/ground_loot_view.* src/platform/raylib/CMakeLists.txt tests/platform
git commit -m "feat: build fixed ground loot labels"
```

