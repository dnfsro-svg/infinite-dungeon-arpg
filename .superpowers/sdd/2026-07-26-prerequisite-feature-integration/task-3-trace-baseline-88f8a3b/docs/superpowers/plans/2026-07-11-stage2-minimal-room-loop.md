# Stage 2 最小房间主循环实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在已验收 Stage 1 战斗实验室之上，交付一个可见、可操作、可无窗口验证的单向无限房间循环：锁门战斗、三木桩清房、四门同时开启、走入一个出口、永久销毁旧房、下一固定 tick 构造唯一新房。

**Architecture:** 新增不依赖 raylib 的 `arpg_dungeon` 静态库，由 `DungeonSession` 独占至多一个 `std::optional<CombatWorld>`，以固定 tick、固定容量事件、值类型快照和确定性种子推进房间生命周期。raylib Host 只提交输入、消费事件和绘制快照；依赖方向固定为 `arpg_core <- arpg_combat <- arpg_dungeon <- arpg_raylib <- arpg_game`。

**Tech Stack:** C++17、raylib 6.0.0（静态链接）、CMake 3.25+、Ninja、CTest、MSVC 19.44 x64、Windows SDK 10.0.26100.0、PowerShell。

## Global Constraints

- 设计来源固定为 `docs/superpowers/specs/2026-07-11-stage2-minimal-room-loop-design.md`，设计提交为 `18a729af0177ce517f8e68b54c2bc1f1b1e5e5ad`。
- 集成基线固定为 `main@79547fbe0a00cb4b1ba372d09f378a5695f69443`；M02 开发期间不切换、不提交、不合并 `main`。
- 本里程碑只实现最小房间主循环。不得加入元素、洞口、深渊、层深、正式怪物、AI、奖励、掉落、存档、旧房图或多房常驻。
- `arpg_core` 与 `arpg_combat` 不得反向 include 或链接 `arpg_dungeon`；`arpg_dungeon` 不得包含、链接或间接依赖 raylib、raymath、rlgl、raylib-cpp。
- 规则只在固定 60 Hz tick 推进；渲染帧率、插值和过渡视觉不得改变房间状态、种子、门提交或战斗结果。
- `DungeonSession` 同时最多拥有一个活动 `CombatWorld`。旧房通过 `std::optional::reset()` 销毁，新房通过 `emplace()` 原地构造；不可复制、移动、交换或按值返回含有 `BoundedQueue` 的对象。
- `DungeonSnapshot`、`DungeonEvent`、`RoomDescriptor` 和测试摘要全部为固定大小值类型；确定性比较逐字段进行，禁止对含 padding 的结构使用 `memcmp`。
- 热路径不得持续堆分配。1000 房测试只能使用公开生产接口和真实 `CombatWorld` 规则，不得增加强制清房、写 HP、暴露 `CombatWorld&` 或测试专用生产 API。
- `CombatLabConfig` 新字段必须追加在现有字段之后，默认配置继续保持 Stage 1 的向右出生与 90 tick 自动复活。
- 每个代码任务必须先观察对应 RED，再实现最小 GREEN，运行定向测试和受影响的完整 CTest，完成规格复审与代码质量复审后才进入下一任务。
- 每次事件队列溢出都必须计数；规则状态不得因事件溢出回滚。测试和 Debug 验收要求所有溢出计数为零。
- Release 继续静态链接 raylib 6.0.0，产物目录不得出现 `raylib*.dll`。

## Worktree Topology

| 角色 | 分支 | 工作树 | 范围 |
|---|---|---|---|
| 集成 | `milestone/m02-room-loop` | `E:\game\.worktrees\m02-room-loop` | 已批准设计、实施计划、任务合并与最终验收 |
| 规则 | `task/m02-dungeon-core` | `E:\game\.worktrees\m02-dungeon-core` | Task 1–4：Combat 配置、Dungeon 规则、无窗口测试、架构守卫 |
| Host | `task/m02-raylib-room-host` | `E:\game\.worktrees\m02-raylib-room-host` | Task 6：raylib Host、门/HUD/过渡与表现层测试 |

计划批准后才创建规则工作树：

```powershell
git -C E:\game\.worktrees\m02-room-loop status --short --branch
git -C E:\game\.worktrees\m02-room-loop check-ignore -v .worktrees
git -C E:\game\.worktrees\m02-room-loop worktree add E:\game\.worktrees\m02-dungeon-core -b task/m02-dungeon-core milestone/m02-room-loop
```

Task 5 把已复审规则分支合入集成分支后，才从新的集成 HEAD 创建 Host 工作树。Host 分支不得复制或重写 Dungeon 实现。

## Plan-Level Decisions Requiring Execution Approval

The approved design intentionally left two implementation-level values implicit. Approving execution of this plan also locks only these Stage 2 values:

- Top/bottom entry rooms use the already approved `CombatLabConfig` default facing right. Their targets reuse the Stage 1 X spacing `2.30/2.80/3.30` in a horizontal row at Y `-2.30` or `2.30`.
- `DungeonSessionConfig::initial_room_index` exists only to exercise the required `UINT64_MAX` production fault path. Stage 2 Host always passes zero; callers cannot inject a room seed, entry side, CombatWorld or cleared state, so this does not add save/resume behavior.

No other unresolved Stage 3 value is selected by this plan.

## Stable Contracts

### 生命周期与事件顺序

