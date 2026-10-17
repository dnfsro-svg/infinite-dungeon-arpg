### Task 9: Render immutable backgrounds, doors, props, and obstacles as world chunks

**Files:**
- Modify: `src/dungeon/room_environment.hpp`
- Modify: `src/dungeon/room_environment.cpp`
- Modify: `src/platform/raylib/environment_prop_layout.hpp`
- Modify: `src/platform/raylib/environment_prop_layout.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `tests/dungeon/room_environment_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/platform/stage12_environment_render_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes the Task 2 deterministic `RoomEnvironmentBlueprint`; this task may not regenerate it, consume RNG, change either blueprint hash, or move colliding obstacles.
- Produces `VisibleEnvironmentSet` bounded to 128 props/obstacles. The maximum 7×5 camera span plus halo visits at most `35*3=105` records before exact rectangle filtering, with a compile-time proof `105<=128`.
- Produces camera-aware world positions for four doors and an optional hole.
- Reuses current environment/material atlases; no new texture residency class.

- [ ] **Step 1: Write RED visible-set and render-integration tests**

Reuse the Task 2 cross-ecology/4096-seed field/hash/collision/path tests as regression gates. Add queries for every camera cell: each visits at most 105 records, returns exact-rectangle matches in stable `(cell,kind,ordinal)` order, and treats any capacity-proof violation as a hard dungeon fault rather than truncating or scanning the whole blueprint. Rendering the same immutable blueprint through camera moves must not change its versions, hashes, record bytes, or RNG counters.

- [ ] **Step 2: Replace screen-normalized placements**

Replace screen-normalized runtime placements with the already-sealed Task 2 world positions and matching world AABBs. Decorative props remain non-colliding. Render the Task 3 stable obstacle ordinals and mutable intact/broken state without changing combat authority; no render or camera path may rebuild placement or migrate obstacle identity.

- [ ] **Step 3: Tile the existing background without large render targets**

Draw only atlas-backed ground/wall tiles intersecting the camera plus one tile margin. Use a maximum 5×5 tile draw plan and the existing material shader. Do not allocate or cache 100 room textures; no additional full-resolution framebuffer is added.

- [ ] **Step 4: Render current doors/hole through camera geometry**

Preserve the improved non-red element-door material and current hole interaction indicator. Door open/closed visuals consume `exits_unlocked`; full-clear-only decoration consumes `full_clear`.

- [ ] **Step 5: Run and commit**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon|platform)\.units$' --output-on-failure
git add src/dungeon/room_environment.* src/platform/raylib tests/dungeon/room_environment_tests.cpp tests/dungeon/dungeon_test_main.cpp tests/platform
git commit -m "feat: render deterministic world-space room chunks"
```

---

