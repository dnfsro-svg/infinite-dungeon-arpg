### Task 3: 四方向门运行时绘制

**Files:**
- Modify: `src/platform/raylib/dungeon_view_math.hpp`
- Modify: `src/platform/raylib/dungeon_view_math.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `tests/platform/dungeon_view_math_tests.cpp`
- Modify: `tests/platform/stage12_environment_render_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes the four door sprite IDs now backed by `MaterialAtlasId::element_doors`.
- Produces `DoorRenderDecision` with sprite/tint/lock state and `door_arrow_geometry(...)` without any text glyph arrow.

- [ ] **Step 1: Write RED tests for direction, state and arrow geometry.**

Use this contract:

```cpp
struct DoorRenderDecision final {
    MaterialSpriteId sprite{MaterialSpriteId::missing};
    const char* label{};
    Rgba8 body_tint{};
    Rgba8 text{};
    bool draw_lock_marker{};
};

struct DoorArrowGeometry final {
    Vector2 tail{};
    Vector2 tip{};
    Vector2 head_left{};
    Vector2 head_right{};
    float thickness{};
};

[[nodiscard]] DoorArrowGeometry door_arrow_geometry(
    dungeon::ExitDirection direction, float center_x, float center_y,
    float scale) noexcept;
```

Assert up/down/left/right return four distinct door sprites matching `door_theme(direction).element`; closed tint alpha remains 255 with RGB dimmed rather than replaced; only closed mode draws a lock marker. For each direction, assert the arrow tip points outward and every point stays inside a 48×48 scaled box.

- [ ] **Step 2: Run RED.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
```

Expected: compile failure because `DoorRenderDecision::sprite` and `door_arrow_geometry` are absent.

- [ ] **Step 3: Implement the pure door plan.**

`door_render_decision(mode, direction)` must select `select_door_sprite(door_theme(direction).element)`. Use closed tint `{150,150,150,255}`, open tint `{255,255,255,255}`, and text colors already defined by the door theme. Remove `arrow`, `locked_interior`, and `draw_locked_interior` fields. Generate a shaft plus two head segments in `door_arrow_geometry`; no UTF-8 arrow string enters the draw path.

- [ ] **Step 4: Replace the red overlay and glyph path.**

Remove the aggregate `draw_material_environment` argument from `draw_doors`. For each direction, attempt `visual.sprite` through `MaterialPack`; on failure draw only that door’s existing outline fallback. Tint closed doors through `body_tint`, draw a small 12×14 scaled dark-steel lock body plus 8-pixel arc at the upper-right of the door, and draw the three arrow segments with `DrawLineEx`. Open doors add only two low-alpha element-colored edge lines. Keep labels on `hud_font` via `MeasureTextEx` and `draw_crisp_ui_text`.

Remove door readiness from `can_draw_room_environment()` so a missing common door atlas cannot disable a healthy room background and prop set. The tests require all four directional sprites to be drawable when `element_doors` is healthy and require the background decision to remain material-backed when that atlas is unavailable.

Register exactly three new cases from Step 1 and change the platform expected case count from 449 to 452.

- [ ] **Step 5: Run GREEN tests.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: four-way selection, arrow geometry, closed/open plans and renderer source guards pass; no `DrawRectangleRec` locked interior and no `MeasureText(visual.arrow` remain.

- [ ] **Step 6: Commit.**

```powershell
git add src/platform/raylib/dungeon_view_math.* src/platform/raylib/combat_renderer.* src/platform/raylib/room_renderer.cpp tests/platform/dungeon_view_math_tests.cpp tests/platform/stage12_environment_render_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "fix: render directional element doors"
```