```text
locked -> combat -> cleared -> awaiting_exit -> transitioning -> locked(new room)
```

- 构造后已有活动房，状态为 `locked`，不发事件。
- Session tick 从 0 开始；一次 `tick()` 内的事件使用本次调用开始时的 tick，所有分支完成后恰好递增一次。`reset_current_room()` 不递增 tick。
- 首 tick 只进入 `combat`，依次发 `room_entered`、`combat_started`，不调用 `CombatWorld::tick()`。
- `combat` tick 先更新真实战斗，再统计 HP。第一次全部归零时同 tick 进入 `cleared`，依次发 `room_cleared`、`exits_opened`。
- 下一 tick 只执行 `cleared -> awaiting_exit`。
- `awaiting_exit` 先移动并取得钳制后位置，再检测门洞与向外输入。
- 成功提交依次记录方向、检查上限、派生唯一下一种子、销毁旧房、发 `exit_committed`、发 `room_destroyed`、进入 `transitioning`。
- 下一 tick 才构造新房并进入 `locked`；再下一 tick 才进入 `combat`。

### 方向、门洞与模板

```cpp
enum class ExitDirection : std::uint8_t {
    up = 0,
    down = 1,
    left = 2,
    right = 3,
    none = 0xFF,
};
```

- 左：`x == -8.0F && abs(y) <= 0.90F && movement.x < 0`。
- 右：`x == 8.0F && abs(y) <= 0.90F && movement.x > 0`。
- 上：`y == -3.5F && abs(x) <= 1.50F && movement.y < 0`。
- 下：`y == 3.5F && abs(x) <= 1.50F && movement.y > 0`。
- 出口与进入侧固定映射为 `up -> bottom`、`down -> top`、`left -> right`、`right -> left`。
- 初始房：玩家 `{0,0,0}` 向右，木桩沿用 Stage 1。
- 从左侧进入：玩家 `{-6.50,0,0}` 向右，木桩沿用 Stage 1 右半区排列。
- 从右侧进入：玩家 `{6.50,0,0}` 向左，木桩 X 镜像为 `-2.30/-2.80/-3.30`，Y 保持 `-0.35/0/0.35`。
- 从下侧进入：玩家 `{0,2.75,0}` 使用 `CombatLabConfig` 默认向右；木桩横排为 `{2.30,-2.30,0}`、`{2.80,-2.30,0}`、`{3.30,-2.30,0}`。
- 从上侧进入：玩家 `{0,-2.75,0}` 使用默认向右；木桩横排为 `{2.30,2.30,0}`、`{2.80,2.30,0}`、`{3.30,2.30,0}`。

上下入口按本计划的执行批准使用默认向右配置，并复用 Stage 1 的 X 横排间距；这不引入新的攻击方向或房间随机规则。

### 种子派生

初始种子和下一种子都是无状态纯函数。实现固定使用两个独立域标签，并连续混入完整 64 位房间序号与方向，禁止左移打包导致高位丢失：

```cpp
constexpr std::uint64_t kInitialRoomDomain = 0x524F4F4D5F494E49ULL;
constexpr std::uint64_t kNextRoomDomain = 0x524F4F4D5F4E4558ULL;

std::uint64_t derive_initial_room_seed(
    std::uint64_t root_seed,
    std::uint64_t room_index) noexcept {
    auto domain = core::DeterministicRng::derive_stream(
        root_seed, kInitialRoomDomain);
    auto indexed = core::DeterministicRng::derive_stream(
        domain.next_u64(), room_index);
    return indexed.next_u64();
}

std::uint64_t derive_next_room_seed(
    std::uint64_t current_seed,
    std::uint64_t next_index,
    ExitDirection exit) noexcept {
    auto domain = core::DeterministicRng::derive_stream(
        current_seed, kNextRoomDomain);
    auto indexed = core::DeterministicRng::derive_stream(
        domain.next_u64(), next_index);
    auto directed = core::DeterministicRng::derive_stream(
        indexed.next_u64(), static_cast<std::uint64_t>(exit));
    return directed.next_u64();
}
```

只有成功提交的方向调用 `derive_next_room_seed`；未选择的三门不调用、不缓存、不消耗状态。

## File Map

### Pure C++ rules

- `src/combat/combat_types.hpp`：追加房间可配置的初始朝向和自动复活开关。
- `src/combat/combat_world.cpp`：应用初始朝向。
- `src/combat/target_simulation.cpp`：按配置保留或禁用死亡复活。
- `src/dungeon/dungeon_types.hpp`：方向、进入侧、生命周期、事件、诊断、快照和配置。
- `src/dungeon/room_generation.hpp/.cpp`：纯种子派生、进入侧映射和固定出生模板。
- `src/dungeon/room_navigation.hpp/.cpp`：纯门洞请求检测。
- `src/dungeon/dungeon_session.hpp/.cpp`：唯一规则入口、状态机、事件转发和房间销毁/构造。

### Headless tests

- `tests/combat/combat_config_tests.cpp`：默认兼容、配置朝向和禁用复活。
- `tests/dungeon/room_generation_tests.cpp`：方向 ID、种子和五类出生模板。
- `tests/dungeon/dungeon_lifecycle_tests.cpp`：锁门、清房、事件、重置和 CombatEvent 转发。
- `tests/dungeon/dungeon_navigation_tests.cpp`：四门、提交、销毁、新房、非法请求和上限。
- `tests/dungeon/dungeon_stress_tests.cpp`：确定性、1000 房、零持续分配和单房常驻。
- `tests/platform/module_boundary_test.cmake`：`combat` 不得 include `dungeon` 的源码门禁。

