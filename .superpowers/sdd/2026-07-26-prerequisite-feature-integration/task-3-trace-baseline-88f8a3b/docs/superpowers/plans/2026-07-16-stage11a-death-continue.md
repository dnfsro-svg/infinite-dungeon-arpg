# Stage 11-A Death and Continue Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task with review checkpoints.

**Goal:** 在 Stage 10 基线上实现可跨重启、不可重骰的死亡回顾、退回上一层和按 `E` 继续闭环，并以 V6 原子存档、1000 次压力测试及真实 raylib 五路径验收收口。

**Architecture:** Combat 只负责五类实际承伤账本、稳定致死来源和冻结的 `CombatDeathSnapshot`；Dungeon 将其与房间、深渊和永久角色状态组合为两次原子事务 `death_retreat` / `death_continue`；Persistence 只编码稳定 checkpoint 并迁移 V5；raylib 只读取 `DungeonSnapshot`、执行输入门禁和绘制死亡覆盖层。所有随机性由独立 `death_retreat` 命名域导出，目标完整写入第一次事务，重启和重试不得重骰。

**Tech Stack:** C++17、raylib 6.0、CMake、Ninja、CTest、MSVC 19.44、Windows SDK 10.0.26100.0、PowerShell、60 Hz 固定逻辑步长。

## Global Constraints

- 基线提交固定为 `a06eddbdf557b7209146f12ec1a6bc75a1aa75b3`；工作分支为 `codex/stage11a-death-continue`。
- 设计规格是 [2026-07-16-stage11a-death-continue-design.md](../specs/2026-07-16-stage11a-death-continue-design.md)。实现与规格冲突时先停下修正规格或计划，不得静默放宽约束。
- 每个任务严格 RED → GREEN → REFACTOR；先看到目标测试按预期失败，再写最小生产实现。
- 不删除已有文件，不重置 Git，不覆盖无关改动，不把 `build/`、截图、临时存档或探针对象提交。
- Combat 更新热路径、300 tick 账本、死亡冻结和 Dungeon 固定池处理不得分配堆内存。
- 玩家死亡与同 tick 清房冲突时，死亡优先；死亡后不得结算当前房经验、掉落或奖励。
- `not_committed` 可确定性重试；`indeterminate`、receipt kind/代数/状态不匹配必须 fault，不得猜测提交结果。
- 本阶段不实现设置、物品过滤、完整 HUD、macOS、首领、召唤、光环、新装备/词缀/怪物/深渊内容或全游戏性能重构。
- 每个任务提交前运行该任务列出的窄测试；最终任务必须从干净构建目录运行 Debug、Release 全量 CTest。

---

### Task 1: 建立 Combat 承伤解析与 300 tick 固定账本

**Files:**

- Create: `src/combat/player_damage_history.hpp`
- Create: `src/combat/player_damage_history.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Create: `tests/combat/player_damage_history_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `tests/combat/CMakeLists.txt`

**Step 1: 写解析与滚动窗口失败测试**

新增 `player_damage_history_suite()`，覆盖：

- `resolve_player_damage_packet()` 保留物理/火/水/电/混沌逐类型最终值，并让 `total` 等于五项饱和和。
- 护甲只处理物理，四元素分别使用现有减伤与上限，`damage_taken` 最后应用；结果与现有总伤害测试一致。
- 逐类型解析值是减伤后的“可结算伤害”；真正写入承伤历史前还必须受当前护盾+生命上限裁剪，过量伤害不得计入“实际承伤”。
- 同 tick 多次 `record()` 逐类型累加。
- 第 300 tick 仍在窗口，第 301 tick 精确淘汰最旧桶。
- `INT_MAX` 输入和重复累计饱和而不回绕。
- 构造、推进、记录、汇总期间 `allocation_probe` 为 0。

在 `combat_types.hpp` 先声明预期接口：

```cpp
struct ResolvedPlayerDamage final {
    std::array<std::uint64_t, modifiers::kDamageTypeCount> by_type{};
    std::uint64_t total{};
};

[[nodiscard]] std::optional<ResolvedPlayerDamage>
resolve_player_damage_packet(
    DamagePacket packet, const PlayerCombatBuild& build) noexcept;
```

在新头文件声明：

```cpp
inline constexpr std::size_t kPlayerDamageHistoryTicks = 300U;

class PlayerDamageHistory final {
public:
    void begin_tick(std::uint64_t tick) noexcept;
    void record(const ResolvedPlayerDamage& damage) noexcept;
    [[nodiscard]] std::array<std::uint64_t,
        modifiers::kDamageTypeCount> totals() const noexcept;
private:
    std::array<std::array<std::uint64_t, modifiers::kDamageTypeCount>,
        kPlayerDamageHistoryTicks> buckets_{};
    std::uint64_t active_tick_{};
    bool initialized_{};
};
```

**Step 2: 运行测试并确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
```

Expected: 编译或链接失败，明确缺少 `resolve_player_damage_packet` / `PlayerDamageHistory` 实现；不得通过删测试规避。

**Step 3: 实现逐类型解析和固定账本**

- 把现有 `resolve_player_damage()` 的计算拆成逐类型结果；保留旧函数作为 `total` 的范围检查兼容包装，避免一次改坏既有调用者。
- `PlayerDamageHistory::begin_tick()` 对上次 tick 与新 tick 之间每个被跨过的 `tick % 300` 桶逐一清零；若倒退或跳跃达到 300 tick，清空全部桶；同 tick 重复调用不得清除已记内容。
- 使用本项目已有饱和工具或局部无异常 `saturating_add_u64`；不使用 `vector`、`deque`、字符串或异常路径。
- 在 `src/combat/CMakeLists.txt` 注册新 `.cpp`。

**Step 4: 运行 GREEN 并回归 Combat**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
ctest --preset windows-msvc-debug -R '^combat\.units$' --output-on-failure
```

