# Stage 10 正式深渊房间战斗 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把现有仅展示 `is_abyss` 的房间升级为房间级、一次机会、可原子存档、可确定性重载的正式深渊挑战，并完整实现九条规则、强化遭遇和 1～3 件宝箱奖励。

**Architecture:** 新建无 raylib、无存档 I/O 的 `arpg_abyss` 纯规则库，集中负责深渊判定、危险/规则抽取、数值缩放、奖励权重与确定性种子。`arpg_dungeon` 持有生命周期、存档请求、奖励落地和离房确认；`arpg_combat` 只消费已求值的 `AbyssCombatConfig`；`arpg_persistence` 只编解码稳定检查点；raylib 只读取快照并绘制。

**Tech Stack:** C++17、raylib 6.0、CMake 3.25/Ninja、MSVC 19.44、Windows SDK 10.0.26100.0、现有轻量测试框架、A/B 原子存档。

## Global Constraints

- 以 `docs/superpowers/specs/2026-07-16-stage10-abyss-combat-design.md` 为唯一产品规格；本计划不扩展 Stage 11。
- 初始房和洞口下层目标永不成为深渊；深渊只允许由四向门目标的固定 1% 命名域判定产生。
- 不改变生态、洞口、普通遭遇、普通词缀、普通掉落的既有随机结果；所有新增随机均使用明确命名域与固定 `abyss_rules_version = 1`。
- 战斗 tick 不分配堆内存；所有运行时容器固定容量。测试和存档编解码可以使用现有 `std::vector`。
- 所有一次机会状态变化先原子保存、后发布运行时副作用；存档失败进入 `RoomPhase::faulted`，不能继续挑战或领取奖励。
- 每个任务严格执行 Red → Green → Refactor；先运行新增测试看到预期失败，再写最小实现，再运行局部和回归测试。
- 不删除旧版本解码路径；V1/V2/V3/V4 均要迁移到 V5。
- 每个任务完成后只提交该任务相关文件，禁止顺手重构无关模块。

---

## Task 1: 建立纯 `arpg_abyss` 规则核

**Files:**

- Create: `src/abyss/CMakeLists.txt`
- Create: `src/abyss/abyss_types.hpp`
- Create: `src/abyss/abyss_rules.hpp`
- Create: `src/abyss/abyss_rules.cpp`
- Create: `src/abyss/abyss_rewards.hpp`
- Create: `src/abyss/abyss_rewards.cpp`
- Create: `tests/abyss/CMakeLists.txt`
- Create: `tests/abyss/abyss_test_main.cpp`
- Create: `tests/abyss/abyss_rules_tests.cpp`
- Create: `tests/abyss/abyss_rewards_tests.cpp`
- Modify: `CMakeLists.txt`

**Produces:** `AbyssDanger`, `AbyssRuleId`, `AbyssLifecycle`, `AbyssCombatConfig`、固定深度权重、规则抽取、比例生命映射、环境伤害原始值、宝箱件数/物品等级/稀有度偏移。

**Consumes:** `core::DeterministicRng` 和 `modifiers::DamageType`；不得包含 `dungeon/`、`persistence/` 或 raylib 头文件。

`CMakeLists.txt` 必须在 `src/core` 之后、`src/combat` 之前加入 `src/abyss`；`arpg_abyss` 只链接 `arpg_core`。后续由 `arpg_combat` PUBLIC 链接 `arpg_abyss`，不能形成 `abyss → combat → abyss` 环。

- [ ] 写 28 个纯函数单测，覆盖四段危险权重、每段边界、每个危险等级三条规则的可达性、同种子稳定性、不同命名域互不漂移、`ceil(1.5x)`、深度词缀下限、整数生命比例映射、三档宝箱和稀有度重分配。

危险权重必须固化为 `1～9: 70/25/5`、`10～19: 45/40/15`、`20～39: 25/45/30`、`40+: 10/35/55`（低/中/高）；选中危险后在该档三条规则中等权选择。低危为 Thunderstorm/Swift Pursuit/Heavy Steps，中危为 Hunting Flames/Abyss Bulwark/Exhausted Recovery，高危为 Chaos Expansion/Abyss Fury/Life Sacrifice。

核心公共类型按以下稳定枚举值创建；这些值会进入 V5 存档，后续不得重排：

```cpp
inline constexpr std::uint32_t kAbyssRulesVersion = 1U;

enum class AbyssDanger : std::uint8_t { low = 0, medium = 1, high = 2 };

enum class AbyssRuleId : std::uint8_t {
    thunderstorm = 0,
    hunting_flames = 1,
    chaos_expansion = 2,
    swift_pursuit = 3,
    abyss_bulwark = 4,
    abyss_fury = 5,
    heavy_steps = 6,
    exhausted_recovery = 7,
    life_sacrifice = 8,
    none = 0xFF,
};

enum class AbyssLifecycle : std::uint8_t {
    none = 0, available = 1, started = 2, cleared = 3, failed = 4,
};

struct AbyssCombatConfig final {
    AbyssRuleId rule{AbyssRuleId::none};
    std::uint16_t player_ground_move_bp{10000};
    std::uint16_t player_resource_restore_bp{10000};
    std::uint16_t player_max_health_bp{10000};
    std::uint16_t monster_move_bp{10000};
    std::uint16_t monster_cooldown_bp{10000};
    std::uint16_t monster_armor_bp{10000};
    std::uint16_t monster_extra_shield_bp{};
    std::uint16_t monster_damage_bp{10000};
    std::uint16_t monster_attack_speed_bp{10000};
};

struct AbyssSelection final {
    AbyssDanger danger{AbyssDanger::low};
    AbyssRuleId rule{AbyssRuleId::thunderstorm};
    std::uint32_t rules_version{kAbyssRulesVersion};
};
```