### raylib presentation

- `src/platform/raylib/dungeon_view_math.hpp/.cpp`：插值门禁、门视觉状态、标签、清理路由和淡出曲线。
- `src/platform/raylib/raylib_host.cpp`：持有 `DungeonSession`，提交输入并排空两类事件。
- `src/platform/raylib/combat_renderer.hpp/.cpp`：改为绘制 DungeonSnapshot、四门、HUD 和过渡。
- `src/platform/raylib/combat_audio.hpp/.cpp`：房间销毁/重置时停止旧房正在播放的短音效。

---

### Task 1: Make CombatWorld Room-Configurable Without Changing Stage 1 Defaults

**Worktree:** `E:\game\.worktrees\m02-dungeon-core`

**Files:**
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/target_simulation.cpp`
- Create: `tests/combat/combat_config_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

- [ ] **Step 1: Add two RED cases and change the exact Combat count 30 -> 32**

Register `combat_config_suite()` and write these independent contracts:

```cpp
arpg::test::Failure configured_facing_survives_reset() noexcept {
    using namespace arpg::combat;
    CombatLabConfig config;
    config.initial_facing = Facing::left;
    CombatWorld world{config};
    ARPG_REQUIRE(world.snapshot().player.facing == Facing::left);
    world.tick(MovementInput{1, 0});
    world.reset();
    ARPG_REQUIRE(world.snapshot().player.facing == Facing::left);
    return {};
}
```

```cpp
arpg::test::Failure disabled_respawn_stays_defeated() noexcept {
    using namespace arpg::combat;
    CombatLabConfig config;
    config.dummy_spawns = {{{2.13F, 0.0F, 0.0F},
                            {6.0F, 2.0F, 0.0F},
                            {7.0F, 2.0F, 0.0F}}};
    config.respawn_defeated_dummies = false;
    CombatWorld world{config};
    for (int hit = 0; hit < 11; ++hit) {
        ARPG_REQUIRE(world.queue_action(Action::light));
        world.tick(MovementInput{});
        ARPG_REQUIRE(finish_attack(world, 80));
    }
    for (int tick = 0; tick < 120; ++tick) {
        world.tick(MovementInput{});
    }
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.dummies[0].hp == 0);
    ARPG_REQUIRE(snapshot.dummies[0].reaction == ReactionState::defeated);
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::respawned);
    }
    return {};
}
```

- [ ] **Step 2: Observe RED**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

Expected: compilation fails because the two `CombatLabConfig` fields do not exist; no existing test is edited to hide the failure.

- [ ] **Step 3: Append the two config fields and apply them**

```cpp
struct CombatLabConfig final {
    Vec3 player_spawn{0.0F, 0.0F, 0.0F};
    std::array<Vec3, kDummyCount> dummy_spawns{{
        {2.30F, -0.35F, 0.0F},
        {2.80F, 0.0F, 0.0F},
        {3.30F, 0.35F, 0.0F},
    }};
    Facing initial_facing{Facing::right};
    bool respawn_defeated_dummies{true};
};
```

In `initialize_runtime()` assign `player_.facing = config_.initial_facing`. In the `ReactionState::defeated` branch, return immediately while `respawn_defeated_dummies == false`; do not decrement the 90-tick timer and do not emit `respawned`.

- [ ] **Step 4: Run GREEN, compatibility gates, and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
git diff --check
git add src/combat tests/combat
git commit -m "feat: make combat room-configurable"
```

Expected: `32 cases, 0 failures`; existing default 90-tick respawn tests remain GREEN.

---

### Task 2: Establish Dungeon Types, Deterministic Room Generation, and Boundaries

**Worktree:** `E:\game\.worktrees\m02-dungeon-core`

**Files:**
- Create: `src/dungeon/CMakeLists.txt`
- Create: `src/dungeon/dungeon_types.hpp`
- Create: `src/dungeon/room_generation.hpp`
- Create: `src/dungeon/room_generation.cpp`
- Create: `tests/dungeon/CMakeLists.txt`
- Create: `tests/dungeon/dungeon_test_main.cpp`
- Create: `tests/dungeon/room_generation_tests.cpp`
- Create: `tests/platform/module_boundary_test.cmake`
- Modify: `CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`

- [ ] **Step 1: Add five RED generation cases**

The cases lock:

1. numeric direction IDs and opposite entry mapping;
2. initial descriptor with Stage 1 positions/default facing but Stage 2 respawn disabled;
3. left/right entry positions and facing;
4. top/bottom entry positions and horizontal rows;
5. deterministic seed equality, direction separation, high-bit room-index sensitivity, and max-index rejection.

`dungeon_test_main.cpp` registers only `room_generation_suite()` with exact count 5. Root CMake adds `src/dungeon` after Combat and `tests/dungeon` after Combat tests, so the first build fails on missing Dungeon headers/target.

- [ ] **Step 2: Observe configure/compiler RED**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

Expected: missing `arpg_dungeon` implementation or missing Dungeon symbols.

- [ ] **Step 3: Implement fixed public value types**

```cpp
enum class ExitDirection : std::uint8_t {
    up = 0, down = 1, left = 2, right = 3, none = 0xFF,
};
enum class EntrySide : std::uint8_t {
    initial, top, bottom, left, right,
};
enum class RoomPhase : std::uint8_t {
    locked, combat, cleared, awaiting_exit, transitioning,
};
enum class DungeonEventKind : std::uint8_t {
    room_entered, combat_started, room_cleared, exits_opened,
    exit_committed, room_destroyed, room_reset, faulted,
};