Expected: `combat.units` 通过，旧伤害/防御测试无变化，账本零分配断言通过。

**Step 5: 提交**

```powershell
git add src/combat tests/combat
git commit -m "feat: add fixed player damage history"
```

---

### Task 2: 在 CombatWorld 接入稳定来源、最后一击与死亡冻结

**Files:**

- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/monster_ai_melee.cpp`
- Modify: `src/combat/monster_ai_ranged.cpp`
- Modify: `src/combat/monster_ai_special.cpp`
- Modify: `src/combat/monster_affix_runtime.cpp`
- Modify: `src/combat/abyss_environment.cpp`
- Create: `tests/combat/player_death_snapshot_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `tests/combat/CMakeLists.txt`

**Step 1: 定义不含 Dungeon 语义的死亡数据**

在 `combat_types.hpp` 增加：

```cpp
enum class PlayerDamageSourceKind : std::uint8_t {
    monster_attack,
    projectile,
    ground_hazard,
    monster_affix,
    abyss_environment,
    unknown,
};

struct PlayerDamageSource final {
    PlayerDamageSourceKind kind{PlayerDamageSourceKind::unknown};
    MonsterId monster{MonsterId::count};
    std::uint16_t detail_id{};
};

struct PlayerDefenseSnapshot final {
    int hp{};
    int max_hp{};
    int barrier{};
    int max_barrier{};
    std::int64_t armor{};
    std::int64_t evasion{};
    std::int32_t armor_reduction_bp{};
    std::int32_t evasion_rate_bp{};
    std::array<std::int32_t, modifiers::kElementCount> damage_reduction{};
    std::array<std::int32_t, modifiers::kElementCount> damage_reduction_cap{};
};

struct CombatDeathSnapshot final {
    std::uint64_t tick{};
    PlayerDamageSource source{};
    modifiers::DamageType primary_type{modifiers::DamageType::physical};
    std::uint64_t raw_damage{};
    std::uint64_t barrier_loss{};
    std::uint64_t health_loss{};
    std::uint64_t final_damage{};
    std::array<std::uint64_t, modifiers::kDamageTypeCount> recent_damage{};
    PlayerDefenseSnapshot defense{};
};
```

`primary_type` 固定取最后一击经过资源上限裁剪后的实际逐类型值最大者，相等时按物理、火、水、电、混沌顺序。`raw_damage` 为最后一击正数原始分量的饱和和，`final_damage = barrier_loss + health_loss`。

**Step 2: 写 CombatWorld 失败测试**

覆盖：

- 怪物近战、投射物、地面危险、怪物词条、深渊环境分别生成正确 `kind` 和稳定 ID；无法归属时显式 `unknown`。
- 闪避命中不进入 5 秒账本。
- 护盾全吸收、部分吸收、穿透到生命的最后一击拆分正确。
- 1000 点过量命中只对剩余 30 护盾+20 生命记录 50 点实际承伤；五类型实际分配之和严格等于 50。
- 第一笔致死伤害冻结 snapshot；同 tick 后续危险不再扣血或覆盖来源。
- 死亡后连续 600 tick：怪物、投射物、危险区、状态计时、账本和 snapshot 均不变化，输入队列不再执行。
- 最后一只怪物与玩家同 tick 死亡时 `player_defeated()` 为真，Dungeon 后续可优先处理死亡。
- 完整致死 tick 的 allocation count 为 0。

**Step 3: 确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
```

Expected: 缺少 `death_snapshot()`、来源参数或新 suite，构建失败。

**Step 4: 接入所有伤害生产路径**

- 将核心入口改为：

```cpp
bool apply_player_damage(
    DamagePacket damage,
    DamageDelivery delivery,
    PlayerDamageSource source,
    Vec3 source_position,
    FeedbackLevel feedback) noexcept;
```

- 每个生产者在创建伤害时传入稳定来源；投射物沿用 owner `MonsterHandle` 和攻击/投射物枚举，危险区使用 `HazardKind`，词条使用词条 ID，深渊环境使用 `AbyssRuleId`。目录无法解析的 owner 写 `unknown`，不得退回默认怪物。
- 每个固定 tick 在伤害结算前推进账本。闪避、无敌或零最终伤害不记录。
- 先算 `actual_total = min(resolved.total, barrier + hp)`，再按 resolved 五类型占比做整数向下分配；余数按物理、火、水、电、混沌固定顺序补到仍有 resolved 容量的类型。只有这份 resource-capped `actual_by_type` 写入账本，保证五项和严格等于实际护盾损失+生命损失且过量伤害不膨胀统计。
- 按现有护盾→生命顺序扣除 `actual_total`；致死时冻结账本汇总与扣除后的防御字段。
- 在 `CombatWorld` 暴露只读接口：

```cpp
[[nodiscard]] const std::optional<CombatDeathSnapshot>&
death_snapshot() const noexcept;
```

- `tick()` 顶部若已有 death snapshot，仅增加 Dungeon 所需的外层 session tick，不推进任何 Combat 状态；不得反复发 `player_defeated`。

**Step 5: 运行 GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
ctest --preset windows-msvc-debug -R '^combat\.units$' --output-on-failure
```

Expected: Combat 全套通过，来源矩阵、冻结和零分配通过。

