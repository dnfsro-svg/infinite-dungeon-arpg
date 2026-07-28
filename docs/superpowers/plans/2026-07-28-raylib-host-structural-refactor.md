# Raylib Host Structural Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Use `superpowers:test-driven-development` for every production change and `superpowers:verification-before-completion` before any completion claim. Track every step with the checkboxes below.

**Goal:** 完成全面结构重构的子项目 A：把 Stage 10/11/11B/11C/11D/17 正式验收逻辑、窗口生命周期和设置/逐帧协调从 4,135 行的 `raylib_host.cpp` 中拆出，同时保持玩法、输入、固定步、像素证据、文件名、摘要字段和退出条件完全不变。

**Architecture:** `run_raylib_host()` 继续是唯一平台入口。正式主循环只依赖一个堆上创建一次的私有 `HostValidationRuntime`，该门面按原顺序组合各 Stage 实现；Stage 文件共享无状态导航原语但不建立新库。窗口关闭由一个无分配 RAII 值类型兜底，设置预览继续通过现有公开函数语义执行。公共 `RaylibHostConfig`、场景枚举和领域门面不变。

**Tech Stack:** C++17, raylib 6.0.0, CMake 3.25+, Ninja, MSVC, PowerShell, CTest.

## Global Constraints

- 只在 `E:/game/.worktrees/whole-game-refactor-2026-07`、分支 `codex/whole-game-refactor-2026-07` 工作；不得修改控制检出 `E:/game` 的未提交内容。
- 基线提交是 `f132f276e5ac8339acb9573146a260193961eeb6`，设计规格提交是 `f489c95`；本计划只执行已批准规格的子项目 A，不启动 B–G，也不实现尚未批准的方形房间后续任务。
- `raylib_host.hpp` 中 Stage 场景枚举、`RaylibHostConfig` 字段、`make_production_host_config()`、`run_raylib_host()` 和所有现有公开帮助函数签名保持不变。
- 输入注入顺序必须保持 `sample -> 11B -> 11C -> 11D -> 17 -> map_host_frame_input`；固定步移动优先级必须保持 `Stage11 -> Stage10 -> production input`；摘要顺序必须保持 `11B -> 11C -> 11D -> 17`。
- 不改变任何按键、数值、RNG 消耗、事件顺序、固定步次数、截图时机、截图名称、证据目录、摘要字段/文本、阶段完成条件、存档格式、颜色、布局、绘制顺序或音频行为。
- 正式验收代码继续编入 `arpg_raylib`，不得移动进测试目标或用测试替身替代。`combat`、`dungeon`、`persistence` 和 `core` 不得包含 raylib 头或依赖 `host_validation_*`。
- 所有状态仍只在初始化期分配，聚合门面不得超过现有的两次堆分配；逐帧和固定步热路径不得新增分配、字符串格式化、虚调用、共享所有权或无界容器。
- 每张卡先运行会失败的精确守卫/测试，确认失败原因就是缺失边界；仅移动最小实现，再运行 GREEN。不得通过删除断言、缩小扫描范围或更新黄金值接受漂移。
- 所有新增 `.cpp` 必须登记进 `src/platform/raylib/CMakeLists.txt`；所有新增 CMake 守卫必须登记进 `tests/platform/CMakeLists.txt`。源码守卫拆分后必须同时扫描主循环与对应 Stage 文件。
- 构建串行运行：`$env:CMAKE_BUILD_PARALLEL_LEVEL='1'` 且 `cmake --build ... -- -j1`。长构建前检查剩余内存与 `cl/link/cmake/ninja/arpg_*` 进程；不得为构建杀死无关程序。
- 每卡完成后运行独立规格审查和代码质量审查；P0/P1/P2 全部关闭才提交。每个提交必须可独立回滚。
- 已知基线红项：`stage11c_hud_evidence_guard_test.cmake` 当前寻找已不存在的栈值声明，直接运行报 `Stage11C evidence guard cannot bind complete host capture surface`。Task 0 必须先修复守卫锚点并证明自测仍能拒绝伪证据。

---

### Task 0: 冻结主机验收基线并修复已漂移的 Stage11C 守卫

**Files:**
- Create: `docs/validation/raylib-host-refactor-baseline.md`
- Create: `tests/platform/host_validation_sequence_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_self_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- 新守卫锁定三组次序：输入注入链、固定步移动优先级、摘要写入顺序。
- Stage11C 守卫从真实引用别名和真实采集赋值建立 capture surface，不再依赖早已删除的 `Stage11CHudValidationState stage11c_validation_state{};` 文本。
- 守卫自测继续证明直接覆盖 `HudViewModel`、伪造 snapshot hash、绕过物理输入或伪造 `result=pass` 会失败。

- [ ] **Step 1: 记录基线规模、符号归属和当前失败**

  记录 `raylib_host.cpp` 行数、六组状态定义区间、各 Stage 函数区间、主循环调用点、分支哈希和工作树状态。直接运行现有 Stage11C 守卫，保存上述已知错误；不得把它描述成重构引入。

  ```powershell
  cmake "-DSOURCE_ROOT=E:/game/.worktrees/whole-game-refactor-2026-07" -P tests/platform/stage11c_hud_evidence_guard_test.cmake
  ```

- [ ] **Step 2: 写入顺序特征守卫**

  在 sanitize 掉注释后，分别查找并比较下列 token 的唯一位置；任何缺失、重复或调序均 `FATAL_ERROR`：

  ```text
  sample_physical_keys -> inject_stage11b_physical_edges -> inject_stage11c_physical_edges
  -> inject_stage11d_physical_edges -> inject_stage17_physical_edges -> map_host_frame_input

  stage11_validation_input -> stage10_validation_input -> frame_input.movement

  write_stage11b_validation_summary -> write_stage11c_hud_validation_summary
  -> write_stage11d_loot_validation_summary -> write_stage17_validation_summary
  ```

- [ ] **Step 3: 修复 Stage11C 守卫并保留变异自测**

  用 `Stage11CHudValidationState& stage11c_validation_state =` 作为当前 runtime 起点；将 driver、runtime 和 summary 分段检查，保留“模型只赋值一次、hash 只能来自 `stage11c_production_snapshot_hash(current)`、真实截图在 `EndDrawing()` 后读取”的全部断言。增加一个把引用别名改成独立值的 mutation，要求守卫拒绝。

- [ ] **Step 4: 配置并验证基线守卫 GREEN**

  ```powershell
  $env:CMAKE_BUILD_PARALLEL_LEVEL='1'
  cmake --preset windows-msvc-debug
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.host_validation_sequence|stage11c\.hud_evidence_guard|stage11c\.hud_evidence_guard_self_test)$' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 5: 独立审查并提交**

  ```powershell
  git add docs/validation/raylib-host-refactor-baseline.md tests/platform
  git commit -m "test: freeze raylib host validation baseline"
  ```

