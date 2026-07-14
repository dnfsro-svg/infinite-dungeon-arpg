# Stage 7：64 节点被动技能星盘 MVP Implementation Plan

> **执行约束：** 必须使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans`，逐任务实现本计划，并用复选框追踪进度。

**Goal:** 在清房阶段提供一张可分配、可退款、实际影响五分量战斗伤害与四抗性、可崩溃恢复的 64 节点永久被动星盘。

**Architecture:** `arpg_passives` 是一个仅依赖 `arpg_modifiers` 与 `arpg_progression` 的无图形静态库，拥有固定节点目录、64 位状态与连通规则。它投影出纯数据 `modifiers::PlayerModifierValues`；`arpg_combat` 消费该数据而不依赖星盘库，`arpg_dungeon` 在构造下一房时求值，存档和 raylib 分别负责事务与表现。

**Tech Stack:** C++17、MSVC 19.44 x64、Windows SDK 10.0.26100.0、raylib 6.0.0、CMake/CTest。

## Global Constraints

- 生产规则、战斗和星盘代码均不得依赖 raylib；仅 `src/platform/raylib/` 可包含 `<raylib.h>`。
- 节点 ID 固定为 0–63；节点 0 永久激活且免费，其余每个节点精确消耗 1 个被动点。
- 只有 `RoomPhase::awaiting_exit` 且没有待提交保存时可操作；`P` 开关覆盖层，`Esc` 继续退出游戏。
- 伤害类型为 physical/fire/water/lightning/chaos；抗性只对后四种生效，最终范围为 -60% 至 75%。
- 不实现装备、掉落、暴击、异常、穿透、伤害转换、资源、重置货币、怪物随机词条或 Stage 8 功能。
- 所有热路径使用固定容量或栈数组；不得在每 tick、每命中、每节点渲染中分配堆内存。
- 默认构筑必须严格保持当前 Stage 6 的战斗数值、固定 tick、RNG、输入缓冲和现有测试黄金值。
- 每个任务先写 RED 测试并观察失败，再做最小 GREEN；任务完成后运行定向测试、受影响 CTest、`git diff --check` 并单独提交。

## File Structure

| 路径 | 职责 |
| --- | --- |
| `src/modifiers/damage_types.hpp` | 五分量伤害类型、元素抗性索引与固定数组常量 |
| `src/modifiers/player_modifier_values.hpp/.cpp` | 从通用 Modifier 得到给 combat 使用的纯数据玩家构筑 |
| `src/passives/passive_tree_types.hpp` | 位图、节点类型、命令、拒绝原因与只读星盘快照 |
| `src/passives/passive_tree_catalog.hpp/.cpp` | 64 个稳定节点、坐标、邻接表、静态 Modifier 数据 |
| `src/passives/passive_tree_rules.hpp/.cpp` | 固定容量 BFS、合法性验证、分配/退款与确定性投影 |
| `src/combat/combat_types.hpp` | `DamagePacket`、`PlayerCombatBuild`、玩家护盾和 typed projectile/hazard 数据 |
| `src/combat/combat_world.cpp`、`hit_resolution.cpp`、`player_simulation.cpp` | 五分量结算、元素怪物受击、护盾和构筑数值生效 |
| `src/dungeon/dungeon_checkpoint.hpp`、`dungeon_session.*`、`dungeon_transition.cpp` | 稳定星盘状态、清房门控和独立 pending checkpoint mutation |
| `src/persistence/checkpoint_codec.*` | 120 字节格式 3、格式 1/2 迁移和位图验证 |
| `src/platform/raylib/passive_tree_view_math.*` | 纯节点投影、点击命中和 UI 门控 |
| `src/platform/raylib/passive_tree_renderer.*` | raylib 全屏星盘覆盖层 |
| `src/platform/raylib/raylib_host.cpp` | P 键、鼠标命令和覆盖层期间的游戏输入抑制 |
| `tests/passives/*` | 目录、连通、点数守恒与投影回归 |
| `tests/combat/player_build_tests.cpp` | 五分量伤害、抗性、护盾、移动/攻击/冲量数值回归 |
| `tests/dungeon/dungeon_passive_tree_tests.cpp` | 阶段门控、mutation 事务和下一房构筑投影 |
| `tests/persistence/passive_tree_checkpoint_tests.cpp` | 格式迁移、畸形位图与双槽故障恢复 |
| `tests/platform/passive_tree_view_tests.cpp` | 无窗口 UI 几何、命中和输入门控回归 |
| `docs/stage7/2026-07-14-result.md` | Stage 7 的独立测试与窗口验收记录，不改写历史重构报告 |

---

### Task 1: 扩展通用 Modifier 的伤害类型和玩家构筑值

**Files:**
- Create: `src/modifiers/damage_types.hpp`
- Create: `src/modifiers/player_modifier_values.hpp`
- Create: `src/modifiers/player_modifier_values.cpp`
- Modify: `src/modifiers/modifier_types.hpp`
- Modify: `src/modifiers/CMakeLists.txt`
- Modify: `tests/modifiers/CMakeLists.txt`
- Modify: `tests/modifiers/modifier_test_main.cpp`
- Create: `tests/modifiers/player_modifier_values_tests.cpp`

**Interfaces:**
- Consumes: 现有 `Modifier`、`ModifierSpan`、`evaluate_stat()` 和 `kFixedOne`。
- Produces: `DamageType`、`kDamageTypeCount`、`is_elemental()`、`element_index()`、`PlayerModifierValues` 和 `evaluate_player_modifiers(ModifierSpan)`；后续 passives 与 combat 只通过这些类型通信。

- [ ] **Step 1: 写出玩家构筑值的 RED 测试**

在 `player_modifier_values_tests.cpp` 中写入以下测试，先引用尚不存在的头文件和函数：

```cpp
using namespace arpg::modifiers;

arpg::test::Failure fire_and_generic_values_are_independent() noexcept {
    const std::array<Modifier, 4> values{{
        {101U, StatId::fire_flat_damage, ModifierOperation::flat, 30000},
        {102U, StatId::fire_damage, ModifierOperation::increased, 800},
        {103U, StatId::fire_resistance, ModifierOperation::flat, 700},
        {104U, StatId::move_speed, ModifierOperation::increased, 600},
    }};
    const PlayerModifierValues result = evaluate_player_modifiers(values);
    ARPG_REQUIRE(result.flat_damage[damage_index(DamageType::fire)] == 30000);
    ARPG_REQUIRE(result.damage_increased[damage_index(DamageType::fire)] == 10800);
    ARPG_REQUIRE(result.resistance[element_index(DamageType::fire)] == 700);
    ARPG_REQUIRE(result.movement_speed == 10600);
    ARPG_REQUIRE(result.resistance[element_index(DamageType::water)] == 0);
    return {};
}

arpg::test::Failure resistance_and_reduction_are_clamped() noexcept {
    const std::array<Modifier, 2> values{{
        {201U, StatId::lightning_resistance, ModifierOperation::flat, 10000},
        {202U, StatId::damage_taken, ModifierOperation::increased, -9000},
    }};
    const PlayerModifierValues result = evaluate_player_modifiers(values);
    ARPG_REQUIRE(result.resistance[element_index(DamageType::lightning)] == 7500);
    ARPG_REQUIRE(result.damage_taken == 5000);
    return {};
}
```

将 suite 加入 `modifier_test_main.cpp`，临时把期望 case 数从 13 改成 15。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --preset windows-msvc-debug && cmake --build --preset windows-msvc-debug --target arpg_modifier_tests'
```

Expected: 编译失败，提示 `modifiers/damage_types.hpp` 或 `evaluate_player_modifiers` 尚不存在。

- [ ] **Step 3: 实现稳定类型和求值投影**

在 `damage_types.hpp` 定义连续枚举；物理索引为 0，四元素索引为 0–3，避免把物理放进抗性数组：

```cpp
enum class DamageType : std::uint8_t { physical, fire, water, lightning, chaos, count };
inline constexpr std::size_t kDamageTypeCount = static_cast<std::size_t>(DamageType::count);
inline constexpr std::size_t kElementCount = 4U;
[[nodiscard]] constexpr std::size_t damage_index(DamageType type) noexcept {
    return static_cast<std::size_t>(type);
}
[[nodiscard]] constexpr bool is_elemental(DamageType type) noexcept {
    return type != DamageType::physical && type != DamageType::count;
}
[[nodiscard]] constexpr std::size_t element_index(DamageType type) noexcept {
    return static_cast<std::size_t>(type) - 1U;
}
```

把 `StatId` 扩展为：`melee_damage`、`fire_flat_damage`、`water_flat_damage`、`lightning_flat_damage`、`chaos_flat_damage`、四个元素伤害提高、四个元素抗性、`max_health`、`max_barrier`、`damage_taken`、`move_speed`、`attack_speed`、`jump_speed`、`air_control`，保留已有 `impulse_scale` 和怪物 `shield`。`PlayerModifierValues` 的乘数默认 `kFixedOne`、抗性/平坦值默认 0，并以 `evaluate_stat()` 得出值；抗性钳制 `[-6000, 7500]`，`damage_taken` 钳制 `[5000, 20000]`，其余速度/冲量下限为 0。所有固定生命、护盾和附加伤害均以 `kFixedOne == 10000` 为单位：例如 +20 HP 写作 `200000`，+10 barrier 写作 `100000`，+3 火伤写作 `30000`；combat 只在投影时向下转换为整数游戏数值。

```cpp
struct PlayerModifierValues final {
    std::array<FixedValue, kDamageTypeCount> flat_damage{};
    std::array<FixedValue, kDamageTypeCount> damage_increased{{kFixedOne, kFixedOne, kFixedOne, kFixedOne, kFixedOne}};
    std::array<FixedValue, kElementCount> resistance{};
    FixedValue melee_damage{kFixedOne};
    FixedValue max_health{};
    FixedValue max_barrier{};
    FixedValue damage_taken{kFixedOne};
    FixedValue movement_speed{kFixedOne};
    FixedValue attack_speed{kFixedOne};
    FixedValue impulse_scale{kFixedOne};
    FixedValue jump_speed{kFixedOne};
    FixedValue air_control{kFixedOne};
    bool valid{true};
};
```

把 `player_modifier_values.cpp` 加入 `arpg_modifiers`；将测试源加到 `arpg_modifier_tests`。

- [ ] **Step 4: 验证 modifiers 回归**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "modifiers.units|architecture.modifiers" --output-on-failure
git diff --check
```

Expected: `modifiers.units` 与 5 个 `architecture.modifiers_*` 测试全部通过；新 suite 显示 15 cases, 0 failures。

- [ ] **Step 5: 提交 Task 1**

```powershell
git add src/modifiers tests/modifiers
git commit -m "feat: add typed player modifier values"
```

### Task 2: 新建无图形的 64 节点星盘目录、连通规则与投影

**Files:**
- Create: `src/passives/CMakeLists.txt`
- Create: `src/passives/passive_tree_types.hpp`
- Create: `src/passives/passive_tree_catalog.hpp`
- Create: `src/passives/passive_tree_catalog.cpp`
- Create: `src/passives/passive_tree_rules.hpp`
- Create: `src/passives/passive_tree_rules.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/passives/CMakeLists.txt`
- Create: `tests/passives/passive_test_main.cpp`
- Create: `tests/passives/passive_tree_catalog_tests.cpp`
- Create: `tests/passives/passive_tree_rules_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 的 `Modifier`、`PlayerModifierValues` 与 `ProgressionState`。
- Produces: `PassiveTreeState`、`PassiveNode`、`PassiveNodeType`、`PassiveTreeError`、`allocate_node()`、`refund_node()`、`valid_passive_tree_state()`、`evaluate_passive_tree()`；dungeon 和 persistence 以后不得重新实现连通搜索。

- [ ] **Step 1: 写目录、BFS 和点数守恒的 RED 测试**

在 `passive_tree_catalog_tests.cpp` 中断言固定目录，在 `passive_tree_rules_tests.cpp` 中断言分配与退款：

```cpp
arpg::test::Failure catalog_has_exact_stable_shape() noexcept {
    const auto nodes = arpg::passives::passive_nodes();
    ARPG_REQUIRE(nodes.size() == 64U);
    ARPG_REQUIRE(nodes[0].id == 0U && nodes[0].type == PassiveNodeType::start);
    ARPG_REQUIRE(nodes[8].type == PassiveNodeType::connector);
    ARPG_REQUIRE(nodes[21].type == PassiveNodeType::keystone);
    ARPG_REQUIRE(nodes[63].type == PassiveNodeType::keystone);
    ARPG_REQUIRE(arpg::passives::catalog_is_valid());
    return {};
}

arpg::test::Failure allocation_requires_adjacency_and_refund_keeps_connectivity() noexcept {
    ProgressionState progress{10U, 0U, 9U, 9U};
    PassiveTreeState state{};
    ARPG_REQUIRE(!allocate_node(state, progress, 21U).changed);
    ARPG_REQUIRE(allocate_node(state, progress, 8U).changed);
    ARPG_REQUIRE(allocate_node(state, progress, 9U).changed);
    ARPG_REQUIRE(allocate_node(state, progress, 10U).changed);
    ARPG_REQUIRE(!refund_node(state, progress, 8U).changed);
    ARPG_REQUIRE(refund_node(state, progress, 10U).changed);
    ARPG_REQUIRE(progress.unspent_passive_points == 7U);
    return {};
}
```

为两个 suite 的总 case 数设置为 9：目录 4 个、规则 5 个，包含重复节点、无点、未知 ID、断链、相同位图的确定性投影。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --preset windows-msvc-debug && cmake --build --preset windows-msvc-debug --target arpg_passive_tests'
```

Expected: CMake 失败，因为 `src/passives/CMakeLists.txt` 和 target `arpg_passives` 尚不存在。

- [ ] **Step 3: 实现固定目录和规则 API**

建立以下公开类型。邻接数组固定为 12 条边，`neighbor_count` 指定有效前缀；容量 12 足以编码起点的 11 条直接边，同时仍是固定容量栈数组。Modifier 数组固定容量为 4，以容纳 stormstep 的四个独立效果，仍不得使用堆内存：

```cpp
inline constexpr std::size_t kPassiveNodeCount = 64U;
using PassiveNodeId = std::uint8_t;
struct PassiveTreeState final { std::uint64_t allocated_bits{1U}; };
enum class PassiveNodeType : std::uint8_t { start, small, notable, keystone, connector };
enum class PassiveTreeError : std::uint8_t {
    none, unknown_node, already_allocated, not_allocated, no_points,
    not_adjacent, disconnects_tree, invalid_state
};
struct PassiveTreeResult final {
    PassiveTreeState state{};
    progression::ProgressionState progression{};
    PassiveTreeError error{PassiveTreeError::none};
    bool changed{};
};
struct PassiveNode final {
    PassiveNodeId id{};
    PassiveNodeType type{};
    const char* name{};
    std::int16_t x{};
    std::int16_t y{};
    std::array<PassiveNodeId, 12> neighbors{};
    std::uint8_t neighbor_count{};
    std::array<modifiers::Modifier, 4> modifiers{};
    std::uint8_t modifier_count{};
};
```

在 `passive_tree_catalog.cpp` 按已批准规格建立：中央 ID 0–7，路线入口 8/22/36/50，每条路线 `B+0..B+13`，关键节点 21/35/49/63。以 [已批准设计第 3.1–3.2 节](../specs/2026-07-14-stage7-passive-star-chart-design.md) 的两张节点表为唯一数据来源：0 同时连接 1–7 和四个路线入口；每条路线严格采用 `B+1` 的三叉、`B+2→3→4→10→13`、`B+5→6→7→11`、`B+8→9→12` 的无环路径，中央及路线数值、关键节点收益和代价逐项照表编码，不作改名或调参。每条边在两端同时出现，所有 Modifier ID 由 `1000U + node_id * 3U + local_index` 生成并唯一。

`passive_tree_rules.cpp` 用 `std::array<PassiveNodeId, 64>` 与 `std::array<bool, 64>` 实现 BFS，不使用 `vector`。`valid_passive_tree_state()` 检查 bit 0、没有超范围位、节点定义与已分配集合连通、以及 `popcount(bits & ~1ULL) + unspent == earned`。`evaluate_passive_tree()` 按节点 ID 顺序写入固定 `std::array<Modifier, 128>`，再调用 Task 1 的 `evaluate_player_modifiers()`。

增加根 CMake 的 `add_subdirectory(src/passives)`，在 `src/modifiers` 后、`src/dungeon` 前；添加 `tests/passives`，并在平台架构 CMake 增加 `architecture.passives_no_raylib` 和 passives 不可达 combat/dungeon/persistence 的边界断言。

- [ ] **Step 4: 验证星盘核心和边界**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_passive_tests
ctest --test-dir out/build/windows-msvc-debug -R "passives.units|architecture.passives" --output-on-failure
git diff --check
```

Expected: `arpg_passive_tests` 为 9 cases, 0 failures；passives 只链接 `arpg_modifiers` 和 `arpg_progression`，没有 raylib/combat/dungeon/persistence 可达路径。

- [ ] **Step 5: 提交 Task 2**

```powershell
git add CMakeLists.txt src/passives tests/passives tests/platform/CMakeLists.txt
git commit -m "feat: add passive tree catalog and allocation rules"
```

### Task 3: 让 CombatWorld 消费玩家构筑并结算五分量伤害

**Files:**
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/hit_resolution.cpp`
- Modify: `src/combat/player_simulation.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/monster_ai_ranged.cpp`
- Modify: `src/combat/monster_ai_special.cpp`
- Modify: `src/combat/monster_catalog.cpp`
- Modify: `src/combat/monster_pool.hpp`
- Modify: `tests/combat/CMakeLists.txt`
- Create: `tests/combat/player_build_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `tests/combat/combat_test_support.hpp`

**Interfaces:**
- Consumes: Task 1 的 `DamageType` 和 `PlayerModifierValues`；不包含或链接 `arpg_passives`。
- Produces: `DamagePacket`、`PlayerCombatBuild`、typed monster/projectile/hazard damage、护盾字段和默认构筑等价保证；dungeon 之后把 passives 投影交给 `CombatEncounterConfig`。

- [ ] **Step 1: 写 typed 伤害、抗性、护盾和移动的 RED 测试**

在 `player_build_tests.cpp` 写入以下测试，并通过 `CombatWorldTestAccess` 提供受控伤害注入：

```cpp
arpg::test::Failure fire_resistance_then_barrier_absorbs_damage() noexcept {
    combat::PlayerCombatBuild build{};
    build.values.resistance[modifiers::element_index(modifiers::DamageType::fire)] = 7500;
    build.values.max_barrier = 100000;
    combat::CombatEncounterConfig config{};
    config.player_build = build;
    combat::CombatWorld world(config);
    arpg::test::CombatWorldTestAccess::apply_damage(
        world, {{0, 40, 0, 0, 0}}, combat::Vec3{}, combat::FeedbackLevel::light);
    const auto player = world.snapshot().player;
    ARPG_REQUIRE(player.barrier == 0);
    ARPG_REQUIRE(player.hp == player.max_hp);
    return {};
}

arpg::test::Failure player_attack_adds_element_without_converting_physical() noexcept {
    combat::PlayerCombatBuild build{};
    build.values.flat_damage[modifiers::damage_index(modifiers::DamageType::fire)] = 30000;
    const combat::DamagePacket packet = combat::build_player_hit_packet(28, build);
    ARPG_REQUIRE(packet.amount[modifiers::damage_index(modifiers::DamageType::physical)] == 28);
    ARPG_REQUIRE(packet.amount[modifiers::damage_index(modifiers::DamageType::fire)] == 3);
    return {};
}
```

再增加：-50% 水抗把 20 水伤变成 30，混沌关键节点 11500 最终承伤把 20 变成 23，默认构筑 J1=28 的既有伤害不变，攻击速度只缩短启动/恢复而 active tick 保持 3，冲量提高同时缩放水平击退和上挑浮空。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
```

Expected: 编译失败，提示 `PlayerCombatBuild`、`DamagePacket`、玩家护盾或 typed `CombatWorldTestAccess::apply_damage()` 未定义。

- [ ] **Step 3: 实现纯数据构筑和结算顺序**

在 `combat_types.hpp` 添加：

```cpp
struct DamagePacket final {
    std::array<int, modifiers::kDamageTypeCount> amount{};
};
struct PlayerCombatBuild final {
    modifiers::PlayerModifierValues values{};
};
```

给 `CombatEncounterConfig` 添加 `PlayerCombatBuild player_build{}`；默认值必须得出当前 1000 HP、0 barrier、无附加元素、100% 速度与当前攻击时序。`PlayerRuntime`/`PlayerSnapshot` 添加 `barrier`、`max_barrier` 和四项已钳制抗性。每次房间创建时，以现有基础生命加 `fixed_floor(values.max_health)` 初始化 HP；以 `fixed_floor(values.max_barrier)` 初始化 barrier，房间内不自动恢复。

实现三个纯函数并用它们替换散落的整数相减：

```cpp
[[nodiscard]] DamagePacket build_player_hit_packet(
    int base_physical, const PlayerCombatBuild& build) noexcept;
[[nodiscard]] int resolve_player_damage(
    DamagePacket packet, const PlayerCombatBuild& build) noexcept;
[[nodiscard]] std::uint16_t scaled_phase_ticks(
    std::uint16_t base, modifiers::FixedValue attack_speed) noexcept;
```

`build_player_hit_packet()` 保留物理基础伤害、添加四种独立附加伤害、对各分量结算元素提高/更多，最后对五种分量共同结算近战伤害。`resolve_player_damage()` 对四元素按抗性结算，再对合计施加 `damage_taken`，向下取整并限制为非负。`apply_player_damage()` 先扣 `player_.barrier` 再扣 HP，并维持生命至少为 1 的现有规则。将 `MonsterDefinition`、`ProjectileRuntime`、`HazardRuntime` 的 `int damage` 升级为 `DamagePacket`；目录中每只怪物标为其生态元素，所有 contact/projectile/hazard 创建点传入该元素 packet。

更新 `CombatWorldTestAccess`：保留现有整数重载（把整数包装成物理 `DamagePacket`，因此旧健康测试不改语义），并新增下列 typed 重载，供本任务新测试直接注入：

```cpp
static void apply_damage(
    combat::CombatWorld& world,
    combat::DamagePacket packet,
    combat::Vec3 source_position,
    combat::FeedbackLevel feedback) noexcept {
    world.apply_player_damage(packet, source_position, feedback);
}
```

把 `AttackRuntime` 增加缓存的 `startup_ticks`、`recovery_ticks`。`start_attack()` 以 `scaled_phase_ticks()` 只计算这两项，至少为 1；`active_ticks` 继续使用目录的原值。为 `attack_phase_at()` 增加接收实际 startup/recovery 的重载，`simulate_player()`、`hit_resolution.cpp` 和 snapshot 都用该重载，确保同一攻击的 active tick 数和命中窗口不变。移动、跳跃、空中机动从 `build.values` 缩放；`target_simulation.cpp` 的水平击退和垂直 launch 共用同一 `impulse_scale`。

在 `hit_resolution.cpp` 用 `build_player_hit_packet(definition->damage, encounter_config_.player_build)` 扣怪物 HP，`CombatEvent::value` 写 packet 总值。

- [ ] **Step 4: 验证 Combat 默认回归和新行为**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "combat.units|architecture.combat" --output-on-failure
git diff --check
```

Expected: combat suite 的原有案例和新增 7 个 player build 案例全部通过；所有 combat 边界测试仍显示无 dungeon/persistence/raylib 依赖。

- [ ] **Step 5: 提交 Task 3**

```powershell
git add src/combat tests/combat
git commit -m "feat: apply passive builds to typed combat damage"
```

### Task 4: 把星盘位图纳入检查点格式 3 并保留旧存档迁移

**Files:**
- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/CMakeLists.txt`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `tests/persistence/CMakeLists.txt`
- Create: `tests/persistence/passive_tree_checkpoint_tests.cpp`
- Modify: `tests/persistence/persistence_test_main.cpp`
- Modify: `tests/persistence/checkpoint_codec_tests.cpp`

**Interfaces:**
- Consumes: Task 2 的 `PassiveTreeState` 与 `valid_passive_tree_state()`。
- Produces: `DungeonRunState::passive_tree`，格式 3 `kEncodedCheckpointSize == 120U`，并能从格式 1/2 返回一个仅起点的合法状态。

- [ ] **Step 1: 写格式 3 的 RED 测试**

增加测试，指定明确的 wire contract：

```cpp
arpg::test::Failure passive_bits_round_trip_at_little_endian_offset_106() noexcept {
    auto state = make_fixture();
    state.progression = {10U, 0U, 9U, 6U};
    state.passive_tree.allocated_bits = (1ULL << 0U) | (1ULL << 8U)
        | (1ULL << 9U) | (1ULL << 10U);
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
    ARPG_REQUIRE(persistence::encode_checkpoint(state, bytes));
    ARPG_REQUIRE(bytes.size() == 120U);
    ARPG_REQUIRE(bytes[106U] == 0x01U && bytes[107U] == 0x07U);
    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.state.passive_tree.allocated_bits == state.passive_tree.allocated_bits);
    return {};
}
```

同时增加格式 2 fixture 迁移为 `allocated_bits == 1ULL`、bit 0 清除/断链/超点被拒绝、CRC 覆盖新增 8 字节和保存槽故障恢复保持位图的测试。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_persistence_tests
out\build\windows-msvc-debug\bin\arpg_persistence_tests.exe
```

Expected: 编译失败，因为当前 checkpoint 大小为 112、没有 `passive_tree` 字段且没有格式 3 解码分支。

- [ ] **Step 3: 实现格式 3 编码、解码和迁移**

在 `DungeonRunState` 加入：

```cpp
passives::PassiveTreeState passive_tree{};
```

格式常量改为：当前版本 3、当前魔数 `IARPGS04`、payload 88、encoded 120；保留格式 1/2 的常量与旧魔数 `IARPGS03`。新版本在 offset 106 写入 `allocated_bits` little-endian；CRC 最大覆盖大小改为 108。解码根据魔数、format、payload size 三元组选择分支：格式 1 沿用现有 progress 迁移，格式 2 保留已有 progress，二者均设置 `PassiveTreeState{1ULL}`；格式 3 读取 106–113 并调用同一状态验证。

`valid_state()` 必须同时调用 progression 与 passives 的验证。更新 `arpg_persistence` 与 `arpg_dungeon` CMake 链接到 `arpg_passives`，但保持 persistence 不可达 combat/dungeon 的现有边界。

- [ ] **Step 4: 验证存档迁移和双槽恢复**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "persistence.units|architecture.persistence" --output-on-failure
git diff --check
```

Expected: 所有旧 checkpoint fixture 仍可迁移，格式 3 round-trip 与新故障恢复测试通过，`architecture.persistence_no_raylib`、`no_dungeon`、`no_combat` 全部通过。

- [ ] **Step 5: 提交 Task 4**

```powershell
git add src/dungeon/dungeon_checkpoint.hpp src/persistence src/dungeon/CMakeLists.txt tests/persistence
git commit -m "feat: persist passive tree allocations"
```

### Task 5: 实现 DungeonSession 的清房加点/退款事务和下一房构筑投影

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/room_combat_template.hpp`
- Modify: `src/dungeon/room_combat_template.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Create: `tests/dungeon/dungeon_passive_tree_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/dungeon/dungeon_test_support.hpp`

**Interfaces:**
- Consumes: Task 2 allocation APIs，Task 3 `PlayerCombatBuild`，Task 4 的稳定 checkpoint 状态。
- Produces: `request_passive_allocation()`、`request_passive_refund()`、`pending_save()`、`resolve_pending_save()` 和包含星盘位图的 `DungeonSnapshot`；platform 之后只通过这些接口请求事务。

- [ ] **Step 1: 写阶段门控和保存收据的 RED 测试**

在 `dungeon_passive_tree_tests.cpp` 先写入完整的有点数 Session helper，再写测试：

```cpp
dungeon::DungeonSession session_with_passive_points(std::uint8_t points) noexcept {
    auto built = dungeon::make_initial_run_state(0x51515151ULL, dungeon::DungeonRules{});
    built.state.progression = {static_cast<std::uint8_t>(points + 1U), 0U, points, points};
    return dungeon::DungeonSession(dungeon::DungeonRules{}, built.state);
}

bool clear_to_awaiting_exit(dungeon::DungeonSession& session) noexcept {
    arpg::test::EventSummary events{};
    if (!arpg::test::drive_until_cleared(session, events)) return false;
    if (session.snapshot().phase == dungeon::RoomPhase::cleared) session.tick({});
    return session.snapshot().phase == dungeon::RoomPhase::awaiting_exit;
}
```

随后写入：

```cpp
arpg::test::Failure passive_mutation_is_only_available_after_clear() noexcept {
    dungeon::DungeonSession session = session_with_passive_points(4U);
    ARPG_REQUIRE(!session.request_passive_allocation(8U));
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    ARPG_REQUIRE(session.request_passive_allocation(8U));
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == dungeon::PendingSaveKind::passive_tree);
    ARPG_REQUIRE(!session.request_descent(true));
    return {};
}

arpg::test::Failure not_committed_passive_mutation_keeps_old_state() noexcept {
    dungeon::DungeonSession session = session_with_passive_points(4U);
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    ARPG_REQUIRE(session.request_passive_allocation(8U));
    const auto pending = *session.pending_save();
    session.resolve_pending_save({dungeon::SaveDisposition::not_committed,
        pending.next_state.commit_generation, pending.next_state});
    ARPG_REQUIRE(session.snapshot().passive_tree.allocated_bits == 1ULL);
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::awaiting_exit);
    return {};
}
```

再加 committed 后位图/未分配点更新、indeterminate 进入 fault、断链退款拒绝、下一房的 `combat.player` 具有已保存的 barrier/抗性/附加元素构筑，以及旧 `pending_transition()` 只在 transition 时返回值的测试。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
out\build\windows-msvc-debug\bin\arpg_dungeon_tests.exe
```

Expected: 编译失败，因为星盘命令、`PendingSaveKind`、`pending_save()` 和 snapshot 字段尚不存在。

- [ ] **Step 3: 用统一 pending save 建立事务边界**

在 `dungeon_types.hpp` 增加：

```cpp
enum class PendingSaveKind : std::uint8_t { transition, passive_tree };
struct PendingSave final {
    PendingSaveKind kind{PendingSaveKind::transition};
    std::uint64_t expected_generation{};
    DungeonRunState next_state{};
    TransitionKind transition{TransitionKind::none};
    ExitDirection direction{ExitDirection::none};
};
struct PendingSaveResult final {
    SaveDisposition disposition{SaveDisposition::indeterminate};
    std::uint64_t generation{};
    DungeonRunState verified_state{};
};
```

`DungeonSession` 用单一 `std::optional<PendingSave> pending_save_` 取代只服务门的 `pending_`，并把旧 `TransitionSaveResult` 统一改名为 `PendingSaveResult`（修改现有 dungeon、platform 和测试的调用点）。保留 `pending_transition()` 兼容包装：只有 `kind == transition` 时返回 `PendingTransition`；新 `pending_save()` 供 runtime 使用。

`request_passive_allocation(id)` 与 `request_passive_refund(id)` 必须仅在 `awaiting_exit`、没有 pending、拥有 combat 时复制 `stable_state_`，调用 Task 2，更新副本的 progression 和 passive tree，安全递增 generation，放入 `PendingSaveKind::passive_tree`。在收到 committed 且 `same_run_state(verified, next)` 前不能改变 `stable_state_`。not_committed 清 pending、停在 `awaiting_exit` 并递增现有保存失败诊断；indeterminate/收据不匹配进入 fault。

房门与下坠改为创建 `PendingSaveKind::transition`；它们在任何 pending 时拒绝。`construct_current_room()` 调用 `passives::evaluate_passive_tree(stable_state_.passive_tree)`，把结果转换为 `CombatEncounterConfig::player_build`。`DungeonSnapshot` 添加只读 `PassiveTreeState passive_tree`、`bool passive_save_pending` 和最近 `PassiveTreeError`。

- [ ] **Step 4: 验证 dungeon 生命周期**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "dungeon.units|architecture.dungeon" --output-on-failure
git diff --check
```

Expected: dungeon suite 既有门/下坠黄金回放保持通过，新增星盘事务案例通过，dungeon 不可达 persistence 的边界仍通过。

- [ ] **Step 5: 提交 Task 5**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: transact passive tree changes between rooms"
```

### Task 6: 让 DungeonRuntime 服务所有 pending save 并覆盖 A/B 槽恢复

**Files:**
- Modify: `src/platform/raylib/dungeon_runtime.hpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes: Task 5 的 `pending_save()` 和 `resolve_pending_save()`，现有 `SaveStore::commit()`。
- Produces: `DungeonRuntime::service_pending_save()`；host 和所有 runtime 测试不再假定只有 transition 会写存档。

- [ ] **Step 1: 写 Runtime 事务 RED 测试**

在现有 runtime helper 基础上补入：

```cpp
arpg::test::Failure committed_passive_save_survives_runtime_restart() noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory));
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(*runtime.session()));
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(8U));
    runtime.service_pending_save();
    const auto saved = runtime.session()->snapshot();
    ARPG_REQUIRE(saved.passive_tree.allocated_bits == ((1ULL << 0U) | (1ULL << 8U)));
    platform::DungeonRuntime resumed(config_for(directory, 999U));
    ARPG_REQUIRE(resumed.initialize());
    ARPG_REQUIRE(resumed.session()->snapshot().passive_tree.allocated_bits
        == saved.passive_tree.allocated_bits);
    return {};
}
```

增加 before_publish 返回 not_committed 时保持旧位图、after_publish indeterminate 时进入 fault、passive pending 时 Door/Descent 均被拒绝三例。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Expected: 编译失败，因为 `service_pending_save()` 与新 Session 接口未定义。

- [ ] **Step 3: 泛化 Runtime 的存档服务**

把 `service_pending_transition()` 重命名为 `service_pending_save()`，并将 `to_session_result()` 返回类型改为 `dungeon::PendingSaveResult`；它从 `session_->pending_save()` 取得下一状态，调用一次 `store_.commit()`，再把转换后的收据交给 `resolve_pending_save()`。保存指示器在开始时为 `saving`，committed 为 `saved`，not_committed 为 `error`，indeterminate 后 runtime 状态为 `faulted`。不要在 host/runtime 中重算点数、位图或 generation。

```cpp
void DungeonRuntime::service_pending_save() noexcept {
    if (state() != DungeonRuntimeState::running || !session_.has_value()) return;
    const auto pending = session_->pending_save();
    if (!pending.has_value()) return;
    status_.indicator = SaveIndicator::saving;
    const persistence::SaveCommitResult result = store_.commit(pending->next_state);
    sync_commit_status(result);
    session_->resolve_pending_save(to_session_result(result));
    if (session_->snapshot().phase == dungeon::RoomPhase::faulted) {
        state_ = DungeonRuntimeState::faulted;
    }
}
```

- [ ] **Step 4: 验证 Runtime 与存档集成**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "platform.view_math|persistence.units" --output-on-failure
git diff --check
```

Expected: 平台 runtime suite 与 persistence 双槽 fault suite 全部通过；重复调用 `service_pending_save()` 在无 pending 时无副作用。

- [ ] **Step 5: 提交 Task 6**

```powershell
git add src/platform/raylib/dungeon_runtime.* tests/platform/dungeon_runtime_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "feat: persist passive mutations through runtime"
```

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

### Task 8: 端到端回归、窗口验收和最终验证

**Files:**
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`
- Modify: `tests/persistence/dungeon_save_integration_tests.cpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Create: `docs/stage7/2026-07-14-result.md`

**Interfaces:**
- Consumes: Tasks 1–7 的已提交接口。
- Produces: 1000 房间确定性星盘回放、保存恢复端到端证明、Debug/Release 绿灯和可视验收记录。

- [ ] **Step 1: 写端到端 RED 回归**

在 `dungeon_stress_tests.cpp` 增加固定根种子用例。先定义精确 trace 和相等比较：

```cpp
struct PassiveStressRecord final {
    std::uint64_t room_seed{};
    std::uint64_t allocated_bits{};
    std::uint64_t generation{};
    int hp{};
    int barrier{};
};

bool same_record(const PassiveStressRecord& left,
    const PassiveStressRecord& right) noexcept {
    return left.room_seed == right.room_seed
        && left.allocated_bits == right.allocated_bits
        && left.generation == right.generation
        && left.hp == right.hp && left.barrier == right.barrier;
}
```

用固定根种子运行两个独立 session 1000 房间；每 25 房清房后按 `8,9,10,22,23,24,36,37,38,50,51,52` 的循环选择下一合法节点并提交；每房记录上述字段。逐项断言两个 1000 项 trace 相等，并断言每条记录的 `allocated_bits` 与 generation 均非零。

在 `dungeon_save_integration_tests.cpp` 加入：分配 8→9→10，注入 before_publish/after_publish，重启后只能得到提交前或验证后的完整位图；不能得到“点数减少但 bit 未置位”或“bit 置位但点数未减少”。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_persistence_tests arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R "dungeon.units|persistence.units|platform.view_math" --output-on-failure
```

Expected: 在最终接线前，至少一个新端到端测试因缺少完整星盘 transaction 或 trace 记录接线而失败；记录该 RED 原因后继续。

- [ ] **Step 3: 补齐 trace 的确定性接线**

让压力 helper 每次加点都通过 `PendingSaveKind::passive_tree` 的 committed 收据，再进行门转换；记录必须从 `session.snapshot()` 读取，不能从待提交状态读取。为 trace 循环实现以下索引规则，避免随机或隐式容器顺序：

```cpp
constexpr std::array<passives::PassiveNodeId, 12> kRouteCycle{{
    8U, 9U, 10U, 22U, 23U, 24U, 36U, 37U, 38U, 50U, 51U, 52U}};
const passives::PassiveNodeId next = kRouteCycle[
    static_cast<std::size_t>((room_index / 25U) % kRouteCycle.size())];
```

当循环请求的节点已分配或不相邻时，按目录邻接表选择该节点的最小未分配邻居；若没有未分配邻居则不变更位图。测试必须仍完成 1000 房间，且两个 trace 完全相等。不得跳过 fault point、使用随机 seed 或降低房间数量。

- [ ] **Step 4: 运行完整 Debug 与 Release 验证**

Run:

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --preset windows-msvc-debug && cmake --build --preset windows-msvc-debug && ctest --preset windows-msvc-debug'
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --preset windows-msvc-release && cmake --build --preset windows-msvc-release && ctest --preset windows-msvc-release'
git diff --check
git status --short
```

Expected: Debug 与 Release 全部 CTest 通过，`git diff --check` 无输出；只允许本任务有关的已跟踪修改，保留用户已有未跟踪文件不处理。

- [ ] **Step 5: 可视验收、记录与提交 Task 8**

启动 `out/build/windows-msvc-release/bin/arpg_game.exe`，清房后按 P，点击路线入口 8，再点击 9、10；确认覆盖层显示已分配连线、未分配点减少、保存指示完成，点击 10 退款，确认点数返还；关闭 P 并选门，下一房显示相同构筑。再在重启后确认位图保留。

把 Debug/Release CTest 计数、1000 房间 trace 结果、窗口的 P 键/加点/退款/重启观察和已知限制写入 `docs/stage7/2026-07-14-result.md`。历史重构报告继续保持“当时未实施 Stage 7”的事实，不修改它。

```powershell
git add tests docs/stage7/2026-07-14-result.md
git commit -m "test: verify passive star chart integration"
```

## Plan Self-Review

- 规格覆盖：Task 1–3 覆盖五分量、抗性、通用属性与四关键节点；Task 2 覆盖 64 节点与连通；Task 4–6 覆盖存档迁移与双槽事务；Task 7 覆盖 P 键和全屏 UI；Task 8 覆盖压力、窗口、Debug/Release 验收。
- 占位扫描：没有未决占位词或含糊实现指令；每个代码任务列出路径、接口、RED、GREEN、命令与提交。
- 类型一致性：`passives` 只输出 `modifiers::PlayerModifierValues`；`combat` 只接收 `PlayerCombatBuild`；`dungeon` 持有 `PassiveTreeState`；`persistence` 只编码该状态；`platform` 只请求 Session mutation。

## Execution Handoff

计划已完成并保存至 `docs/superpowers/plans/2026-07-14-stage7-passive-star-chart.md`。执行方式二选一：

1. **子代理分任务执行（推荐）**：每个任务使用独立子代理，主代理在任务间审查与验证。
2. **当前会话串行执行**：使用 `executing-plans` 按任务分批执行并设检查点。

Which approach?
