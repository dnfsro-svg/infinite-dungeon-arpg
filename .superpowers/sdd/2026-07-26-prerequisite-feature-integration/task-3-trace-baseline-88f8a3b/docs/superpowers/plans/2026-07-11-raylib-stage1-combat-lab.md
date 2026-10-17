# raylib Stage 1 DNF 式战斗实验室实施计划

> 状态说明：本计划记录最初六攻击 Stage 1 的历史实施过程。当前键位与攻击目录已由 `2026-07-11-launcher-key-remap.md` 和 `../specs/2026-07-11-launcher-key-remap-design.md` 覆盖：重击已删除，L 为上挑，U 无绑定。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Stage 0 基线上交付一个可见、可操作、可无窗口验证的 DNF 式单房间战斗实验室，包含移动、跳跃、三段普攻、重击、上挑、单次空中攻击、命中停顿、硬直、击退、浮空、倒地、霸体、破韧和三种被动木桩。

**Architecture:** 战斗规则位于不依赖 raylib 的 `arpg_combat` 静态库，以固定 60 Hz 接收方向状态和有界动作缓存，输出只读快照与有界事件。raylib Host 只做输入映射、快照插值、灰盒绘制、固定容量视觉反馈和程序化音频；Core 与表现层分别在任务工作树开发，复审后合入 `milestone/m01-combat-lab`。

**Tech Stack:** C++17、raylib 6.0.0（固定提交 `dbc56a87da87d973a9c5baa4e7438a9d20121d28`，静态链接）、CMake 3.25+、Ninja、CTest、MSVC 19.44 x64、Windows SDK 10.0.26100.0。

## Global Constraints

- 生产代码基线必须保持与设计提交 `bacc15a1cd2488a657b8d5f3695b4c29b10b32ad` 一致；该提交的父提交是已验收 Stage 0 `b4f4f89d2ca90592852b0fe6d14b4ab66dd162a9`。任务工作树从 `milestone/m01-combat-lab` 当前已提交文档的 HEAD 创建，以同时携带设计和本计划；后续文档提交不得预先修改生产代码。
- `src/core/` 与 `src/combat/` 不得 include、链接或通过编译/链接选项间接依赖 raylib、raymath、rlgl 或 raylib-cpp。
- 规则更新固定为 60 Hz；渲染帧率不得改变动作帧、轨迹、碰撞、输入消费或结果。
- Combat Update 热路径不得调用堆分配；对象、输入、命中记录和事件容量全部固定。
- 游戏规则事件不得静默丢失；纯装饰事件容量不足时可以计数后丢弃。
- 每个实现任务必须先观察 RED，再做最小 GREEN，运行对应完整测试，完成规格复审与代码质量复审后才能提交下一任务。
- Debug 与 Release 必须保持 `/W4 /permissive-`，raylib 必须保持静态链接且运行目录无 raylib DLL。
- 不实现敌人 AI、敌人攻击、玩家受伤、房间循环、经验、装备、掉落、存档或任何 Stage 2 内容。
- Stage 1 完成后保留 `milestone/m01-combat-lab` 工作树，不合并 `main`，不创建 Stage 2 分支。

## Worktree Topology

| 阶段 | 分支 | 工作树 | 内容 |
|---|---|---|---|
| 集成 | `milestone/m01-combat-lab` | `E:\game\.worktrees\m01-combat-lab` | 设计、计划、合并与最终验收 |
| Combat | `task/m01-combat-core` | `E:\game\.worktrees\m01-combat-core` | Task 1–7，纯 C++ 规则与无窗口测试 |
| Host | `task/m01-raylib-combat-host` | `E:\game\.worktrees\m01-raylib-combat-host` | Task 8–9，基于已合入 Combat API 的表现层 |

执行开始时创建 Combat 工作树：

```powershell
git check-ignore -v .worktrees
git worktree add .worktrees/m01-combat-core -b task/m01-combat-core milestone/m01-combat-lab
```

Task 7 完成并复审后，先把 `task/m01-combat-core` 以 `--no-ff` 合入集成分支，再从新的集成 HEAD 创建 Host 工作树。不得让 Host 分支复制或重写 Combat 实现。

## File Map

### Pure C++ combat

- `src/combat/CMakeLists.txt`：`arpg_combat` 静态库定义。
- `src/combat/combat_types.hpp`：稳定枚举、Vec3/AABB、输入、快照与事件值类型。
- `src/combat/attack_catalog.hpp/.cpp`：六种攻击的唯一数据表和查询/验证函数。
- `src/combat/input_buffer.hpp/.cpp`：32 槽、8 tick 的动作边沿缓存。
- `src/combat/combat_collision.hpp/.cpp`：AABB、朝向镜像和轻微位置修正。
- `src/combat/combat_world.hpp/.cpp`：公共 API、固定对象、tick 编排、快照与事件。
- `src/combat/player_simulation.cpp`：移动、跳跃、攻击时钟和连段。
- `src/combat/target_simulation.cpp`：木桩受击运动、倒地、起身、破韧和复位。
- `src/combat/hit_resolution.cpp`：固定顺序命中收集、结算和反馈聚合。

