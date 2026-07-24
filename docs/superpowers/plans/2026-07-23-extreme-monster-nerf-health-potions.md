# 怪物大幅削弱与生命药掉落实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 全局降低八种怪物的基础生命与伤害，仅让第 1–3 层怪物固定无词条，并加入可原子保存、自动使用、具有独立高清表现的生命药掉落。

**Architecture:** 怪物数值与词条保护留在战斗域；生命药的确定性判定、固定地面池和领取事务留在地下城域，并只复用现有 `material_claimed_drop_bits` 的次级领取位。战斗域提供绕过深渊恢复倍率的百分比回血接口，渲染域只消费快照与已提交回执；不改变 V8 存档结构或现有掉落随机域。

**Tech Stack:** C++17、raylib 6.0、CMake/Ninja、MSVC 19.44、Windows SDK 10.0.26100.0、CTest、Python 3 + Pillow、固定容量容器、现有异步双槽 `SaveStore`。

## Global Constraints

- 八种怪物的新基础生命/伤害必须严格为：火焰炸弹 100/36、火焰冲锋 190/27、水之盾卫 315/21、水之辅助 135/0、雷电射手 110/12、雷电突进 125/18、混沌追击 120/14、混沌地面 145/11。
- 最大破韧、护盾、移动速度、预警、攻击有效帧、恢复帧、冷却、投射物速度、怪物数量、房间大小和房间密度保持不变。
- 深度 1、2、3 的普通与深渊怪物都必须是零词条；深度 4 起沿用当前全部权重。
- 强化券先判定；只有未掉强化券时才以独立随机域判定生命药，概率固定 `3000 bp`。普通材料独立判定并可与生命药共存。
- 每瓶恢复实际最大生命 `2500 bp`，向上取整、封顶最大生命，并绕过 `player_resource_restore_bp`；生命为零或已有死亡快照时不得回血或复活。
- 自动使用阈值是 `hp * 10000 <= max_hp * 7500`；恰好 75% 必须使用，且不检查距离。
- 每次成功领取后重新计算阈值；同房间药瓶按稳定序号升序处理，生命严格高于 75% 后停止。
- 生命药不进入背包、不累计数量、无快捷键、无冷却、无品阶、不可跨房间保存；离开房间后全部未用药瓶消失。
- `common ordinal = spawn * 2`，`coupon or potion ordinal = spawn * 2 + 1`；生命药不能成为 `MaterialId`，不能扩大 `kMaterialCount`，不能升级或扩展 V8 codec。
- 地面生命药池固定为 `kGroundDropCapacity == 192`；热路径不得动态分配，池满只增加诊断计数，不能覆盖对象。
- 回血、移除药瓶和成功 HUD 回执只能发生在保存提交被精确确认后；失败或不确定结果不得产生以上副作用。
- 最后一只怪物掉落的低血量生命药必须并入同一 `room_clear/abyss_clear` 事务；高于 75% 时保留在已清理房间，离开时消失。
- 深渊清房提交成功时先执行 `clear_abyss_rule_preserving_resources()`，再按清房后的实际最大生命结算生命药，保证回执数字等于最终血条增量。
- 视觉必须使用独立高清红色玻璃瓶/金属金盖图标，无烘焙文字；地面使用纯红高对比标签“生命药”，HUD 使用“生命药 +N HP”。
- 本次不调整玩家伤害、防御装备、深渊环境百分比伤害、现有材料、强化、装备、出口和房间随机规则。

---

## 文件职责映射

- `src/combat/monster_catalog.cpp`：唯一的八种怪物冻结基础数值表。
- `src/combat/monster_affix_generation.cpp`：深度词条权重与深渊补充保护。
- `src/combat/combat_world.hpp/.cpp`：存活校验、实际最大生命百分比回血和实际恢复量。
- `src/dungeon/health_potion_loot.hpp/.cpp`：生命药常量、独立随机域、阈值函数和固定地面对象。
- `src/dungeon/dungeon_types.hpp`：药瓶快照、回执、待提交元数据和公开地下城快照。
- `src/dungeon/dungeon_session.hpp/.cpp`：击杀掉落、固定池、稳定扫描、最终击杀清房选择。
- `src/dungeon/dungeon_transition.cpp`：单瓶/批量领取的存档状态差异校验、提交/回滚与提交后回血。
- `src/dungeon/dungeon_snapshot.cpp`：将活动药瓶、回执和待提交序号打包为只读快照。
- `src/platform/raylib/material_asset_types.hpp` 与 `material_manifest.hpp`：生命药专用图集帧契约。
- `src/platform/raylib/material_loot_view.hpp/.cpp`：复用现有次级地面掉落标签管线构建生命药标签及 HUD 回执文本，不伪造材料 ID。
- `tools/build_item_material_atlas.py`：把独立高分辨率药瓶源图放入 `items_ui` 第 30 号空单元并生成材质图。
- `art_source/stage12/items/health-potion-v1.png`：不含文字的高分辨率 authored source。
- `assets/stage12/items_ui.png` 与 `items_ui_material.png`：可运行游戏加载的生成图集。
- `tests/combat/*`、`tests/dungeon/dungeon_health_potion_tests.cpp`、`tests/platform/*`：行为、事务、随机隔离、固定容量、视觉资源与文字契约。

### Task 1: 冻结八种怪物的新基础数值

**Files:**
- Modify: `tests/combat/monster_catalog_tests.cpp`
- Modify: `tests/combat/monster_affix_runtime_tests.cpp`
- Modify: `tests/combat/monster_melee_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `src/combat/monster_catalog.cpp:42-79`

**Interfaces:**
- Consumes: `const MonsterDefinition* monster_definition(MonsterId) noexcept`。
- Produces: 八种怪物固定的 `max_hp` 与单一元素 `contact_damage`，供后续词条、深渊与掉落测试使用。

- [ ] **Step 1: 写入精确基础数值失败测试**

在 `monster_catalog_tests.cpp` 增加：

```cpp
arpg::test::Failure catalog_matches_approved_health_and_damage_table() noexcept {
    struct Expected final {
        MonsterId id;
        int max_hp;
        modifiers::DamageType damage_type;
        int damage;
    };
    constexpr std::array<Expected, 8U> kExpected{{
        {MonsterId::fire_bomber, 100, modifiers::DamageType::fire, 36},
        {MonsterId::fire_charger, 190, modifiers::DamageType::fire, 27},
        {MonsterId::water_bulwark, 315, modifiers::DamageType::water, 21},
        {MonsterId::water_support, 135, modifiers::DamageType::water, 0},
        {MonsterId::lightning_shooter, 110, modifiers::DamageType::lightning, 12},
        {MonsterId::lightning_dasher, 125, modifiers::DamageType::lightning, 18},
        {MonsterId::chaos_chaser, 120, modifiers::DamageType::chaos, 14},
        {MonsterId::chaos_hazard, 145, modifiers::DamageType::chaos, 11},
    }};
    for (const Expected expected : kExpected) {
        const MonsterDefinition* definition = monster_definition(expected.id);
        ARPG_REQUIRE(definition != nullptr);
        ARPG_REQUIRE(definition->max_hp == expected.max_hp);
        for (std::size_t index = 0U; index < definition->contact_damage.amount.size(); ++index) {
            const int amount = index == modifiers::damage_index(expected.damage_type)
                ? expected.damage : 0;
            ARPG_REQUIRE(definition->contact_damage.amount[index] == amount);
        }
    }
    return {};
}
```

把该用例加入 `kCases`，并把 `combat_test_main.cpp` 的总用例数从 `241` 改为 `242`。

- [ ] **Step 2: 运行测试确认 RED**

Run:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-core-debug
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure
```

Expected: `monster_catalog.catalog matches approved health and damage table` 因旧值 `220/120` 等失败。

- [ ] **Step 3: 只替换目录中的生命和伤害字段**

在 `kCatalog` 中保留每个条目的其他字段不动，使用下面完整表：