---

### Task 1: 抽取跨 Stage 的无状态输入与导航原语

**Files:**
- Create: `src/platform/raylib/host_validation_navigation.hpp`
- Create: `src/platform/raylib/host_validation_navigation.cpp`
- Create: `src/platform/raylib/host_validation_input.hpp`
- Create: `src/platform/raylib/host_validation_input.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_architecture_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_architecture_guard_self_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_self_test.cmake`

**Interfaces:**

```cpp
namespace arpg::platform::host_validation {
void inject_validation_pressed(PhysicalKeySnapshot&,
    settings::StableKey) noexcept;
void inject_validation_action(PhysicalKeySnapshot&,
    const settings::SettingsData&, settings::SettingAction,
    bool pressed) noexcept;
void inject_validation_movement(PhysicalKeySnapshot&,
    const settings::SettingsData&, combat::MovementInput) noexcept;
[[nodiscard]] combat::MovementInput validation_route_fire_movement(
    combat::Vec3 player, combat::Vec3 target,
    combat::MovementInput requested) noexcept;
[[nodiscard]] const combat::MonsterSnapshot* nearest_living_monster(
    const combat::CombatSnapshot&) noexcept;
[[nodiscard]] combat::MovementInput validation_movement_toward(
    combat::Vec3 from, combat::Vec3 to) noexcept;
[[nodiscard]] combat::Vec3 validation_door_position(
    dungeon::ExitDirection) noexcept;
[[nodiscard]] combat::MovementInput validation_exit_movement(
    combat::Vec3 player, dungeon::ExitDirection) noexcept;
[[nodiscard]] bool validation_attack_lane(
    const combat::CombatSnapshot&,
    const combat::MonsterSnapshot&) noexcept;
[[nodiscard]] dungeon::ExitDirection validation_direction(
    const RaylibHostConfig&) noexcept;
}
```

- [ ] **Step 1: 扩展结构守卫并确认 RED**

  要求三个输入定义只存在于 `host_validation_input.cpp`、七个导航定义只存在于 `host_validation_navigation.cpp`，头中只有声明，两个 `.cpp` 均登记 `arpg_raylib`，且 `raylib_host.cpp` 不再定义旧 helper。当前应因新文件缺失而失败。

- [ ] **Step 2: 创建具名私有命名空间和声明**

  input 头只包含 `PhysicalKeySnapshot`、settings 和 movement 所需声明；navigation 头只包含 combat/dungeon 值类型与 `RaylibHostConfig` 所需声明。两者均不得包含 `<raylib.h>`、renderer、persistence 或测试头。

- [ ] **Step 3: 逐函数原样移动实现**

  input 文件移动原 696–703、1857–1884 行，并把私有误名 `inject_stage11b_pressed`、`inject_stage11c_binding`、`inject_stage11c_movement` 统一为上面的三个通用名字；11B/11C/11D/17 全部显式调用同一实现。navigation 文件移动原 739–757、1693–1718、1820–1850、2606–2624 行。不合并重复分支、不改变浮点常量、不重排遍历。

- [ ] **Step 4: 迁移受影响的 Stage11C 守卫锚点**

  driver 仍从 `inject_stage11c_physical_edges` 开始检查，物理 action/movement helper 改从共享 input 源检查；mutation self-test 必须能分别拒绝“直接逻辑 action 队列”和“跳过 stable binding”的变异。

- [ ] **Step 5: 编译并验证 GREEN**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_raylib arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_sequence|architecture\.|stage11c\.architecture\.|stage11c\.hud_evidence_guard)' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 6: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/platform/host_validation_sequence_guard_test.cmake tests/platform/stage11c_hud_architecture_guard_test.cmake tests/platform/stage11c_hud_architecture_guard_self_test.cmake tests/platform/stage11c_hud_evidence_guard_test.cmake tests/platform/stage11c_hud_evidence_guard_self_test.cmake
  git commit -m "refactor: extract host validation input and navigation"
  ```

---

### Task 2: 抽取 Stage10 与 Stage11 状态、路线和完成判定

**Files:**
- Create: `src/platform/raylib/host_validation_stage10_11.hpp`
- Create: `src/platform/raylib/host_validation_stage10_11.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/dungeon/stage10_evidence_guard_test.cmake`
- Modify: `tests/dungeon/stage11_death_evidence_guard_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`

**Interfaces:**

```cpp
struct Stage10ValidationState final {
    bool entered_abyss{};
    bool reset_requested{};
    bool descent_warning_seen{};
    std::uint32_t chaos_presented_frames{};
};
struct Stage11ValidationState final {
    bool entered_abyss{};
    bool saw_depth_two{};
    bool continue_requested{};
    std::uint32_t target_presented_frames{};
};
combat::MovementInput stage10_validation_input(
    dungeon::DungeonSession&, const dungeon::DungeonSnapshot&,
    const RaylibHostConfig&, Stage10ValidationState&) noexcept;