### Headless tests

- `tests/combat/CMakeLists.txt`：`arpg_combat_tests` 与 `combat.units`。
- `tests/combat/combat_test_main.cpp`：套件注册与精确用例数门禁。
- `tests/combat/attack_catalog_tests.cpp`：攻击表与帧段。
- `tests/combat/input_buffer_tests.cpp`：缓存、过期、暂停与溢出。
- `tests/combat/movement_jump_tests.cpp`：边界、朝向、跳跃与落地。
- `tests/combat/attack_state_tests.cpp`：动作状态、连段和空中攻击。
- `tests/combat/hit_resolution_tests.cpp`：X/Y/Z、单次命中与聚合。
- `tests/combat/dummy_reaction_tests.cpp`：硬直、击退、浮空、倒地和复位。
- `tests/combat/break_stress_tests.cpp`：霸体、破韧、确定性和 36,000 tick 无分配压力。

### raylib presentation

- `src/platform/raylib/raylib_host.cpp`：窗口、固定步循环、输入映射和事件分发。
- `src/platform/raylib/combat_view_math.hpp/.cpp`：无窗口可测的投影与固定数组排序。
- `src/platform/raylib/combat_renderer.hpp/.cpp`：投影、稳定排序、演员、HUD 与 F1 调试盒。
- `src/platform/raylib/combat_feedback.hpp/.cpp`：256 槽视觉池和三级震屏。
- `src/platform/raylib/combat_audio.hpp/.cpp`：启动期程序化生成三层短音效。

## Canonical Fixed-Tick Order

每个 `CombatWorld::tick` 必须按同一顺序执行：

1. `reset()` 只在 fixed tick 之间同步执行；它先清除旧瞬时状态，再让下一 tick 从稳定初始状态开始。
2. 递减演员 Hit Stop；被冻结演员跳过自己的状态、运动和碰撞，输入寿命在玩家 Hit Stop 期间暂停。
3. 未冻结玩家按 K→L→U→J 固定优先级消费当前允许动作，再更新移动、攻击时钟和攻击位移。
4. 未冻结木桩更新击退、Z 速度、重力和本地状态计时。
5. 处理落地、倒地、起身、破韧窗口与复位转换。
6. 为有效攻击构造一次世界攻击盒，按木桩索引 0→1→2 收集命中。
7. 先收集全部命中，再逐目标应用伤害、破韧和反应，避免遍历顺序影响结果。
8. 聚合攻击者 Hit Stop、震屏等级与单次低频请求，写入容量 64 的规则事件队列。
9. 固定 tick 加一并生成当前快照；Host 在每个 fixed tick 后立即排空事件。

---

### Task 1: Combat Target, Stable Types, Attack Catalog, and Architecture Gate

**Worktree:** `E:\game\.worktrees\m01-combat-core`

**Files:**
- Create: `src/combat/CMakeLists.txt`
- Create: `src/combat/combat_types.hpp`
- Create: `src/combat/attack_catalog.hpp`
- Create: `src/combat/attack_catalog.cpp`
- Create: `tests/combat/CMakeLists.txt`
- Create: `tests/combat/combat_test_main.cpp`
- Create: `tests/combat/attack_catalog_tests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/core_boundary_test.cmake`

**Interfaces:**
- Consumes: `arpg_core` and `tests/core/test_framework.hpp`.
- Produces: `Vec3`、`Aabb`、六个 AttackId、帧段/反馈枚举、`AttackDefinition`、`find_attack_definition`、`attack_phase_at`、`validate_attack_catalog` and `arpg_combat`.

- [ ] **Step 1: Write the first RED catalog suite**

Create three cases that include the missing `combat/attack_catalog.hpp` and assert:

```cpp
using namespace arpg::combat;
ARPG_REQUIRE(kAttackCount == 6);
ARPG_REQUIRE(find_attack_definition(AttackId::j1) != nullptr);
ARPG_REQUIRE(find_attack_definition(AttackId::air_j) != nullptr);
ARPG_REQUIRE(find_attack_definition(AttackId::none) == nullptr);
```

```cpp
const AttackDefinition& j1 = *find_attack_definition(AttackId::j1);
ARPG_REQUIRE(j1.startup_ticks == 5);
ARPG_REQUIRE(j1.active_ticks == 3);
ARPG_REQUIRE(j1.recovery_ticks == 9);
ARPG_REQUIRE(j1.damage == 28);
ARPG_REQUIRE(j1.break_damage == 10);
```

```cpp
ARPG_REQUIRE(attack_phase_at(j1, 4) == AttackPhase::startup);
ARPG_REQUIRE(attack_phase_at(j1, 5) == AttackPhase::active);
ARPG_REQUIRE(attack_phase_at(j1, 8) == AttackPhase::recovery);
ARPG_REQUIRE(attack_phase_at(j1, 17) == AttackPhase::finished);
ARPG_REQUIRE(validate_attack_catalog());
```

