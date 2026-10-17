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

