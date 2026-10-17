### Task 6: Render the expanded room and density HUD

**Files:**
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/hud_view_model.hpp`
- Modify: `src/platform/raylib/hud_view_model.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `tests/platform/hud_view_model_tests.cpp`
- Modify: `tests/platform/hud_render_plan_tests.cpp`
- Modify: `tests/platform/stage12_environment_render_tests.cpp`

**Interfaces:**
- Produces: `const char* room_density_label(RoomDensityAffix) noexcept` returning UTF-8 `拥挤/密集/兽潮`.
- Produces in `RoomHudModel`: affix, initial count, remaining count, and preformatted secondary line.
- Consumes: Task 5 `CombatCameraView` for all room geometry.

- [ ] **Step 1: Write RED HUD tests**

```cpp
snapshot.density_affix = dungeon::RoomDensityAffix::horde;
snapshot.base_monster_count = 27U;
snapshot.initial_monster_count = 27U;
snapshot.remaining_targets = 19U;
build_hud_view_model(output, snapshot, {}, default_hints());
ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), u8"兽潮") != nullptr);
ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), u8"19/27") != nullptr);
```

For abyss, set base 27/final 41 and require the density line shows the actual `19/41` while the existing abyss rule remains visible. Assert no truncation at the minimum supported 1024×704 layout.

- [ ] **Step 2: Run platform tests to verify RED**

Run the Task 5 Step 2 commands. Expected: compile or text assertion FAIL.

- [ ] **Step 3: Replace wave wording with density/count wording**

Format combat objective as `"%s · 怪物 %u/%u"`, using remaining then initial. Put abyss rule/effect in its existing section and density/count in the room objective; do not reintroduce wave text. Add affix and initial count to `ObjectiveKey` so cached text refreshes when a room changes.

- [ ] **Step 4: Make room geometry visibly scroll**

Project the four world corners and all door/hole positions through the shared camera. Draw floor grid lines at fixed 2-world-unit intervals across X `[-24,24]` and Y `[-11,11]`; cull lines whose projected bounding box is outside the viewport. Keep the material floor as the base layer, with the world-space grid and boundary treatment above it so movement is visible without stretching the material asset.

- [ ] **Step 5: Verify door, hole, hazard, and loot alignment**

Add pure projection assertions that each object shares the same projected coordinate when routed through room rendering and actor/loot helpers. At each camera edge, the corresponding door lies inside the viewport and the opposite door lies outside it.

- [ ] **Step 6: Run platform tests to verify GREEN**

Run Step 2. Expected: PASS, no HUD truncation, no stale cached density text.

- [ ] **Step 7: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render large rooms and density encounter HUD"
```

---