Register only this suite with exact case count 3. Add the combat test subdirectory so the missing header produces a compiler RED.

- [ ] **Step 2: Observe RED**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

Expected: missing combat target/header; existing 22 Core cases remain untouched.

- [ ] **Step 3: Implement the stable value types and six-row table**

Use these public contracts:

```cpp
struct Vec3 final { float x{}; float y{}; float z{}; };
struct Aabb final { Vec3 minimum{}; Vec3 maximum{}; };
enum class AttackId : std::uint8_t {
    j1 = 0, j2, j3, heavy, launcher, air_j, none = 0xFF
};
enum class AttackPhase : std::uint8_t {
    startup, active, recovery, finished
};
enum class FeedbackLevel : std::uint8_t { light, medium, heavy };
enum class ImpactKind : std::uint8_t {
    light_hitstun, medium_hitstun, knockdown, launch
};
struct AttackDefinition final {
    AttackId id{AttackId::none};
    std::uint16_t startup_ticks{};
    std::uint16_t active_ticks{};
    std::uint16_t recovery_ticks{};
    int damage{};
    int break_damage{};
    ImpactKind impact{ImpactKind::light_hitstun};
    Aabb local_hitbox{};
    float lunge_distance{};
    float knockback_speed{};
    float launch_speed{};
    FeedbackLevel feedback{FeedbackLevel::light};
};
```

The private `constexpr std::array` rows are:

```cpp
{AttackId::j1,       5, 3,  9, 28, 10, ImpactKind::light_hitstun,
 {{0.20F,-0.65F, 0.10F},{1.50F,0.65F,1.50F}},0.10F,0.0F,0.0F,FeedbackLevel::light},
{AttackId::j2,       6, 3, 10, 34, 12, ImpactKind::medium_hitstun,
 {{0.20F,-0.65F, 0.10F},{1.65F,0.65F,1.55F}},0.14F,2.2F,0.0F,FeedbackLevel::medium},
{AttackId::j3,       8, 4, 16, 52, 20, ImpactKind::knockdown,
 {{0.15F,-0.70F, 0.05F},{1.90F,0.70F,1.65F}},0.20F,5.0F,0.0F,FeedbackLevel::heavy},
{AttackId::heavy,   14, 5, 22, 90, 40, ImpactKind::knockdown,
 {{0.10F,-0.75F, 0.00F},{2.20F,0.75F,1.75F}},0.24F,7.0F,0.0F,FeedbackLevel::heavy},
{AttackId::launcher, 7, 4, 17, 38, 18, ImpactKind::launch,
 {{0.10F,-0.65F, 0.00F},{1.40F,0.65F,1.90F}},0.12F,1.2F,9.5F,FeedbackLevel::medium},
{AttackId::air_j,    4, 5, 12, 42, 15, ImpactKind::medium_hitstun,
 {{0.10F,-0.65F,-0.40F},{1.60F,0.65F,1.20F}},0.08F,2.5F,0.0F,FeedbackLevel::medium},
```

Use half-open phase intervals. Validation rejects duplicate IDs, non-positive phases/damage, negative break, inverted AABBs or invalid feedback.

- [ ] **Step 4: Wire targets and two architecture tests**

`arpg_combat` is STATIC, `PUBLIC` includes `${PROJECT_SOURCE_DIR}/src`, `PRIVATE` links `arpg_core`, uses C++17 and project warnings. Root CMake adds it immediately after Core and also adds `tests/combat` under BUILD_TESTING. CMake will preserve the static link-only dependency for final executables without exposing Core as part of Combat's public API.

Generalize the source scanner from `CORE_DIR` to `SOURCE_DIR`/`SOURCE_LABEL` without changing its lexer. At configure time call:

```cmake
arpg_assert_target_dependency_boundary(arpg_core)
arpg_assert_target_dependency_boundary(arpg_combat)
```

Register `architecture.core_no_raylib` for `src/core` and `architecture.combat_no_raylib` for `src/combat`; both use label `headless;architecture`.