**Step 6: 提交**

```powershell
git add src/combat tests/combat
git commit -m "feat: freeze combat death snapshots"
```

---

### Task 3: 增加稳定 DeathCheckpoint 与确定性退层生成器

**Files:**

- Create: `src/dungeon/death_checkpoint.hpp`
- Create: `src/dungeon/death_checkpoint.cpp`
- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `src/dungeon/dungeon_progression.cpp`
- Modify: `src/dungeon/room_generation.hpp`
- Modify: `src/dungeon/room_generation.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Create: `tests/dungeon/death_checkpoint_tests.cpp`
- Modify: `tests/dungeon/dungeon_progression_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`

**Step 1: 写稳定状态和随机域失败测试**

在 `dungeon_checkpoint.hpp` 的 `dungeon::checkpoint` 命名空间定义稳定类型；`death_checkpoint.hpp/.cpp` 只承载 Combat→checkpoint 转换和需要 Dungeon/目录语义的校验，避免 Persistence 反向链接 `arpg_dungeon`：

```cpp
enum class DeathLifecycle : std::uint8_t { none, pending_continue };
enum class DeathSourceKind : std::uint8_t {
    monster_attack, projectile, ground_hazard,
    monster_affix, abyss_environment, unknown,
};
enum class DeathDamageType : std::uint8_t {
    physical, fire, water, lightning, chaos,
};

struct DeathCheckpoint final {
    DeathLifecycle lifecycle{DeathLifecycle::none};
    std::uint8_t data_version{};
    std::uint64_t death_depth{};
    std::uint64_t death_floor_room_index{};
    DungeonElement death_ecology{DungeonElement::fire};
    bool death_was_abyss{};
    DeathSourceKind source_kind{DeathSourceKind::unknown};
    std::uint8_t source_monster_id{0xFFU};
    std::uint16_t source_detail_id{};
    DeathDamageType damage_type{DeathDamageType::physical};
    std::uint64_t raw_damage{};
    std::uint64_t barrier_loss{};
    std::uint64_t health_loss{};
    std::uint64_t final_damage{};
    std::array<std::uint64_t, 5> recent_damage{};
    std::int32_t hp{};
    std::int32_t max_hp{};
    std::int32_t barrier{};
    std::int32_t max_barrier{};
    std::int64_t armor{};
    std::int64_t evasion{};
    std::int32_t armor_reduction_bp{};
    std::int32_t evasion_rate_bp{};
    std::array<std::int32_t, 4> damage_reduction{};
    std::array<std::int32_t, 4> damage_reduction_cap{};
    RoomDescriptor target_room{0U, 0U, 0U, 0U,
        EntrySide::initial, DungeonElement::fire, false, false};
};
```

在 `DungeonRunState` 追加 `std::uint64_t death_sequence{}` 和 `DeathCheckpoint death{}`。测试要求 `same_run_state()` 比较每个新增字段；初始 run 为序号 0、规范 `none`。`none` 的 target 深度/层内编号必须显式为 0，不能继承普通 `RoomDescriptor` 的 depth/floor 默认 1；枚举使用各自规定的 canonical 默认，其余数值和布尔为 0。

退层生成接口固定为：

```cpp
struct DeathRetreatTargetResult final {
    DungeonFault fault{DungeonFault::none};
    checkpoint::RoomDescriptor room{};
};

[[nodiscard]] DeathRetreatTargetResult make_death_retreat_target(
    const checkpoint::DungeonRunState& current,
    std::uint64_t next_death_sequence,
    const DungeonRules& rules) noexcept;
```

测试覆盖深度夹紧、`floor_room_index=0`、初始入口、非深渊、零偏向生成、普通洞口规则、同输入逐字段相同、改变任一输入改变命名流、非零 seed、最大序号/代数 fault，且不改变门/下降/深渊/掉落流的既有 golden 值。

**Step 2: 确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
```

Expected: 新稳定类型、生成接口或 suite 未定义导致失败。

**Step 3: 实现命名域与规则无关结构校验**

- 在 `room_generation.cpp` 增加唯一常量 `kDeathRetreatDomain`，依次混入当前房 seed、当前 `commit_generation`、下一 `death_sequence`、目标深度；若结果为 0，固定映射为 `0xD34D5EEDULL`，不二次抽样。
- 用 `generate_room_descriptor(seed, current.index + 1, target_depth, 0, initial, {0,0,0,0}, rules)` 生成生态和洞口，再强制 `is_abyss=false`。
- 对 `current.index`、`commit_generation`、`death_sequence` 的最大值先返回明确 overflow fault；新增 fault 枚举必须有测试和可读诊断。
- `dungeon_checkpoint.hpp` 提供 `constexpr/noexcept` 的 header-only `valid_death_checkpoint_structural()`，检查规范零值、枚举范围、最后一击和防御字段，因此 Persistence 只依赖稳定类型且无需链接 `arpg_dungeon`。Combat snapshot 转换、目录 ID 和需要 `DungeonRules` 的重生成校验放在 `death_checkpoint.hpp/.cpp` 的 Dungeon 层。

**Step 4: 运行 GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --preset windows-msvc-debug -R '^dungeon\.units$' --output-on-failure
```

Expected: Dungeon 既有 golden 和新死亡生成测试全部通过。

**Step 5: 提交**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: add deterministic death checkpoints"
```

---

### Task 4: 将检查点升级到 V6 并保持 V5 迁移

**Files:**

- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `tests/persistence/checkpoint_codec_tests.cpp`
- Modify: `tests/persistence/passive_tree_checkpoint_tests.cpp`
- Modify: `tests/persistence/persistence_test_main.cpp`

**Step 1: 固定 V6 二进制布局**

V6 magic 使用新的 8 字节 `ARPGSV6\0`，format 为 6。保留 V5 decoder 和 V5 magic。V6 在 V5 204-byte base payload 后插入 224-byte death block，item record 仍为 40 bytes：

| 绝对偏移 | 大小 | 字段 |
|---:|---:|---|
| 236 | 8 | `death_sequence` |
| 244 | 1 | lifecycle |
| 245 | 1 | data version |
| 246 | 1 | source kind |
| 247 | 1 | damage type |
| 248 | 1 | monster id |
| 249 | 1 | reserved zero |
| 250 | 2 | detail id |
| 252 | 1 | was abyss |
| 253 | 1 | death ecology |
| 254 | 2 | reserved zero |
| 256 | 8 | death depth |
| 264 | 8 | death floor-room index |
| 272 | 8 | raw damage |
| 280 | 8 | barrier loss |
| 288 | 8 | health loss |
| 296 | 8 | final damage |
| 304 | 40 | recent five-type damage |
| 344 | 16 | hp/max hp/barrier/max barrier as i32 |
| 360 | 16 | armor/evasion as i64 |
| 376 | 8 | armor reduction/evasion rate as i32 |
| 384 | 16 | four elemental reductions as i32 |
| 400 | 16 | four elemental caps as i32 |
| 416 | 32 | target room index/seed/depth/floor index |
| 448 | 4 | target entry/ecology/hole/abyss |
| 452 | 8 | reserved zero |

因此 `kV6DeathPayloadSize=224`、`kV6BasePayloadSize=428`、`kV6BaseEncodedCheckpointSize=460`。V6 item records 从绝对偏移 460 开始。V5 item records仍从 236 开始。

**Step 2: 写 codec RED 测试**

覆盖：

- `none` 和完整 `pending_continue` V6 round-trip 逐字段相同。
- 真实 V5 fixture 解码为 `migrated=true`、`death_sequence=0`、规范 `none`，原 V5 字段和 item records 不变。
- lifecycle、来源、伤害类型、布尔、reserved、`final != barrier+health`、负生命/护盾、防御比率越界、目标结构非法均返回精确 `CodecError`。
- 每个 death block 截断点、尾随字节、payload length、CRC、magic/version 不匹配都拒绝。
- item count 最大值和乘法/加法溢出继续防御。

**Step 3: 确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_persistence_tests
```

Expected: format/version/size 仍为 V5，新测试失败。

**Step 4: 实现 V6 编解码和迁移**

- `encode_checkpoint()` 只写 V6。
- `decode_checkpoint()` 接受 V1–V6；V5 进入既有 abyss/item 解析后补规范死亡状态；`migrated = format != 6`。
- 把 V5/V6 base offset 分支集中为局部布局常量，禁止复制两套 item decoder。
- Codec 只调用 `valid_death_checkpoint_structural()`；不包含 `DungeonRules`、房间生成器、Combat 目录或 raylib。

**Step 5: 运行 GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_persistence_tests
ctest --preset windows-msvc-debug -R '^persistence\.units$' --output-on-failure
```

Expected: V1–V6、V5 迁移和所有错误矩阵通过。

**Step 6: 提交**

```powershell
git add src/persistence tests/persistence
git commit -m "feat: persist death checkpoints in save v6"
```

---

### Task 5: Dungeon 构造、快照和加载时校验 pending death

**Files:**

- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/dungeon_progression.cpp`
- Create: `tests/dungeon/dungeon_death_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`

**Step 1: 写加载和公开快照失败测试**

新增：

```cpp
enum class RoomPhase : std::uint8_t { /* existing */, death_pending, /* faulted */ };

struct DeathSnapshot final {
    checkpoint::DeathCheckpoint checkpoint{};
    bool saving{};
    bool can_continue{};
};
```

`DungeonSnapshot` 增加 `std::optional<DeathSnapshot> death`。测试覆盖：

- 从合法 `pending_continue` 构造 Session 后 phase 为 `death_pending`、没有 Combat、没有 ground item、公开完整死亡数据。
- `none` 正常构建现有房间。
- 目标重新生成不一致、目标深度/入口/层内编号/非深渊、序号、last transition、零偏向、普通/深渊失败关系任一非法都在构造时 fault。
- 来源 monster/attack/hazard/affix/abyss rule ID 不在各自目录时 fault；`unknown` 的 ID 必须为规范零/无怪物。
- `queue_action`、移动、拾取、装备、配方、星盘、门、洞、重置在 death phase 全部拒绝。

**Step 2: 确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
```

Expected: `death_pending`、snapshot 或校验逻辑缺失导致失败。

**Step 3: 实现构造与只读投影**

- `construct_current_room()` 最先检查 `stable_state_.death.lifecycle`；pending 时不调用 encounter/Combat 生成，直接清空 transient pools 并进入 `death_pending`。
- 新增 `validate_death_checkpoint_for_session(state, rules)`：调用结构校验；对 pending state 使用保留的死亡房 seed、`commit_generation - 1` 和当前 `death_sequence` 重建 `make_death_retreat_target()` 后逐字段比较，再调用各拥有模块目录验证 ID。这样验证使用的输入与首次准备事务完全相同，不会拿已递增代数重新抽样。
- `DungeonSnapshot` 在已提交时从稳定 checkpoint 复制死亡数据；“正在记录死亡”则从 `pending_save.next_state.death` 投影摘要并令 `saving=true`，已提交时 `can_continue=true`。不得从已销毁或仍冻结的 Combat 临时对象临时拼 UI 数据。
- 把 death 字段加入 `same_run_state()`，receipt 比较自动覆盖完整摘要和目标。

