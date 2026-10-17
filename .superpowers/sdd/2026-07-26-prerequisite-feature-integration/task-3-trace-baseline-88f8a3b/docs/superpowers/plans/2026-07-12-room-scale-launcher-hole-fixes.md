# Room Scale, Launcher, and Hole Interaction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 扩大房间到 24 x 11，恢复 L 上挑至少 0.5 秒浮空，并把洞口 E 交互半径扩大到 2.0。

**Architecture:** 新建无 raylib 依赖的共享房间边界常量，供战斗、地下城和渲染共同使用。浮空用现有 `reaction_ticks` 表示最低浮空保护时间，普通目标与 Stage 4 怪物执行相同规则。洞口范围与提示由纯函数决定，raylib host 只读取结果。

**Tech Stack:** C++20、raylib 6.0.0、CMake、CTest、项目内轻量测试框架。

## Global Constraints

- 房间逻辑边界为 X `[-12, 12]`、Y `[-5.5, 5.5]`。
- `L` 上挑至少保持 30 个有效模拟 tick 的浮空状态；Hit Stop 不消耗计时。
- 洞口 XY 圆形交互半径为 2.0。
- 不改变攻击伤害、破韧、房间生成概率、怪物种类、波次规则或存档格式。
- 不引入战斗热路径堆分配，Combat Core 不依赖 raylib。

---

### Task 1: 统一房间边界并扩容

**Files:**
- Create: `src/combat/room_bounds.hpp`
- Modify: `src/combat/player_simulation.cpp`
- Modify: `src/combat/target_simulation.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/hit_resolution.cpp`
- Modify: `src/dungeon/room_navigation.cpp`
- Modify: `src/dungeon/encounter_director.cpp`
- Modify: `tests/combat/movement_jump_tests.cpp`
- Modify: `tests/combat/monster_special_tests.cpp`
- Modify: `tests/dungeon/dungeon_navigation_tests.cpp`
- Modify: `tests/dungeon/encounter_director_tests.cpp`

**Interfaces:**
- Produces: `arpg::combat::room_bounds::{min_x,max_x,min_y,max_y,width,depth}` as `inline constexpr float`.
- Consumes: existing `combat::Vec3`, movement and room navigation APIs.

- [ ] **Step 1: Write failing boundary tests**

Add assertions that movement reaches but never exceeds X 12/Y 5.5, door requests occur at the new edges, and encounter spawns remain inside the new bounds.

```cpp
ARPG_REQUIRE(snapshot.player.position.x <= room_bounds::max_x);
ARPG_REQUIRE(snapshot.player.position.y <= room_bounds::max_y);
ARPG_REQUIRE(requested_exit(Vec3{-12.0F, 0.90F, 0.0F}, {-1, 0})
    == ExitDirection::left);
```

- [ ] **Step 2: Run tests and verify RED**

Run:

```powershell
cmake --build out/build/windows-msvc-debug --config Debug --target combat_tests dungeon_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug -R "combat_tests|dungeon_tests" --output-on-failure
```

Expected: assertions fail because production code still clamps/navigates at ±8 and ±3.5.

- [ ] **Step 3: Add shared constants and replace hard-coded logical bounds**

```cpp
namespace arpg::combat::room_bounds {
inline constexpr float min_x = -12.0F;
inline constexpr float max_x = 12.0F;
inline constexpr float min_y = -5.5F;
inline constexpr float max_y = 5.5F;
inline constexpr float width = max_x - min_x;
inline constexpr float depth = max_y - min_y;
}
```

Update all listed simulation, navigation and spawn code to reference these constants. Preserve current door half-widths.

- [ ] **Step 4: Run focused tests and verify GREEN**

Run the command from Step 2. Expected: `combat_tests` and `dungeon_tests` pass.

- [ ] **Step 5: Commit**

```powershell
git add src/combat src/dungeon tests/combat tests/dungeon
git commit -m "feat: expand shared combat room bounds"
```

### Task 2: 统一 L 上挑最低浮空时间

**Files:**
- Modify: `src/combat/target_simulation.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `tests/combat/dummy_reaction_tests.cpp`
- Modify: `tests/combat/monster_melee_tests.cpp`

**Interfaces:**
- Consumes: `AttackDefinition::impact`, `ImpactKind::launch`, `MonsterRuntime::reaction_ticks`.
- Produces: `launcher` 命中时 `reaction_ticks == 30`；两条模拟路径在计时归零前不允许触地。

- [ ] **Step 1: Write failing launcher tests**

Add one legacy-target test and one Stage 4 monster test. After the launcher hit, simulate 29 effective ticks and require `airborne`; after protection ends, require normal gravity/landing progression.

```cpp
ARPG_REQUIRE(target.reaction == ReactionState::airborne);
ARPG_REQUIRE(target.reaction_ticks == 30);
tick_n(world, 29);
ARPG_REQUIRE(world.snapshot().monsters[index].reaction
    == ReactionState::airborne);