combat::MovementInput stage11_validation_input(
    dungeon::DungeonSession&, const dungeon::DungeonSnapshot&,
    const RaylibHostConfig&, Stage11ValidationState&) noexcept;
bool stage10_validation_reached(
    const dungeon::DungeonSnapshot&, const RaylibHostConfig&,
    const Stage10ValidationState&) noexcept;
bool stage11_validation_reached(
    const dungeon::DungeonSnapshot&, const RaylibHostConfig&,
    const Stage11ValidationState&) noexcept;
```

- [ ] **Step 1: 先更新证据守卫的目标源并确认 RED**

  Stage10/11 守卫分别扫描 `raylib_host.cpp` 的正式调用次序和 `host_validation_stage10_11.cpp` 的路线/完成算法；在新源缺失时失败。禁止删除任何真实 Session 请求、物理移动和截图次序断言。

- [ ] **Step 2: 原样移动两个状态结构和四个公开给主机的函数**

  同时移动它们独占的 `has_environment_visual` 等叶子帮助函数。场景枚举仍留在 `raylib_host.hpp`，状态类型进入具名私有命名空间。

- [ ] **Step 3: 保持主循环判定和移动优先级**

  `validation_continue`、`continue_requested`、presented-frame 计数和 `stage10_validation_captured` 的更新位置不变；只替换类型/函数限定名。

- [ ] **Step 4: 运行聚焦回归**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_dungeon_tests arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.host_validation_sequence|stage10\.|stage11\.|dungeon\.units|platform\.units)$' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 5: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/dungeon tests/platform/host_validation_sequence_guard_test.cmake
  git commit -m "refactor: extract stage10 and stage11 host validation"
  ```

---

### Task 3: 抽取 Stage11B 设置验收

**Files:**
- Create: `src/platform/raylib/host_validation_stage11b.hpp`
- Create: `src/platform/raylib/host_validation_stage11b.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/stage11b_architecture_guard_test.cmake`
- Modify: `tests/platform/stage11b_settings_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11b_settings_evidence_guard_self_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`

**Interfaces:**

```cpp
struct Stage11BValidationState final;  // 完整定义仅在私有 Stage 头中
PhysicalKeySnapshot inject_stage11b_physical_edges(
    PhysicalKeySnapshot, const RaylibHostConfig&,
    Stage11BValidationState&) noexcept;
bool stage11b_validation_complete(
    const RaylibHostConfig&, const Stage11BValidationState&,
    const PauseMenuState&) noexcept;
std::uint64_t stage11b_snapshot_hash(
    const dungeon::DungeonSnapshot&) noexcept;
void write_stage11b_validation_summary(
    const RaylibHostConfig&, const Stage11BValidationState&,
    const PauseMenuState&) noexcept;
```

- [ ] **Step 1: 将 11B 守卫改成双表面检查并确认 RED**

  算法 token、summary 字段和按键注入只从新 Stage 源读取；`EndDrawing -> LoadImageFromScreen`、pause capture、fixed tick 和输入链仍从主循环读取。三个 mutation self-test 改为复制/变异正确的源文件。

- [ ] **Step 2: 原样移动状态与 1541–1691 行实现**

  `load_status` 初始化仍取自同一次 `SettingsStore::load()`；暂停前后 tick/hash 采集点和重绑帧号保持不变。

- [ ] **Step 3: 编译并运行 11B 守卫/单元测试**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests arpg_stage11b_settings_formal -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_sequence|stage11b\.architecture\.|stage11b\.settings_evidence_guard)' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 4: 运行正式 11B 证据链**

  ```powershell
  ctest --test-dir out/build/windows-msvc-debug -R '^stage11b\.settings_(formal|evidence_validator)$' --output-on-failure -j1
  ```

- [ ] **Step 5: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "refactor: extract stage11b host validation"
  ```

---

### Task 4: 抽取 Stage11C HUD 验收

**Files:**
- Create: `src/platform/raylib/host_validation_stage11c.hpp`
- Create: `src/platform/raylib/host_validation_stage11c.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/stage11c_hud_architecture_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_architecture_guard_self_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_self_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`

**Interfaces:**

```cpp
struct Stage11CHudValidationState final;  // 拥有 HudViewModel/Notice/Layout 值
PhysicalKeySnapshot inject_stage11c_physical_edges(
    PhysicalKeySnapshot, const RaylibHostConfig&,
    const settings::SettingsData&, const dungeon::DungeonSnapshot&,
    Stage11CHudValidationState&) noexcept;
std::uint64_t stage11c_production_snapshot_hash(
    const dungeon::DungeonSnapshot&) noexcept;
bool stage11c_hud_validation_reached(
    const dungeon::DungeonSnapshot&, Stage11CHudValidationScenario,
    const Stage11CHudValidationState&, bool draw_debug) noexcept;