```cpp
constexpr std::array<MonsterDefinition, 8> kCatalog{{
    {MonsterId::fire_bomber, 0, tags(MonsterTag::high_priority,
         MonsterTag::direct_target), 3, 100, 0, 0.040F, 1.2F,
     60, 1, 0, 0, elemental_damage(modifiers::DamageType::fire, 36),
     0.0F, 0, FeedbackLevel::heavy},
    {MonsterId::fire_charger, 0, tags(MonsterTag::melee,
         MonsterTag::high_priority, MonsterTag::direct_target),
     4, 190, 60, 0.030F, 5.0F, 45, 18, 35, 100,
     elemental_damage(modifiers::DamageType::fire, 27), 0.0F, 0,
     FeedbackLevel::heavy},
    {MonsterId::water_bulwark, 1, tags(MonsterTag::melee,
         MonsterTag::direct_target), 4, 315, 120, 0.025F, 1.0F,
     24, 6, 30, 72, elemental_damage(modifiers::DamageType::water, 21),
     0.0F, 0, FeedbackLevel::heavy},
    {MonsterId::water_support, 1, tag(MonsterTag::support),
     3, 135, 0, 0.030F, 4.0F, 36, 1, 24, 120,
     elemental_damage(modifiers::DamageType::water, 0), 0.0F, 0,
     FeedbackLevel::medium, 90, 120},
    {MonsterId::lightning_shooter, 2, tags(MonsterTag::ranged,
         MonsterTag::direct_target, MonsterTag::projectile_capable),
     3, 110, 0, 0.035F, 4.5F, 30, 1, 20, 75,
     elemental_damage(modifiers::DamageType::lightning, 12), 0.14F, 0,
     FeedbackLevel::medium},
    {MonsterId::lightning_dasher, 2, tags(MonsterTag::melee,
         MonsterTag::high_priority, MonsterTag::direct_target),
     3, 125, 0, 0.050F, 3.0F, 24, 12, 30, 80,
     elemental_damage(modifiers::DamageType::lightning, 18), 0.0F, 0,
     FeedbackLevel::medium},
    {MonsterId::chaos_chaser, 3, tags(MonsterTag::melee,
         MonsterTag::direct_target), 2, 120, 0, 0.045F, 0.9F,
     12, 4, 18, 42, elemental_damage(modifiers::DamageType::chaos, 14),
     0.0F, 0, FeedbackLevel::light},
    {MonsterId::chaos_hazard, 3, tags(MonsterTag::ranged,
         MonsterTag::high_priority, MonsterTag::ground_hazard,
         MonsterTag::direct_target), 4, 145, 0, 0.030F, 4.0F,
     45, 1, 25, 120, elemental_damage(modifiers::DamageType::chaos, 11),
     0.0F, 180, FeedbackLevel::heavy},
}};
```

同步冻结由相同公式产生的断言：

```text
monster_affix_runtime_tests.cpp: mighty bulwark max_hp 630; shielding m2 max_shield 110;
runtime/snapshot mighty bulwark max_hp 630。

monster_melee_tests.cpp:
abyss bulwark armored max_shield/shield = 126/36；
mighty max_hp/max_shield/shield = 240/162/72；
shielding max_shield/shield = 78/36；
abyss fury contact = 20；projectile raw lightning = 12；
chaos hazard raw damage = 15；frenzy contact = 30；
chilling+blink contact = 27；chilling projectile raw/hit = 12/19。
```

- [ ] **Step 4: 运行战斗测试确认 GREEN**

Run: `ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure`

Expected: `242 cases, 0 failures`。

- [ ] **Step 5: 提交**

```powershell
git add src/combat/monster_catalog.cpp tests/combat/monster_catalog_tests.cpp tests/combat/monster_affix_runtime_tests.cpp tests/combat/monster_melee_tests.cpp tests/combat/combat_test_main.cpp
git commit -m "balance: sharply reduce monster base stats"
```

### Task 2: 保护第 1–3 层普通与深渊怪物零词条

**Files:**
- Modify: `tests/combat/monster_affix_generation_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `src/combat/monster_affix_generation.cpp:46-53,305-321`

**Interfaces:**
- Consumes: `generate_monster_affixes(...)` 与 `supplement_abyss_affixes(...)`。
- Produces: 深度 1–3 始终返回合法空 `MonsterAffixSet`；深度 4+ 的随机与权重不变。

- [ ] **Step 1: 写入前三层普通/深渊失败测试**

```cpp
arpg::test::Failure first_three_depths_are_affix_free_in_normal_and_abyss_rooms() noexcept {
    MonsterAffixSet nonempty{};
    nonempty.values[0] = {MonsterAffixId::mighty, MonsterAffixTier::m1};
    nonempty.count = 1U;
    for (std::uint64_t depth = 1U; depth <= 3U; ++depth) {
        for (std::uint8_t raw = 0U;
             raw < static_cast<std::uint8_t>(MonsterId::count); ++raw) {
            const MonsterDefinition* monster = monster_definition(static_cast<MonsterId>(raw));
            ARPG_REQUIRE(monster != nullptr);
            for (std::uint64_t seed = 0U; seed < 256U; ++seed) {
                const auto normal = generate_monster_affixes(seed, depth, 1U, raw, *monster);
                ARPG_REQUIRE(normal.has_value());
                ARPG_REQUIRE(normal->count == 0U);
                const auto abyss = supplement_abyss_affixes(seed, depth, 1U, raw,
                    *monster, *normal);
                ARPG_REQUIRE(abyss.has_value());
                ARPG_REQUIRE(abyss->count == 0U);
            }
            const auto cleared = supplement_abyss_affixes(7U, depth, 0U, raw,
                *monster, nonempty);
            ARPG_REQUIRE(cleared.has_value());
            ARPG_REQUIRE(cleared->count == 0U);
        }
    }
    return {};
}
```

同时把 `depth_bands_match_frozen_weights()` 的深度 3 数量权重改成 `{100,0,0,0}`，把深渊最小词条测试的首项从 `{1U,1U}` 改为 `{4U,1U}`。加入一个新用例后把总数 `242 -> 243`。

- [ ] **Step 2: 运行测试确认 RED**

Run: `cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1; ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure`

Expected: 深度 1–3 仍可能生成普通词条，深渊还会强制补充至少一个词条。

- [ ] **Step 3: 实现精确深度保护**

```cpp
constexpr std::array<AffixDepthBand, 5> kAffixDepthBands{{
    {3U, {{100U, 0U, 0U, 0U}}, {{100U, 0U, 0U}}},
    {9U, {{55U, 38U, 7U, 0U}}, {{80U, 20U, 0U}}},
    {19U, {{30U, 45U, 20U, 5U}}, {{50U, 40U, 10U}}},
    {39U, {{15U, 35U, 35U, 15U}}, {{25U, 50U, 25U}}},
    {(std::numeric_limits<std::uint64_t>::max)(),
        {{5U, 20U, 40U, 35U}}, {{10U, 35U, 55U}}},
}};
```

在 `supplement_abyss_affixes_with_catalog()` 完成目录、怪物和输入集合合法性校验之后、读取 `minimum_abyss_affixes()` 之前加入：

```cpp
if (depth <= 3U) return MonsterAffixSet{};
```

- [ ] **Step 4: 运行战斗测试确认 GREEN**

Run: `ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure`

Expected: `243 cases, 0 failures`，且深度 4 的既有权重断言保持通过。

- [ ] **Step 5: 提交**

```powershell
git add src/combat/monster_affix_generation.cpp tests/combat/monster_affix_generation_tests.cpp tests/combat/combat_test_main.cpp
git commit -m "balance: suppress monster affixes through depth three"
```

### Task 3: 增加绕过深渊倍率的百分比回血接口

**Files:**
- Modify: `tests/combat/player_health_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `src/combat/combat_world.hpp:41-52`
- Modify: `src/combat/combat_world.cpp:726-741`

**Interfaces:**
- Consumes: `scale_basis_points(..., BasisPointRounding::ceil)` 与 `player_defeated()`。
- Produces: `[[nodiscard]] int restore_player_health_percent(std::uint16_t maximum_health_basis_points) noexcept`，返回实际恢复量。

- [ ] **Step 1: 写入回血边界失败测试**

