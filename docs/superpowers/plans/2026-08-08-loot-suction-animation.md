# Loot Suction Animation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 成功拾取装备或材料后，让原地面图标沿弧线吸入角色腰后的背包位置。

**Architecture:** 新建纯平台表现状态 `LootSuctionState`，只观察前后 `DungeonSnapshot` 与已经提交的拾取回执，不改地下城掉率、拾取事务或存档。状态使用固定 12 槽，输出无窗口可测的绘制计划；`CombatRenderer` 只负责消费计划并用现有 MaterialPack 图标绘制。

**Tech Stack:** C++17、raylib 6.0、现有固定容量测试框架与 CMake/MSVC presets。

## Global Constraints

- 使用 C++17 与 raylib 6.0。
- 不改变掉率、1.5 米自动拾取、生命药直接治疗、存档格式或拾取事务。
- 动画状态不得使用动态容量容器；固定容量 12，容量满时确定性替换最接近结束的一项。
- 暂停时冻结；保存错误、恢复、故障或跨房间时不得产生“已入包”假动画。
- 只运行定向 Debug 测试；最后才执行一次 Windows Release 构建和确定性实机验收。

---

### Task 1: 固定容量动画状态与轨迹

**Files:**
- Create: `src/platform/raylib/loot_suction_animation.hpp`
- Create: `src/platform/raylib/loot_suction_animation.cpp`
- Create: `tests/platform/loot_suction_animation_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes: `DungeonSnapshot`、`DungeonRenderStatus`、`CombatCameraView`、`ground_loot_item_sprite()`、`material_loot_sprite()`、`material_color()`。
- Produces: `LootSuctionState::observe(previous, current, status)`、`update(frame_seconds, paused)`、`build_plan(camera, width, height)`、`clear()` 与 `LootSuctionPlan`。

- [ ] **Step 1: 写行为测试**

```cpp
LootSuctionState state{};
state.observe(empty, empty, ok_status); // attach baseline
state.observe(previous_with_item_42, current_without_item,
    committed_receipt_for_item_42);
ARPG_REQUIRE(state.active_count() == 1U);
const LootSuctionPlan start = state.build_plan(
    camera, {0.0F, 0.0F, 0.0F}, 1280.0F, 720.0F);
ARPG_REQUIRE(start.count == 1U);
ARPG_REQUIRE(start.flights[0].center.x > start.destination.x);

state.update(0.35F, false);
const LootSuctionPlan middle = state.build_plan(
    camera, {0.0F, 0.0F, 0.0F}, 1280.0F, 720.0F);
ARPG_REQUIRE(middle.flights[0].center.x < start.flights[0].center.x);
ARPG_REQUIRE(middle.flights[0].center.x > middle.destination.x);
ARPG_REQUIRE(middle.flights[0].center.y <
    start.flights[0].center.y +
        (middle.destination.y - start.flights[0].center.y) * 0.75F);
```

同文件再锁定：保存错误不触发、地面仍存在不触发、同一回执不重复；材料回执只吸入真正消失的 ordinal；暂停冻结；0.45 秒后回收并产生到达脉冲；13 次触发后 `active_count() == 12` 且无堆分配。

- [ ] **Step 2: 运行定向测试确认 RED**

Run:
```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 4
$env:ARPG_CPLAY021_LOOT_SUCTION_ONLY='1'
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Expected: 编译因 `loot_suction_animation.hpp`/`LootSuctionState` 尚不存在而失败，或新增行为用例在旧实现上失败；不能接受无关编译失败作为 RED。

- [ ] **Step 3: 写最小纯状态实现**

```cpp
class LootSuctionState final {
public:
    void observe(const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& status) noexcept;
    void update(float frame_seconds, bool paused) noexcept;
    [[nodiscard]] LootSuctionPlan build_plan(CombatCameraView camera,
        combat::Vec3 player_position, float width, float height) noexcept;
    void clear() noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;
private:
    std::array<LootSuctionFlight, 12U> flights_{};
    std::uint64_t equipment_generation_{};
    std::uint64_t equipment_item_id_{};
    std::uint64_t material_generation_{};
    float destination_pulse_seconds_{};
    bool attached_{};
};
```

装备必须同时满足：新 committed receipt、`previous` 找到同 item_id、`current` 已不存在。材料必须同时满足：新 material receipt、同房间、`previous` ordinal 在 `current` 消失。轨迹使用 `smoothstep` 加速与 `sin(pi*t)` 上拱，目标由当前插值角色位置投影并上移到腰后。