void write_stage11c_hud_validation_summary(
    const RaylibHostConfig&, const Stage11CHudValidationState&) noexcept;
```

- [ ] **Step 1: 扩展 Task 0 守卫到新 Stage 源并确认 RED**

  driver 和 summary 从 `host_validation_stage11c.cpp` 读取；主循环 capture surface 独立检查真实 renderer model/notices/layout/hash 的唯一赋值和真实 presented-frame 截图。

- [ ] **Step 2: 原样移动状态和 1857–1936、2145–2299 行**

  低血量夹具、debug visibility、CJK font readiness、target presented-frame 计数与 hash 采样时机不变。

- [ ] **Step 3: 编译并运行所有 11C headless 门禁**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests arpg_stage11c_hud_stress arpg_stage11c_hud_formal -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_sequence|stage11c\.architecture\.|stage11c\.hud_evidence_guard|stage11c\.hud_stress)' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 4: 运行正式 11C HUD 证据链**

  ```powershell
  ctest --test-dir out/build/windows-msvc-debug -R '^stage11c\.hud_(formal|evidence_validator)' --output-on-failure -j1
  ```

- [ ] **Step 5: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "refactor: extract stage11c host validation"
  ```

---

### Task 5A: 抽取 Stage11D 状态、选择器和输入运行时

**Files:**
- Create: `src/platform/raylib/host_validation_stage11d.hpp`
- Create: `src/platform/raylib/host_validation_stage11d_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/stage11d_loot_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11d_loot_evidence_guard_self_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`

**Interfaces:**

```cpp
struct Stage11DLootValidationState final;
PhysicalKeySnapshot inject_stage11d_physical_edges(
    PhysicalKeySnapshot, const RaylibHostConfig&,
    const settings::SettingsData&, const dungeon::DungeonSnapshot&,
    Stage11DLootValidationState&) noexcept;
```

- [ ] **Step 1: 让 11D runtime 守卫分别扫描新源与主循环并确认 RED**

  状态、selector、物理输入和固定步相关的 `STAGE11D_LOOT_VALIDATION_SEAM_*` 标记迁到 runtime 源；守卫继续检查主循环输入顺序和 abyss claim 提交，不能把整个 `raylib_host.cpp` 排除。

- [ ] **Step 2: 原样移动 11D 状态、专属导航和输入**

  移动原 376–430、629–694、1721–1818、1937–2144 行。复用 Task1 的共享 input/navigation 原语；固定容量数组、monster ordinal、receipt generation 与 abyss claim 标志逐字段保持。

- [ ] **Step 3: 编译并运行 runtime 门禁**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_sequence|stage11d\.architecture\.|stage11d\.loot_evidence_guard)' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 4: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "refactor: extract stage11d validation runtime"
  ```

---

### Task 5B: 抽取 Stage11D 呈现语义与报告

**Files:**
- Create: `src/platform/raylib/host_validation_stage11d_report.cpp`
- Modify: `src/platform/raylib/host_validation_stage11d.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/stage11d_loot_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11d_loot_evidence_guard_self_test.cmake`
- Modify: `tests/platform/stage11d_renderer_integration_guard_test.cmake`
- Modify: `tests/platform/stage11d_renderer_integration_guard_self_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`

**Interfaces:**

```cpp
bool stage11d_target_visible(
    const RaylibHostConfig&, const dungeon::DungeonSnapshot&,
    const PauseMenuState&, const DungeonRenderStatus&,
    settings::LootFilterMode, const GroundLootView&, HudNoticeView,
    Stage11DLootValidationState&) noexcept;
void stage11d_record_semantics(
    Stage11DLootValidationState&, const dungeon::DungeonSnapshot&,
    const items::ItemOwnershipState*, const GroundLootView&,
    HudNoticeView) noexcept;
void write_stage11d_loot_validation_summary(
    const RaylibHostConfig&, const Stage11DLootValidationState&,
    const PauseMenuState&) noexcept;
```

- [ ] **Step 1: 迁移 presented/capture/report 守卫并确认 RED**

  其余 seam marker 随具体实现迁到 report 源；renderer integration 守卫继续从主循环证明真实 `GroundLootView`、HUD notice、`EndDrawing()` 和 capture result 的顺序，从 report 源证明 semantic snapshot 与摘要字段。

- [ ] **Step 2: 原样移动语义记录、可见判定尾部与摘要**

  移动原 2300–2605 行中属于呈现/报告的实现；rarity 计数、item id 集合、pickup notice rect、`result=pass/fail` 和异常处理逐字段保持。

- [ ] **Step 3: 编译并运行全部 11D headless 门禁**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests arpg_stage11d_loot_formal -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_sequence|stage11d\.architecture\.|stage11d\.renderer_integration_guard|stage11d\.loot_evidence_guard|stage11d\.loot_root_safety)$' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 4: 运行正式 11D 证据链**

  ```powershell
  ctest --test-dir out/build/windows-msvc-debug -R '^stage11d\.loot_(formal|evidence_validator)' --output-on-failure -j1
  ```

- [ ] **Step 5: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "refactor: extract stage11d validation reporting"
  ```

---

### Task 6A: 抽取 Stage17 主动技能运行时验收

**Files:**
- Create: `src/platform/raylib/host_validation_stage17.hpp`
- Create: `src/platform/raylib/host_validation_stage17_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/host_input_source_test.cmake`
- Modify: `tests/platform/input_latency_source_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`
- Modify: `tests/platform/stage12_environment_render_tests.cpp`

**Interfaces:**