```

- [ ] **Step 2: Run tests and verify RED**

```powershell
cmake --build out/build/windows-msvc-debug --config Debug --target combat_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug -R combat_tests --output-on-failure
```

Expected: `reaction_ticks` is currently 0 for launcher impacts.

- [ ] **Step 3: Implement minimum-airtime behavior in both simulation paths**

On `ImpactKind::launch`, set `reaction_ticks = 30`. During airborne simulation, decrement it only on effective combat ticks. If the ballistic position reaches the floor while ticks remain, clamp `z` above zero and hold vertical velocity at zero; once zero, resume gravity and use the existing landing transition.

```cpp
constexpr std::uint16_t kLauncherMinimumAirTicks = 30;
dummy.reaction = ReactionState::airborne;
dummy.reaction_ticks = kLauncherMinimumAirTicks;
```

- [ ] **Step 4: Run combat tests and verify GREEN**

Run Step 2. Expected: all `combat_tests` pass, including existing armor/break stress cases.

- [ ] **Step 5: Commit**

```powershell
git add src/combat/target_simulation.cpp src/combat/monster_ai.cpp tests/combat
git commit -m "fix: preserve launcher minimum airtime"
```

### Task 3: 扩大洞口 E 范围并增加靠近提示

**Files:**
- Modify: `src/platform/raylib/dungeon_view_math.hpp`
- Modify: `src/platform/raylib/dungeon_view_math.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `tests/platform/dungeon_view_math_tests.cpp`

**Interfaces:**
- Produces: `inline constexpr float kHoleInteractionRadius = 2.0F` and `can_prompt_descent(snapshot, player_position)`.
- Consumes: `hole_visual_mode(snapshot)` and shared room bounds for the hole center/layout.

- [ ] **Step 1: Write failing range and prompt tests**

```cpp
ARPG_REQUIRE(player_in_hole_range(center, center, kHoleInteractionRadius));
ARPG_REQUIRE(player_in_hole_range({center.x + 2.0F, center.y, 0.0F},
    center, kHoleInteractionRadius));
ARPG_REQUIRE(!player_in_hole_range({center.x + 2.01F, center.y, 0.0F},
    center, kHoleInteractionRadius));
ARPG_REQUIRE(can_prompt_descent(ready_snapshot, center));
ARPG_REQUIRE(!can_prompt_descent(sealed_snapshot, center));
```

- [ ] **Step 2: Run platform tests and verify RED**

```powershell
cmake --build out/build/windows-msvc-debug --config Debug --target platform_tests
ctest --test-dir out/build/windows-msvc-debug -C Debug -R platform_tests --output-on-failure
```

Expected: new constants/helpers do not exist and old host radius remains 1.2.

- [ ] **Step 3: Implement range helper, host usage and renderer prompt**

Make the host call `request_descent(can_prompt_descent(...))`. Pass the computed prompt state to the renderer and draw `Press E to descend` beside the ready hole only while in range.

- [ ] **Step 4: Run platform tests and verify GREEN**

Run Step 2. Expected: all `platform_tests` pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "fix: widen and expose hole interaction"
```

### Task 4: 适配扩容后的渲染并完成全量验证

**Files:**
- Modify: `src/platform/raylib/combat_view_math.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `tests/platform/combat_view_math_tests.cpp`

**Interfaces:**
- Consumes: shared room bounds.
- Produces: new room corners, doors, actors and hole all remain inside viewport.

- [ ] **Step 1: Write failing projection tests**

Project all four new room corners at 1280 x 720 and assert they remain inside the drawable viewport with HUD margins.

- [ ] **Step 2: Run platform tests and verify RED**

Run Task 3 Step 2. Expected: old projection scale does not satisfy new-corner assertions.

- [ ] **Step 3: Update projection and room geometry**

Derive depth normalization and X scale from shared room width/depth; move door world positions to ±12/±5.5; keep actor scale clamped to the existing readable range.

- [ ] **Step 4: Run all automated verification**

```powershell
cmake --build out/build/windows-msvc-debug --config Debug
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure
```

Expected: build succeeds and 100% tests pass.

- [ ] **Step 5: Launch the game for visual acceptance**

Start `out/build/windows-msvc-debug/bin/arpg_game.exe`. Verify the expanded room is fully visible, L keeps a live monster airborne for at least 0.5 seconds, and E descent works comfortably within radius 2.0.

- [ ] **Step 6: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render expanded dungeon rooms"
```