```cpp
arpg::test::Failure percent_health_restore_uses_actual_max_and_bypasses_abyss_multiplier() noexcept {
    CombatEncounterConfig config = single_chaser_encounter();
    config.player_build.values.max_health = 10000;  // actual max becomes 1001
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 500, 0);
    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 251);
    ARPG_REQUIRE(world.snapshot().player.hp == 751);
    arpg::test::CombatWorldTestAccess::set_player_resources(world, 900, 0);
    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 101);
    ARPG_REQUIRE(world.snapshot().player.hp == 1001);
    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 0);

    config = single_chaser_encounter();
    config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::exhausted_recovery);
    CombatWorld abyss{config};
    arpg::test::CombatWorldTestAccess::set_player_resources(abyss, 600, 0);
    ARPG_REQUIRE(abyss.restore_player_health_percent(2500U) == 250);
    ARPG_REQUIRE(abyss.snapshot().player.hp == 850);
    ARPG_REQUIRE(abyss.restore_player_health_percent(0U) == 0);
    return {};
}

arpg::test::Failure percent_health_restore_never_revives_defeated_player() noexcept {
    CombatEncounterConfig config = single_chaser_encounter();
    config.wave = {};
    CombatWorld world{config};
    arpg::test::CombatWorldTestAccess::apply_damage(world,
        world.snapshot().player.max_hp, Vec3{}, FeedbackLevel::heavy);
    const auto death = world.death_snapshot();
    ARPG_REQUIRE(death.has_value());
    const std::uint64_t death_tick = death->tick;
    const std::uint64_t final_damage = death->final_damage;
    ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 0);
    ARPG_REQUIRE(world.snapshot().player.hp == 0);
    ARPG_REQUIRE(world.death_snapshot().has_value());
    ARPG_REQUIRE(world.death_snapshot()->tick == death_tick);
    ARPG_REQUIRE(world.death_snapshot()->final_damage == final_damage);
    return {};
}
```

加入两个用例并把总数 `243 -> 245`。

- [ ] **Step 2: 运行测试确认 RED**

Run: `cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1; ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure`

Expected: 编译失败，提示 `CombatWorld` 没有 `restore_player_health_percent`。

- [ ] **Step 3: 实现最小战斗层接口**

在头文件 public 区声明接口；在 `restore_player_resources()` 旁实现：

```cpp
int CombatWorld::restore_player_health_percent(
    std::uint16_t maximum_health_basis_points) noexcept {
    if (maximum_health_basis_points == 0U || player_.max_hp <= 0
            || player_defeated()) {
        return 0;
    }
    const int requested = scale_basis_points(player_.max_hp,
        maximum_health_basis_points, BasisPointRounding::ceil);
    const int missing = (std::max)(0, player_.max_hp - player_.hp);
    const int actual = (std::min)(requested, missing);
    player_.hp += actual;
    return actual;
}
```

不要调用 `restore_player_resources()`，因为它会乘 `player_resource_restore_bp`。

- [ ] **Step 4: 运行战斗测试确认 GREEN**

Run: `ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure`

Expected: `245 cases, 0 failures`。

- [ ] **Step 5: 提交**

```powershell
git add src/combat/combat_world.hpp src/combat/combat_world.cpp tests/combat/player_health_tests.cpp tests/combat/combat_test_main.cpp
git commit -m "feat: restore fixed percent health after committed potion use"
```

### Task 4: 建立独立随机域与固定容量生命药数据模型

**Files:**
- Create: `src/dungeon/health_potion_loot.hpp`
- Create: `src/dungeon/health_potion_loot.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `src/dungeon/dungeon_types.hpp`
- Create: `tests/dungeon/dungeon_health_potion_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Interfaces:**
- Consumes: `core::DeterministicRng::derive_stream()`、`combat::Vec3`、现有 `DungeonSnapshot`。
- Produces: `roll_health_potion_drop(room_seed, spawn_ordinal)`、`health_potion_auto_use_eligible(hp,max_hp)`、`GroundHealthPotion`、快照、回执及最多四瓶的待提交声明。

- [ ] **Step 1: 写入常量、随机确定性、阈值与固定容量失败测试**

新建 `dungeon_health_potion_tests.cpp`，先加入三个用例：

```cpp
arpg::test::Failure health_potion_rules_are_frozen() noexcept {
    ARPG_REQUIRE(arpg::dungeon::kHealthPotionDropChanceBp == 3000U);
    ARPG_REQUIRE(arpg::dungeon::kHealthPotionRestoreBp == 2500U);
    ARPG_REQUIRE(arpg::dungeon::kHealthPotionAutoUseThresholdBp == 7500U);
    ARPG_REQUIRE(arpg::dungeon::kGroundHealthPotionCapacity == 192U);
    ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(0U) == 1U);
    ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(191U) == 383U);
    return {};
}

arpg::test::Failure health_potion_roll_is_deterministic_and_has_both_outcomes() noexcept {
    bool found_drop = false;
    bool found_miss = false;
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        const bool first = arpg::dungeon::roll_health_potion_drop(seed, 37U);
        const bool second = arpg::dungeon::roll_health_potion_drop(seed, 37U);
        ARPG_REQUIRE(first == second);
        found_drop = found_drop || first;
        found_miss = found_miss || !first;
    }
    ARPG_REQUIRE(found_drop);
    ARPG_REQUIRE(found_miss);
    return {};
}

arpg::test::Failure health_potion_threshold_is_integer_exact() noexcept {
    using arpg::dungeon::health_potion_auto_use_eligible;
    ARPG_REQUIRE(!health_potion_auto_use_eligible(751, 1000));
    ARPG_REQUIRE(health_potion_auto_use_eligible(750, 1000));
    ARPG_REQUIRE(health_potion_auto_use_eligible(749, 1000));
    ARPG_REQUIRE(!health_potion_auto_use_eligible(0, 1000));
    ARPG_REQUIRE(!health_potion_auto_use_eligible(1, 0));
    ARPG_REQUIRE(health_potion_auto_use_eligible(3, 4));
    ARPG_REQUIRE(!health_potion_auto_use_eligible(4, 4));
    return {};
}
```

把新文件加入 `arpg_dungeon_tests`，在 `dungeon_test_main.cpp` 声明并注册 `dungeon_health_potion_suite()`；增加 `ARPG_HEALTH_POTION_ONLY` 分支，focused 期望为 `3`，全量期望从 `287` 改为 `290`。

- [ ] **Step 2: 运行测试确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1
$env:ARPG_HEALTH_POTION_ONLY='1'; & .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe; Remove-Item Env:ARPG_HEALTH_POTION_ONLY
```

Expected: 编译失败，缺少 `health_potion_loot.hpp` 及对应符号。

- [ ] **Step 3: 写入独立规则与固定类型**

`health_potion_loot.hpp` 使用以下完整公共契约：

```cpp
#pragma once

#include "combat/combat_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::dungeon {

inline constexpr std::uint16_t kHealthPotionDropChanceBp = 3000U;
inline constexpr std::uint16_t kHealthPotionRestoreBp = 2500U;
inline constexpr std::uint16_t kHealthPotionAutoUseThresholdBp = 7500U;
inline constexpr std::size_t kGroundHealthPotionCapacity = 192U;
inline constexpr std::size_t kPendingHealthPotionClaimCapacity = 4U;

[[nodiscard]] constexpr std::uint16_t health_potion_claim_ordinal(
    std::uint16_t spawn_ordinal) noexcept {
    return static_cast<std::uint16_t>(spawn_ordinal * 2U + 1U);
}

struct GroundHealthPotion final {
    bool active{};
    std::uint16_t spawn_ordinal{};
    std::uint16_t claim_ordinal{};
    combat::Vec3 position{};
};

struct GroundHealthPotionSnapshot final {
    std::uint16_t spawn_ordinal{};
    std::uint16_t claim_ordinal{};
    combat::Vec3 position{};
};

struct HealthPotionPickupReceipt final {
    bool valid{};
    bool room_clear{};
    std::uint16_t consumed_count{};
    std::uint64_t commit_generation{};
    int restored_hp{};
};

struct PendingHealthPotionClaim final {
    std::array<std::uint16_t, kPendingHealthPotionClaimCapacity> spawn_ordinals{};
    std::uint8_t count{};
    int expected_hp{};
    int expected_max_hp{};
};

[[nodiscard]] bool roll_health_potion_drop(
    std::uint64_t room_seed, std::uint16_t spawn_ordinal) noexcept;
[[nodiscard]] bool health_potion_auto_use_eligible(
    int hp, int max_hp) noexcept;

}  // namespace arpg::dungeon
```

`health_potion_loot.cpp` 使用不会触碰任何现有 RNG 状态的独立域：

```cpp
#include "dungeon/health_potion_loot.hpp"

#include "core/deterministic_rng.hpp"