struct DungeonEvent final {
    DungeonEventKind kind{};
    std::uint64_t session_tick{};
    std::uint64_t room_index{};
    std::uint64_t room_seed{};
    ExitDirection direction{ExitDirection::none};
};

struct DungeonDiagnostics final {
    std::uint32_t event_overflow_count{};
    std::uint32_t combat_relay_overflow_count{};
    std::uint32_t rejected_exit_count{};
    bool room_index_overflow{};
};

struct DungeonSessionConfig final {
    std::uint64_t root_seed{0x6D30305F5241594CULL};
    std::uint64_t initial_room_index{};
};

struct RoomDescriptor final {
    std::uint64_t index{};
    std::uint64_t seed{};
    EntrySide entry{EntrySide::initial};
    combat::CombatLabConfig combat{};
};

struct DungeonSnapshot final {
    std::uint64_t session_tick{};
    std::uint64_t room_index{};
    std::uint64_t room_seed{};
    RoomPhase phase{RoomPhase::locked};
    bool has_active_room{};
    std::array<bool, 4> exits_open{};
    std::uint8_t remaining_targets{};
    EntrySide entry_side{EntrySide::initial};
    ExitDirection last_exit{ExitDirection::none};
    std::optional<combat::CombatSnapshot> combat{};
    DungeonDiagnostics diagnostics{};
};
```

`initial_room_index` is the narrow production initialization parameter needed to prove overflow behavior without adding save/load, arbitrary seed/entry injection or a test-only mutator. `make_initial_room` always uses `EntrySide::initial`; Host always supplies index zero in Stage 2.

- [ ] **Step 4: Implement pure seed and room-template functions**

Expose exactly:

```cpp
[[nodiscard]] EntrySide entry_side_for_exit(ExitDirection exit) noexcept;
[[nodiscard]] std::uint64_t derive_initial_room_seed(
    std::uint64_t root_seed, std::uint64_t room_index) noexcept;
[[nodiscard]] std::uint64_t derive_next_room_seed(
    std::uint64_t current_seed,
    std::uint64_t next_index,
    ExitDirection exit) noexcept;
[[nodiscard]] RoomDescriptor make_initial_room(
    const DungeonSessionConfig& config) noexcept;
[[nodiscard]] std::optional<RoomDescriptor> make_next_room(
    const RoomDescriptor& current,
    ExitDirection exit) noexcept;
```

`make_initial_room` and `make_next_room` both set `respawn_defeated_dummies=false`. `make_next_room` returns `nullopt` for `none` or `current.index == UINT64_MAX`, calls `derive_next_room_seed` exactly once after validating input, and applies the fixed template from Stable Contracts.

- [ ] **Step 5: Wire target and both architecture directions**

```cmake
add_library(arpg_dungeon STATIC
    room_generation.cpp)
target_include_directories(arpg_dungeon PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_dungeon PUBLIC cxx_std_17)
target_link_libraries(arpg_dungeon PUBLIC arpg_combat PRIVATE arpg_core)
arpg_enable_project_warnings(arpg_dungeon)
```

Add `arpg_assert_target_dependency_boundary(arpg_dungeon)` and `architecture.dungeon_no_raylib`. Add a recursive `arpg_assert_target_not_reachable(arpg_combat arpg_dungeon)` configure guard using the existing target-candidate extraction helper, plus `architecture.combat_no_dungeon` source scanning through `module_boundary_test.cmake`. The source scanner matches real `#include <dungeon/dungeon_types.hpp>` and `#include "dungeon/dungeon_session.hpp"` directives and self-tests real includes, comments and strings.

- [ ] **Step 6: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
ctest --test-dir .\out\build\windows-msvc-core-debug -N
git diff --check
git add CMakeLists.txt src/dungeon tests/dungeon tests/platform
git commit -m "feat: add deterministic dungeon room descriptors"
```

Expected: Dungeon `5 cases, 0 failures`; core-only lists 7 CTests after both new architecture gates are registered.

---

### Task 3: Implement DungeonSession Lifecycle, Event Relay, and Reset

**Worktree:** `E:\game\.worktrees\m02-dungeon-core`

**Files:**
- Create: `src/dungeon/dungeon_session.hpp`
- Create: `src/dungeon/dungeon_session.cpp`
- Create: `tests/dungeon/dungeon_test_support.hpp`
- Create: `tests/dungeon/dungeon_lifecycle_tests.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

- [ ] **Step 1: Add five lifecycle RED cases and change Dungeon count 5 -> 10**

Cases:

1. construction snapshot is `locked`; first tick emits exactly entered/started and leaves combat tick zero;
2. four doors stay closed before clear and pre-clear outward contact cannot transition;
3. real attacks defeat all targets; clear/open events occur once and targets never respawn;
4. reset preserves index/seed/entry, reconstructs at combat tick zero, clears transient queues, and emits exactly one `room_reset`;
5. CombatEvents relay in source order and relay diagnostics stay zero.

