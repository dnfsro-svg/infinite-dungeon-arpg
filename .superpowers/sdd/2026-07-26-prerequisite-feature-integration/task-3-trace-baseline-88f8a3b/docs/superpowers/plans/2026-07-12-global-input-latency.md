# Global Input Latency Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除所有操作共同的额外显示延迟，并让怪物 Z 轴击飞真实显示在画面上。

**Architecture:** 保持 60 Hz 确定性逻辑不变，只调整 raylib host 的帧节流和 renderer 的快照选择。新增纯渲染数学函数测试玩家取最新位置、怪物使用投影 Y；输入缓存继续由 DungeonSession 持久保存边沿事件。

**Tech Stack:** C++17、raylib 6.0.0、CMake、CTest。

## Global Constraints

- 不修改 L 的 7/4/17 攻击帧数据、伤害、破韧或范围。
- 玩家绘制使用最新逻辑快照；怪物和环境可继续插值。
- 只保留 VSync，不再调用 `SetTargetFPS(60)`。
- 固定逻辑步保持 60 Hz，不新增热路径动态分配。

---

### Task 1: 修复最新玩家状态与怪物 Z 投影

**Files:**
- Modify: `src/platform/raylib/combat_view_math.hpp`
- Modify: `src/platform/raylib/combat_view_math.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `tests/platform/combat_view_math_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces: `local_player_render_position(current)` 返回当前玩家位置。
- Produces: `actor_body_y(ScreenProjection)` 返回 `projection.y`，怪物阴影仍使用 `ground_y`。

- [ ] **Step 1: Add failing tests**

```cpp
ARPG_REQUIRE(vec_equal(local_player_render_position(current), current.position));
ARPG_REQUIRE(actor_body_y(projected) == projected.y);
ARPG_REQUIRE(actor_body_y(projected) != projected.ground_y);
```

- [ ] **Step 2: Verify RED**

Build and run `arpg_platform_tests`; expected failure because the helpers do not exist and current monster body code uses `ground_y`.

- [ ] **Step 3: Implement minimal rendering changes**

Use the current player snapshot directly when populating its render item. In `draw_monster_silhouette`, change body/label origin from `projected.ground_y` to `projected.y`; keep the separate shadow pass at ground level.

- [ ] **Step 4: Verify GREEN**

Run `arpg_platform_tests`; expected all cases pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "fix: render immediate player and airborne monsters"
```

### Task 2: 移除重复帧节流并验证输入缓存

**Files:**
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/core_boundary_test.cmake`
- Test: `tests/combat/input_buffer_tests.cpp`

**Interfaces:**
- Consumes: raylib `FLAG_VSYNC_HINT`。
- Produces: host source中不存在 `SetTargetFPS`；已有 `InputBuffer` 测试继续证明边沿输入跨 tick 保存。

- [ ] **Step 1: Add failing source-boundary assertion**

Extend the platform boundary test to reject `SetTargetFPS(` in `raylib_host.cpp` while requiring `FLAG_VSYNC_HINT`.

- [ ] **Step 2: Verify RED**

Run `ctest -R architecture.core_no_raylib --output-on-failure`; expected failure identifying `SetTargetFPS(60)`.

- [ ] **Step 3: Remove software frame cap**

Delete `SetTargetFPS(60)` and keep `SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE)`.

- [ ] **Step 4: Verify GREEN and regression suite**

Build Debug and run all CTest tests. Expected 13/13 pass.

- [ ] **Step 5: Launch latest game**

Verify WASD/K/J/L respond without the prior common extra frame, and L visibly raises a non-armored monster above its ground shadow.

- [ ] **Step 6: Commit**

```powershell
git add src/platform/raylib/raylib_host.cpp tests/platform/core_boundary_test.cmake
git commit -m "fix: remove duplicate raylib frame throttle"
```