- [ ] 先只声明接口并接入 CMake，运行：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests
```

预期：链接失败，缺少 `select_abyss_rule`、`map_resource_ratio`、`shift_abyss_rarity_weights` 等实现，证明测试先红。

- [ ] 实现下列无状态接口，所有乘法先提升到 `std::uint64_t` 或已检查的 `std::int64_t`：

```cpp
[[nodiscard]] bool is_abyss_roll(std::uint64_t room_seed) noexcept;
[[nodiscard]] std::optional<AbyssSelection> select_abyss_rule(
    std::uint64_t room_seed, std::uint64_t depth) noexcept;
[[nodiscard]] std::uint8_t abyss_encounter_budget(
    std::uint8_t normal_budget) noexcept;
[[nodiscard]] std::uint8_t minimum_abyss_affixes(
    std::uint64_t depth) noexcept;
[[nodiscard]] std::optional<int> map_resource_ratio(
    int current, int old_max, int new_max, bool alive) noexcept;
[[nodiscard]] std::optional<int> percent_of_actual_max_hp(
    int actual_max_hp, std::uint16_t basis_points) noexcept;
[[nodiscard]] AbyssCombatConfig combat_config_for(
    AbyssRuleId rule) noexcept;
[[nodiscard]] AbyssRewardProfile reward_profile_for(
    AbyssDanger danger, std::uint8_t base_item_level) noexcept;
[[nodiscard]] std::optional<AbyssRarityWeights> shift_abyss_rarity_weights(
    AbyssRarityWeights base, AbyssDanger danger) noexcept;
```

`map_resource_ratio` 必须精确计算 `floor((current * new_max + old_max / 2) / old_max)`，存活角色最少为 1；稀有度重分配必须保持总和 100，并以最大余数法处理普通权重不足。

奖励 profile 固定为低危 `1 件/+1 ilvl/白蓝黄 -10/+5/+5`，中危 `2 件/+3/-20/+10/+10`，高危 `3 件/+5/-30/+10/+20`，最终 ilvl 上限 100。

- [ ] 运行局部测试，预期 `28 cases, 0 failures`：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests
& .\out\build\windows-msvc-debug\bin\arpg_abyss_tests.exe
```

- [ ] 提交：

```powershell
git add CMakeLists.txt src/abyss tests/abyss
git commit -m "feat: add deterministic abyss rules core"
```

---

## Task 2: 升级 V5 检查点并迁移旧存档

**Files:**

- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Create: `src/dungeon/abyss_checkpoint_migration.hpp`
- Create: `src/dungeon/abyss_checkpoint_migration.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/save_store.hpp`
- Modify: `src/persistence/save_store_detail.hpp`
- Modify: `src/persistence/save_slot.cpp`
- Modify: `src/persistence/save_recovery.cpp`
- Modify: `tests/persistence/checkpoint_codec_tests.cpp`
- Modify: `tests/persistence/save_store_tests.cpp`
- Modify: `tests/persistence/save_store_fault_tests.cpp`
- Modify: `tests/persistence/persistence_test_main.cpp`

**Produces:** V5 的当前深渊状态、三位奖励掩码和最近一次稳定结算摘要；V1～V4 确定性迁移。

**Consumes:** Task 1 稳定枚举；存档层不得包含战斗运行时类型。

- [ ] 先增加 15 个测试：V5 全字段往返、每个非法枚举/布尔/掩码拒绝、CRC 拒绝、V1～V4 迁移、非法初始/下层深渊清除、`started` 载入可被上层识别、摘要往返、A/B 发布故障不伪造提交。

在 `DungeonRunState` 增加：

```cpp
struct AbyssCheckpoint final {
    abyss::AbyssLifecycle lifecycle{abyss::AbyssLifecycle::none};
    abyss::AbyssDanger danger{abyss::AbyssDanger::low};
    abyss::AbyssRuleId rule{abyss::AbyssRuleId::none};
    std::uint32_t rules_version{};
    std::uint8_t reward_total{};
    std::uint8_t generated_mask{};
    std::uint8_t claimed_mask{};
    std::uint8_t abandoned_mask{};
    std::uint32_t reward_revision{};
};

struct LastAbyssResolution final {
    bool valid{};
    std::uint64_t room_seed{};
    abyss::AbyssRuleId rule{abyss::AbyssRuleId::none};
    std::uint8_t total{};
    std::uint8_t generated{};
    std::uint8_t claimed{};
    std::uint8_t abandoned{};
};
```

`generated_mask | claimed_mask | abandoned_mask` 只能使用低三位和 `reward_total` 对应的有效位；`claimed_mask` 必须是 `generated_mask` 的子集；`abandoned_mask` 只能标记从未生成的奖励，因此必须与 `generated_mask` 不相交。终态摘要满足 `popcount(generated_mask) + popcount(abandoned_mask) == reward_total`，其中已生成未领取数由 `generated - claimed` 表示。

状态校验还必须保证：`available/started/cleared` 对应 `current_room.is_abyss=true`；`none/failed` 不允许正在运行的深渊规则；非 none 状态的 `rules_version` 必须等于 `kAbyssRulesVersion`，且 rule/danger 必须与 room seed + depth 的确定性重建结果一致。