```cpp
struct Stage17SkillStonesValidationState final;
void observe_stage17_draw_runtime(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&, const dungeon::DungeonSnapshot&,
    const ActiveSkillDrawRuntimeStatus&) noexcept;
void observe_stage17_combat_event(Stage17SkillStonesValidationState*,
    const combat::CombatEvent&) noexcept;
PhysicalKeySnapshot inject_stage17_physical_edges(
    PhysicalKeySnapshot, const RaylibHostConfig&,
    const settings::SettingsData&, const dungeon::DungeonSnapshot&,
    Stage17SkillStonesValidationState&) noexcept;
void observe_stage17_submitted_actions(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&, const SubmittedFrameActions&) noexcept;
void observe_stage17_snapshot(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&,
    const dungeon::DungeonSnapshot&) noexcept;
void observe_stage17_inventory(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&, const InventoryRenderer&,
    const dungeon::DungeonSnapshot&) noexcept;
```

- [ ] **Step 1: 先迁移 Stage17 runtime 源码守卫并确认 RED**

  `host_input_source_test.cmake` 和 `input_latency_source_test.cmake` 同时读取主循环与 runtime；原有 loadout transaction、cooldown drain、storm isolation、public input path 和 renderer evidence 断言一个不少。

- [ ] **Step 2: 移动状态与运行逻辑**

  将原 441–626、705–1239 行移入 Stage17 头/runtime。`Rectangle`、`Vector2`、`GetScreenWidth/Height` 和 `<raylib.h>` 只能出现在 `.cpp`；状态头不得泄漏 raylib 类型。

- [ ] **Step 3: 编译并运行 runtime GREEN**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_input_source|platform\.input_latency_source|platform\.host_validation_sequence|stage17\.active_skill_material_asset_pipeline)$' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 4: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "refactor: extract stage17 validation runtime"
  ```

---

### Task 6B: 抽取 Stage17 截图决策与报告

**Files:**
- Create: `src/platform/raylib/host_validation_stage17_report.cpp`
- Modify: `src/platform/raylib/host_validation_stage17.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/host_input_source_test.cmake`
- Modify: `tests/platform/input_latency_source_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`

**Interfaces:**

```cpp
std::optional<std::string> stage17_capture_path(const RaylibHostConfig&,
    Stage17SkillStonesValidationState&) noexcept;
void mark_stage17_capture_complete(Stage17SkillStonesValidationState&) noexcept;
bool stage17_validation_complete(const RaylibHostConfig&,
    const Stage17SkillStonesValidationState&) noexcept;
void write_stage17_validation_summary(const RaylibHostConfig&,
    const Stage17SkillStonesValidationState&) noexcept;
```

- [ ] **Step 1: 增加 report 唯一归属守卫并确认 RED**

  要求 capture basename、完成判定、clean-shutdown 字段与 summary token 只存在于 report 源；主循环仍单独检查 `EndDrawing -> capture result -> mark complete -> clean shutdown -> write summary`。

- [ ] **Step 2: 原样移动 capture/report 逻辑**

  将原 1240–1540 行移入 report；截图 basename、字段顺序、`result=pass/fail` 条件、I/O 异常吞吐语义保持逐字一致。

- [ ] **Step 3: 编译并运行 Stage17 全部门禁**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_input_source|platform\.input_latency_source|platform\.host_validation_sequence|stage17\.)' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 4: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "refactor: extract stage17 validation reporting"
  ```

---

### Task 7A: 建立不透明 HostValidationRuntime 并收口输入/事件观察

**Files:**
- Create: `src/platform/raylib/host_validation.hpp`
- Create: `src/platform/raylib/host_validation_state.hpp`
- Create: `src/platform/raylib/host_validation_runtime.cpp`
- Modify: `src/platform/raylib/host_validation_stage10_11.hpp`
- Modify: `src/platform/raylib/host_validation_stage11b.hpp`
- Modify: `src/platform/raylib/host_validation_stage11c.hpp`
- Modify: `src/platform/raylib/host_validation_stage11d.hpp`
- Modify: `src/platform/raylib/host_validation_stage17.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Create: `tests/platform/host_validation_source_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**

```cpp
class HostValidationRuntime final {
public:
    static std::unique_ptr<HostValidationRuntime> create(
        const RaylibHostConfig&,
        settings::SettingsLoadStatus) noexcept;
    ~HostValidationRuntime();
    void set_render_readiness(bool cjk_font_ready,
        bool active_skill_atlases_ready) noexcept;
    PhysicalKeySnapshot inject_physical_edges(
        PhysicalKeySnapshot, const settings::SettingsData&,
        const dungeon::DungeonSnapshot&,
        bool gameplay_rearm_required) noexcept;
    void observe_combat_event(const combat::CombatEvent&) noexcept;
    void observe_snapshot(const dungeon::DungeonSnapshot&) noexcept;
    void observe_inventory(const InventoryRenderer&,
        const dungeon::DungeonSnapshot&) noexcept;
    void observe_submitted_actions(const SubmittedFrameActions&) noexcept;
private:
    struct Impl;
    explicit HostValidationRuntime(std::unique_ptr<Impl>) noexcept;
    std::unique_ptr<Impl> impl_;
};
```

- [ ] **Step 1: 写 runtime 创建与输入/事件边界守卫并确认 RED**

  守卫要求 runtime/state 文件存在并登记 `arpg_raylib`，领域源不包含任何 `host_validation_*`；主循环不再直接调用四个 `inject_stage*`、`observe_stage17_*` 或直接处理 11B submitted-action 计数，但暂时允许固定步与呈现字段留给 7B/7C。