namespace arpg::dungeon {
namespace {
constexpr std::uint64_t kHealthPotionChanceDomain = 0x48505F504F544E31ULL;
}

bool roll_health_potion_drop(
    std::uint64_t room_seed, std::uint16_t spawn_ordinal) noexcept {
    auto ordinal = core::DeterministicRng::derive_stream(
        room_seed, static_cast<std::uint64_t>(spawn_ordinal));
    auto chance = core::DeterministicRng::derive_stream(
        ordinal.next_u64(), kHealthPotionChanceDomain);
    return chance.next_bounded(10000U).value_or(9999U)
        < kHealthPotionDropChanceBp;
}

bool health_potion_auto_use_eligible(int hp, int max_hp) noexcept {
    if (hp <= 0 || max_hp <= 0 || hp > max_hp) return false;
    return static_cast<std::int64_t>(hp) * 10000
        <= static_cast<std::int64_t>(max_hp)
            * kHealthPotionAutoUseThresholdBp;
}

}  // namespace arpg::dungeon
```

在 `dungeon_types.hpp` 引入该头文件，加入：

```cpp
static_assert(kGroundHealthPotionCapacity == kGroundDropCapacity);
```

向 `DungeonDiagnostics` 追加 `health_potion_ground_saturation_count`；向 `DungeonSnapshot` 追加：

```cpp
std::uint16_t ground_health_potion_count{};
std::array<GroundHealthPotionSnapshot, kGroundHealthPotionCapacity>
    ground_health_potions{};
HealthPotionPickupReceipt health_potion_pickup_receipt{};
std::optional<std::uint16_t> pending_health_potion_spawn_ordinal{};
```

把 `health_potion_pickup` 追加在 `PendingSaveKind` 末尾，并在 `PendingSave` 末尾追加：

```cpp
std::optional<PendingHealthPotionClaim> health_potion_claim{};
```

不改 `DungeonRunState`、`ItemOwnershipState` 或任何 persistence codec。

- [ ] **Step 4: 运行 focused 测试确认 GREEN**

Run: `cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1; $env:ARPG_HEALTH_POTION_ONLY='1'; & .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe; Remove-Item Env:ARPG_HEALTH_POTION_ONLY`

Expected: `3 cases, 0 failures`。

- [ ] **Step 5: 提交**

```powershell
git add src/dungeon/health_potion_loot.hpp src/dungeon/health_potion_loot.cpp src/dungeon/CMakeLists.txt src/dungeon/dungeon_types.hpp tests/dungeon/dungeon_health_potion_tests.cpp tests/dungeon/CMakeLists.txt tests/dungeon/dungeon_test_main.cpp
git commit -m "feat: define deterministic fixed-capacity health potion loot"
```

### Task 5: 接入击杀掉落、固定地面池与地下城快照

**Files:**
- Modify: `tests/dungeon/dungeon_health_potion_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_support.hpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp:462-482,557-563,1044-1064,1431-1500`
- Modify: `src/dungeon/dungeon_snapshot.cpp:122-148`
- Modify: `src/dungeon/dungeon_transition.cpp:1652-1661`

**Interfaces:**
- Consumes: `roll_coupon_drop()`、`roll_material_drop()`、`roll_health_potion_drop()` 及 `material_claimed_drop_bits`。
- Produces: 强化券优先、普通材料可共存、固定 192 槽、可快照且可随房间销毁的地面生命药。

- [ ] **Step 1: 写入掉落优先级、随机隔离、池边界和快照失败测试**

向同一 suite 增加四个用例并把 focused `3 -> 7`、全量 `290 -> 294`：

```cpp
arpg::test::Failure coupon_has_priority_and_common_material_can_coexist_with_potion() noexcept;
arpg::test::Failure adding_potions_does_not_change_existing_material_or_coupon_rolls() noexcept;
arpg::test::Failure ground_potion_pool_accepts_spawn_zero_and_191_without_overwrite() noexcept;
arpg::test::Failure potion_snapshot_is_sorted_by_spawn_ordinal_and_clears_on_room_exit() noexcept;
```

每个用例使用测试访问器推入 reward-eligible `CombatEventKind::defeated`。测试循环寻找三类固定种子并冻结断言：

```cpp
const dungeon::GroundMaterialSnapshot* find_ground_material_snapshot(
    const dungeon::DungeonSnapshot& snapshot,
    std::uint16_t ordinal) noexcept {
    for (std::uint16_t index = 0U;
         index < snapshot.ground_material_count; ++index) {
        if (snapshot.ground_materials[index].ordinal == ordinal) {
            return &snapshot.ground_materials[index];
        }
    }
    return nullptr;
}

if (coupon.has_value()) {
    ARPG_REQUIRE(snapshot.ground_health_potion_count == 0U);
    const auto* secondary = find_ground_material_snapshot(
        snapshot, health_potion_claim_ordinal(spawn));
    ARPG_REQUIRE(secondary != nullptr);
    ARPG_REQUIRE(secondary->source == GroundMaterialSource::monster_coupon);
} else if (roll_health_potion_drop(seed, spawn)) {
    ARPG_REQUIRE(snapshot.ground_health_potion_count == 1U);
    ARPG_REQUIRE(snapshot.ground_health_potions[0].claim_ordinal
        == health_potion_claim_ordinal(spawn));
}
```

随机隔离测试在接入前先保存同一批 `roll_material_drop()` 与 `roll_coupon_drop()` 结果，接入击杀后逐项比较；另找一个 common 与 potion 同时命中的种子，确认二者都在快照中。

池边界测试安装 spawn 0 与 191，并填满全部 192 槽；重复放入活动槽必须保持旧对象字节不变且 `health_potion_ground_saturation_count` 增加 1。包含 `allocation_probe.hpp`，在填池、快照打包和稳定扫描前后比较 `allocation_count()`，结果必须完全相同。

- [ ] **Step 2: 运行 focused 测试确认 RED**

Run: `cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1; $env:ARPG_HEALTH_POTION_ONLY='1'; & .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe; Remove-Item Env:ARPG_HEALTH_POTION_ONLY`

Expected: 新测试找不到任何地面生命药，且快照计数始终为零。

- [ ] **Step 3: 实现固定地面池和强化券优先掉落**

向 `DungeonSession` 增加：

```cpp
[[nodiscard]] bool place_ground_health_potion(
    std::uint16_t spawn_ordinal, combat::Vec3 position) noexcept;

std::array<GroundHealthPotion, kGroundHealthPotionCapacity>
    ground_health_potions_{};