- [ ] **Step 4: 运行同一定向测试确认 GREEN**

Expected: `loot_suction_animation` 全部用例通过，进程退出码 0。

- [ ] **Step 5: 提交 Task 1**

```powershell
git add -- src/platform/raylib/loot_suction_animation.hpp src/platform/raylib/loot_suction_animation.cpp src/platform/raylib/CMakeLists.txt tests/platform/loot_suction_animation_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git commit -m "feat: add bounded loot suction animation state"
```

### Task 2: CombatRenderer 绘制接入

**Files:**
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `tests/platform/loot_suction_animation_tests.cpp`

**Interfaces:**
- Consumes: Task 1 的 `LootSuctionState` 和 `LootSuctionPlan`。
- Produces: 成功拾取后由正式 `observe_hud -> draw` 路径显示的装备/材料飞行图标与角色腰侧收纳光环。

- [ ] **Step 1: 添加渲染接入测试并确认 RED**

```cpp
CombatRenderer renderer{};
renderer.observe_hud(previous_with_item, current_without_item,
    committed_status, hints, 0.0F, false);
ARPG_REQUIRE(renderer.loot_suction_active_count() == 1U);
renderer.clear_combat_transients();
ARPG_REQUIRE(renderer.loot_suction_active_count() == 0U);
```

Run Task 1 的定向命令。Expected: 因 `CombatRenderer` 尚未接入或无查询接口而失败。

- [ ] **Step 2: 接入观察、更新、清理和绘制**

在 `observe_hud()` 先按 `frame_seconds/paused` 前进动画，再调用 `loot_suction_.observe(previous, current, runtime_status)`；在 `clear_combat_transients()` 清空。`draw()` 的 actors stage 之后、结束 world mode 之前绘制计划：装备同时画品质光晕与槽位图标，材料画材料图标；目标取插值后的角色位置并上移到腰后，到达脉冲放大/提亮。

```cpp
const LootSuctionPlan suction = loot_suction_.build_plan(
    camera, interpolated_player, width, height);
for (std::size_t i = 0; i < suction.count; ++i) {
    const auto& flight = suction.flights[i];
    static_cast<void>(material_pack_.draw(
        flight.sprite, flight.center, false, flight.scale));
}
```

- [ ] **Step 3: 运行定向 GREEN 和静态自检**

Run Task 1 定向命令，再运行：
```powershell
git diff --check -- src/platform/raylib/combat_renderer.hpp src/platform/raylib/combat_renderer.cpp src/platform/raylib/loot_suction_animation.hpp src/platform/raylib/loot_suction_animation.cpp tests/platform/loot_suction_animation_tests.cpp
```

Expected: 定向用例全绿，diff check 退出码 0。

- [ ] **Step 4: 提交 Task 2**

```powershell
git add -- src/platform/raylib/combat_renderer.hpp src/platform/raylib/combat_renderer.cpp tests/platform/loot_suction_animation_tests.cpp
git commit -m "feat: render loot flying into backpack"
```

### Task 3: Windows Release 确定性验收

**Files:**
- Modify: `.superpowers/sdd/issues.md`

**Interfaces:**
- Consumes: Task 2 的正式 Release 游戏与 seed 6118 确定掉落路线。
- Produces: 安装版哈希一致、吸入动画截图序列、背包数量变化和问题闭环记录。

- [ ] **Step 1: 单次构建与部署**

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='4'
.\scripts\Build.ps1 -Preset windows-msvc-release
.\scripts\DeployLauncher.ps1
```

读取 Build/安装版 `arpg_game.exe` SHA-256，必须一致。

- [ ] **Step 2: seed 6118 内部截图验收**

使用全新隔离 save 目录与静音 settings 启动。输入 `R`、等待 100ms、`Num1`、等待约 350ms，确认 ordinal 158 掉落 `普通 Shadow Hood · i1`；接近到 1.5 米内后以 80–120ms 间隔连续 F12，至少捕获“地面起点 / 飞行中段 / 角色腰侧到达脉冲”三态。

- [ ] **Step 3: 状态与资源读回**

正常关闭后确认：背包数量增加 1、ground item 归零、存档有效；`arpg_game`、`cl`、`cmake`、`ctest`、`ninja` 无残留。若动画无法从静态截图可靠判定，停止并保留现场，不以单元测试代替 Windows 视觉验收。

- [ ] **Step 4: 更新问题记录并提交**

在 `.superpowers/sdd/issues.md` 记录根因、实现、定向测试、Release 哈希、截图路径和资源读回，只提交本任务文件，不纳入共享工作树其他改动。