The test helper may only inspect snapshots, queue public actions, submit movement, tick and drain public event pop APIs. A clearing driver uses the nearest living target and queues `Action::light` only when the player has no active attack:

```cpp
bool drive_until_cleared(
    DungeonSession& session,
    EventSummary& summary,
    int max_ticks = 4096) noexcept {
    for (int tick = 0; tick < max_ticks; ++tick) {
        const DungeonSnapshot state = session.snapshot();
        if (state.phase == RoomPhase::cleared
                || state.phase == RoomPhase::awaiting_exit) {
            return true;
        }
        combat::MovementInput movement{};
        if (state.combat.has_value() && state.phase == RoomPhase::combat) {
            const auto& combat_state = *state.combat;
            const combat::DummySnapshot* target =
                nearest_living_dummy(combat_state);
            if (target != nullptr) {
                movement = movement_toward(
                    combat_state.player.position, target->position);
                if (in_light_attack_lane(combat_state.player, *target)
                        && combat_state.player.active_attack
                            == combat::AttackId::none) {
                    static_cast<void>(
                        session.queue_action(combat::Action::light));
                }
            }
        }
        session.tick(movement);
        drain_all_events(session, summary);
    }
    return false;
}
```

`EventSummary` contains only fixed counters and a fixed event-kind array, so the helper preserves the evidence needed to assert exactly one clear/open event while still draining every tick. `nearest_living_dummy` scans the fixed three slots without sorting, `movement_toward` uses fixed X/Y tolerances, and no helper allocates. Failure to clear within 4096 ticks fails the case.

- [ ] **Step 2: Observe missing-session RED**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

- [ ] **Step 3: Implement the final public Session API**

```cpp
class DungeonSession final {
public:
    static constexpr std::size_t kDungeonEventCapacity = 32;
    static constexpr std::size_t kCombatRelayCapacity = 64;

    explicit DungeonSession(DungeonSessionConfig config = {}) noexcept;
    [[nodiscard]] bool queue_action(combat::Action action) noexcept;
    void tick(combat::MovementInput movement) noexcept;
    void reset_current_room() noexcept;
    [[nodiscard]] DungeonSnapshot snapshot() const noexcept;
    [[nodiscard]] std::optional<DungeonEvent> try_pop_event() noexcept;
    [[nodiscard]] std::optional<combat::CombatEvent>
    try_pop_combat_event() noexcept;

private:
    void construct_current_room() noexcept;
    void relay_combat_events() noexcept;
    void attempt_exit(ExitDirection direction) noexcept;
    void emit(DungeonEventKind kind,
              ExitDirection direction = ExitDirection::none) noexcept;
    [[nodiscard]] std::uint8_t remaining_targets() const noexcept;

    DungeonSessionConfig config_{};
    RoomDescriptor current_room_{};
    std::optional<RoomDescriptor> pending_room_{};
    std::optional<combat::CombatWorld> combat_{};
    core::BoundedQueue<DungeonEvent, kDungeonEventCapacity> events_{};
    core::BoundedQueue<combat::CombatEvent, kCombatRelayCapacity>
        combat_events_{};
    RoomPhase phase_{RoomPhase::locked};
    ExitDirection last_exit_{ExitDirection::none};
    std::uint64_t session_tick_{};
    DungeonDiagnostics diagnostics_{};
    bool overflow_fault_emitted_{};
};
```

Combat relay is required because successful exit destroys the old `CombatWorld` before Host drains events. After every real combat tick, `relay_combat_events()` drains the world queue into the fixed relay before any possible destruction. Relay overflow increments `combat_relay_overflow_count`; it never allocates.

`emit()` and `relay_combat_events()` first complete the rule transition, then try the bounded push. A failed push saturates the matching diagnostic counter and triggers `assert` in Debug; Release retains the completed rule state and diagnostic without rollback.

- [ ] **Step 4: Implement locked/combat/cleared/reset behavior**

For this task, `awaiting_exit` continues movement but has no commit logic until Task 4. `queue_action` forwards only while `phase == combat` and an active room exists. `snapshot()` sets all four `exits_open` entries from `phase == cleared || phase == awaiting_exit`, counts HP-positive targets, and includes a combat snapshot only while the optional exists.

`reset_current_room()` performs this exact order:

1. drain Dungeon and combat relay queues;
2. `combat_.reset()` and clear pending descriptor;
3. preserve session tick, current descriptor, last committed exit and accumulated diagnostics;
4. reconstruct the same descriptor with `emplace(current_room_.combat)`;
5. set phase `locked` without changing index, seed or entry;
6. emit exactly one `room_reset`.

If `reset_current_room()` is called during `transitioning`, it is a no-op because no current active room exists; it must not resurrect the permanently destroyed old room or skip the already committed next room.

- [ ] **Step 5: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
git diff --check
git add src/dungeon tests/dungeon
git commit -m "feat: add dungeon room lifecycle"
```

Expected: Dungeon `10 cases, 0 failures`; Combat remains `32 cases, 0 failures`.

---

### Task 4: Add Physical Exits, One-Way Transitions, Diagnostics, and 1000-Room Stress

**Worktree:** `E:\game\.worktrees\m02-dungeon-core`

**Files:**
- Create: `src/dungeon/room_navigation.hpp`
- Create: `src/dungeon/room_navigation.cpp`
- Create: `tests/dungeon/dungeon_navigation_tests.cpp`
- Create: `tests/dungeon/dungeon_stress_tests.cpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

