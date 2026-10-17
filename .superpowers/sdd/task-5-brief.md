### Task 5: Add a deterministic, zero-lag scrolling camera

**Files:**
- Modify: `src/platform/raylib/combat_view_math.hpp`
- Modify: `src/platform/raylib/combat_view_math.cpp`
- Modify: `src/platform/raylib/render_layout.hpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/active_skill_renderer.hpp`
- Modify: `src/platform/raylib/active_skill_renderer.cpp`
- Modify: `src/platform/raylib/debug_renderer.cpp`
- Modify: `src/platform/raylib/ground_loot_view.cpp`
- Modify: `src/platform/raylib/material_loot_view.cpp`
- Modify: `tests/platform/combat_view_math_tests.cpp`

**Interfaces:**
- Produces:

```cpp
struct CombatCameraView final {
    combat::Vec3 center{};
    float visible_width{24.0F};
    float visible_depth{11.0F};
};

CombatCameraView make_combat_camera_view(
    combat::Vec3 interpolated_player, float width, float height) noexcept;
ScreenProjection project_combat_position(
    combat::Vec3 position, CombatCameraView view,
    float width, float height) noexcept;
```

- Consumes: the same interpolated player position already used for actor rendering.

- [ ] **Step 1: Write RED camera math tests**

```cpp
const CombatCameraView center = make_combat_camera_view({0,0,0}, 1280, 720);
ARPG_REQUIRE(near(center.visible_width, 24.0F));
ARPG_REQUIRE(near(center.visible_depth, 11.0F));
ARPG_REQUIRE(near(center.center.x, 0.0F));

const CombatCameraView right = make_combat_camera_view({24,0,0}, 1280, 720);
ARPG_REQUIRE(near(right.center.x, 12.0F));
const CombatCameraView front = make_combat_camera_view({0,11,0}, 1280, 720);
ARPG_REQUIRE(near(front.center.y, 5.5F));
```

At 2560×1080 require `visible_depth==11`, `visible_width==24*(64/27)/(16/9)==32`, and unchanged actor pixel scale. Require the player projection to react to the current interpolated position in the same render call; there is no retained camera interpolation state.

- [ ] **Step 2: Run platform tests to verify RED**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R platform.units --output-on-failure
```

Expected: compile FAIL on missing camera APIs.

- [ ] **Step 3: Implement pure camera derivation**

Use 11 world units of visible depth. Compute visible width as `24 * max(1, aspect/(16/9))`, capped at room width 48. Clamp the center so the visible rectangle remains inside room bounds; when a dimension shows the whole room, force that center coordinate to zero. Do not store velocity, smoothing factor, or previous camera center.

- [ ] **Step 4: Make projection camera-relative**

Compute depth from `position.y - (view.center.y - view.visible_depth/2)`, clamp only the visual scale input to `[0,1]`, and preserve the existing `0.70..1.00` perspective scale and `0.38..0.88` ground band. Compute horizontal pixels from `(position.x-view.center.x)/view.visible_width`. Update projectile, hazard, render-layout, active-skill, loot, debug, and actor calls to accept the same view.

- [ ] **Step 5: Derive one camera per frame and share it across every world pass**

In `CombatRenderer::draw`, interpolate the player from `previous.combat->player.position` to `current.combat->player.position` using the existing render alpha, build one `CombatCameraView`, and pass it to room, actors, skills, loot-world anchors, and debug rendering. Camera shake remains a final screen-space offset and must not alter the logical camera center.

- [ ] **Step 6: Run platform tests to verify GREEN**

Run Step 2. Expected: PASS at 16:9, ultrawide, room center, and all four clamped edges.

- [ ] **Step 7: Commit**

```powershell
git add src/platform/raylib tests/platform/combat_view_math_tests.cpp
git commit -m "feat: add zero-lag scrolling combat camera"
```

---

