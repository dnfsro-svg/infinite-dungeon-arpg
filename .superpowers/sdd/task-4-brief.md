## Task 4: 战斗闪避、护甲、元素减伤与热换 build

### Frozen combat integration decisions

- `combat` 不得依赖 `items`。扩展既有 `PlayerCombatBuild` 作为桥：

```cpp
struct PlayerCombatBuild final {
    modifiers::PlayerModifierValues values{};
    std::int64_t weapon_physical{};
    std::int32_t local_attack_speed_bp{};
};
```

`weapon_physical` 与 `local_attack_speed_bp` 必须非负；任何 build 字段无效或 checked 组合溢出时，构造/reset 拒绝该 build，`apply_player_build` 保持旧 build 与角色数值不变。
- J/K/L 的每次有效近战命中在现有基础物理分量上加 `weapon_physical`，然后与其他物理 flat 一起进入现有 melee/global increased/more 求值；不得改变攻击 hitbox、active ticks 或命中次数。无武器时字段为 0，既有 J/K/L 仍可使用。
- 局部攻速先形成 `local_multiplier_bp=10000+local_attack_speed_bp`，再与现有全局 attack speed 最终倍率做 checked 乘法/10000 向下取整；只缩放现有前摇/后摇，不缩放 active ticks。最终速度继续遵守现有 combat clamp。
- `DamageDelivery::direct` 用于怪物近战、投射物、冲锋/直接接触伤害；hazard/地面持续与环境伤害使用 `ground_or_environment`。一个五分量 direct packet 最多调用一次 `next_bounded(10000)`；仅当包至少一个正分量且 `evasion_rate_bp>0` 时消费。成功条件为 roll `< evasion_rate_bp`，成功则整个包为 0。非 direct、全零包或 0 闪避不得消费 RNG。
- 每次 encounter/reset 用 `CombatEncounterConfig::evasion_seed` 重新构造独立 RNG；相同 seed 和输入 trace 必须相同，不能复用 dungeon/drop RNG。
- 物理最终分量使用 `armor_reduction_bp=rating_to_basis_points(armor)`；元素 i 的 cap=`min(9500,7500+cap_bonus[i])`，reduction clamp 到 `[-6000,cap]`。物理和四元素每个正分量都使用 checked 正整数 ceil：`ceil(raw*(10000-rate)/10000)`；五分量 checked 求和后再进入现有 barrier→health 顺序。任何算术溢出必须使该伤害解析无效且不产生部分扣血。
- `PlayerSnapshot` 字段类型固定为：`damage_reduction[4]:int32`、`damage_reduction_cap[4]:int32`、`armor:int64`、`evasion:int64`、`armor_reduction_bp:int32`、`evasion_rate_bp:int32`。旧 `resistance` 名称必须完全移除。
- `apply_player_build` 成功时保留当前位置、速度、攻击状态、hurt/invulnerability、当前 hp/barrier 的绝对值，只更新 build/派生最大值并向下 clamp；增加上限不回复，降低上限不低于 0。失败时任何字段均不变。

**Files:**
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/combat_snapshot.cpp`
- Modify: `src/combat/player_simulation.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/monster_ai_melee.cpp`
- Modify: `src/combat/monster_ai_ranged.cpp`
- Modify: `src/combat/monster_ai_special.cpp`
- Test: `tests/combat/player_health_tests.cpp`
- Test: `tests/combat/player_build_tests.cpp`
- Create: `tests/combat/player_defense_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`

- [ ] **Step 1: 写防御包解析失败测试**

覆盖：一个五类型包只闪避一次；直接近战/投射/冲锋可闪避；hazard 不闪避且不消费 RNG；护甲只减物理；元素分别使用自身减免；-6000、7500、词缀抬高至 9500；正的非零分量经 ceil 后仍至少 1；相同 seed/trace 等价。

- [ ] **Step 2: 写热换 build 失败测试**

先造成伤害与护盾损失，再应用更高/更低上限 build，断言：

```cpp
world.apply_player_build(higher_build);
CHECK_EQ(world.snapshot().player.hp, old_hp);
CHECK_EQ(world.snapshot().player.barrier, old_barrier);
world.apply_player_build(lower_build);
CHECK_EQ(world.snapshot().player.hp, lower_max_hp);
CHECK_EQ(world.snapshot().player.barrier, lower_max_barrier);
```

- [ ] **Step 3: 运行 RED**

```powershell
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure
```

Expected: `DamageDelivery`、evasion seed、热换 build 或新快照字段未定义。

- [ ] **Step 4: 定义交付类型与战斗随机流**

```cpp
enum class DamageDelivery : std::uint8_t { direct, ground_or_environment };

struct CombatEncounterConfig final {
    // existing fields remain in order used by source initialization
    std::uint64_t evasion_seed{};
};

void apply_player_build(PlayerCombatBuild build) noexcept;
void apply_player_damage(const DamagePacket& packet,
                         DamageDelivery delivery,
                         Vec3 source,
                         FeedbackLevel feedback) noexcept;
```

`CombatWorld` 拥有独立 `core::DeterministicRng evasion_rng_`。只有 `delivery == direct`、包总量非零且 evasion rate 非零时取一次随机数；hazard 使用 `ground_or_environment`。

- [ ] **Step 5: 实现每分量最终伤害**

物理使用 armor 曲线；四元素把 `damage_reduction[i]` clamp 到 `[-6000, min(9500, 7500 + cap_bonus[i])]`。所有非零分量用 64 位 checked ceil division；五分量和再做 checked 求和。快照字段统一改为 `damage_reduction`、`damage_reduction_cap`、`armor`、`evasion`、`armor_reduction_bp`、`evasion_rate_bp`。

- [ ] **Step 6: 实现热更新语义**

`apply_player_build` 验证 build 后重新计算 max，执行：

```cpp
player_.hp = std::min(player_.hp, player_.max_hp);
player_.barrier = std::min(player_.barrier, player_.max_barrier);
```

不得使用 reset 或按比例换算。新增武器物理分量进入 J/K/L 每次有效命中；局部攻速倍率与全局 attack speed 相乘，但不修改有效帧窗口。

- [ ] **Step 7: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure
git diff --check
```

- [ ] **Step 8: 提交**

```powershell
git add src/combat tests/combat
git commit -m "feat: apply equipment defenses in combat"
```

---