`DecodeResult` 和 `SaveLoadResult` 各增加 `bool migrated`，并由 slot scan/recovery 原样传播。legacy codec 只保留旧字段并标记 migrated，不调用深渊生成逻辑；`dungeon::migrate_legacy_abyss_checkpoint` 再把旧状态转换为合法 V5 值。这样 persistence 仍只依赖稳定 checkpoint 值类型，runtime 也能区分“本来就是 V5 普通房”和“需要发布迁移结果的旧房”。

- [ ] 运行新增测试并确认先红：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_persistence_tests
& .\out\build\windows-msvc-debug\bin\arpg_persistence_tests.exe
```

预期：V5 版本号、负载长度和深渊字段断言失败。

- [ ] 将 `kCheckpointFormatVersion` 提升到 5，保留 V4 magic/长度常量并添加 V5 magic/长度；编码严格按固定字段顺序写入，解码先验证长度、版本、CRC，再验证状态不变量。

- [ ] V1～V4 codec 解码后只设置 `migrated=true`。随后由 `dungeon::migrate_legacy_abyss_checkpoint` 处理：非深渊房为 `none`；合法四向门目标深渊根据 room seed + depth 派生并写入 rule/danger/version，状态为 `available`；旧 `current_room.is_abyss` 且 `entry == initial` 或 `last_transition == descent` 时清除 `is_abyss` 并置 `none`，保留 seed、索引、生态、洞口、偏向、进度和物品。runtime 必须先把这一合法 V5 迁移结果原子提交成功，之后才创建 session。

- [ ] 运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_persistence_tests
ctest --preset windows-msvc-debug -R "persistence.units" --output-on-failure
```

预期：persistence 局部测试全部通过，V1～V4 golden bytes 仍可读取。

- [ ] 提交：

```powershell
git add src/dungeon/dungeon_checkpoint.hpp src/persistence tests/persistence
git commit -m "feat: persist abyss lifecycle in checkpoint v5"
```

---

## Task 3: 固定四向门预告并排除初始房/下层房

**Files:**

- Modify: `src/dungeon/room_generation.hpp`
- Modify: `src/dungeon/room_generation.cpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `tests/dungeon/room_generation_tests.cpp`
- Modify: `tests/dungeon/dungeon_navigation_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Produces:** `AbyssDoorPreview` 四向快照，门预告与实际目标完全一致。

**Consumes:** `derive_door_room_seed` 和 Task 1 的 `is_abyss_roll`；不消费任何现有顺序 RNG。

- [ ] 增加 10 个测试，覆盖四方向 seed、0～4 扇深渊门、预告/提交一致、初始房 false、下层目标 false、洞与深渊共存、bias 变化不改变深渊门、门预告不泄露规则。

新增纯函数和快照字段：

```cpp
[[nodiscard]] std::array<bool, 4> preview_abyss_doors(
    const checkpoint::RoomDescriptor& current) noexcept;

struct DungeonSnapshot final {
    std::array<bool, 4> abyss_doors{};
};
```

`preview_abyss_doors` 对每个方向使用 `derive_door_room_seed(current.seed, current.index + 1, direction)`，随后调用 `is_abyss_roll`。目标房生成不再从 `generate_room_descriptor` 的旧 `samples.abyss` 决定资格：门路径显式传入预告结果；初始和 descent 路径显式写 `false`。门 transition 的 next state 若命中深渊，还必须用目标 room seed + depth 写入 available、rule、danger 和 rules version，但快照的门预告只暴露布尔图标。

- [ ] 先运行并观察预告字段/初始房断言失败：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
& .\out\build\windows-msvc-debug\bin\arpg_dungeon_tests.exe
```

- [ ] 实现后运行 dungeon 与 persistence 回归：

```powershell
ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
```

预期：两组通过；旧 ecology/hole golden samples 不变。

- [ ] 提交：

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: preview deterministic abyss door targets"
```

---

## Task 4: 实现一次机会生命周期与原子启动/失败门禁

**Files:**

- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/dungeon/dungeon_transaction_tests.cpp`
- Modify: `tests/dungeon/dungeon_lifecycle_tests.cpp`
- Modify: `tests/combat/player_health_tests.cpp`
- Modify: `tests/persistence/dungeon_save_integration_tests.cpp`

**Produces:** `available/started/failed` 状态机、进入前预计算、死亡/R/异常退出失败、故障矩阵。

**Consumes:** V5 `AbyssCheckpoint` 和现有 `PendingSave`/`SaveStore::commit`。

- [ ] 扩展 `PendingSaveKind`：

```cpp
enum class PendingSaveKind : std::uint8_t {
    transition,
    passive_tree,
    loot_pickup,
    equipment,
    recipe,
    abyss_start,
    abyss_fail,
    abyss_clear,
    abyss_reward_materialized,
    abyss_reward_claim,
    abyss_abandon,
};
```

同时在 `DungeonFault` 增加 `invalid_abyss_state`、`abyss_generation_failed`、`abyss_reward_collision`、`abyss_reward_revision_overflow`；这些故障都必须进入 faulted，不能降级为普通房或重投随机数。

- [ ] 增加 18 个事务测试，逐项覆盖：普通门原行为、深渊门目标先以 `available` 完成普通房间切换、available 房自动排队 `started`、started 提交前无战斗副作用、not_committed/indeterminate 均 fault、receipt mismatch fault、成功后才构造战斗、R 先存 failed、玩家死亡先存 failed、失败后同 seed 普通房、load-started 自动提交 failed、load-available 自动启动、自动失败提交失败 fault、initial/descent 旧非法标记需要迁移提交。

- [ ] 先运行测试，预期深渊门仍走普通 `transition`、R 立即重建房间，新增断言失败。

- [ ] `prepare_transition` 仍先用现有 transition 事务提交目标房；若门预告为深渊，其 `next_state.current_room.is_abyss=true` 且 lifecycle 为 `available`。transition receipt 成功后进入目标房，但不生成怪物、不应用规则。

- [ ] `construct_current_room` 看到 `available` 时先完整预计算并验证 `AbyssSelection`、强化遭遇和奖励 profile；全部有效后创建 `PendingSave{kind=abyss_start}`，把同一房 lifecycle 改为 `started`。started receipt 成功后才封门、应用规则、加载怪物并进入 combat；失败调用现有 `enter_fault`。这样严格保持 `available → started` 两个稳定检查点。

- [ ] 给战斗增加 `CombatEventKind::player_defeated`：生命可降到 0，只发一次事件。普通房收到该事件执行现有普通重置；`started` 深渊房创建 `abyss_fail` 保存请求，保存成功后清除 `is_abyss`、将 lifecycle 置 `failed`，再调用同一个普通重置内部函数。

- [ ] 将 `reset_current_room()` 改为返回 `RequestResult`。普通房仍立即重置；started 深渊房只排队 `abyss_fail`，成功 receipt 前保持 `committing`，杜绝 R 绕过一次机会。

- [ ] `DungeonRuntime::initialize` 载入 `started` 后不构造挑战，先把同 seed 状态转为 failed 并调用 `SaveStore::commit`；只有成功发布后创建普通房 session。载入 V5 `available` 时由 session 自动执行 started 门禁。若 `SaveLoadResult::migrated`，先调用 dungeon migration helper 并原子提交合法 V5 结果；提交成功后才创建 session，即使结果为 available 也不能把迁移提交和 started 提交合并。迁移发布或自动失败发布失败时 runtime 为 `faulted`。

- [ ] 运行故障矩阵：

```powershell
ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
```

预期：每个 `SaveFaultPoint` 下都只有“旧稳定状态”或“新稳定状态”，不存在挑战已开始但存档仍 available 的可玩状态。

- [ ] 提交：

```powershell
git add src/dungeon src/combat src/platform/raylib/dungeon_runtime.cpp tests/dungeon tests/persistence
git commit -m "feat: gate abyss attempts behind atomic saves"
```

---

## Task 5: 强化遭遇预算并补足普通怪物词缀

**Files:**

- Modify: `src/dungeon/encounter_director.hpp`
- Modify: `src/dungeon/encounter_director.cpp`
- Modify: `src/dungeon/encounter_budget.cpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/combat/monster_affix_generation.hpp`
- Modify: `src/combat/monster_affix_generation.cpp`
- Modify: `tests/dungeon/encounter_director_tests.cpp`
- Modify: `tests/dungeon/dungeon_wave_tests.cpp`
- Modify: `tests/combat/monster_affix_generation_tests.cpp`

**Produces:** `ceil(normal_budget * 1.5)`、每怪先普通词缀后深渊补足的稳定结果。

**Consumes:** 普通 `build_encounter_plan`、普通 `generate_monster_affixes` 与 Task 1 深度下限。

- [ ] 增加测试：预算 8→12、9→14、24→36 且合法性上限同步为 `ceil(config.max_budget*1.5)`；深度 1/19 至少 1 条、20/39 至少 2 条、40+ 至少 3 条；原本已满足时逐字节不变；补足不重复词缀；同 seed 稳定；补足不改变普通词缀前缀；96 怪上限合法。

新增接口：

```cpp
[[nodiscard]] EncounterPlanResult build_abyss_encounter_plan(
    std::uint64_t room_seed,
    std::uint64_t depth,
    checkpoint::DungeonElement ecology,
    const EncounterDirectorConfig& config) noexcept;

[[nodiscard]] std::optional<MonsterAffixSet> supplement_abyss_affixes(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint8_t wave_index,
    std::uint8_t spawn_index,
    const MonsterDefinition& monster,
    MonsterAffixSet normal) noexcept;
