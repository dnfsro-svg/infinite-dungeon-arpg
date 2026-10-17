### Task 7: 实现无窗口可测的星盘视图数学和 raylib 覆盖层

**Files:**
- Create: `src/platform/raylib/passive_tree_view_math.hpp`
- Create: `src/platform/raylib/passive_tree_view_math.cpp`
- Create: `src/platform/raylib/passive_tree_renderer.hpp`
- Create: `src/platform/raylib/passive_tree_renderer.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `tests/platform/passive_tree_view_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes: Task 2 的 node coordinates/types，Task 5 的 `DungeonSnapshot` 与 Task 6 的 save indicator。
- Produces: `passive_tree_can_open()`、`project_passive_node()`、`hit_test_passive_node()`、`draw_passive_tree_overlay()`；raylib host 不把鼠标坐标或 P 键泄露到规则模块。

- [ ] **Step 1: 写 UI 几何与输入门控的 RED 测试**

在 `passive_tree_view_tests.cpp` 写入：

```cpp
arpg::test::Failure overlay_only_opens_for_clean_awaiting_exit() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    snapshot.passive_save_pending = false;
    ARPG_REQUIRE(platform::passive_tree_can_open(snapshot));
    snapshot.passive_save_pending = true;
    ARPG_REQUIRE(!platform::passive_tree_can_open(snapshot));
    snapshot.passive_save_pending = false;
    snapshot.phase = dungeon::RoomPhase::combat;
    ARPG_REQUIRE(!platform::passive_tree_can_open(snapshot));
    return {};
}

arpg::test::Failure node_projection_and_hit_test_are_stable() noexcept {
    const auto projected = platform::project_passive_node(8U, 1280.0F, 720.0F);
    ARPG_REQUIRE(projected.visible);
    ARPG_REQUIRE(platform::hit_test_passive_node(projected.center, 1280.0F, 720.0F)
        == std::optional<passives::PassiveNodeId>{8U});
    ARPG_REQUIRE(!platform::hit_test_passive_node({-1.0F, -1.0F}, 1280.0F, 720.0F));
    return {};
}
```

再加节点 0/21/63 的投影互异、disabled/allocated/available 视觉状态、覆盖层打开时动作/移动/门/下坠输入全部 gate 的测试。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Expected: 编译失败，因为星盘视图数学和 UI 状态函数尚不存在。

- [ ] **Step 3: 实现纯视图层和覆盖渲染**

`passive_tree_view_math` 不包含 raylib，使用固定的设计画布 1200×760 映射到 viewport，定义：

```cpp
struct PassiveScreenPoint final { float x{}; float y{}; };
struct PassiveNodeProjection final {
    PassiveScreenPoint center{};
    float radius{};
    bool visible{};
};
enum class PassiveNodeVisualState : std::uint8_t {
    locked, available, allocated, rejected, pending
};
[[nodiscard]] bool passive_tree_can_open(const dungeon::DungeonSnapshot&) noexcept;
[[nodiscard]] PassiveNodeProjection project_passive_node(
    passives::PassiveNodeId, float width, float height) noexcept;
[[nodiscard]] std::optional<passives::PassiveNodeId> hit_test_passive_node(
    PassiveScreenPoint screen, float width, float height) noexcept;
```

`passive_tree_renderer` 是唯一使用 raylib 的星盘文件：先绘制半透明全屏背景、无向连线、再绘制节点，最后绘制节点名称、收益/代价、`Passive Points N`、`P Close` 与 `Autosave SAVING/ERROR`。关键节点始终显示收益和代价两行；颜色只表达火/水/电/混沌路线，不改变规则数据。

在 host 维护 `bool passive_overlay_open`。按 P 时仅当 `passive_tree_can_open(snapshot)` 为真才切换；打开时不调用 `submit_frame_actions()`、不发送 E、不给 `sample_movement_input()` 的结果，鼠标左键命中已分配节点调用退款，命中未分配节点调用分配。每次固定 tick 前后若 phase 不再是 `awaiting_exit` 或 pending 为真，强制关闭覆盖层。把 HUD 第一行更新为 `P Star Chart after clear`，并显示玩家 barrier（非零时）和未分配点。

- [ ] **Step 4: 验证无窗口表现层与输入延迟边界**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "platform.view_math|platform.input_latency_source|architecture.*raylib" --output-on-failure
git diff --check
```

Expected: platform suite 的新增星盘 view cases 与既有固定输入源测试通过；核心/战斗/星盘目录均无 raylib include。

- [ ] **Step 5: 提交 Task 7**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: add passive star chart overlay"
```