- [ ] **Step 5: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
ctest --test-dir .\out\build\windows-msvc-core-debug -N
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
git diff --check
git add CMakeLists.txt src/combat tests/combat tests/platform
git commit -m "feat: establish stage 1 combat module"
```

Expected: four CTests; combat `3 cases, 0 failures`.

---

### Task 2: Eight-Tick Bounded Input Buffer

**Files:** `src/combat/input_buffer.hpp/.cpp`, `tests/combat/input_buffer_tests.cpp`, related CMake/test-main entries.

**Interface:** 32 fixed entries, each valid for exactly 8 active input ticks; oldest matching action can be consumed without head-of-line blocking.

- [ ] **Step 1: Add four RED cases**

Prove oldest matching consumption, exact T..T+7 validity, `age(true)` pause, 33rd push overflow, stable removal, clear and diagnostics. Change exact combat count 3→7.

```cpp
InputBuffer buffer;
ARPG_REQUIRE(buffer.push(Action::light));
for (int tick = 0; tick < 7; ++tick) { buffer.age(false); }
ARPG_REQUIRE(buffer.consume(Action::light));
ARPG_REQUIRE(buffer.push(Action::heavy));
for (int tick = 0; tick < 8; ++tick) { buffer.age(false); }
ARPG_REQUIRE(!buffer.consume(Action::heavy));
ARPG_REQUIRE(buffer.expired_count() == 1);
```

- [ ] **Step 2: Observe missing-header RED**

Run core-debug; expected missing `combat/input_buffer.hpp`.

- [ ] **Step 3: Implement fixed storage**

```cpp
enum class Action : std::uint8_t { light, jump, heavy, launcher };
class InputBuffer final {
public:
    static constexpr std::size_t kCapacity = 32;
    static constexpr std::uint8_t kLifetimeTicks = 8;
    [[nodiscard]] bool push(Action) noexcept;
    [[nodiscard]] bool consume(Action) noexcept;
    void age(bool paused) noexcept;
    void clear() noexcept;
    void reset_diagnostics() noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::uint32_t expired_count() const noexcept;
    [[nodiscard]] std::uint32_t overflow_count() const noexcept;
private:
    struct Entry final { Action action{}; std::uint8_t remaining_ticks{}; };
    std::array<Entry,kCapacity> entries_{};
    std::size_t size_{};
    std::uint32_t expired_count_{};
    std::uint32_t overflow_count_{};
};
```

Push appends `{action,8}`; consume removes the oldest matching entry; age(false) decrements once and stably compacts nonzero entries; age(true) does nothing. No allocation.

- [ ] **Step 4: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
rg -n "malloc|calloc|realloc|new " src/combat/input_buffer.hpp src/combat/input_buffer.cpp
git add src/combat tests/combat
git commit -m "feat: add bounded combat input buffer"
```

Expected combat `7 cases, 0 failures`.

---

### Task 3: Deterministic Ground Movement and Jump Arc

**Files:** `src/combat/combat_world.hpp/.cpp`, `tests/combat/movement_jump_tests.cpp`, type/CMake/test-main updates.

**Interface:** `CombatWorld::{queue_action,tick,reset,snapshot}`; no delta-time parameter.

To keep a fixed object size without a heap-backed pimpl, `PlayerRuntime`, `DummyRuntime` and `AttackRuntime` are private nested structs fully defined in `combat_world.hpp`. `player_simulation.cpp`, `target_simulation.cpp` and `hit_resolution.cpp` implement private `CombatWorld` member functions declared in the same class; they do not duplicate runtime structs or expose them through snapshots.

- [ ] **Step 1: Add four RED cases**

Prove 60 right ticks = 5.4 units, diagonal normalization, room clamps, deterministic jump/apex/landing, 70% air control and one landing transition. Change count 7→11.

- [ ] **Step 2: Observe missing-world RED**

Run core-debug; expected missing `combat/combat_world.hpp`.

- [ ] **Step 3: Add public states, config and snapshots**

```cpp
enum class Facing : std::int8_t { left = -1, right = 1 };
enum class PlayerState : std::uint8_t {
 idle, move, attack_startup, attack_active, attack_recovery,
 jump_rise, jump_fall, landing
};
enum class DummyKind : std::uint8_t { light, normal, heavy };
enum class ReactionState : std::uint8_t {
 idle, hitstun, airborne, knockdown, rising, defeated, respawning
};
enum class ArmorState : std::uint8_t { none, armored, broken };
struct MovementInput final { std::int8_t x{}; std::int8_t y{}; };
struct CombatLabConfig final {
 Vec3 player_spawn{0,0,0};
 std::array<Vec3,3> dummy_spawns{{
   {2.30F,-0.35F,0},{2.80F,0,0},{3.30F,0.35F,0}}};
};
```

Snapshots include every field required by the design: position/velocity, player/attack phase/combo/hit-stop/air quota, and per-dummy kind/reaction/armor/HP/break/break-window/hit-stop plus diagnostics.

- [ ] **Step 4: Implement exact movement and jump constants**

```cpp
constexpr float kTickSeconds = 1.0F/60.0F;
constexpr float kGroundSpeed = 5.4F;
constexpr float kAirRatio = 0.70F;
constexpr float kJumpSpeed = 8.5F;
constexpr float kGravity = 24.0F;
constexpr float kDiagonal = 0.7071067811865475F;
```

Clamp X `[-8,8]`, Y `[-3.5,3.5]`. Preserve facing without horizontal input. Integrate Z position then gravity; clamp landing Z/velocity to zero. Reset restores config and diagnostics.