**Step 4: 运行 GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --preset windows-msvc-debug -R '^dungeon\.units$' --output-on-failure
```

Expected: 加载校验、公开快照和输入拒绝矩阵通过，既有房间构造无回归。

**Step 5: 提交**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: load pending death sessions safely"
```

---

### Task 6: 实现普通房 `death_retreat` 原子事务

**Files:**

- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `tests/dungeon/dungeon_death_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_transaction_tests.cpp`

**Step 1: 写普通死亡事务 RED 测试**

新增 `PendingSaveKind::death_retreat` 和事件 `death_detected`、`death_retreat_committed`。以生产 Combat 伤害路径造成死亡，测试：

- `tick()` 在 `relay_combat_events()` 后先处理死亡，再判断 `remaining_targets()==0`；同 tick 清房不结算经验/奖励。
- 准备态 phase 为 `committing`、Combat 冻结、pending kind 唯一为 `death_retreat`。
- next state 保留等级/经验/技能点/星盘/装备/背包，丢弃 `pending_room_experience_`、ground pool、rolled bits、当前偏向和未提交奖励。
- `death_sequence` 只在 next state 增 1；稳定状态提交前不变。
- death room 摘要来自 `CombatDeathSnapshot`；target 为 Task 3 确定性结果；`current_room.is_abyss=false`、`last_transition=death_retreat`、`last_direction=none`。
- committed receipt 发布 `death_pending` 且销毁 Combat/ground；重复 receipt fault。
- not_committed 清 pending、保留冻结 Combat 和同一 death snapshot；下个 tick 重建逐字段相同 pending。
- indeterminate、generation、kind 或 verified state mismatch fault。
- generation/death sequence/index overflow 在创建 pending 前 fault。

**Step 2: 确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --preset windows-msvc-debug -R '^dungeon\.units$' --output-on-failure
```

Expected: 新事务测试失败，旧 `reset_to_normal_room(false)` 行为暴露。

**Step 3: 实现准备与 receipt 状态机**

- `handle_player_defeat()` 只调用 `prepare_death_retreat()`，删除普通死亡直接 reset 分支。
- `prepare_death_retreat()` 先构造局部 `next`，所有校验成功后一次性 `pending_save_.emplace`；不得部分修改 stable/transient 状态。
- 在 `PendingSave` 增加冻结的 `combat::CombatDeathSnapshot death_snapshot` 或等价固定字段缓存，并由一致性检查保证重试与 next checkpoint 相符。
- `commit_pending_save()` 将 death 两种事务作为受保护的专用分支处理：`not_committed` 不 fault；retreat 恢复到可自动重试的冻结 phase，continue 恢复 `death_pending`。
- receipt committed 后清地面池、Combat、未提交经验，发布 `death_retreat_committed` 并进入 `death_pending`。

**Step 4: 运行 GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --preset windows-msvc-debug -R '^dungeon\.units$' --output-on-failure
```

Expected: 普通死亡两阶段第一半和完整 receipt 矩阵通过。

**Step 5: 提交**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: commit atomic death retreats"
```

---

### Task 7: 将深渊死亡合并进 `death_retreat`

**Files:**

- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/abyss_reward.cpp`
- Modify: `tests/dungeon/dungeon_death_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_reward_tests.cpp`
- Modify: `tests/dungeon/dungeon_transaction_tests.cpp`

**Step 1: 写深渊死亡 RED 测试**

使用生产 `request_abyss_start`、Combat 和 Save receipt 进入正式深渊，再造成死亡。验证同一个 `death_retreat` next state：

- `death_was_abyss=true`，摘要保留原深渊房生态/深度/编号。
- `current_room.is_abyss=false`，`abyss.lifecycle=failed`。
- `LastAbyssResolution.valid=true`，room seed/rule/total 与死亡前挑战一致，generated/claimed/abandoned 满足 failed 规范且不发布奖励。
- ground chest、待物化奖励和未拾取奖励全部销毁。
- 不再执行 Stage 10 的 `prepare_abyss_failure()` → 原房 reset 行为。
- not_committed 保持深渊 Combat 冻结且重试 next state 相同；committed 后重启仍是相同 pending death。

**Step 2: 确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --preset windows-msvc-debug -R '^dungeon\.units$' --output-on-failure
```

Expected: Stage 10 同房失败逻辑使新断言失败。

**Step 3: 合并深渊失败发布**

- 抽取纯函数 `apply_abyss_failure_resolution(next, previous)`，供手动放弃/重置的既有路径与死亡事务按各自规则复用，但死亡只允许一次存档提交。
- `prepare_death_retreat()` 在局部 next 上同时写 failed lifecycle、resolution、death checkpoint 和退层目标。
- Runtime 重启时“started 自动转 failed”的既有迁移仅处理旧 started save；合法 pending death 不得再次改写或增加代数。

**Step 4: 运行 GREEN 和 Stage 10 回归**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_stage10_validation_fixture
ctest --preset windows-msvc-debug -R '^(dungeon\.units|stage10\.validation_fixture\.real_abyss_transactions)$' --output-on-failure
```

Expected: 新死亡规则通过，Stage 10 除被正式替代的“死亡留原房”断言外其余深渊不变量保持；若 fixture 含旧断言，更新为 Stage 11-A 规则并保留其他覆盖。