HealthPotionPickupReceipt health_potion_pickup_receipt_{};
```

放置函数必须按 spawn index 定位：

```cpp
bool DungeonSession::place_ground_health_potion(
    std::uint16_t spawn_ordinal, combat::Vec3 position) noexcept {
    if (spawn_ordinal >= ground_health_potions_.size()) return false;
    GroundHealthPotion& slot = ground_health_potions_[spawn_ordinal];
    if (slot.active) {
        saturating_increment(
            diagnostics_.health_potion_ground_saturation_count);
        return false;
    }
    slot = {true, spawn_ordinal,
        health_potion_claim_ordinal(spawn_ordinal), position};
    return true;
}
```

把 `roll_ground_materials()` 的次级位改为单次领取位判定、先券后药：

```cpp
if (claim_material_roll(coupon_ordinal)) {
    const bool abyss_monster = stable_state_.current_room.is_abyss
        && stable_state_.abyss.lifecycle == abyss::AbyssLifecycle::started;
    const auto coupon = roll_coupon_drop(stable_state_.current_room.seed,
        spawn_ordinal, stable_state_.current_room.depth,
        event.affix_score, abyss_monster);
    if (coupon.has_value()) {
        static_cast<void>(place_ground_material(coupon_ordinal,
            GroundMaterialSource::monster_coupon, position, *coupon));
    } else if (roll_health_potion_drop(
            stable_state_.current_room.seed, spawn_ordinal)) {
        static_cast<void>(place_ground_health_potion(spawn_ordinal, position));
    }
}
```

普通材料分支保持原样。把药瓶池清零接入 `construct_current_room()`、`clear_transient_room_state()`、成功 transition/abyss abandon 分支；不要在普通/深渊清房成功时整体清零。

在 `build_dungeon_snapshot()` 中按数组索引升序打包所有活动药瓶，并复制回执。待提交序号在 Task 6 接入。

- [ ] **Step 4: 运行 focused 与全量地下城测试确认 GREEN**

Run:

```powershell
$env:ARPG_HEALTH_POTION_ONLY='1'; & .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe; Remove-Item Env:ARPG_HEALTH_POTION_ONLY
ctest --preset windows-msvc-core-debug -R '^dungeon\.units$' --output-on-failure
```

Expected: focused `7 cases, 0 failures`；全量 `294 cases, 0 failures`。

- [ ] **Step 5: 提交**

```powershell
git add src/dungeon/dungeon_session.hpp src/dungeon/dungeon_session.cpp src/dungeon/dungeon_snapshot.cpp src/dungeon/dungeon_transition.cpp tests/dungeon/dungeon_health_potion_tests.cpp tests/dungeon/dungeon_test_support.hpp tests/dungeon/dungeon_test_main.cpp
git commit -m "feat: drop and snapshot ground health potions"
```

### Task 6: 实现无距离自动使用、原子保存和最终击杀清房合并

**Files:**
- Modify: `tests/dungeon/dungeon_health_potion_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_support.hpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp:285-375,1532-1656`
- Modify: `src/dungeon/dungeon_transition.cpp:653-706,1204-1275,1370-1400,1402-1689`
- Modify: `src/dungeon/dungeon_snapshot.cpp:138-160`

**Interfaces:**
- Consumes: Task 3 的 `restore_player_health_percent(2500U)`、Task 5 的固定药瓶池、现有 `PendingSaveResult` 与双槽 `SaveStore`。
- Produces: 私有 `request_health_potion_pickup(spawn_ordinal)`、清房批量声明、精确提交后的回血/回执和失败无副作用语义。

- [ ] **Step 1: 写入阈值、距离、顺序与死亡失败测试**

增加以下四个用例：

```cpp
arpg::test::Failure potion_above_threshold_stays_grounded_but_75_percent_ignores_distance() noexcept;
arpg::test::Failure potion_auto_use_scans_stable_spawn_order_and_rechecks_after_commit() noexcept;
arpg::test::Failure dead_player_or_death_snapshot_never_requests_or_consumes_potion() noexcept;
arpg::test::Failure committed_pickup_heals_caps_removes_and_publishes_exact_receipt() noexcept;
```

关键断言必须为：

```cpp
// 100 units away still accepts at exactly 75%.
set_player_resources(session, 750, 0);
install_ground_health_potion(session, 9U, {100.0F, 100.0F, 0.0F});
session.tick({});
ARPG_REQUIRE(session.pending_save_view()->kind
    == dungeon::PendingSaveKind::health_potion_pickup);
ARPG_REQUIRE(session.pending_save_view()->pickup_ordinal == 9U);

// Stable order is spawn 2, 6, 9 regardless of installation order.
ARPG_REQUIRE(session.snapshot().pending_health_potion_spawn_ordinal == 2U);

// Exact commit from 750/1000.
ARPG_REQUIRE(before.combat->player.hp == 750);
resolve_exact_commit(session);
const auto after = session.snapshot();
ARPG_REQUIRE(after.combat->player.hp == 1000);
ARPG_REQUIRE(after.health_potion_pickup_receipt.valid);
ARPG_REQUIRE(after.health_potion_pickup_receipt.restored_hp == 250);
ARPG_REQUIRE(after.health_potion_pickup_receipt.consumed_count == 1U);
```

HP 为 751 时不创建 pending；随后在同一 `awaiting_exit` 房间把 HP 改为 750，下一 tick 必须创建 pending，证明高血量留地药会在同房间后续自动使用。HP 为 750、749 时创建。真实死亡快照、无 combat 实例或不允许领取的 phase 存在时 pending、HP、地面药、回执均不变。稳定顺序用例在 `session.tick()` 与 exact resolve 前后包住 `allocation_count()`，证明领取热路径没有新增分配。

- [ ] **Step 2: 运行 focused 测试确认 RED**

把 suite 数从 `7 -> 11`，全量 `294 -> 298` 后运行 focused。Expected: 不会创建 `health_potion_pickup` pending，回血与回执均缺失。

- [ ] **Step 3: 实现单瓶待提交请求与一致性校验**

向 `DungeonSession` private 区增加：

```cpp
[[nodiscard]] RequestResult request_health_potion_pickup(
    std::uint16_t spawn_ordinal) noexcept;
[[nodiscard]] bool pending_health_potion_cache_consistent() const noexcept;
void apply_committed_health_potions(
    const PendingHealthPotionClaim& claim, bool room_clear) noexcept;
```

请求函数必须遵守以下顺序；它不读取位置：

```cpp
RequestResult DungeonSession::request_health_potion_pickup(
    std::uint16_t spawn_ordinal) noexcept {
    if (!pending_item_cache_consistent()
            || !pending_material_cache_consistent()
            || !pending_health_potion_cache_consistent()) {
        enter_fault(DungeonFault::save_receipt_mismatch);
        return RequestResult::faulted;
    }
    if (!item_request_phase(phase_) || pending_save_.has_value()
            || !combat_.has_value()
            || spawn_ordinal >= ground_health_potions_.size()) {
        return RequestResult::rejected;
    }
    const GroundHealthPotion& ground = ground_health_potions_[spawn_ordinal];
    const combat::CombatSnapshot snapshot = combat_->snapshot();
    if (!ground.active || ground.spawn_ordinal != spawn_ordinal
            || ground.claim_ordinal != health_potion_claim_ordinal(spawn_ordinal)
            || combat_->death_snapshot().has_value()
            || !health_potion_auto_use_eligible(
                snapshot.player.hp, snapshot.player.max_hp)) {
        return RequestResult::rejected;
    }
    if (bit_is_set(stable_state_.item_ownership.material_claimed_drop_bits,
            ground.claim_ordinal)) {
        enter_fault(DungeonFault::invalid_item_state);
        return RequestResult::faulted;
    }
    if (stable_state_.commit_generation
            == (std::numeric_limits<std::uint64_t>::max)()) {
        enter_fault(DungeonFault::commit_generation_overflow);
        return RequestResult::faulted;
    }
    PendingSave& pending = pending_save_.prepare();
    if (!copy_run_state_reusing_items(pending.next_state, stable_state_)) {
        pending_save_.reset();
        return RequestResult::rejected;
    }
    ++pending.next_state.commit_generation;
    set_bit(pending.next_state.item_ownership.material_claimed_drop_bits,
        ground.claim_ordinal);
    pending.kind = PendingSaveKind::health_potion_pickup;
    pending.expected_generation = pending.next_state.commit_generation;
    pending.transition = TransitionKind::none;
    pending.direction = ExitDirection::none;
    pending.resume_phase = phase_;
    pending.pickup_ordinal = spawn_ordinal;
    pending.death_snapshot.reset();
    pending.reinforcement_receipt.reset();
    pending.health_potion_claim = PendingHealthPotionClaim{
        {{spawn_ordinal, 0U, 0U, 0U}}, 1U,
        snapshot.player.hp, snapshot.player.max_hp};
    phase_ = RoomPhase::committing;
    return RequestResult::accepted;
}
```

`pending_health_potion_cache_consistent()` 必须校验：kind 与 optional 是否一致；count 在 1–4；spawn 严格升序；每个地面对象仍活动且领取位正确；stable 位为 0、next 位为 1；`expected_hp/max_hp` 与当前战斗快照相同；单瓶事务除 generation 和对应 claim bit 外不改变 ownership。

在 `commit_pending_save()` 的所有其他 cache 检查旁调用该函数。`not_committed` 与 `indeterminate` 分支保持在 publish 之前，所以不需要补偿副作用。

- [ ] **Step 4: 接入稳定自动扫描和精确提交后副作用**

在 `request_nearby_pickups()` 最前按 spawn 0..191 扫描：

```cpp
for (std::uint16_t spawn = 0U;
     spawn < ground_health_potions_.size(); ++spawn) {
    if (!ground_health_potions_[spawn].active) continue;
    const RequestResult result = request_health_potion_pickup(spawn);
    if (result != RequestResult::rejected) return;
}
```

保留装备和材料的既有距离扫描。精确提交校验通过、`publish_run_state_reusing_items()` 完成后，复制 pending claim，再 reset pending。单瓶成功分支调用：

```cpp
void DungeonSession::apply_committed_health_potions(
    const PendingHealthPotionClaim& claim, bool room_clear) noexcept {
    HealthPotionPickupReceipt receipt{};
    receipt.valid = true;
    receipt.room_clear = room_clear;
    receipt.commit_generation = stable_state_.commit_generation;
    for (std::uint8_t index = 0U; index < claim.count; ++index) {
        const std::uint16_t spawn = claim.spawn_ordinals[index];
        receipt.restored_hp += combat_->restore_player_health_percent(
            kHealthPotionRestoreBp);
        ground_health_potions_[spawn] = {};
        ++receipt.consumed_count;
    }
    health_potion_pickup_receipt_ = receipt;
}
```

单瓶成功后恢复 `resume_phase`。`DungeonSnapshot` 仅在 kind 是 `health_potion_pickup` 时公开 `pending_health_potion_spawn_ordinal`。失败或不确定结果不更新 receipt。

- [ ] **Step 5: 运行前四个事务用例确认 GREEN**

Run: `cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1; $env:ARPG_HEALTH_POTION_ONLY='1'; & .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe; Remove-Item Env:ARPG_HEALTH_POTION_ONLY`

Expected: `11 cases, 0 failures`。

- [ ] **Step 6: 写入失败回滚、错误回执与崩溃窗口测试**

再增加四个用例：

```cpp
arpg::test::Failure failed_or_indeterminate_save_never_heals_removes_or_reports_success() noexcept;
arpg::test::Failure mismatched_pending_or_replaced_ground_faults_before_health_side_effects() noexcept;
arpg::test::Failure committed_claim_without_runtime_resolve_does_not_replay_after_reload() noexcept;
```

冻结以下结果：

```text
not_committed: phase 恢复、HP/ground/stable claim/receipt 不变，可重试且 next_state 相同。
indeterminate: phase=faulted，但 HP/ground/stable claim/receipt 不变。
错误 generation、错误 kind、被替换地面对象：save_receipt_mismatch，且无回血。
真实 SaveStore commit 后故意不 resolve：重建 session 后同一击杀不再生成药，claim 位仍为 1，
没有 pickup receipt，也没有对旧房间 HP 的重放。
```

把 focused `11 -> 14`，全量 `298 -> 301`。

- [ ] **Step 7: 运行失败路径测试确认 RED 后补齐校验**

Run: focused executable。Expected RED: 至少错误 pending/被替换 ground 未被新 cache 拒绝。补齐 Step 3 的一致性校验后再次运行，Expected: `14 cases, 0 failures`。

- [ ] **Step 8: 写入最终击杀、批量选择和高血量保留测试**

再增加三个用例：

```cpp
arpg::test::Failure final_kill_low_health_folds_sorted_potions_into_clear_transaction() noexcept;
arpg::test::Failure clear_batch_selects_only_until_health_is_strictly_above_75_percent() noexcept;
arpg::test::Failure final_kill_high_health_keeps_potion_until_room_transition() noexcept;
arpg::test::Failure abyss_clear_potion_uses_post_clear_actual_max_health() noexcept;
```

低血量测试必须在同一个 `session.tick()` 后断言：

```cpp
ARPG_REQUIRE(pending->kind == PendingSaveKind::room_clear);
ARPG_REQUIRE(pending->health_potion_claim.has_value());
ARPG_REQUIRE(bit_is_set(pending->next_state.item_ownership
    .material_claimed_drop_bits, claim_ordinal));
