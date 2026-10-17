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