```

- [ ] 先确认预算与词缀下限测试失败，再实现独立命名域。不得重新调用或重抽普通词缀；从 `normal.count` 开始补齐，保持已有元素和顺序完全不变。

- [ ] 补足时逐个输出位置分别派生“词缀选择”和“tier”子域，继续复用 required tags、重复和冲突校验；候选不足立即返回 `nullopt` 并阻止 started 提交，不得静默少给词缀或退回普通遭遇。`MonsterAffixSet` 上限仍为 3，不扩大 Stage 9 高危词缀容量。

- [ ] 运行：

```powershell
ctest --preset windows-msvc-debug -R "combat.units|dungeon.units" --output-on-failure
```

预期：两组通过，Stage 9 的 affix golden 和掉落压力测试不漂移。

- [ ] 提交：

```powershell
git add src/combat src/dungeon tests/combat tests/dungeon
git commit -m "feat: strengthen abyss encounters deterministically"
```

---

## Task 6: 接入六条怪物/玩家数值规则

**Files:**

- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/player_simulation.cpp`
- Modify: `src/combat/monster_pool.cpp`
- Modify: `src/combat/monster_ai_common.hpp`
- Modify: `src/combat/monster_ai_melee.cpp`
- Modify: `src/combat/monster_ai_ranged.cpp`
- Modify: `src/combat/monster_ai_special.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Modify: `src/dungeon/room_combat_template.hpp`
- Modify: `src/dungeon/room_combat_template.cpp`
- Modify: `tests/combat/combat_config_tests.cpp`
- Modify: `tests/combat/player_health_tests.cpp`
- Modify: `tests/combat/movement_jump_tests.cpp`
- Modify: `tests/combat/monster_melee_tests.cpp`

**Produces:** Swift Pursuit、Abyss Bulwark、Abyss Fury、Heavy Steps、Exhausted Recovery、Life Sacrifice。

**Consumes:** `CombatEncounterConfig::abyss`，战斗层不得读取存档、room seed 或奖励状态。

- [ ] 在 `CombatEncounterConfig` 增加 `abyss::AbyssCombatConfig abyss{}`，并由 `make_combat_encounter_config` 显式传入。

- [ ] 将 `arpg_combat` PUBLIC 链接 `arpg_abyss`，因为 `combat_types.hpp` 的公共配置暴露 `AbyssCombatConfig`；确认 `arpg_abyss` 不反向链接 combat。

- [ ] 增加精确测试：

  - Swift：移动速度 115%，冷却 `ceil(base*8500/10000)`。
  - Bulwark：护甲值 130%，额外盾为实际最大生命 30%。
  - Fury：所有伤害 145%；telegraph/recovery/cooldown 为 `ceil(base*10000/14500)`；active ticks 不变。
  - Heavy Steps：仅地面水平移动 85%；跳跃、空中速度和攻击阶段不变。
  - Exhausted Recovery：进入时生命与护盾上限的 70%，未来资源恢复量 70%。
  - Life Sacrifice：最大生命 55%，当前生命按确认过的最近整数公式保持比例，至少 1；退出时反向映射，护盾不免费回满。

- [ ] 先运行并确认所有倍率仍为 100% 的失败结果。

- [ ] 实现统一整数缩放助手；不得在各 AI 文件复制浮点乘法。Fury 的 active ticks 明确绕过攻击速度缩放。Heavy Steps 只在 `PlayerState` 属于地面移动时缩放 `movement.x/y`，不修改空中速度或跳跃冲量。

- [ ] 所有 tick 缩放使用向上取整且结果至少 1 tick；深渊倍率与 Stage 9 affix profile 乘算，不能覆盖或重置已有 mighty/frenzy/swift/armored/shielding 数值。

- [ ] Fury 的伤害只经一个 `scale_monster_outgoing_damage(DamagePacket, basis_points)` 边界缩放一次；接触、投射物和怪物原生 hazard 都从该边界取得结果，禁止生成时和命中时重复乘 145%。Bulwark 同时提升 `MonsterAffixRuntimeProfile::armor_rating` 并把 `max_shield/current_shield` 增加实际 max HP 的 30%。

- [ ] 将所有“设置/恢复玩家资源”收口到 `CombatWorld::restore_player_resources(int hp, int barrier)`，由该函数应用 `player_resource_restore_bp`；初始 70% cap 只应用一次，跨 wave `reset_player_health=false` 不重复扣减。

- [ ] 运行：

```powershell
ctest --preset windows-msvc-debug -R "combat.units|dungeon.units" --output-on-failure
```

预期：新增规则测试通过，既有 J/K/L/WASD、浮空、闪避、护甲与词缀测试不变。

- [ ] 提交：

```powershell
git add src/combat src/dungeon/room_combat_template.* tests/combat
git commit -m "feat: apply abyss combat stat rules"
```

---

## Task 7: 实现三条固定容量环境规则

**Files:**

- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Create: `src/combat/abyss_environment.cpp`
- Modify: `src/combat/combat_snapshot.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Create: `tests/combat/abyss_environment_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `tests/combat/CMakeLists.txt`

**Produces:** Thunderstorm、Hunting Flames、Chaos Expansion 的无分配固定 tick 运行时与可绘制快照。

**Consumes:** 实际 `player_.max_hp`、`DamageDelivery::ground_or_environment` 和既有元素减伤；不走闪避。

- [ ] 新增 `AbyssEnvironmentRuntime` 固定状态，不使用动态容器：

```cpp
struct AbyssEnvironmentRuntime final {
    abyss::AbyssRuleId rule{abyss::AbyssRuleId::none};
    std::uint16_t cycle_tick{};
    std::uint16_t stage_tick{};
    Vec3 locked_center{};
    std::uint8_t expansion_stage{};
    bool warning{};
    bool active{};
};
```

- [ ] 增加测试并精确检查：雷暴 180 周期/45 预警/0.8 半径/15% 雷电；追猎火焰 240 周期/45 预警/1.0 半径/180 持续/每 60 tick 10% 火焰；混沌扩张 1.0、2.3、3.6、4.9、6.2 半径且每 180 tick 扩张、每 60 tick 8% 混沌。

- [ ] 测试地面环境伤害：100% 闪避仍命中；对应元素减伤生效；无敌帧仍遵循现有统一伤害语义；原始伤害为 `max(1, ceil(actual_max_hp * bp / 10000))`。

- [ ] 预填满 `HazardPool` 后触发环境规则，断言旧 hazard 不被覆盖、环境生成失败、`hazard_saturation_count` 加一且没有伤害/成功事件；释放一个槽后下一周期可正常生成。

- [ ] 先运行测试，预期环境事件和快照为空。

- [ ] 扩展 `HazardKind` 为 thunderstorm/hunting_flame/chaos_expansion，并为 `HazardRuntime` 增加 `HazardSource::monster/abyss_environment`。新增 `spawn_environment_hazard` 仍使用现有 `HazardPool` 固定容量，但不要求怪物 owner；怪物死亡清理只处理 monster source。池满时拒绝新区域、增加 `hazard_saturation_count`，不得覆盖旧对象或伪造成功。

- [ ] 实现 `simulate_abyss_environment()`：雷暴在挑战 tick 180 的瞬间锁定玩家位置并产生 45 tick 预警，此后每 180 tick 重复；追猎烈焰在 tick 240 锁点并预警 45 tick，随后生成持续 180 tick、每 60 tick 伤害一次的区域；混沌扩散从 tick 0 就建立半径 1.0 的中心区域，在 180/360/540/720 tick 更新为后四档并保持。所有命中统一调用 `apply_player_damage(packet, DamageDelivery::ground_or_environment, center, FeedbackLevel::heavy)`。

- [ ] 环境 hazard 不携带怪物 affix，也不调用 chain/corrosion/death trigger 路径，确保不会递归触发连锁闪电、腐蚀或直接命中词缀。

- [ ] 运行战斗回归：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
& .\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
```