ARPG_REQUIRE(session.snapshot().combat->player.hp == hp_before);
ARPG_REQUIRE(session.snapshot().ground_health_potion_count != 0U);
```

成功提交后才回血、移除被选药瓶并开放出口。失败提交后完整恢复，并在下一固定帧形成相同事务。批量测试从 `1/1000` 开始安装乱序 spawn 9、2、6、3，pending 必须只记录 `{2,3,6}`，投影为 `1 -> 251 -> 501 -> 751` 后停止，spawn 9 留在地面。

低血量最终击杀用例同时运行 normal 与 started abyss 两个场景，分别要求 `room_clear` 与 `abyss_clear`；场景内同时安装一个 common material，确认材料计数、common claim 和 potion secondary claim 在同一 next state 中都正确。高血量最终击杀不得产生 potion claim；清房后药瓶继续可见，成功 room transition 后池清空。

深渊顺序用例使用 `life_sacrifice` 规则，记录 clear 前受压低的 max HP，exact commit 后断言规则已清除、实际 max HP 已恢复、`restored_hp` 等于清房后 max HP 的 25% 向上取整（受缺失生命封顶），而不是按清房前 max HP 计算。

把 focused `14 -> 18`，全量 `301 -> 305`。

- [ ] **Step 9: 实现清房批量声明并合并材料 cache**

向 session 增加：

```cpp
[[nodiscard]] bool has_claimable_health_potion() const noexcept;
[[nodiscard]] bool append_clear_health_potion_claims(
    PendingSave& pending) noexcept;
```

`append_clear_health_potion_claims()` 获取当前战斗快照；若死亡或阈值不满足则返回 true 且不设置 optional。否则按 spawn 升序选择，逐瓶用：

```cpp
const int restore = combat::scale_basis_points(max_hp,
    kHealthPotionRestoreBp, combat::BasisPointRounding::ceil);
projected_hp = (std::min)(max_hp, projected_hp + restore);
```

每选一瓶就把 secondary claim bit 写入 `pending.next_state`，最多四瓶，严格高于 75% 后停止。`prepare_room_clear()` 的直接 publish 条件改为：

```cpp
if (!started_abyss && !has_ground_materials()
        && !has_claimable_health_potion()) {
    settle_room_experience();
    publish_room_clear();
    return;
}
```

创建 clear pending 后依次执行 `vacuum_room_materials()` 和 `append_clear_health_potion_claims()`。`pending_material_cache_consistent()` 构造 expected claims 时，在材料 claims 之外合并 pending 中每个药瓶的 claim ordinal；材料计数与 discovery bits 仍只由材料改变。

clear exact commit 中：

```cpp
if (abyss_clear_commit) {
    combat_->clear_abyss_rule_preserving_resources();
}
if (committed_health_claim.has_value()) {
    apply_committed_health_potions(*committed_health_claim, true);
}
publish_room_clear();
```

只移除声明中的药瓶，不能整体清 `ground_health_potions_`。

- [ ] **Step 10: 运行 focused、地下城全量和 runtime 测试**

Run:

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1
$env:ARPG_HEALTH_POTION_ONLY='1'; & .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe; Remove-Item Env:ARPG_HEALTH_POTION_ONLY
ctest --preset windows-msvc-core-debug -R '^dungeon\.units$' --output-on-failure
```

Expected: focused `18 cases, 0 failures`；地下城全量 `305 cases, 0 failures`。图形 platform target 在 core preset 不存在时，平台测试留到 Task 7 的 `windows-msvc-debug`。

- [ ] **Step 11: 提交**

```powershell
git add src/dungeon/dungeon_session.hpp src/dungeon/dungeon_session.cpp src/dungeon/dungeon_transition.cpp src/dungeon/dungeon_snapshot.cpp tests/dungeon/dungeon_health_potion_tests.cpp tests/dungeon/dungeon_test_support.hpp tests/dungeon/dungeon_test_main.cpp
git commit -m "feat: atomically auto-consume health potions"
```

### Task 7: 制作高清药瓶资源、纯色中文标签与 HUD 成功反馈

**Files:**
- Create: `art_source/stage12/items/health-potion-v1.png`
- Modify: `tools/build_item_material_atlas.py`
- Modify: `assets/stage12/items_ui.png`
- Modify: `assets/stage12/items_ui_material.png`
- Modify: `src/platform/raylib/material_asset_types.hpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/material_loot_view.hpp`
- Modify: `src/platform/raylib/material_loot_view.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp:441-471`
- Modify: `tests/platform/material_loot_view_tests.cpp`
- Modify: `tests/platform/item_material_asset_pipeline_tests.py`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes: `DungeonSnapshot::ground_health_potions` 与 `health_potion_pickup_receipt`、现有高分辨率 CJK HUD 字体和 `items_ui` 图集。
- Produces: `MaterialSpriteId::health_potion`、`SecondaryLootKind::health_potion`、无重叠纯红“生命药”标签、红色光柱和“生命药 +N HP”回执。

- [ ] **Step 1: 写入专用资源、纯色标签、避让和 HUD 文案失败测试**

向 `material_loot_view_tests.cpp` 增加三个用例，把 platform 总数 `443 -> 446`：