- [ ] **Step 5: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
git add src/combat tests/combat
git commit -m "feat: add deterministic combat movement and jump"
```

Expected combat `11 cases, 0 failures`.

---

### Task 4: Attack State Machine, Whiff Timing, and Air Attack Limit

**Files:** `src/combat/player_simulation.cpp`, `tests/combat/attack_state_tests.cpp`, world/CMake/test-main updates.

**Interface:** six attack timelines; J1/J2 whiff continuation; J3 terminal; one air J per airtime. Hit-confirm early windows are enabled by real hits in Task 5.

- [ ] **Step 1: Add four RED timeline cases**

Prove J1 phases 0–4/5–7/8–16, L/U catalog totals, J1 whiff opens at elapsed 13, J2 at 15, J3 never wraps, attacks reject illegal K/L/U/J cancels, and air permits only one J. Change count 11→15.

- [ ] **Step 2: Observe RED**

Run core-debug; expected queued attack remains unconsumed.

- [ ] **Step 3: Implement attack runtime and deterministic action priority**

```cpp
struct AttackRuntime final {
 AttackId id{AttackId::none};
 std::uint16_t elapsed_ticks{};
 std::uint64_t serial{};
 bool connected{};
 bool impact_event_emitted{};
 std::array<bool,3> hit_targets{};
};
constexpr std::uint16_t kJ1WhiffCancelTick = 13;
constexpr std::uint16_t kJ2WhiffCancelTick = 15;
```

On a non-frozen tick consume K→L→U→J from Idle/Move; airborne consumes only J when quota remains. Starting an attack increments a nonzero serial, clears hit flags, applies one lunge and records combo stage. Whiff J1/J2 consumes buffered J only at the constants above. J3/L/U return Idle. Air J returns rise/fall from Z velocity. Input aging occurs after consumption and pauses during player Hit Stop.

- [ ] **Step 4: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
git add src/combat tests/combat
git commit -m "feat: add combat attack state machine"
```

Expected combat `15 cases, 0 failures`.

---

### Task 5: XYZ Collision, Position Correction, Hit Registry, and Feedback Aggregation

**Files:** `combat_collision.hpp/.cpp`, `hit_resolution.cpp`, `hit_resolution_tests.cpp`, type/world/CMake/test-main updates.

**Interfaces:** inclusive AABB, facing mirror, one assist correction, per-serial hit flags, `CombatEvent`, event queue capacity 64 and `try_pop_event`.

- [ ] **Step 1: Add five RED hit cases**

Prove inclusive X/Y/Z boundaries and epsilon miss, facing mirror, depth tolerance, one target once across active ticks, three-target independent hits plus one impact summary/max Hit Stop, and J1 real-hit cancel at elapsed 8 versus whiff 13. Change count 15→20.

- [ ] **Step 2: Observe RED**

Run core-debug; expected HP/events unchanged because collision is absent.

- [ ] **Step 3: Add events and collision functions**

```cpp
enum class CombatEventKind : std::uint8_t {
 swing, hit, impact_summary, landing, break_started,
 defeated, respawned, reset
};
struct CombatEvent final {
 CombatEventKind kind{};
 std::uint64_t tick{};
 AttackId attack{AttackId::none};
 std::uint8_t target_index{0xFF};
 std::uint8_t hit_count{};
 FeedbackLevel feedback{};
 Vec3 position{};
 int value{};
};
```

CombatWorld owns `core::BoundedQueue<CombatEvent,64>`; every failed push increments diagnostics. Hurtbox half extents are light `{0.45,0.35,1.40}`, normal `{0.55,0.40,1.60}`, heavy `{0.70,0.50,1.90}`. Mirror only local X, translate all axes, and use inclusive overlap.

Assist selects by smallest hurtbox surface gap then index. Require horizontal gap `<=0.35`, absolute Y `<=0.45`, correction only X, magnitude `<=0.18`, clamped to room.

- [ ] **Step 4: Collect before applying attacker freeze**

Build one world attack box, collect indices 0→2 without mutating iteration order, then apply HP/raw-break changes and record the pending ImpactKind. Mark each hit, emit per-target hit, set connected, accumulate max feedback/hit count, and set target Hit Stop 3/5/7. Task 6 turns ImpactKind into motion/reaction state; Task 7 adds armored suppression and breaking-blow priority. After the loop set player Hit Stop once and emit one first-hit `impact_summary`; emit swing at attack start. A frozen attack does not perform collision again.