预期：全部通过；新增 600 tick 分配探针为 0。

- [ ] 提交：

```powershell
git add src/combat tests/combat
git commit -m "feat: simulate abyss environment rules"
```

---

## Task 8: 深渊清场原子结算与资源还原

**Files:**

- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `tests/dungeon/dungeon_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_transaction_tests.cpp`

**Produces:** 最后一只怪死亡后先保存 cleared，再开放门/洞、移除规则和开始宝箱结算。

**Consumes:** `abyss_clear` pending save、Task 6 的资源比例映射。

- [ ] 增加测试：最后一怪死亡后 phase 立即变 committing；提交前门和洞封闭、规则仍在；commit 成功后 lifecycle cleared、规则关闭、生命比例映回普通最大生命、当前护盾只 clamp 不补满、门和洞开放；三类失败均 fault；普通房清场路径不改变。

- [ ] 把现有清场直接 `phase_=cleared` 分成 `prepare_room_clear()` 与 `publish_room_clear()`。普通房仍同步发布；started 深渊房构造 `abyss_clear` 保存请求，`next_state.abyss.reward_total` 由危险档固定为 1/2/3。

- [ ] 清场保存成功后才调用 `combat_->clear_abyss_rule_preserving_resources()`，随后发 `room_cleared` 和 `exits_opened`。任何 receipt 不一致不得发这两个事件。

- [ ] 运行：

```powershell
ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
```

预期：清场故障矩阵通过；洞与深渊同房时洞在成功清场后为 ready。

- [ ] 提交：

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: atomically settle cleared abyss rooms"
```

---

## Task 9: 生成确定性深渊宝箱并支持重载重建

**Files:**

- Create: `src/dungeon/abyss_reward.cpp`
- Create: `src/dungeon/abyss_reward.hpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Create: `tests/dungeon/dungeon_abyss_reward_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Produces:** 低/中/高 1/2/3 件、+1/+3/+5 物品等级、稀有度偏移、中心落地、池满重试、重载重建。

**Consumes:** `items::generate_item`、Task 1 奖励 profile、V5 三位掩码。

- [ ] 新增奖励计划结构：

```cpp
struct AbyssRewardSlot final {
    std::uint8_t slot_index{};
    items::ItemSlot item_slot{items::ItemSlot::weapon};
    items::ItemRarity rarity{items::ItemRarity::normal};
    std::uint8_t item_level{1};
    std::uint64_t item_seed{};
    std::uint64_t item_id{};
};

[[nodiscard]] std::optional<AbyssRewardSlot> derive_abyss_reward_slot(
    std::uint64_t room_seed,
    abyss::AbyssDanger danger,
    std::uint8_t base_item_level,
    std::uint8_t reward_ordinal) noexcept;
```

- [ ] 增加测试：三档数量/等级/权重；基础 item level 精确取当前房间 depth 并在加成后 cap100；每个 slot 独立随机且允许重复；同 room seed + ordinal + rules version 得到同内容；时间/build/retry 不参与；地面池空位只生成可放入数量；每 tick 最多尝试 3 次；池满不丢失、不推进 generated mask；清空槽位后继续；reload 后重建 generated 未 claimed 的相同物品。

- [ ] 先运行并确认深渊清场后无奖励。

- [ ] 用 room seed、深渊命名域、reward ordinal 和 rules version 分别派生 item seed、slot、rarity 与 item id；不要把 `next_item_sequence`、清场时间或重试次数混入内容。item id 使用独立深渊 ID 命名域，若与 ownership 现有 ID 碰撞则 fault，不得重投。奖励位置固定在房间中心的三个小偏移点，自动拾取仍复用 `request_nearby_pickups`。

- [ ] GroundItem 增加稳定来源：

```cpp
enum class GroundItemSource : std::uint8_t { monster_drop, abyss_chest };

struct GroundItem final {
    bool active{};
    std::uint16_t drop_ordinal{};
    GroundItemSource source{GroundItemSource::monster_drop};
    std::uint8_t abyss_reward_ordinal{0xFF};
    combat::Vec3 position{};
    items::ItemInstance item{};
};
```

- [ ] 每次成功物化 reward slot 后先创建 `abyss_reward_materialized` 保存请求，同时设置 generated bit 并递增 reward revision；保存成功才把地面物品设 active。这样 crash 不会出现未记录物品。加载 cleared 房时按 masks 重建未 claimed 且已 generated 的地面物品，并继续生成未 generated。

- [ ] 深渊宝箱是本任务唯一新增奖励源；普通怪物经验和掉落继续完全走 Stage 9 的 affix score 逻辑，深渊遭遇预算或房间规则不得额外修改它们。

- [ ] 运行：

```powershell
ctest --preset windows-msvc-debug -R "dungeon.units|items.units|persistence.units" --output-on-failure
```

预期：奖励、物品目录、存档回归全部通过；Stage 9 普通掉落 seed 和概率测试不变。

- [ ] 提交：

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: materialize deterministic abyss chest rewards"
```