- [ ] **Step 2: 建立初始化期不透明状态**

  `Impl` 聚合六组状态；`create(config, load_status)` 用 `new (std::nothrow)`/`unique_ptr` 保持在 `InitWindow()` 之前，外壳加 `Impl` 总计不得超过当前两次堆分配。renderer 初始化后仅调用无分配 `set_render_readiness()`。失败继续映射为 `save_initialization_failed`。

- [ ] **Step 3: 收口输入与事件 hook**

  封装 combat event、snapshot、四段输入注入、背包观察与 submitted actions；`drain_events()` 只接收一个 runtime 指针。输入调用位置不能跨越 gameplay rearm 或 `map_host_frame_input()`。

- [ ] **Step 4: 运行输入/事件相关门禁**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_(source|sequence)|platform\.host_input_source|platform\.input_latency_source|stage11b\.|stage17\.)' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 5: 独立审查并提交**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "refactor: encapsulate host validation input"
  ```

---

### Task 7B: 收口验收固定步与死亡路径

**Files:**
- Modify: `src/platform/raylib/host_validation.hpp`
- Modify: `src/platform/raylib/host_validation_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/host_validation_source_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`

**Interfaces:**

```cpp
[[nodiscard]] bool should_continue_death(
    const dungeon::DungeonSnapshot&) const noexcept;
combat::MovementInput fixed_step_movement(
    dungeon::DungeonSession&, const dungeon::DungeonSnapshot&,
    combat::MovementInput production_input) noexcept;
void observe_fixed_tick() noexcept;
void observe_death_continue_result(dungeon::RequestResult) noexcept;
void observe_post_fixed_tick(const dungeon::DungeonSnapshot&,
    const items::ItemOwnershipState*) noexcept;
[[nodiscard]] bool fixed_step_target_reached(
    const dungeon::DungeonSnapshot&) const noexcept;
```

- [ ] **Step 1: 增加固定步/死亡边界 RED 守卫**

  要求主循环不再访问 Stage10/11 continue/reached 字段、11B fixed-tick 字段或 11D abyss-claim 字段；固定步仍严格保持 `Stage11 -> Stage10 -> production input`，死亡 continue 仍只提交真实 runtime 请求。

- [ ] **Step 2: 收口死亡 continue 与固定步移动**

  `should_continue_death()` 只复现原 scenario/flag 条件；`fixed_step_movement()` 保持原优先级；`observe_post_fixed_tick()` 只观察同一 snapshot/ownership，不提交额外事务。

- [ ] **Step 3: 收口 fixed-tick 计数、abyss claim 与提前 break**

  hook 位置保留在 `runtime.fixed_tick()` 和 `session->snapshot(current)` 之后、事件排空之前；不得调换 snapshot、claim 观察、death overlay 清理或 Stage10/11 break。

- [ ] **Step 4: 编译、验证、审查并提交**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_dungeon_tests arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_(source|sequence)|stage10\.|stage11\.|stage11b\.|stage11d\.)' --output-on-failure -j1
  git diff --check
  git add src/platform/raylib tests/platform
  git commit -m "refactor: encapsulate host validation fixed step"
  ```

---

### Task 7C: 收口 HUD、掉落、截图与摘要并完成最终边界

**Files:**
- Modify: `src/platform/raylib/host_validation.hpp`
- Modify: `src/platform/raylib/host_validation_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/host_validation_source_test.cmake`
- Modify: `tests/platform/host_validation_sequence_guard_test.cmake`
- Modify: `tests/dungeon/stage10_evidence_guard_test.cmake`
- Modify: `tests/dungeon/stage11_death_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11b_architecture_guard_test.cmake`
- Modify: `tests/platform/stage11b_settings_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11b_settings_evidence_guard_self_test.cmake`
- Modify: `tests/platform/stage11c_hud_architecture_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_architecture_guard_self_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11c_hud_evidence_guard_self_test.cmake`
- Modify: `tests/platform/stage11d_loot_evidence_guard_test.cmake`
- Modify: `tests/platform/stage11d_loot_evidence_guard_self_test.cmake`
- Modify: `tests/platform/stage11d_renderer_integration_guard_test.cmake`
- Modify: `tests/platform/stage11d_renderer_integration_guard_self_test.cmake`
- Modify: `tests/platform/host_input_source_test.cmake`
- Modify: `tests/platform/input_latency_source_test.cmake`
- Modify: `tests/platform/stage12_environment_render_tests.cpp`
- Modify: `docs/validation/raylib-host-refactor-baseline.md`

**Interfaces:**

```cpp
enum class CaptureOwner : std::uint8_t {
    none,
    generic_validation,
    stage17,
};
struct PresentationDecision final {
    bool validation_complete{};
    bool generic_capture_visible{};
    CaptureOwner capture_owner{CaptureOwner::none};
    std::optional<std::string> stage17_capture_path{};
};
void prepare_hud_snapshot(dungeon::DungeonSnapshot&) noexcept;
void observe_hud(const dungeon::DungeonSnapshot&,
    const HudViewModel&, HudNoticeView, bool draw_debug,
    int screen_width, int screen_height) noexcept;
void observe_active_skill_draw(const dungeon::DungeonSnapshot&,
    const ActiveSkillDrawRuntimeStatus&) noexcept;
PresentationDecision observe_presented_frame(
    const dungeon::DungeonSnapshot&, const PauseMenuState&,
    const DungeonRenderStatus&, settings::LootFilterMode,
    const GroundLootView&, const items::ItemOwnershipState*,
    bool pause_cjk_ready, int screen_width, int screen_height) noexcept;
void observe_capture_result(CaptureOwner, bool succeeded) noexcept;
void write_summaries(CleanShutdownState,
    const PauseMenuState&) noexcept;
```