- [ ] **Step 5: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
git add src/combat tests/combat
git commit -m "feat: resolve deterministic combat hits"
```

Expected combat `20 cases, 0 failures`.

---

### Task 6: Dummy Hitstun, Knockback, Launch, Knockdown, and Respawn

**Files:** `target_simulation.cpp`, `dummy_reaction_tests.cpp`, world/hit/CMake/test-main updates.

**Interface:** orthogonal ReactionState/ArmorState; deterministic target motion and local timers.

- [ ] **Step 1: Add five RED reaction cases**

Prove normal J1 hitstun 10 versus light 13, J2 X velocities 2.2 versus 2.75, U Z velocity 9.5 and gravity landing, J3/L Knockdown 45→Rising 30→Idle, and Defeated exactly 90 active ticks then one respawn event. Change count 20→25.

- [ ] **Step 2: Observe RED**

Run core-debug; targets currently lose HP but stay static.

- [ ] **Step 3: Implement separate reaction and armor runtime**

```cpp
struct DummyRuntime final {
 DummyKind kind{}; Vec3 spawn{}; Vec3 position{}; Vec3 velocity{};
 ReactionState reaction{}; ArmorState armor{};
 std::uint16_t reaction_ticks{};
 std::uint16_t break_window_ticks{};
 std::uint16_t hit_stop_ticks{};
 int hp{}; int max_hp{}; int break_value{}; int max_break{};
};
constexpr std::uint16_t kLightHitstunTicks=10;
constexpr std::uint16_t kMediumHitstunTicks=16;
constexpr std::uint16_t kHeavyHitstunTicks=22;
constexpr std::uint16_t kKnockdownTicks=45;
constexpr std::uint16_t kRisingTicks=30;
constexpr std::uint16_t kRespawnTicks=90;
```

Light multiplies reaction tick durations and impulses by 1.25; tick values use ceiling. Normal uses 1.0. Target Hit Stop decrements first and pauses local motion/timers. Airborne integrates position then gravity. Any launch landing and knockdown impact follows Knockdown→Rising. HP zero immediately enters Defeated, ignores hits, and respawns at config spawn with full stats.

Stats: HP `{300,450,700}`, break `{0,0,120}`, armor `{none,none,armored}`.

- [ ] **Step 4: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
git add src/combat tests/combat
git commit -m "feat: add combat target reactions"
```

Expected combat `25 cases, 0 failures`.

---

### Task 7: Heavy Armor, Break Window, Reset, Determinism, and Ten-Minute Stress Gate

**Files:** `break_stress_tests.cpp`, world/hit/target/CMake/test-main updates.

**Interface:** 120 break, immediate breaking-blow control, 180 local-tick broken window, complete reset, deterministic replay and zero-allocation pressure.

- [ ] **Step 1: Add five RED cases**

Prove armored suppression before zero; breaking blow immediately sets Broken and applies its control; Broken lasts exactly 180 non-frozen target ticks then restores 120; Defeated overrides recovery; reset clears all transients and leaves one reset event; two-world replay matches; 36,000 ticks allocate/overflow zero. Change count 25→30.

- [ ] **Step 2: Observe RED**

Run core-debug; expected heavy reacts incorrectly or lacks lifecycle/reset evidence.

- [ ] **Step 3: Implement break/defeat priority**

On hit: subtract HP first; HP zero enters Defeated. Otherwise armored heavy subtracts break. If break reaches zero, set Broken 180, emit `break_started`, and apply the current breaking hit's reaction immediately. Already Broken uses normal reaction. Broken countdown pauses only during target Hit Stop, continues through other reactions, then restores 120/armored without overwriting current ReactionState.

- [ ] **Step 4: Implement complete reset and two stress scripts**

Reset drains events, reconstructs all runtime fields/config spawns, clears input/diagnostics/hit flags/Hit Stop/tick, then emits exactly one reset event at tick zero.

The 2,400-tick replay compares every snapshot field and each event field. The 36,000-tick allocation loop constructs world/schedules first, records `allocation_count`, then runs:

```cpp
if (tick % 37 == 0)  { world.queue_action(Action::light); }
if (tick % 181 == 0) { world.queue_action(Action::jump); }
if (tick % 251 == 0) { world.queue_action(Action::heavy); }
if (tick % 307 == 0) { world.queue_action(Action::launcher); }
world.tick({
 static_cast<std::int8_t>((tick/120)%2==0 ? 1 : -1),
 static_cast<std::int8_t>((tick/180)%2==0 ? 1 : -1)});
while (world.try_pop_event().has_value()) {}
if (tick > 0 && tick % 3600 == 0) {
 world.reset();
 while (world.try_pop_event().has_value()) {}
}
```

Assert allocation delta, input overflow and event overflow are zero.

