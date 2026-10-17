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

在 `hit_resolution.cpp` 用 `build_player_hit_packet(definition->damage, encounter_config_.player_build)` 扣怪物 HP，`CombatEvent::value` 写 packet 总值。玩家最大生命必须按 `floor((base_hp + fixed_floor(values.max_health)) * values.max_health_more / kFixedOne)` 计算，确保 Stormstep 的 8000 乘数将基础生命降低 20%，而 core_vitality 的平坦生命仍独立生效。

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