```cpp
arpg::test::Failure health_potion_uses_dedicated_sprite_and_pure_red_label() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.ground_health_potion_count = 1U;
    snapshot.ground_health_potions[0] = {7U, 15U, {2.0F, 1.0F, 0.0F}};
    const platform::MaterialLootView view =
        platform::build_material_loot_view(snapshot, 1280.0F, 720.0F);
    ARPG_REQUIRE(view.count == 1U);
    ARPG_REQUIRE(view.labels[0].kind
        == platform::SecondaryLootKind::health_potion);
    ARPG_REQUIRE(view.labels[0].sprite
        == platform::MaterialSpriteId::health_potion);
    ARPG_REQUIRE(view.labels[0].text_color.r == 255U);
    ARPG_REQUIRE(view.labels[0].text_color.g == 48U);
    ARPG_REQUIRE(view.labels[0].text_color.b == 48U);
    ARPG_REQUIRE(view.labels[0].text_color.a == 255U);
    ARPG_REQUIRE(view.labels[0].emphasized);
    ARPG_REQUIRE(std::strcmp(view.labels[0].text.data(), "生命药") == 0);
    const auto manifest = platform::default_material_manifest();
    const auto* frame = platform::find_material_frame(
        manifest, platform::MaterialSpriteId::health_potion);
    ARPG_REQUIRE(frame != nullptr);
    ARPG_REQUIRE(frame->atlas == platform::MaterialAtlasId::items_ui);
    ARPG_REQUIRE(frame->source.x == 768.0F);
    ARPG_REQUIRE(frame->source.y == 384.0F);
    return {};
}

arpg::test::Failure overlapping_secondary_loot_labels_are_resolved() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.ground_health_potion_count = 2U;
    snapshot.ground_health_potions[0] = {2U, 5U, {0.0F, 0.0F, 0.0F}};
    snapshot.ground_health_potions[1] = {3U, 7U, {0.0F, 0.0F, 0.0F}};
    const auto view = platform::build_material_loot_view(snapshot, 1280.0F, 720.0F);
    ARPG_REQUIRE(view.count == 2U);
    ARPG_REQUIRE(!platform::loot_label_rects_overlap(
        view.labels[0].rect, view.labels[1].rect));
    return {};
}

arpg::test::Failure health_potion_feedback_only_observes_new_committed_receipts() noexcept {
    platform::MaterialPickupFeedbackState feedback{};
    dungeon::DungeonSnapshot snapshot{};
    snapshot.health_potion_pickup_receipt = {true, false, 1U, 10U, 250};
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);  // establish baseline
    snapshot.health_potion_pickup_receipt.commit_generation = 11U;
    snapshot.health_potion_pickup_receipt.restored_hp = 251;
    const auto committed = feedback.observe(snapshot);
    ARPG_REQUIRE(committed.ready);
    ARPG_REQUIRE(committed.emphasized);
    ARPG_REQUIRE(std::strcmp(
        committed.text.bytes.data(), "生命药 +251 HP") == 0);
    ARPG_REQUIRE(!feedback.observe(snapshot).ready);
    return {};
}
```

向 Python asset test 增加专用源图与图集单元测试：

```python
def test_health_potion_source_and_atlas_cell_are_high_resolution_red_glass(self) -> None:
    source = Image.open(BUILDER.HEALTH_POTION_SOURCE).convert("RGBA")
    self.assertGreaterEqual(source.width, 1024)
    self.assertGreaterEqual(source.height, 1024)
    icon = BUILDER.crop_cell(
        Image.open(ROOT / "assets/stage12/items_ui.png").convert("RGBA"),
        BUILDER.ICON_CELLS["health_potion"])
    visible = [pixel for pixel in icon.get_flattened_data() if pixel[3] >= 96]
    self.assertGreater(len(visible), 1500)
    self.assertGreater(sum(red > green * 1.35 and red > blue * 1.15
                           for red, green, blue, _ in visible),
                       len(visible) * 0.28)
    self.assertGreaterEqual(BUILDER.outline_contrast_score(icon), 0.18)
```

- [ ] **Step 2: 运行测试确认 RED**

Run:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
ctest --preset windows-msvc-debug -R '^(platform\.units|stage12\.item_material_asset_pipeline)$' --output-on-failure
```

Expected: C++ 编译失败，缺少药瓶 sprite/kind；Python 失败，缺少高分辨率 source 与 `ICON_CELLS["health_potion"]`。

- [ ] **Step 3: 使用图像生成技能创建无文字高分辨率源图**

先完整读取 `imagegen` skill，然后调用图像生成工具，使用以下冻结提示词；输出必须保存为 `art_source/stage12/items/health-potion-v1.png`：

```text
Create a single 1024x1024 transparent-background game inventory icon for a dark-fantasy 2.5D action ARPG. Center one compact health potion bottle: saturated ruby-red transparent glass, clearly visible red liquid, thick dark high-contrast silhouette, antique brushed-gold metal cap and neck band, subtle white specular highlights, physically coherent glass reflections, premium hand-painted realistic material rendering, front three-quarter view, symmetric readable silhouette, generous transparent padding, no platform, no scenery, no particles, no glow outside the bottle, absolutely no letters, numbers, symbols, logo, UI frame, watermark, or baked text.
```

用 `view_image` 原始分辨率检查：透明背景、单一主体、红玻璃、金盖、无任何文字；不符合任一项就以同一约束重新生成，不用绘图脚本伪造主视觉。

- [ ] **Step 4: 把专用源图接入第 30 号空图集单元**

在 builder 中把现有 30 个 board 图标保留为 `BOARD_ICON_NAMES`，新增：

```python
HEALTH_POTION_SOURCE = (
    ROOT / "art_source" / "stage12" / "items" / "health-potion-v1.png")
ICON_CELLS = {name: index for index, name in enumerate(BOARD_ICON_NAMES)}
ICON_CELLS["health_potion"] = len(BOARD_ICON_NAMES)  # cell 30
```

在 `build_color_atlas()` 完成 board 循环后加入：

```python
potion = contain_icon(Image.open(HEALTH_POTION_SOURCE).convert("RGBA"))
if outline_contrast_score(potion) < 0.18:
    raise RuntimeError("health_potion: authored outline contrast is too low")