- [ ] **Step 5: Run Debug/Release Core gates and reviews**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
.\out\build\windows-msvc-release\bin\arpg_combat_tests.exe
rg -n "raylib|raymath|rlgl|std::vector|std::deque|unordered|new |malloc" src/combat
git diff --check
```

Expected combat `30 cases, 0 failures`, no forbidden/deferred allocation path. Complete specification and quality reviews.

- [ ] **Step 6: Commit and merge reviewed Combat**

```powershell
git add src/combat tests/combat
git commit -m "feat: complete deterministic combat lab core"
```

From integration worktree:

```powershell
git merge --no-ff task/m01-combat-core -m "merge: integrate stage 1 combat core"
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
git worktree add ..\m01-raylib-combat-host -b task/m01-raylib-combat-host milestone/m01-combat-lab
```

Expected four Core-only CTests pass; Host starts from reviewed merged API.

---

### Task 8: raylib Input Bridge, Projection, Actors, HUD, and F1 Boxes

**Worktree:** `E:\game\.worktrees\m01-raylib-combat-host`

**Files:**
- Create: `combat_view_math.hpp/.cpp`, `combat_renderer.hpp/.cpp`
- Create: `tests/platform/platform_test_main.cpp`, `combat_view_math_tests.cpp`
- Modify: `raylib_host.cpp/.hpp`, raylib/platform CMake files.

**Interfaces:** one input submission per render frame; one world tick and immediate event drain per FixedStep step; previous/current snapshots; raylib-free view math.

- [ ] **Step 1: Add three RED projection/sort cases**

Create `platform.view_math` without InitWindow. Prove back/front center projection and scale, Z moves only actor screen Y, and fixed four-actor ordering uses Y→Z→X→index. Expected missing view header RED.

In `tests/platform/CMakeLists.txt`, wrap only the platform executable/test in `if(TARGET arpg_raylib)` so `windows-msvc-core-debug` still configures with graphics disabled. Both architecture tests remain unconditional.

- [ ] **Step 2: Implement exact projection**

```cpp
struct ScreenProjection final {
 float x{}; float y{}; float ground_y{}; float scale{};
};
const float depth = clamp((position.y + 3.5F)/7.0F,0.0F,1.0F);
const float scale = 0.70F + 0.30F*depth;
const float ground_y = height*(0.38F + 0.50F*depth);
const float x = width*0.50F + position.x*(width/18.0F)*scale;
const float y = ground_y - position.z*70.0F*scale;
```

Use insertion sort on `std::array<ActorDrawItem,4>`; do not allocate.

- [ ] **Step 3: Bridge controls and fixed steps**

Queue IsKeyPressed J/K/L/U once per render frame; F1 toggles Host-only debug. Held WASD produces -1/0/1 movement. R calls `world.reset()`, refreshes both snapshots, then immediately drains the single reset event through the same renderer/feedback/audio route; this clears old presentation state even when the current render frame has zero fixed steps.

For every `FixedStepFrame::steps` iteration:

```cpp
previous = current;
world.tick(movement);
current = world.snapshot();
while (const auto event = world.try_pop_event()) {
 renderer.consume_event(*event);
}
```

Drain per tick, never once per render frame. If steps is zero, queued actions remain and are not resubmitted.

- [ ] **Step 4: Draw actors, bars, HUD and F1 boxes**

Player cyan; light/normal/heavy green/yellow/red with hurtbox sizes. Shadows use ground projection; bodies use interpolated X/Y/Z. HP/state/phase use current snapshot. Heavy alone has break bar. HUD includes controls, tick, player/action/phase/combo, input size/expired/overflow, Z, Hit Stop, event overflow and last event. F1 draws attack/hurt/assist volumes. Minimal `consume_event` records last event and target flash for two ticks, so it is useful before Task 9.

Title becomes `Infinite Dungeon - Stage 1 Combat Lab`; retain raylib 6.0 assertions.

- [ ] **Step 5: Run GREEN, visible check, and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
Start-Process .\out\build\windows-msvc-debug\bin\arpg_game.exe
git add src/platform/raylib tests/platform
git commit -m "feat: present stage 1 combat lab"
```

Expected five full-build CTests; platform `3 cases, 0 failures`; all controls/actors/bars/F1 visible; Esc exits.

---

### Task 9: Fixed Visual Pool, Shake, Damage Feedback, and Procedural Audio

**Files:** `combat_feedback.hpp/.cpp`, `combat_audio.hpp/.cpp`, `combat_feedback_tests.cpp`, renderer/host/CMake/test-main updates.

**Interfaces:** 256 fixed effects, per-target visuals, maximum shake, pure audio routing and startup-only sound generation.

- [ ] **Step 1: Add three RED feedback/router cases**

Without window/audio prove three heavy hit events create independent visuals but one max heavy shake; swing→weapon, hit→material, heavy summary→one low cue; 257th decoration increments drop count; reset clears all. Change platform count 3→6.

- [ ] **Step 2: Implement fixed feedback**

```cpp
enum class VisualEffectKind : std::uint8_t {
 spark, dust, damage_number, weapon_trail
};
struct VisualEffect final {
 bool active{}; VisualEffectKind kind{}; combat::Vec3 position{};
 float age_seconds{}; float lifetime_seconds{}; int value{};
};
class CombatFeedback final {
public:
 static constexpr std::size_t kCapacity=256;
 void consume(const combat::CombatEvent&) noexcept;
 void update(float) noexcept;
 void clear() noexcept;
 [[nodiscard]] bool try_spawn(const VisualEffect&) noexcept;
 [[nodiscard]] std::size_t active_count() const noexcept;
 [[nodiscard]] std::uint32_t dropped_count() const noexcept;
 [[nodiscard]] float shake_amplitude() const noexcept;
 [[nodiscard]] Vector2 camera_offset() const noexcept;
private:
 std::array<VisualEffect,kCapacity> effects_{};
 std::array<float,3> flash_seconds_{};
 float shake_amplitude_{}; float shake_time_{};
 std::uint32_t dropped_count_{};
};
```

