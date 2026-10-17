### Task 1: Expand shared room geometry and entry spawns

**Files:**
- Modify: `src/combat/room_bounds.hpp`
- Modify: `src/dungeon/room_combat_template.cpp`
- Modify: `tests/combat/movement_jump_tests.cpp`
- Modify: `tests/combat/monster_special_tests.cpp`
- Modify: `tests/combat/monster_affix_trigger_tests.cpp`
- Modify: `tests/dungeon/room_generation_tests.cpp`
- Modify: `tests/dungeon/dungeon_navigation_tests.cpp`
- Modify: `tests/dungeon/stage10_validation_fixture.cpp`
- Modify: `tests/dungeon/dungeon_affix_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_reward_tests.cpp`

**Interfaces:**
- Produces: `combat::room_bounds::{min_x,max_x,min_y,max_y,width,depth}` equal to `-24,24,-11,11,48,22`.
- Produces: entry spawns at left/right X `-22.5/22.5` and top/bottom Y `-10.25/10.25`.
- Consumes: existing `make_combat_lab_config(EntrySide, rules_version)` interface unchanged.

- [ ] **Step 1: Write failing geometry and navigation assertions**

```cpp
static_assert(arpg::combat::room_bounds::min_x == -24.0F);
static_assert(arpg::combat::room_bounds::max_x == 24.0F);
static_assert(arpg::combat::room_bounds::min_y == -11.0F);
static_assert(arpg::combat::room_bounds::max_y == 11.0F);
static_assert(arpg::combat::room_bounds::width == 48.0F);
static_assert(arpg::combat::room_bounds::depth == 22.0F);

ARPG_REQUIRE(requested_exit(Vec3{-24.0F, 0.90F, 0.0F}, {-1, 0})
    == ExitDirection::left);
ARPG_REQUIRE(requested_exit(Vec3{24.0F, -0.90F, 0.0F}, {1, 0})
    == ExitDirection::right);
ARPG_REQUIRE(requested_exit(Vec3{1.50F, -11.0F, 0.0F}, {0, -1})
    == ExitDirection::up);
ARPG_REQUIRE(requested_exit(Vec3{-1.50F, 11.0F, 0.0F}, {0, 1})
    == ExitDirection::down);
```

- [ ] **Step 2: Build and run focused tests to verify RED**

Run:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_combat_tests arpg_dungeon_tests
ctest --test-dir out/build/windows-msvc-debug -R "combat.units|dungeon.units" --output-on-failure
```

Expected: FAIL on old `±12/±5.5` bounds or old entry spawn values.

- [ ] **Step 3: Change shared bounds and entry locations**

```cpp
namespace arpg::combat::room_bounds {
inline constexpr float min_x = -24.0F;
inline constexpr float max_x = 24.0F;
inline constexpr float min_y = -11.0F;
inline constexpr float max_y = 11.0F;
inline constexpr float width = max_x - min_x;
inline constexpr float depth = max_y - min_y;
}
```

Update `make_combat_lab_config` entry values to `±22.5F` and `±10.25F`; keep the initial spawn at `{0,0,0}` and preserve facing behavior.

- [ ] **Step 4: Replace test-only old boundary literals with shared constants where the literal is not the subject of the test**

Use `combat::room_bounds::min_x/max_x/min_y/max_y` in stress, abyss-reward, and validation fixtures. Keep exact numeric assertions only in the new geometry test.

- [ ] **Step 5: Run focused tests to verify GREEN**

Run the Step 2 commands. Expected: both suites PASS.

- [ ] **Step 6: Commit**

```powershell
git add src/combat/room_bounds.hpp src/dungeon/room_combat_template.cpp tests/combat tests/dungeon
git commit -m "feat: expand combat room to four times area"
```

---