- [ ] **Step 1: Add six navigation RED cases and change Dungeon count 10 -> 16**

Cases:

1. four exact door apertures accept their outward input;
2. wrong direction, wrong boundary and outside-aperture positions reject;
3. the first exit emits one commit/destroy pair and immediately removes active combat;
4. transition tick creates one new locked room, following tick starts combat, with correct entry template;
5. closed-door requests, held-input duplicate attempts and reverse-door use cannot create duplicate/old rooms;
6. a session initialized at `UINT64_MAX` stays awaiting, sets fault, emits one `faulted`, and never derives/destroys/wraps.

- [ ] **Step 2: Add four stress RED cases and change Dungeon count 16 -> 20**

Cases:

1. identical root seed and route yield field-by-field equal room, combat and event summaries;
2. changing one committed direction changes the next seed/template without deriving other candidates;
3. 1000 real rooms keep exactly zero-or-one active world, transition snapshots have no combat, new combat tick starts at zero, and final index is 1000;
4. after an independent warm-up session, a newly constructed measured session completes 1000 exits with zero global-allocation delta and zero Dungeon, relay and Combat event overflows.

Every driver tick drains both event queues. Route is the fixed array `{up, right, down, left}` repeated 250 times; door approach first aligns with the aperture center and then holds the corresponding outward input.

- [ ] **Step 3: Observe RED**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

- [ ] **Step 4: Implement pure physical-door detection**

```cpp
[[nodiscard]] std::optional<ExitDirection> requested_exit(
    combat::Vec3 position,
    combat::MovementInput movement) noexcept;
```

Use only the exact constants in Stable Contracts. Check left, right, up, down in stable direction-ID order only after the corresponding boundary/aperture condition is true. Positions come from the post-movement `CombatSnapshot`, never renderer projection.

- [ ] **Step 5: Complete transition and fault logic**

In `awaiting_exit`, tick and relay Combat first, then call `requested_exit`. On a valid request, call `attempt_exit`; the following return exits only that helper, so the outer `tick()` still performs its single session-tick increment:

```cpp
if (current_room_.index == std::numeric_limits<std::uint64_t>::max()) {
    diagnostics_.room_index_overflow = true;
    if (!overflow_fault_emitted_) {
        emit(DungeonEventKind::faulted, requested);
        overflow_fault_emitted_ = true;
    }
    return;
}

pending_room_ = make_next_room(current_room_, requested);
last_exit_ = requested;
combat_.reset();
emit(DungeonEventKind::exit_committed, requested);
emit(DungeonEventKind::room_destroyed, requested);
phase_ = RoomPhase::transitioning;
```

At the next `transitioning` tick, move the already committed descriptor from `pending_room_` by field assignment, clear it, `emplace(current_room_.combat)`, and set `locked`; do not call Combat that tick. Because `RoomDescriptor` is movable/copyable but `CombatWorld` is not, only descriptor values move.

For the allocation case, first drive a separate warm-up session through one transition and destroy it. Then construct the measured session and all fixed test summaries, drain construction events, record `allocation_count()`, complete exactly 1000 real exits, and compare the counter. The measured session must finish at room index 1000.

Closed-door contact in `locked/combat` and outside-aperture movement are ignored without incrementing diagnostics, matching the approved physical-door rules. `rejected_exit_count` is a defensive saturating counter inside `attempt_exit` for an invalid phase, missing active room, `none` direction or an already populated pending descriptor; the production tick only calls this helper after a valid `awaiting_exit` door request. Held input through a committed transition cannot commit again because there is no active room. Saturate diagnostic counters at `UINT32_MAX` rather than wrap.

- [ ] **Step 6: Run targeted GREEN and stress executable**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
ctest --test-dir .\out\build\windows-msvc-core-debug `
  -R "^(dungeon\.units|architecture\.(dungeon_no_raylib|combat_no_dungeon))$" `
  --output-on-failure
git diff --check
```

Expected: Dungeon `20 cases, 0 failures`, including 1000 rooms with zero allocation delta and zero overflow.

- [ ] **Step 7: Commit**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: complete deterministic dungeon transitions"
```

---

### Task 5: Review and Merge the Pure-C++ Rules

**Integration worktree:** `E:\game\.worktrees\m02-room-loop`

- [ ] **Step 1: Run rule-branch gates from a clean tree**

```powershell
git -C E:\game\.worktrees\m02-dungeon-core status --short --branch
E:\game\.worktrees\m02-dungeon-core\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
E:\game\.worktrees\m02-dungeon-core\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
E:\game\.worktrees\m02-dungeon-core\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
```

- [ ] **Step 2: Perform two review gates**

Specification review checks all 14 headless requirements, exact event order, max-index check before `+1`, real-combat clearing, no candidate-room generation and main untouched. Code-quality review checks fixed storage, relay drain ordering, no `memcmp`, no `CombatWorld` move/assignment, no Dungeon include/link in Combat, and no raylib in Dungeon.

Fix every Critical or Important issue with a new RED regression and rerun Step 1. Minor issues are either fixed or recorded with a concrete reason.

- [ ] **Step 3: Merge reviewed rules into M02 integration**