**Step 5: 提交**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: retreat after abyss deaths"
```

---

### Task 8: 实现 `death_continue` 和跨重启闭环

**Files:**

- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `tests/dungeon/dungeon_death_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_transaction_tests.cpp`
- Modify: `tests/persistence/dungeon_save_integration_tests.cpp`
- Modify: `tests/persistence/save_store_fault_tests.cpp`

**Step 1: 写继续与重启 RED 测试**

公开接口：

```cpp
[[nodiscard]] RequestResult request_death_continue() noexcept;
```

新增 `PendingSaveKind::death_continue` 和 `death_continue_requested` / `death_continued` 事件。测试：

- 只在 `death_pending`、无 pending save、无 Combat 且 target 校验通过时 accepted。
- next state 将 target 发布为 `current_room`，清空 `DeathCheckpoint`，保留已递增的 `death_sequence`、永久角色状态和零 biases；不再次抽 RNG。
- committed 后 phase 进入 `transitioning`，下一 tick 构建普通初始房 Combat。
- not_committed 返回同一死亡界面，再按 E 生成相同 next state。
- indeterminate 和任意 receipt mismatch fault。
- 在 retreat 后销毁进程并用 `SaveStore::load` + 新 `DungeonSession`：摘要和 target 完全相同；继续后再次重启处于目标房，不重现死亡界面。
- 深度 1 继续到新的深度 1 房；深度 N 继续到 N-1；seed 不等于死亡房且与首次生成一致。
- 双槽故障 winner 在 retreat/continue 两代之间选择最高合法 committed generation，不跳过 pending death。

**Step 2: 确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_persistence_tests
ctest --preset windows-msvc-debug -R '^(dungeon|persistence)\.units$' --output-on-failure
```

Expected: continue API 和持久化闭环缺失导致失败。

**Step 3: 实现第二次事务**

- `request_death_continue()` 从稳定 `death.target_room` 复制，不调用 room RNG。
- committed receipt 清死亡 checkpoint、进入 `transitioning`；`construct_current_room()` 走现有普通房路径。
- 将 continue failure 状态保留到 snapshot，供 UI 显示“保存失败，请重试”；成功请求期间显示“正在继续”。
- Save integration 测试使用真实临时目录和双槽文件，不伪造 `PendingSaveResult` 代替磁盘路径。

**Step 4: 运行 GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_persistence_tests
ctest --preset windows-msvc-debug -R '^(dungeon|persistence)\.units$' --output-on-failure
```

Expected: 两阶段、重启和双槽故障矩阵全部通过。

**Step 5: 提交**

```powershell
git add src/dungeon tests/dungeon tests/persistence
git commit -m "feat: continue from persisted death screens"
```

---

### Task 9: 接入 DungeonRuntime 的保存顺序与死亡输入门禁

**Files:**

- Modify: `src/platform/raylib/dungeon_runtime.hpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/raylib_input.hpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Create: `tests/platform/death_input_gate_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/CMakeLists.txt`

**Step 1: 写 Runtime 和输入矩阵 RED 测试**

新增纯函数：

```cpp
struct DeathInputGate final {
    bool continue_death{};
    bool screenshot{};
    bool debug_toggle{};
    bool exit{};
    bool forward_gameplay{};
};

struct FrameKeyState final {
    bool e{};
    bool f12{};
    bool v{};
    bool f1{};
    bool escape{};
    bool movement{};
    bool attack{};
    bool reset{};
    bool inventory{};
    bool passives{};
    bool mouse_gameplay{};
};

[[nodiscard]] DeathInputGate death_input_gate(
    bool death_saving, bool death_pending,
    const FrameKeyState& keys) noexcept;
```

覆盖：

- death saving：仅 F12/V、F1、Esc；E 也禁用。
- pending continue：E、F12/V、F1、Esc 可用；WASD/J/K/L/R/I/P、门、洞、鼠标背包/星盘全部不转发。
- 非死亡沿用现有 passive/inventory gates。
- Runtime 每个 fixed tick 顺序固定为 `session.tick` → `service_pending_save` → refresh snapshot；死亡产生的同帧后续 UI/拾取请求被拒绝。
- `request_death_continue()` 只转发 Session 公开 API。
- V5 load 迁移提交后再构造 Session；V6 pending death load 不走 started-abyss 二次修复。

**Step 2: 确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
```

Expected: 纯输入 gate、Runtime continue API 或 host 分支缺失。

**Step 3: 实现 host 顺序与门禁**

- 在 host 每帧获取 snapshot 后，死亡 gate 优先于 passive/inventory gate。
- pending death 的 `KEY_E` 调用 `runtime.request_death_continue()`，不再落入 `request_descent()`。
- death saving/pending 时 fixed-step 可继续驱动 Dungeon receipt/phase，但传空 MovementInput 且不提交动作。
- 截图仍只设置既有 `frame_toggles.take_screenshot`，最终走唯一 `present_frame_and_maybe_capture()`。

**Step 4: 运行 GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
ctest --preset windows-msvc-debug -R '^platform\.units$' --output-on-failure
```

Expected: 保存顺序和允许/禁止按键矩阵通过。

**Step 5: 提交**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: gate input during death flow"
```

---

### Task 10: 绘制可测试的死亡回顾覆盖层

**Files:**

- Create: `src/platform/raylib/death_overlay_view.hpp`
- Create: `src/platform/raylib/death_overlay_view.cpp`
- Create: `src/platform/raylib/death_overlay_renderer.hpp`
- Create: `src/platform/raylib/death_overlay_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Create: `tests/platform/death_overlay_view_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/CMakeLists.txt`