- [ ] **Step 1: 启用最终源码边界并确认 RED**

  守卫扫描 `raylib_host.cpp` 和全部 `host_validation_*.cpp`，禁止重复定义、漏 CMake 登记、领域反向依赖、主循环 `Stage*ValidationState` 类型/字段访问、`inject_stage*` 和 `write_stage*_validation_summary`。主机表面只证明 production input/authority/render/`EndDrawing`/capture/clean-shutdown 的次序；runtime 表面证明 facade 转发次序与唯一 hook；各 Stage 表面证明算法、字段和 evidence token；self-test 必须变异实际拥有锚点的文件。不允许拼接后用跨文件字符位置伪造次序，也不允许排除任一 Stage 源。

- [ ] **Step 2: 收口 HUD 与 active-skill 呈现观察**

  `prepare_hud_snapshot()` 只保留现有 low-health 正式夹具对 snapshot 副本的 barrier 修改；HUD model/notices/layout/hash 继续从真实 renderer 输出采集，位置仍在 `BeginDrawing()` 前。

- [ ] **Step 3: 收口掉落语义、完成判定和 capture owner**

  `observe_presented_frame()` 在真实 `GroundLootView` 生成后执行，只返回决策值；主机仍唯一调用 `present_frame_and_maybe_capture()`。`observe_capture_result()` 仅在 `EndDrawing()` 后消费同一次结果并更新 Stage10/11C/11D/17 状态。

- [ ] **Step 4: 收口 clean shutdown 与四组摘要**

  `write_summaries()` 内部固定顺序为 11B、11C、11D、17；调用仍在 clean shutdown 状态确定后、audio/renderer/window 清理前。

- [ ] **Step 5: 运行全部 Stage 守卫、记录行数并提交**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_(source|sequence)|stage10\.|stage11\.|stage11b\.|stage11c\.|stage11d\.|stage17\.)' --output-on-failure -j1
  git diff --check
  git add src/platform/raylib tests/platform tests/dungeon/stage10_evidence_guard_test.cmake tests/dungeon/stage11_death_evidence_guard_test.cmake docs/validation/raylib-host-refactor-baseline.md
  git commit -m "refactor: complete host validation boundary"
  ```

---

### Task 8A: 独立抽取固定步帧门控

**Files:**
- Create: `src/platform/raylib/host_frame_gate.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/host_validation_source_test.cmake`

**Interfaces:**

```cpp
HostFrameGateResult gate_host_frame(core::FixedStepRunner&,
    bool& pause_latched, bool paused,
    double frame_seconds) noexcept;
```

- [ ] **Step 1: 增加定义唯一归属 RED 守卫**

  要求公开声明仍在 `raylib_host.hpp`，定义只在 `host_frame_gate.cpp`，源已登记 `arpg_raylib`，主循环只调用该函数。

- [ ] **Step 2: 原样移动现有 gate 定义**

  不改变 paused 首帧 accumulator clear、`pause_latched` 更新或 `FixedStepRunner::advance()` 参数/返回值。

- [ ] **Step 3: 编译、验证、审查并提交**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_(source|sequence))$' --output-on-failure -j1
  git diff --check
  git add src/platform/raylib tests/platform/host_validation_source_test.cmake
  git commit -m "refactor: extract host frame gate"
  ```

---

### Task 8B: 独立抽取设置预览/提交协调

**Files:**
- Create: `src/platform/raylib/host_settings_runtime.hpp`
- Create: `src/platform/raylib/host_settings_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Create: `tests/platform/host_settings_runtime_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/host_validation_source_test.cmake`

**Interfaces:**

```cpp
struct HostSettingsRuntime final {
    HostSettingsNotice* notice{};
    PauseMenuState* pause_menu{};
    settings::SettingsData* live{};
    settings::SettingsData* input{};
    const settings::SettingsStore* store{};
    WindowSettingsBackend backend{};
    void consume_notice(PauseScreen previous_screen) noexcept;
    [[nodiscard]] bool settle(PauseCommand,
        bool window_close_requested);
};
```

- [ ] **Step 1: 写 settings runtime RED 单元测试**

  用注入 backend 证明 preview/apply/rollback/quit、message 文本、loot filter 延迟提交、revision 和 input settings 更新与现有 `settle_host_pause_command()` 逐字段相同。

- [ ] **Step 2: 移动 notice/filter/pause command 算法**

  公开 `make_host_settings_notice()`、`consume_host_settings_notice()`、`renderer_loot_filter_mode()` 与 `settle_host_pause_command()` 签名保持，旧函数变成对同一算法的薄包装；不得复制第二套 switch。

- [ ] **Step 3: 主循环接入值型 coordinator**

  coordinator 只保存现有对象指针与 backend，不拥有路径/窗口/renderer，不分配；settings screen 进入、预览、apply、rollback 和 close 的调用位置保持。

- [ ] **Step 4: 编译、验证、审查并提交**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_(source|sequence)|stage11b\.)' --output-on-failure -j1
  git diff --check
  git add src/platform/raylib tests/platform
  git commit -m "refactor: extract host settings runtime"
  ```

---

### Task 8C: 独立引入窗口生命周期兜底