Shake amplitudes 2/5/9 and durations 0.10/0.14/0.20 seconds combine by max; offsets use deterministic sine/cosine. Clamp frame dt 0..0.1. Hit spawns two-tick flash, spark, number; landing dust; swing trail; reset clear.

- [ ] **Step 3: Generate audio only at initialization**

Pure `route_audio_cues` returns weapon/material/low bitmask. `CombatAudio::initialize` initializes audio device, fills fixed int16 sample arrays at 22,050 Hz, and creates: 0.08s descending weapon tone, 0.06s deterministic material noise, 0.12s 80Hz low tone. No frame-loop allocation. Failure leaves `ready=false`, HUD shows unavailable, game continues silently. Swing plays weapon, each hit material, heavy summary low once.

- [ ] **Step 4: Integrate, run GREEN, and commit**

Pass each per-tick event to feedback/audio. Apply camera offset to world, not HUD. Draw trail behind actors, dust ground layer, sparks/numbers actor layer. Reset event is the sole transient clear trigger.

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
.\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
.\out\build\windows-msvc-release\bin\arpg_platform_tests.exe
Start-Process .\out\build\windows-msvc-release\bin\arpg_game.exe
git add src/platform/raylib tests/platform
git commit -m "feat: add combat impact feedback"
```

Expected five CTests; platform `6 cases, 0 failures`; J3/L feedback exceeds J1; U launches; multi-target heavy gives one low/max shake; R clears.

- [ ] **Step 5: Review Host branch**

Complete specification and code-quality reviews. Fix every Critical/Important issue with focused RED and rerun both configurations before integration.

---

### Task 10: Merge, Full Verification, GUI Acceptance, and Completion Report

**Worktree:** `E:\game\.worktrees\m01-combat-lab`

**Files:** merge Host; create ignored `.superpowers/sdd/task-10-report.md`; do not touch main/Stage 2.

- [ ] **Step 1: Merge reviewed Host**

```powershell
git merge --no-ff task/m01-raylib-combat-host -m "merge: integrate stage 1 raylib combat host"
```

- [ ] **Step 2: Run all Fresh gates**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
ctest --test-dir .\out\build\windows-msvc-debug -N
ctest --test-dir .\out\build\windows-msvc-release -N
```

Full builds list exactly: `core.units`, `combat.units`, `platform.view_math`, `architecture.core_no_raylib`, `architecture.combat_no_raylib`. Core-only lists the same except platform.view_math. All pass.

- [ ] **Step 3: Run direct executables**

Run Debug and Release versions of `arpg_core_tests`, `arpg_combat_tests`, `arpg_platform_tests`. Expected respectively `22/0`, `30/0` (including 36,000-tick zero-allocation/overflow), `6/0`.

- [ ] **Step 4: Verify toolchain/artifacts/boundary**

Check both caches for cl.exe 19.44, correct build type, `BUILD_SHARED_LIBS=OFF`; verify Release EXE and raylib.lib exist, `raylib*.dll` count zero, and `rg raylib|raymath|rlgl src/core src/combat` has no forbidden include.

- [ ] **Step 5: Run visible matrix and capture screenshot**

Verify WASD/facing/bounds, J1→J2→J3 and whiff delay, K+one air J, L, U, light/normal scaling, heavy armor→breaking blow→post-break control→180 recovery, multi-target independent flash with one max shake/low cue, R reset, F1 boxes, overflow HUD all zero. Capture 1280×720 screenshot with four actors, bars and HUD.

- [ ] **Step 6: Verify Esc, close button and cleanup**

Run twice, once each exit path; after each `@(Get-Process arpg_game -ErrorAction SilentlyContinue).Count` must be zero.

- [ ] **Step 7: Final review and report**

Cross-review all commits since bacc15a. Resolve Critical/Important issues with RED and rerun Steps 2–6. Run `git diff --check`, clean status, graph/history and main/milestone SHAs. Write exact commands/results, counts, sizes, screenshot, GUI matrix, cleanup, HEAD and limitations to ignored report. Stop clean, keep worktrees, no main merge, no Stage 2.

---

## Specification Coverage Matrix

| Design requirement | Plan coverage |
|---|---|
| Pure Combat Core and no raylib dependency | Task 1; Task 10 |
| 60 Hz movement/jump and fixed input | Tasks 2–3 |
| Six attacks, phases, combo/whiff/air limit | Tasks 1 and 4 |
| XYZ boxes, assist, per-target once, aggregation | Task 5 |
| Hit Stop, hard stun, knockback, launch, knockdown | Tasks 5–6 |
| Three target classes, armor, break, defeat/reset | Tasks 6–7 |
| Determinism, no allocation, 10-minute simulation | Task 7 |
| WASD/J/K/L/U/R/F1, actors, bars, HUD | Task 8 |
| Flash/trail/spark/dust/numbers/shake/audio | Task 9 |
| Debug/Release, GUI, artifacts, exits | Task 10 |
| Worktrees retained, no main merge or Stage 2 | Task 10 |