cell = ICON_CELLS["health_potion"]
column = cell % (ATLAS_SIZE // CELL)
row = cell // (ATLAS_SIZE // CELL)
atlas.alpha_composite(potion, (column * CELL, row * CELL))
```

`main()` 同时检查两个 source 存在。运行：

```powershell
python .\tools\build_item_material_atlas.py
python -m unittest tests.platform.item_material_asset_pipeline_tests -v
```

Expected: 所有图标唯一、透明边缘干净、outline contrast 合格，药瓶源图至少 1024×1024。

- [ ] **Step 5: 接入 manifest、地面标签、红色光柱和 HUD**

在 `MaterialSpriteId` 的 material 段加入 `health_potion`；manifest 加：

```cpp
ARPG_ITEM_FRAME(health_potion, 30,
    MaterialLayer::body, MaterialClass::loot),
```

在 `material_loot_view.hpp` 加入：

```cpp
enum class SecondaryLootKind : std::uint8_t {
    material,
    health_potion,
};
```

并在 `MaterialLootLabel` 增加 `kind`，公开：

```cpp
[[nodiscard]] bool loot_label_rects_overlap(
    LootLabelRect left, LootLabelRect right) noexcept;
```

构建 view 时先插入材料，再遍历 `ground_health_potions`，为药瓶写入：

```cpp
label.kind = SecondaryLootKind::health_potion;
label.ordinal = potion.claim_ordinal;
label.text_color = {255U, 48U, 48U, 255U};
label.sprite = MaterialSpriteId::health_potion;
label.emphasized = true;
std::snprintf(label.text.data(), label.text.size(), "%s", "生命药");
```

全部标签按 ordinal 排序后，从前到后检测矩形重叠；重叠时每次向上移动 `kLabelHeight + 3.0F` 并重新 clamp，直到与所有前项分离。函数只使用固定数组。

`room_renderer.cpp` 不能再假设每个 label 都是材料。增加按 `SecondaryLootKind` 查找 `GroundMaterialSnapshot` 或 `GroundHealthPotionSnapshot` 位置的 helper，再沿用 emphasized 分支绘制纯红 `DrawLineEx` 光柱和专用 sprite。

`MaterialPickupFeedbackState` 增加独立 `health_potion_generation_`。第一次 observe 把材料和药瓶 generation 都设为当前基线；只对更新的有效药瓶回执输出：

```cpp
std::snprintf(feedback.text.bytes.data(), feedback.text.bytes.size(),
    "生命药 +%d HP", receipt.restored_hp);
feedback.ready = receipt.restored_hp > 0;
feedback.emphasized = feedback.ready;
```

相同 generation 不重复提示，失败事务没有有效回执，因此无成功文字。

- [ ] **Step 6: 运行平台与资产测试确认 GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
ctest --preset windows-msvc-debug -R '^(platform\.units|stage12\.item_material_asset_pipeline)$' --output-on-failure
```

Expected: platform `446 cases, 0 failures`；Python asset test 全部通过。

- [ ] **Step 7: 提交**

```powershell
git add art_source/stage12/items/health-potion-v1.png tools/build_item_material_atlas.py assets/stage12/items_ui.png assets/stage12/items_ui_material.png src/platform/raylib/material_asset_types.hpp src/platform/raylib/material_manifest.hpp src/platform/raylib/material_loot_view.hpp src/platform/raylib/material_loot_view.cpp src/platform/raylib/room_renderer.cpp tests/platform/material_loot_view_tests.cpp tests/platform/item_material_asset_pipeline_tests.py tests/platform/platform_test_main.cpp
git commit -m "feat: add high-resolution health potion presentation"
```

### Task 8: 更新有意变化的黄金轨迹并完成 Debug/Release 与三层试玩验收

**Files:**
- Modify: `tests/dungeon/dungeon_abyss_stress_tests.cpp`（仅当前三层零词条导致冻结 hash 变化）
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`（仅当快照/诊断字段的冻结比较需要纳入生命药）
- Create: `docs/validation/extreme-monster-nerf-health-potions.md`

**Interfaces:**
- Consumes: Tasks 1–7 的完整功能和正式 raylib 6.0 Release 可执行文件。
- Produces: 可复现的随机域证据、全套测试结果、前三层实际试玩记录和视觉截图。

- [ ] **Step 1: 先运行 core Debug 全量并只接受解释得通的失败**

Run:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-core-debug -Fresh
cmake --build --preset windows-msvc-core-debug --clean-first -- -j1
ctest --preset windows-msvc-core-debug --output-on-failure
```

Expected: 所有 core CTest 通过。若仅 `dungeon_abyss_stress` 的冻结 hash 因深度 1–3 从可能有词条变为无词条而失败，打印并核对轨迹的深度、门洞命中、奖励、词条数量与深度 4+ 后再更新该常量；不得用删除断言或放宽比较通过。

- [ ] **Step 2: 冻结压力快照中的新字段和随机隔离证据**

在 `dungeon_stress_tests.cpp` 的 snapshot equality/summary 中加入：

```cpp
lhs.ground_health_potion_count == rhs.ground_health_potion_count
lhs.health_potion_pickup_receipt.valid
    == rhs.health_potion_pickup_receipt.valid
lhs.health_potion_pickup_receipt.commit_generation
    == rhs.health_potion_pickup_receipt.commit_generation
lhs.health_potion_pickup_receipt.restored_hp
    == rhs.health_potion_pickup_receipt.restored_hp
lhs.diagnostics.health_potion_ground_saturation_count
    == rhs.diagnostics.health_potion_ground_saturation_count
```

并逐个比较活动药瓶的 `spawn_ordinal`、`claim_ordinal` 与 position。保留现有装备、common material、coupon、出口和房间 hash 断言，证明新随机域没有改变它们。

- [ ] **Step 3: 运行正式 Debug 图形构建与完整 CTest**

Run:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug --clean-first -- -j1
ctest --preset windows-msvc-debug --output-on-failure
```

Expected: 所有 Debug CTest 通过，包括 `combat.units`、`dungeon.units`、`platform.units` 与 `stage12.item_material_asset_pipeline`。

- [ ] **Step 4: 运行正式 Release 干净构建与完整 CTest**

Run:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-release -Fresh
cmake --build --preset windows-msvc-release --clean-first -- -j1
ctest --preset windows-msvc-release --output-on-failure
```

Expected: 全部 Release CTest 通过，`out/build/windows-msvc-release/bin/arpg_game.exe` 与两张更新后的 `items_ui` 图集已复制到运行目录。

- [ ] **Step 5: 验证没有扩大存档或材料边界**

Run:

```powershell
git diff 44f2e590413b9e17ddbf915a09b6acd9cac511fb -- src/persistence src/items/item_types.hpp src/items/material_catalog.hpp
rg -n "health_potion|生命药" src/persistence src/items
```

Expected: persistence codec、`ItemOwnershipState` 固定材料数组和 `MaterialId` 没有生命药改动；第二条命令无匹配。

- [ ] **Step 6: 使用全新存档实际试玩到第 3 层**

先完整读取 `computer-use:computer-use` skill，启动：

```powershell
& .\out\build\windows-msvc-release\bin\arpg_game.exe `
  --seed 20260723 `
  --save-dir .\out\validation\hp-potion-depth3-save
```

通过真实窗口操作完成至少第 1、2、3 层，并逐层记录：

```text
层数；普通/深渊；怪物词条数量；代表怪物 max_hp；
掉落药瓶序号；掉落时 HP/max_hp；是否立即使用；
提交前 HP；提交后 HP；HUD 文案；高血量留地表现；
退出房间后未用药是否消失；发现的问题。
```

至少主动制造以下三种现场：

1. HP 高于 75% 时看到地面红玻璃药瓶、红光柱和不重叠的纯色“生命药”。
2. 同房间把 HP 降到恰好或低于 75%，不移动、不按键，等待保存成功后看到血量增加和“生命药 +N HP”。
3. 最后一只怪物掉药时确认出口只在保存成功后开放，回血与清房同时发布。

把至少一张地面药瓶截图和一张 HUD 回执截图保存到 `docs/validation/evidence/`，用 `view_image` 原始分辨率检查文字清晰、颜色纯正、标签无重叠、图标不是旧材质占位。

- [ ] **Step 7: 写入验收记录**

`docs/validation/extreme-monster-nerf-health-potions.md` 必须包含：

```markdown
# 怪物大幅削弱与生命药验收

- Commit: 运行 `git rev-parse HEAD` 后粘贴完整 SHA
- Debug CTest: 粘贴 `ctest --preset windows-msvc-debug` 的实际通过数、总数与耗时
- Release CTest: 粘贴 `ctest --preset windows-msvc-release` 的实际通过数、总数与耗时
- Seed: 20260723
- Save: out/validation/hp-potion-depth3-save
- 深度 1–3 词条：普通/深渊均为 0
- 深度 4 回归：既有词条权重仍启用
- 药瓶事务：提交前无回血；exact commit 后回血；失败/不确定无副作用
- 视觉：独立红玻璃金盖图标、纯色“生命药”、红光柱、HUD 实际恢复量
- 试玩问题：逐条记录实际问题；没有问题时写“未发现阻断问题”
```

验收时必须把前三项说明替换为命令产生的实际值。

- [ ] **Step 8: 最终差异审查与提交**

Run:

```powershell
git status --short
git diff --check
git diff --stat 44f2e590413b9e17ddbf915a09b6acd9cac511fb
git diff 44f2e590413b9e17ddbf915a09b6acd9cac511fb -- src/persistence src/items
```

Expected: 无 whitespace error；没有无关主控工作树文件；没有存档版本/材料枚举变化。

```powershell
git add tests/dungeon/dungeon_abyss_stress_tests.cpp tests/dungeon/dungeon_stress_tests.cpp docs/validation/extreme-monster-nerf-health-potions.md docs/validation/evidence
git commit -m "test: validate monster nerf and health potion flow"
```

若两个 stress 文件没有有意变化，只提交实际改变的验证文档与截图，不用制造空 diff。

---

## 完成定义

- 八种怪物目录值与用户批准表逐项一致，所有非目标字段未改。
- 深度 1–3 普通和深渊怪物在多 seed、全怪物测试与实际试玩中均为零词条；深度 4+ 规则未退化。
- 生命药随机域独立，券优先，common material 可共存，领取位不扩展存档。
- 75% 边界、无距离扫描、稳定顺序、死亡拒绝、固定容量和房间销毁语义全部由测试冻结。
- 单瓶与最终击杀批量领取都只在 exact save commit 后回血；失败、indeterminate、错误回执及崩溃窗口不会重复回血。
- 游戏中显示专用高分辨率红玻璃金盖药瓶、纯色清晰中文、红色光柱和实际恢复量 HUD；标签不重叠。
- core Debug、图形 Debug、Release 全量 CTest 与第 1–3 层实际试玩全部完成并有证据。