**Step 1: 写无窗口 view-model RED 测试**

`build_death_overlay_view(const DungeonSnapshot&)` 返回固定容量文本/数值模型，不在 draw 时查询 Session。测试覆盖：

- 普通/深渊标签、深度/房号/生态。
- 来源 kind + 目录 ID 到中文显示；unknown 显式显示“未知来源”。
- 最后一击 raw/barrier/hp/final 和五类 5 秒承伤。
- HP/护盾、护甲减伤、闪避率、四元素减伤/上限。
- `第 N 层 → 第 M 层`。
- saving 显示“正在记录死亡”；pending 显示“E 继续”；continue not_committed 显示“保存失败，请重试”。
- 1280×720 和最小支持窗口下所有 panel/text bounds 在屏幕内，不与底部提示重叠。

**Step 2: 确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
```

Expected: view builder 和 renderer 文件缺失。

**Step 3: 实现 view-model 与 raylib renderer**

- view 层只依赖 `dungeon_types.hpp` 和目录只读 API，不 include raylib。
- renderer 使用半透明全屏遮罩、居中面板、两列统计和清晰底部提示；使用当前字体/色板，不改完整 HUD。
- `CombatRenderer::draw()` 先画世界和既有 HUD，再在最后画 death overlay，确保死亡界面覆盖玩法提示。
- 不在 renderer 保存死亡业务状态，不允许 renderer 直接调用 Session 或 SaveStore。

**Step 4: 运行 GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --preset windows-msvc-debug -R '^platform\.units$' --output-on-failure
```

Expected: view/layout 测试通过，正式游戏目标可链接。

**Step 5: 提交**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render death recap overlay"
```

---

### Task 11: 增加 1000 次确定性、零分配和架构护栏

**Files:**

- Create: `tests/dungeon/dungeon_death_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Create: `tests/platform/stage11_architecture_guard_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`

**Step 1: 写压力测试与护栏**

新增 `ARPG_STAGE11_DEATH_STRESS_ONLY=1` 分支，仅运行死亡压力 suite。生产路径循环 1000 次：

- 混合深度 1 夹紧、深层普通死亡和正式深渊死亡。
- 每次用 CombatWorld 实际伤害生成 snapshot，走 Session pending save、真实 codec encode/decode、重建 Session、continue receipt。
- 两个相同初始状态使用不同批处理节奏，最终每次 death checkpoint、target seed/descriptor 和累计 hash 必须相同。
- 每 17 次 V6 round-trip，每 31 次在 pending death 重启，每 43 次在 continue 后重启。
- 填满生产 ground pool 后死亡，验证 checkpoint 不含 ground 状态且 saturation 计数稳定。
- 对 Combat 账本推进、致死 tick、Dungeon prepare/commit 热路径包围 allocation probe，结果为 0；进程 working-set 只作诊断，不作为跨机器脆弱阈值。
- 记录现有 door/descent/abyss/drop golden stream，循环前后相同。

架构 guard 扫描：

- `src/combat` 禁止 include `dungeon/`、`persistence/`、`raylib.h`。
- `src/persistence` 的 death codec 禁止 include room generation、CombatWorld、raylib。
- death overlay 禁止访问 `stable_state_`、`pending_save_`、SaveStore 或 test access。
- 生产模块禁止 `DungeonSessionTestAccess` / `CombatWorldTestAccess`。

**Step 2: 确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --preset windows-msvc-debug -R '^stage11\.(death_stress|architecture)' --output-on-failure
```

Expected: 新 CTest 尚未注册或压力/护栏失败。

**Step 3: 只修生产缺陷，不弱化压力条件**

- 修复压力测试揭示的饱和、重启、命名流或 allocation 问题。
- CMake 注册：

```cmake
add_test(NAME stage11.death_stress.determinism_zero_alloc
    COMMAND arpg_dungeon_tests)
set_tests_properties(stage11.death_stress.determinism_zero_alloc PROPERTIES
    ENVIRONMENT "ARPG_STAGE11_DEATH_STRESS_ONLY=1"
    LABELS "headless;dungeon;stage11;stress"
    TIMEOUT 300)
```

**Step 4: 运行 GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --preset windows-msvc-debug -R '^stage11\.(death_stress|architecture)' --output-on-failure
```

Expected: 1000 次、零分配、随机流和架构护栏全部通过。

**Step 5: 提交**

```powershell
git add tests/dungeon tests/platform
git commit -m "test: stress death continue determinism"
```

---

### Task 12: 建立真实 raylib 五路径正式验收与证据守卫

**Files:**

- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/host_launch_options.cpp`
- Create: `tests/dungeon/stage11_death_validation_fixture.cpp`
- Create: `tests/dungeon/stage11_death_formal_game_validation.cpp`
- Create: `tests/dungeon/stage11_death_formal_capture_test.ps1`
- Create: `tests/dungeon/stage11_death_evidence_guard_test.cmake`
- Create: `tests/dungeon/stage11_evidence_bad_injection.txt`
- Create: `tests/dungeon/stage11_evidence_bad_capture_order.txt`
- Modify: `tests/dungeon/CMakeLists.txt`

**Step 1: 先写 headless 正式 fixture**

fixture 只使用生产 API，验证：普通死亡、pending death 重启、深层 E 继续、第一层 E 继续、深渊死亡。它必须通过 Combat 真实命中、`DungeonSession::tick()`、`pending_save_view()`、`SaveStore::commit()` 和 `request_death_continue()`，不得直接写 Session 私有成员或伪造 death snapshot。

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_stage11_death_validation_fixture
ctest --preset windows-msvc-debug -R '^stage11\.death_fixture\.transactions$' --output-on-failure
```