---

## Task 10: 原子领取、背包满和离房二次确认放弃

**Files:**

- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `tests/dungeon/dungeon_item_transaction_tests.cpp`
- Modify: `tests/dungeon/dungeon_navigation_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_reward_tests.cpp`
- Modify: `tests/persistence/save_store_fault_tests.cpp`

**Produces:** 领取掩码、背包满留地、同门双触确认、放弃摘要、防复制故障矩阵。

**Consumes:** 普通 pickup 事务、`requested_exit` 和 `LastAbyssResolution`。

- [ ] 增加 transient 确认状态，不写存档：

```cpp
struct AbyssExitConfirmation final {
    bool armed{};
    TransitionKind transition{TransitionKind::none};
    ExitDirection direction{ExitDirection::none};
    std::uint32_t reward_revision{};
};
```

快照增加 `abyss_pending_rewards`、`abyss_unpicked_rewards`、`abyss_exit_confirmation_armed` 和确认方向。

- [ ] 增加测试：背包满时物品保持 ground；任意顺序领取都正确更新 claimed mask；领取提交失败不移除地面物品；第一触同门取消并提示计数；离开门范围、换方向、奖励状态变化使确认失效；revision 溢出进入 fault；第二次同门创建 `abyss_abandon`；同房洞口第一次 E 警告、离开洞范围失效、第二次 E 原子放弃并下层；成功后仅未生成槽位进入 abandoned mask，已生成未领取物品随普通离房销毁，摘要的 generated/claimed/abandoned 计数正确；放弃保存失败 fault 且仍在原房；下一次结算覆盖旧摘要。

- [ ] `request_pickup` 对 monster_drop 保持原路径；abyss_chest 的 `next_state` 同时加入物品、设置 claimed bit 并增加 reward revision，但不推进普通掉落使用的 `next_item_sequence`。receipt 成功才移除 ground item。

- [ ] `attempt_exit` 在 `cleared` 深渊存在 pending/unpicked 时：第一次只 arm 并 emit 警告事件；第二次且方向和 revision 相同才把所有未生成位写入 abandoned mask，并在同一个离房存档事务中提交。成功 receipt 后按普通门转换；已生成未领取物品按普通离房规则丢失但不写入 abandoned mask；不存在剩余奖励时直接转换。

- [ ] `request_descent` 复用同一确认状态，以 `TransitionKind::descent + ExitDirection::none` 作为确认键；第一次 E 只提示，玩家离开洞口范围后失效，第二次 E 才把 abandoned 和 descent 放进同一原子事务。奖励已全部领取时仍保持原来的一次 E 下层。

- [ ] `LastAbyssResolution` 只在离房前生成稳定摘要，保存 room seed、rule、total 和四个计数；下一次深渊 resolution 覆盖。

- [ ] 离房 next state 必须把当前房深渊 checkpoint 重置为 `none`，但保留刚写入的 `LastAbyssResolution`；普通门和 descent 都走同一清理逻辑，不能把 cleared/reward masks 泄漏到新房。

- [ ] 运行事务与故障测试：

```powershell
ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
```

预期：所有 fault point 下无重复物品、无凭空丢失 eligibility，放弃必须有明确已提交状态。

- [ ] 提交：

```powershell
git add src/dungeon tests/dungeon tests/persistence
git commit -m "feat: protect abyss reward claims and abandonment"
```

---

## Task 11: 完成 raylib 6.0 门标、规则警告与奖励提示

**Files:**

- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/dungeon_view_math.hpp`
- Modify: `src/platform/raylib/dungeon_view_math.cpp`
- Modify: `tests/platform/dungeon_view_math_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Produces:** 四向门深渊图标、房内危险/规则 HUD、环境预警范围、待领取/待生成计数和二次确认文案。

**Consumes:** `DungeonSnapshot`/`CombatSnapshot`；不得在 renderer 重新推导 seed、规则、伤害或奖励。

- [ ] 先写纯 view-math 测试：普通门无图标、四方向各自图标、门仍 locked/open 原样式、三个环境规则 warning/active 颜色与半径、确认提示计数与方向、没有规则详情泄漏到未进入的门。

- [ ] `draw_doors` 读取 `snapshot.abyss_doors[index]`，在箭头旁绘制统一紫红菱形图标，只表示目标房是深渊。

- [ ] `draw_abyss` 保留现有房间紫色脉冲，并增加当前危险等级/规则名称；环境 warning 使用 snapshot 的 locked center/radius 绘制地面圈，不调用 `GetRandomValue`。

- [ ] HUD 显示 `ABYSS LOW/MED/HIGH`、规则名、`Reward pending X / unpicked Y`；第一次触门显示 `再次进入同一扇门将放弃全部剩余奖励`，洞口确认则显示 `再次按 E 将放弃剩余奖励并下层`，位置、方向/transition 或 revision 失效后立即消失。

- [ ] 运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
ctest --preset windows-msvc-debug -R "platform.units|architecture" --output-on-failure
```

预期：platform 与所有 architecture 边界测试通过；core/combat/dungeon/persistence 均无 raylib 依赖。

- [ ] 提交：

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render abyss combat and reward state"
```

---

## Task 12: 1000 房确定性、600 tick 零分配与正式验证夹具

**Files:**