```powershell
git -C E:\game\.worktrees\m02-room-loop merge --no-ff task/m02-dungeon-core -m "merge: integrate stage 2 dungeon rules"
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

- [ ] **Step 4: Create Host worktree from the merged integration HEAD**

```powershell
git -C E:\game\.worktrees\m02-room-loop worktree add E:\game\.worktrees\m02-raylib-room-host -b task/m02-raylib-room-host milestone/m02-room-loop
```

---

### Task 6: Connect DungeonSession to raylib and Render the Room Loop

**Worktree:** `E:\game\.worktrees\m02-raylib-room-host`

**Files:**
- Create: `src/platform/raylib/dungeon_view_math.hpp`
- Create: `src/platform/raylib/dungeon_view_math.cpp`
- Create: `tests/platform/dungeon_view_math_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/combat_audio.hpp`
- Modify: `src/platform/raylib/combat_audio.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

- [ ] **Step 1: Add five presentation-math RED cases and change Platform count 7 -> 12**

Expose and test:

```cpp
enum class DoorVisualMode : std::uint8_t { closed, open, hidden };

[[nodiscard]] DoorVisualMode door_visual_mode(
    dungeon::RoomPhase phase, bool has_active_room) noexcept;
[[nodiscard]] bool can_interpolate_room(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current) noexcept;
[[nodiscard]] bool dungeon_event_clears_transients(
    dungeon::DungeonEventKind kind) noexcept;
[[nodiscard]] const char* room_phase_label(
    dungeon::RoomPhase phase) noexcept;
[[nodiscard]] const char* exit_direction_label(
    dungeon::ExitDirection direction) noexcept;
[[nodiscard]] float transition_overlay_alpha(float seconds_left) noexcept;
```

The five cases cover door modes, same-room interpolation, cross/no-room rejection, room-destroy/reset cleanup routing plus labels, and fade-alpha clamping.

- [ ] **Step 2: Observe RED**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
```

- [ ] **Step 3: Implement pure view decisions and link Dungeon**

Add `dungeon_view_math.cpp` to `arpg_raylib` and link:

```cmake
target_link_libraries(arpg_raylib
    PUBLIC arpg_dungeon raylib
    PRIVATE arpg_core)
```

`can_interpolate_room` returns true only when both snapshots have active combat, equal room index and equal room seed. Closed modes are `locked/combat`; open modes are `cleared/awaiting_exit`; `transitioning` or no active room is hidden. Cleanup is true only for `room_destroyed` and `room_reset`.

- [ ] **Step 4: Make Host own DungeonSession and drain events safely**

Replace the direct `CombatWorld` with:

```cpp
dungeon::DungeonSession session{
    dungeon::DungeonSessionConfig{config.root_seed, 0}};
dungeon::DungeonSnapshot current = session.snapshot();
dungeon::DungeonSnapshot previous = current;
```

`submit_frame_actions` accepts `DungeonSession&`. `R` calls `reset_current_room()`, immediately refreshes both snapshots from the session, and drains the reset event before drawing. Each fixed step does `previous=current`, `session.tick(movement)`, `current=session.snapshot()`, then drains all CombatEvents before DungeonEvents:

```cpp
while (const auto event = session.try_pop_combat_event()) {
    renderer.consume_event(*event);
    feedback.consume(*event);
    audio.consume_event(*event);
}
while (const auto event = session.try_pop_event()) {
    renderer.consume_dungeon_event(*event);
    if (dungeon_event_clears_transients(event->kind)) {
        renderer.clear_combat_transients();
        feedback.clear();
        audio.stop_all();
    }
}
```

At frame time call `renderer.update(frame_seconds)`. Change title to `Infinite Dungeon - Stage 2 Room Loop` and F12 target to `room-loop.png`.

- [ ] **Step 5: Render doors, correct actors, HUD, and transition**

Change Renderer draw input to two `DungeonSnapshot` values. If `can_interpolate_room` is false but current combat exists, pass the current combat snapshot as both previous/current actor endpoints. If current combat is absent, draw no actors and no old CombatSnapshot.

Use fixed world door centers:

```cpp
constexpr std::array<combat::Vec3, 4> kDoorCenters{{
    {0.0F, -3.5F, 0.0F},
    {0.0F, 3.5F, 0.0F},
    {-8.0F, 0.0F, 0.0F},
    {8.0F, 0.0F, 0.0F},
}};
```

Project them with existing `project_combat_position`. Closed doors use dark-gray frames and opaque red barriers. Open doors use neutral cyan-white frames, no barrier, and `^/v/</>` arrows. Do not use element names or element colors.

HUD shows a one-based room ordinal, phase label, remaining targets, `Doors: LOCKED/OPEN` and last exit; the ordinal uses a max-value guard before adding one. F1 additionally shows full 16-digit hexadecimal room seed and all diagnostics. `transitioning` and `room_destroyed` start a 0.12-second black overlay; old actors/effects/audio are cleared before drawing.

Add `CombatAudio::stop_all()` that checks `ready_`, calls `StopSound` for weapon, three material voices and low cue, then resets `material_voice_` without unloading sounds.

- [ ] **Step 6: Run GREEN and visible smoke**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
$game = Start-Process -FilePath .\out\build\windows-msvc-debug\bin\arpg_game.exe -WindowStyle Normal -PassThru
```