Expected first run: fixture target/CTest 未注册或行为失败。

**Step 2: 增加正式 host 场景但只驱动生产输入**

新增 `Stage11ValidationScenario`：

- `normal_death_recap`
- `restart_same_recap`
- `deep_continue`
- `floor_one_continue`
- `abyss_death_recap`

host 场景只能选择确定性 seed、生成真实 MovementInput/动作和按键请求；不得调用私有访问器、直接改 HP、phase、stable state、death checkpoint 或 pending save。正式程序为每条路径使用独立真实 save directory，并在重启前后比较摘要和 target 的字段/hash。

**Step 3: 正式截图内容检查**

`stage11_death_formal_capture_test.ps1` 必须：

- 启动 `arpg_stage11_death_formal_game_validation`。
- 要求 5 张 1280×720 新鲜截图：`01-normal-death.png`、`02-restarted-death.png`、`03-deep-continued.png`、`04-floor-one-continued.png`、`05-abyss-death.png`。
- 用 `System.Drawing` 验证尺寸、非背景像素、颜色/亮度变化和死亡面板区域内容；普通死亡与重启截图面板区域 hash 相同。
- 验证 `formal-path-summary.txt` 中五条 PASS、相同 target/hash、深度退层、floor-one clamp 和 abyss failed resolution。

截图继续只允许 host 的唯一 helper：先 `EndDrawing()`，再 `LoadImageFromScreen()` / `ExportImage()`；不得新增第二套截图路径。

**Step 4: 写并验证证据守卫**

guard 拒绝：

- `DungeonSessionTestAccess`、`CombatWorldTestAccess`、`force_defeat`、`stable_state_`、`phase_=`、直接 death checkpoint 注入。
- 在 `EndDrawing` 前截图，或新增额外 `LoadImageFromScreen` / `ExportImage`。
- fixture 不含真实 `SaveStore`、`session.tick`、`pending_save_view`、`request_death_continue` 和重启 load。
- formal host 不含真实 `queue_action` / movement / runtime save service。

同时用两个 `WILL_FAIL TRUE` bad fixtures 证明守卫能拒绝私有注入和 pre-present capture。

**Step 5: 运行正式 RED→GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_stage11_death_validation_fixture arpg_stage11_death_formal_game_validation
ctest --preset windows-msvc-debug -R '^stage11\.(death_fixture|death_formal|death_evidence)' --output-on-failure
```

Expected: 实现前失败；完成后 headless fixture、真实 raylib 五路径、截图内容和正/负证据守卫全部通过。

**Step 6: 提交**

```powershell
git add src/platform/raylib tests/dungeon
git commit -m "test: validate stage 11a death flow in raylib"
```

---

### Task 13: 全量 Debug/Release 验证、文档收口和最终审查

**Files:**

- Modify: `README.md`
- Create: `docs/validation/stage11a-death-continue.md`

**Step 1: 更新用户与工程文档**

- README：明确死亡规则、退层规则、死亡界面只接受 E/F12/V/F1/Esc，并链接正式验证报告。
- `docs/validation/stage11a-death-continue.md`：记录 Combat→Dungeon→Persistence→raylib 单向边界、两次提交、V6 layout、五条正式路径、1000 次压力、证据目录和复现命令。
- 在验证报告中把 Stage 11-A 逐条映射到测试名；设置、物品过滤、完整 HUD、macOS 明确保持未实现。

**Step 2: 从头运行 Debug 全量**

Run:

```powershell
cmake --preset windows-msvc-debug --fresh
cmake --build --preset windows-msvc-debug --clean-first
ctest --preset windows-msvc-debug --output-on-failure
```

Expected: 所有 CTest 0 失败；Stage 11 stress、formal raylib、evidence guard 均包含在全量结果中。

**Step 3: 从头运行 Release 全量**

Run:

```powershell
cmake --preset windows-msvc-release --fresh
cmake --build --preset windows-msvc-release --clean-first
ctest --preset windows-msvc-release --output-on-failure
```

Expected: 所有 CTest 0 失败，正式游戏链接 raylib 6.0，Release 也通过五路径截图验收。

**Step 4: 检查提交边界与工作树**

Run:

```powershell
git status --short
git diff --check
git log --oneline a06eddbdf557b7209146f12ec1a6bc75a1aa75b3..HEAD
git diff --stat a06eddbdf557b7209146f12ec1a6bc75a1aa75b3..HEAD
```

Expected: `git diff --check` 无输出；没有 build、png、存档、日志或无关文件进入提交；改动只覆盖 Stage 11-A。

**Step 5: 进行两轮代码审查并修复全部等级问题**

- 第一轮按任务不变量审查 Combat、Dungeon、Persistence、Platform、测试证据。
- 第二轮从完整 diff 独立审查，特别检查同 tick 优先级、receipt、重启、不重骰、V5 迁移、零分配和私有注入。
- 任何 Critical、Important 或 Minor 都必须修复并重跑受影响窄测试；最后再次跑 Debug 全量。不得以“后续阶段处理”关闭本阶段问题。

**Step 6: 提交文档和最终修复**

```powershell
git add README.md docs src tests
git commit -m "docs: complete stage 11a death milestone"
```

如果审查修复已单独提交，最后只确认工作树干净；不要创建空提交，不合并 `main`，不开始 Stage 11-B。