- Create: `tests/dungeon/dungeon_abyss_stress_tests.cpp`
- Create: `tests/dungeon/stage10_validation_fixture.cpp`
- Create: `tests/dungeon/stage10_validation_game.cpp`
- Create: `tests/dungeon/stage10_formal_game_validation.cpp`
- Create: `tests/dungeon/stage10_validation_capture_test.ps1`
- Create: `tests/dungeon/stage10_formal_game_capture_test.ps1`
- Create: `tests/dungeon/stage10_evidence_guard_test.cmake`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`

**Produces:** 可重复的 headless 轨迹、真实窗口 present 后截图、证据防注入守卫。

**Consumes:** 现有 Stage 8/9 validation capture 机制；不得伪造 combat defeat trace 或直接写最终截图。

- [ ] 压力测试遍历固定 root seed 的 1000 个房间；每 37 房执行真实 `encode_checkpoint`/`decode_checkpoint`，并验证门预告、实际 room descriptor、危险/规则、遭遇、奖励和摘要逐字段相同。

- [ ] 在 1000 房样本中统计四向门总判定次数和命中数，只验证固定 golden 数值与约 1% 的宽松统计护栏；概率正确性的严格证明仍由阈值边界单测负责。

- [ ] 构造最极端 96 怪、3 词缀、高危环境规则、满 projectile/hazard/ground pool 场景，启动 `AllocationProbe` 后运行 600 tick；预期 `allocations == 0`，无 fixed-pool 越界且 saturation 只增加诊断计数。

- [ ] validation fixture 必须走真实 session、SaveStore 和 pickup/abandon API，输出稳定文本证据：门预告 seed、started commit generation、rule/danger、环境伤害、clear commit、三件 reward id、reload masks、abandon summary。

- [ ] formal game validation 使用 raylib 窗口真实运行，先用纯生成 API 搜索固定会出现目标深渊规则的 root seeds，再只通过正常 MovementInput/Action/SaveStore 驱动窗口；至少捕获七张已 present 帧：深渊门图标、雷暴预警、追猎烈焰预警、混沌扩散区域、宝箱落地、待生成提示、离房二次确认。沿用 `validation_exit_after_presented_frames`，截图必须发生在 `EndDrawing` 后。

- [ ] 用临时存档分别跑死亡、R、started 状态重启和“深渊+洞口”四条正式路径；前三条重载后必须只剩同 seed 普通房，洞口路径必须在清场后完成奖励确认并成功下层。

- [ ] evidence guard 扫描夹具与 host，禁止调用测试私有访问器、直接注入 defeated event、直接改 phase/masks、用静态图片替代窗口截图。

- [ ] 先运行夹具并确认新 target 不存在，再接入 CMake；完成后运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_stage10_validation_fixture arpg_stage10_validation_game arpg_stage10_formal_game_validation
ctest --preset windows-msvc-debug -R "stage10" --output-on-failure
```

预期：Stage 10 headless fixture、evidence guard、present capture、formal game capture 全部通过并生成非空 PNG。

- [ ] 提交：

```powershell
git add tests/dungeon src/platform/raylib/raylib_host.*
git commit -m "test: add formal stage 10 abyss validation"
```

---

## Task 13: 全量验证、规格审计与里程碑封口

**Files:**

- Modify: `README.md`
- Create: `docs/validation/stage10-abyss-combat.md`
- Modify only if evidence finds a defect: Stage 10 files named in Tasks 1～12

**Produces:** Debug/Release 双配置证据、33 个既有 CTest 加 5 个 Stage 10 新测试入口的完整回归、规格逐条对应表。

- [ ] 运行格式/占位符/边界扫描：

```powershell
rg -n "TODO|FIXME|PLACEHOLDER|stage 10 later" src/abyss src/combat src/dungeon src/persistence src/platform/raylib tests/abyss tests/combat tests/dungeon tests/persistence tests/platform
rg -n "#include <raylib.h>|#include \"raylib.h\"" src/core src/abyss src/combat src/dungeon src/persistence
```

预期：第一条无输出；第二条无输出。

- [ ] 运行 Debug 全量：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
```

预期：构建 0 error，`38/38` CTest 通过。

- [ ] 运行 Release 全量：

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release --output-on-failure
```

预期：构建 0 error，`38/38` CTest 通过。

- [ ] 单独重跑压力与正式 raylib 证据，记录命令、退出码、耗时、截图路径、1000 房 golden 摘要和 600 tick 分配计数到 `docs/validation/stage10-abyss-combat.md`。

- [ ] 按设计文档第 4～18 节逐条审计并在验证文档中建立“规格条目 → 测试名/证据”对应表，特别复核：四门 1%、启动/失败/清场原子门禁、started 重载失败、洞共存、地面池满、背包满、同门二次确认、离房放弃、LastAbyssResolution、普通随机不漂移。

- [ ] 检查工作树只包含 Stage 10：

```powershell
git status --short
git diff --check
git log --oneline main..HEAD
```

预期：`git diff --check` 无输出；没有 `.scratch/`、`task3_trace_probe.obj`、构建目录或截图误入提交。

- [ ] 更新 README 的当前里程碑和操作说明，然后提交：

```powershell
git add README.md docs/validation/stage10-abyss-combat.md
git commit -m "docs: record stage 10 abyss validation"
```

- [ ] 最终确认：

```powershell
git status --short
```

预期：Stage 10 工作树干净。停在 `codex/stage10-abyss-combat`，不自动合并 main，不推进 Stage 11。

---

## 实施时的固定检查点

每完成一个任务，执行者必须报告：

1. 新增/修改的稳定接口。
2. Red 测试具体失败原因。
3. Green 后局部测试命令与结果。
4. 是否影响随机域、存档格式或零分配约束。
5. 提交哈希。

遇到存档 receipt、固定容量、规则语义或普通随机 golden 的冲突时，停止该任务并回到设计规格，不得通过放宽断言完成测试。