Expected: Platform `12 cases, 0 failures`; visible first room has four closed doors, one player and three dummies. Manually clear four successive rooms and choose up/right/down/left once each, verifying every entry template and the absence of old actors. Close with Esc and confirm the process exits.

- [ ] **Step 7: Commit**

```powershell
git diff --check
git add src/platform tests/platform
git commit -m "feat: connect stage 2 room loop to raylib"
```

---

### Task 7: Merge, Full Verification, 20-Room Acceptance, and Handoff

**Integration worktree:** `E:\game\.worktrees\m02-room-loop`

- [ ] **Step 1: Review and merge Host**

Run specification and code-quality reviews over the Host branch. Resolve Critical/Important findings with a RED test. Then:

```powershell
git -C E:\game\.worktrees\m02-room-loop merge --no-ff task/m02-raylib-room-host -m "merge: integrate stage 2 raylib room host"
```

- [ ] **Step 2: Run all Fresh gates**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
ctest --test-dir .\out\build\windows-msvc-core-debug -N
ctest --test-dir .\out\build\windows-msvc-debug -N
ctest --test-dir .\out\build\windows-msvc-release -N
```

Core-only lists exactly 7 CTests: `core.units`、`combat.units`、`dungeon.units`、three raylib architecture boundaries and `architecture.combat_no_dungeon`. Full builds add `platform.view_math` for exactly 8. All pass.

- [ ] **Step 3: Run direct executables**

```powershell
.\out\build\windows-msvc-debug\bin\arpg_core_tests.exe
.\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
.\out\build\windows-msvc-debug\bin\arpg_dungeon_tests.exe
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
.\out\build\windows-msvc-release\bin\arpg_core_tests.exe
.\out\build\windows-msvc-release\bin\arpg_combat_tests.exe
.\out\build\windows-msvc-release\bin\arpg_dungeon_tests.exe
.\out\build\windows-msvc-release\bin\arpg_platform_tests.exe
```

Expected case counts: Core `22/0`, Combat `32/0`, Dungeon `20/0`, Platform `12/0` in both configurations.

- [ ] **Step 4: Verify toolchain, static raylib, dependency boundaries, and plan hygiene**

```powershell
Select-String -Path .\out\build\windows-msvc-release\CMakeCache.txt -Pattern 'CMAKE_CXX_COMPILER:FILEPATH|CMAKE_BUILD_TYPE:STRING|BUILD_SHARED_LIBS:BOOL'
Get-Item .\out\build\windows-msvc-release\bin\arpg_game.exe
Get-ChildItem .\out\build\windows-msvc-release\bin -Filter 'raylib*.dll'
rg -n -i '#\s*include\s*[<"](raylib|raymath|rlgl|raylib-cpp)' src/core src/combat src/dungeon
rg -n '#\s*include\s*[<"]dungeon/' src/combat
git diff --check
git status --short --branch
```

Expected: MSVC 19.44 x64 / SDK 10.0.26100.0 already enforced by configure; Release EXE exists; raylib DLL count zero; both source scans return no matches; tree clean.

- [ ] **Step 5: Run visible 20-room matrix and capture evidence**

Start Release normally. Verify:

1. first room title/HUD/three targets/four closed doors;
2. J/K/L and WASD remain Stage 1 behavior;
3. all targets defeated exactly once and all four doors open together;
4. walking outward submits without interaction key;
5. each entry side uses the plan-locked position/facing/template;
6. old actors, sparks, numbers and audio do not survive transition;
7. room number increments exactly once, reverse door never returns;
8. `R` keeps room index and seed but restores locked room;
9. F1 seed/diagnostics visible and all overflow counters zero;
10. continue through at least 20 rooms, press F12 to create `room-loop.png`.

- [ ] **Step 6: Verify both exit paths and no orphan process**

Run once with Esc and once with the window close button. After each:

```powershell
@(Get-Process arpg_game -ErrorAction SilentlyContinue).Count
```

Expected: `0`.

- [ ] **Step 7: Final cross-review and handoff**

Review every commit after `18a729a`. Re-run Steps 2–6 after any code fix. Confirm:

```powershell
git -C E:\game rev-parse main
git rev-parse milestone/m02-room-loop
git log --oneline --decorate --graph --max-count 20
```

Expected: `main` remains `79547fbe0a00cb4b1ba372d09f378a5695f69443`; M02 is clean and contains both reviewed merges. Keep all worktrees. Do not merge main, push, create PR or start Stage 3 until the user chooses the integration action.

## Plan Self-Review Checklist

- [ ] Every section 1–13 of the approved design maps to a task and an executable assertion.
- [ ] Public names are consistent: `ExitDirection`、`EntrySide`、`RoomPhase`、`RoomDescriptor`、`DungeonSession`、`DungeonSnapshot`。
- [ ] Direction IDs, event order, door coordinates, spawn coordinates, seed domains and capacities have one exact definition.
- [ ] Combat remains independent of Dungeon and Dungeon remains independent of raylib in both target graph and source scans.
- [ ] 1000-room tests use only public production behavior and drain events every tick.
- [ ] No `CombatWorld` copy/move/assignment, no `memcmp`, no room history, no four-candidate seed generation, no test-only force-clear path.
- [ ] Build `$forbidden = @(('TO'+'DO'), ('TB'+'D'), ('待'+'定'), ('占'+'位'))` and run `Select-String` for every token against this plan; expected no matches.
- [ ] Run `git diff --check`; expected no output.