**Files:**
- Create: `src/platform/raylib/host_window_lifetime.hpp`
- Create: `src/platform/raylib/host_window_lifetime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Create: `tests/platform/host_window_lifetime_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/host_validation_source_test.cmake`
- Modify: `docs/validation/raylib-host-refactor-baseline.md`

**Interfaces:**

```cpp
struct HostWindowBackend final {
    void (*set_config_flags)(unsigned int){};
    void (*init_window)(int, int, const char*){};
    bool (*is_window_ready)(){};
    void (*close_window)(){};
};
class HostWindowLifetime final {
public:
    explicit HostWindowLifetime(HostWindowBackend) noexcept;
    bool initialize(const RaylibHostConfig&,
        const settings::SettingsData&) noexcept;
    void close() noexcept;
    [[nodiscard]] bool ready() const noexcept;
    ~HostWindowLifetime();
    HostWindowLifetime(const HostWindowLifetime&) = delete;
    HostWindowLifetime& operator=(const HostWindowLifetime&) = delete;
private:
    HostWindowBackend backend_{};
    bool ready_{};
};
[[nodiscard]] HostWindowBackend raylib_host_window_backend() noexcept;
```

- [ ] **Step 1: 写窗口 backend/lifetime RED 测试**

  用计数 backend 证明初始化失败不关闭、成功显式关闭一次、重复 close 不重复、异常/早退由析构兜底；测试不得打开真实窗口。

- [ ] **Step 2: 实现 RAII 并保持初始化顺序**

  `SetConfigFlags -> InitWindow -> IsWindowReady` 保持原顺序；`SetWindowMinSize/SetExitKey/ChangeDirectory/foreground` 仍由 host 在成功后原位调用。

- [ ] **Step 3: 替换 catch/成功路径关闭逻辑**

  显式 audio/renderer/pause renderer shutdown 必须发生在 `HostWindowLifetime::close()` 前；catch 不再手写重复 `CloseWindow()`，但返回码与日志文本不变。

- [ ] **Step 4: 编译并运行平台回归**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_(source|sequence)|stage11b\.|architecture\.)' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 5: 检查完成度并提交**

  `raylib_host.cpp` 不得包含 Stage 状态机定义，目标约不超过 1,200 行；若仍超过约 1,200 行，在基线文档逐段解释剩余职责，不用机械拆分换数字。

  ```powershell
  git add src/platform/raylib tests/platform docs/validation/raylib-host-refactor-baseline.md
  git commit -m "refactor: guard raylib window lifetime"
  ```

---

### Task 9: 子项目 A 全量门禁、独立审查和真实窗口验收

**Files:**
- Modify: `docs/validation/raylib-host-refactor-baseline.md`
- Modify only if required by a proven defect: files owned by Tasks 0–8

- [ ] **Step 1: 检查资源并执行 Debug 全量构建**

  ```powershell
  Get-Process | Where-Object { $_.ProcessName -match '^(cl|link|ninja|cmake|ctest|arpg_)' } | Select-Object ProcessName,Id,CPU,WorkingSet64
  Get-CimInstance Win32_OperatingSystem | Select-Object FreePhysicalMemory,FreeVirtualMemory
  $env:CMAKE_BUILD_PARALLEL_LEVEL='1'
  cmake --build --preset windows-msvc-debug -- -j1
  ctest --preset windows-msvc-debug -j1
  ```

- [ ] **Step 2: 执行 Release 全量构建与门禁**

  ```powershell
  cmake --preset windows-msvc-release
  cmake --build --preset windows-msvc-release -- -j1
  ctest --preset windows-msvc-release -j1
  ```

- [ ] **Step 3: 对比正式证据**

  对 Stage10/11/11B/11C/11D/17 的 summary 字段顺序、截图文件名、图像尺寸和已有 validator 结果逐项对比基线；任何黄金字节/像素守卫变化都回到对应任务修复，不更新期望值。

- [ ] **Step 4: 启动真实 raylib 游戏验收**

  验收移动、攻击、跳跃、L 上挑与 0.5 秒浮空、两个主动技能、背包、掉落拾取、暂停/设置、房门、下一层洞、死亡继续、HUD/字体和重启存档。运行期间记录 CPU/内存，确认无空闲高占用或编译器残留。

- [ ] **Step 5: 最终独立双审查**

  一个审查者逐条核对设计/计划/范围，另一个审查者检查所有改动的生命周期、输入顺序、热路径分配、守卫完整性和异常安全。P0/P1/P2 必须为零；最多五轮修复，否则按设计停止条件保留现场并报告。

- [ ] **Step 6: 更新度量、提交、推送并读回远端哈希**

  文档记录生产总行数变化、`raylib_host.cpp` 变化、每个新文件行数、最大文件、测试总数/耗时、真实窗口结果和已知限制。

  ```powershell
  git diff --check
  git status --short
  git add docs/validation/raylib-host-refactor-baseline.md
  git commit -m "docs: close raylib host refactor validation"
  git push -u origin codex/whole-game-refactor-2026-07
  git ls-remote origin refs/heads/codex/whole-game-refactor-2026-07
  ```

## Completion Gate

- `raylib_host.cpp` 不再定义或直接操作任何 Stage 验收状态机，只调用 `HostValidationRuntime`。
- 六组正式验收实现仍由 `arpg_raylib` 编译，公共 CLI/config/scenario 接口不变。
- 输入、固定步、事件、截图、summary 和 clean-shutdown 顺序由源码守卫与正式证据共同证明未漂移。
- Debug/Release 全量 CTest、独立双审查和真实窗口验收全部通过；工作树干净，远端哈希与本地 HEAD 一致。
- 本计划不把子项目 B 的改动混入 A；A 的交付状态明确记录后，主控再按已批准总规格生成并审查 B 的独立计划。
