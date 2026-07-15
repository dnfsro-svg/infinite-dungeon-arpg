# Stage 9 Monster Affixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在已验收 Stage 8 基线上交付首批 12 个可组合、确定性生成、具有 M1～M3 强度、明确预警和风险奖励的怪物词缀。

**Architecture:** 在现有 `arpg_combat` 内新增数据驱动的 `combat/monster_affix_*` 子模块；遭遇导演只负责给稳定出生项附加确定性词缀，战斗系统通过通用属性、命中、定时和死亡触发原语执行，地下城会话只消费击杀事件结算奖励，raylib 只读取快照和事件做表现。所有运行时集合保持固定容量，词缀数据不进入存档。

**Tech Stack:** C++17, raylib 6.0.0, CMake 3.25+, Ninja, MSVC 19.44, Windows SDK 10.0.26100.0, CTest

## Global Constraints

- 只实现 Stage 9；不得进入第 13 条词缀、深渊专属词缀、正式深渊系统、Stage 10、新怪物或新装备玩法。
- 每只怪物独立生成 0～3 条不重复词缀；不设置房间危险预算、单怪高危上限或单房高危上限。
- 仅拒绝适用性不匹配、词缀重复、机制互斥和理论无解组合；三高危组合必须可以生成和运行。
- 相同 `room_seed + depth + wave_index + spawn_index` 必须生成相同词缀；词缀随机域不得改变怪物类型、数量、波次或位置。
- 所有随机抽取使用 `core::DeterministicRng::derive_stream()` 与 `next_bounded()`；禁止标准库随机分布。
- 96 怪、384 投射物、96 区域和 192 地面掉落容量保持固定；战斗固定步不得堆分配。
- 除死亡爆破外，owner 死亡时清除其投射物和持续区域；容量耗尽必须安全取消并增加诊断计数。
- 无词缀怪保持现有 1% 掉落算法和基础经验；每怪最多直接掉 1 件装备。
- 词缀由当前房间稳定输入推导，不修改 V1～V4 存档格式。
- 每个生产代码任务必须先写失败测试、运行 RED、做最小实现、运行 GREEN 和相关回归后再提交。
- 保留工作树所有既有文件和无关改动，不重置、不删除、不覆盖。

---

## Frozen Data Constants

### Stable IDs and danger

```cpp
enum class MonsterAffixId : std::uint8_t {
    mighty = 0,
    frenzy = 1,
    swift = 2,
    armored = 3,
    shielding = 4,
    multishot = 5,
    burning_ground = 6,
    chilling = 7,
    chain_lightning = 8,
    chaos_corrosion = 9,
    blink_assault = 10,
    death_blast = 11,
    count = 12,
};

enum class MonsterAffixTier : std::uint8_t { m1 = 1, m2 = 2, m3 = 3 };
enum class MonsterAffixDanger : std::uint8_t { low = 1, medium = 2, high = 3 };
```

危险级依次为：低 `mighty, armored`；中 `frenzy, swift, shielding, chilling, chaos_corrosion`；高 `multishot, burning_ground, chain_lightning, blink_assault, death_blast`。

### Depth tables

```cpp
struct AffixDepthBand final {
    std::uint64_t maximum_depth;
    std::array<std::uint16_t, 4> count_weights;
    std::array<std::uint16_t, 3> tier_weights;
};

inline constexpr std::array<AffixDepthBand, 5> kAffixDepthBands{{
    {3U,  {80, 20,  0,  0}, {100,  0,  0}},
    {9U,  {55, 38,  7,  0}, { 80, 20,  0}},
    {19U, {30, 45, 20,  5}, { 50, 40, 10}},
    {39U, {15, 35, 35, 15}, { 25, 50, 25}},
    {UINT64_MAX, {5, 20, 40, 35}, {10, 35, 55}},
}};
```

### M1/M2/M3 values

所有百分比用 basis points，所有秒数在目录内冻结为 60 Hz tick：

| Affix | M1 | M2 | M3 |
|---|---|---|---|
| mighty | HP `13000`, horizontal impulse `8500` | `16000 / 7000` | `20000 / 5500` |
| frenzy | damage `11500`, attack timing `9000` | `13000 / 8000` | `15000 / 7000` |
| swift | move `11500`, cooldown `9200` | `13000 / 8400` | `14500 / 7600` |
| armored | armor rating `75` → 15% | `325` → 25% | `775` → 35% |
| shielding | HP-derived shield `2000`, delay `180` | `3500 / 150` | `5000 / 120` |
| multishot | count `2`, each `7500` | `3 / 6000` | `4 / 5000` |
| burning_ground | spawn `180`, radius `.75`, active `120`, tick damage `25` | `150/.90/180/35` | `120/1.05/240/45` |
| chilling | water damage `11500`, slow `1500/60` | `12500/2500/90` | `13500/3500/120` |
| chain_lightning | delay `42`, radius `.65`, lightning `70` | `42/.80/110` | `42/.95/160` |
| chaos_corrosion | per-second chaos `20`, duration `120` | `30/180` | `45/240` |
| blink_assault | cooldown `480`, warning `42`, next hit `12000` | `360/36/13500` | `240/30/15000` |
| death_blast | warning `66`, radius `.90`, physical `120` | `54/1.15/190` | `45/1.40/280` |

`armored` 的护甲值必须通过 `modifiers::rating_to_basis_points()` 求出最终物理减伤；不得直接绕过 Stage 8 曲线。`mighty` 只缩放水平击退，上挑垂直速度和 30 tick 浮空保持不变。

### Reward formulas

```cpp
score = sum(static_cast<int>(danger) * static_cast<int>(tier));
drop_chance_bp = min(5000, 100 + score * 150);
xp_percent = 100 + score * 10;
item_level_bonus = min(10, (score + 2) / 3);
item_level = min(100, depth + item_level_bonus);
```

分数为 0 时必须沿用 `next_bounded(100) == 0` 的旧 1% 路径，保证既有无词缀固定种子掉落不漂移。

---

## File Map

### New files

- `src/combat/monster_affix_types.hpp` — 稳定 ID、等级、危险级、固定 3 槽集合与预警/区域种类。
- `src/combat/monster_affix_catalog.hpp/.cpp` — 12 条定义、M1～M3 参数、标签和目录验证。
- `src/combat/monster_affix_generation.hpp/.cpp` — 深度表、独立随机域、无放回选择和危险分。
- `src/combat/monster_affix_runtime.hpp/.cpp` — 纯属性投影、计时缩放、伤害缩放和通用触发参数。
- `tests/combat/monster_affix_catalog_tests.cpp` — 目录 golden 测试。
- `tests/combat/monster_affix_generation_tests.cpp` — 深度、适用性和确定性测试。
- `tests/combat/monster_affix_runtime_tests.cpp` — 静态属性、击退、护盾和状态测试。
- `tests/combat/monster_affix_trigger_tests.cpp` — 弹体、区域、闪现和死亡触发测试。
- `tests/dungeon/dungeon_affix_reward_tests.cpp` — 经验、直接掉落和重载防复制测试。
- `tests/dungeon/dungeon_affix_stress_tests.cpp` — 1,000 房双 session 确定性压力测试。
- `tests/dungeon/stage9_validation_fixture.cpp` — 无窗口确定性验收入口。
- `tests/dungeon/stage9_validation_game.cpp` — 仅测试构建的高深度 raylib 验收入口。
- `docs/validation/stage9-monster-affixes.md` — 最终验证记录。

### Modified files

- `src/combat/CMakeLists.txt`、`tests/combat/CMakeLists.txt`、`tests/combat/combat_test_main.cpp` — 注册词缀源码和测试套件。
- `src/combat/combat_types.hpp`、`monster_catalog.cpp`、`monster_pool.hpp/.cpp`、`combat_world.hpp/.cpp`、`combat_snapshot.cpp` — 能力标签、出生词缀、运行时状态、快照、诊断和事件负载。
- `src/combat/hit_resolution.cpp`、`target_simulation.cpp`、`player_simulation.cpp`、`monster_ai.cpp`、`monster_ai_melee.cpp`、`monster_ai_ranged.cpp`、`monster_ai_special.cpp` — 伤害、状态、时序和触发执行。
- `src/dungeon/encounter_director.cpp` — 基础遭遇完成后附加词缀和稳定出生序号。
- `src/dungeon/dungeon_session.cpp`、`tests/dungeon/dungeon_test_support.hpp`、`tests/dungeon/CMakeLists.txt`、`tests/dungeon/dungeon_test_main.cpp` — 击杀奖励与测试注入。
- `src/platform/raylib/combat_view_math.hpp/.cpp`、`actor_renderer.cpp`、`combat_audio.hpp/.cpp`、`tests/platform/monster_view_tests.cpp` — 徽标、描边、预警区域和提示音。
- `tests/dungeon/encounter_director_tests.cpp`、`dungeon_loot_drop_tests.cpp`、`dungeon_progression_reward_tests.cpp` — 既有规则回归和新奖励断言。

---

## Task 1: 稳定类型与 12 条目录

**Files:**
- Create: `src/combat/monster_affix_types.hpp`
- Create: `src/combat/monster_affix_catalog.hpp`
- Create: `src/combat/monster_affix_catalog.cpp`
- Create: `tests/combat/monster_affix_catalog_tests.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/monster_catalog.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Produces: `MonsterAffixId`, `MonsterAffixTier`, `MonsterAffixSet`, `MonsterAffixDefinition`, `monster_affix_definition()`, `monster_affix_catalog_valid()`.
- Produces: `MonsterTag::projectile_capable`，只赋给 `lightning_shooter`。

- [ ] **Step 1: 写目录失败测试**

新增 suite，逐 ID 断言连续、名称/简称非空、危险级、必需标签和冻结数值；额外断言 `lightning_shooter` 有 `projectile_capable`，`chaos_hazard` 没有：

```cpp
ARPG_REQUIRE(static_cast<std::uint8_t>(MonsterAffixId::count) == 12U);
ARPG_REQUIRE(monster_affix_catalog_valid());
const auto* multishot = monster_affix_definition(MonsterAffixId::multishot);
ARPG_REQUIRE(multishot != nullptr);
ARPG_REQUIRE(multishot->danger == MonsterAffixDanger::high);
ARPG_REQUIRE(multishot->tiers[2].projectile_count == 4U);
ARPG_REQUIRE(has_tag(*monster_definition(MonsterId::lightning_shooter),
    MonsterTag::projectile_capable));
ARPG_REQUIRE(!has_tag(*monster_definition(MonsterId::chaos_hazard),
    MonsterTag::projectile_capable));
```

- [ ] **Step 2: 运行 RED**

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DARPG_FETCH_RAYLIB=OFF -DCMAKE_PREFIX_PATH=E:/game/.deps/raylib-6.0
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure
```

Expected: 编译失败，指出词缀类型和目录接口不存在。

- [ ] **Step 3: 实现固定类型和目录**

`monster_affix_types.hpp` 固定无分配集合：

```cpp
struct MonsterAffixInstance final {
    MonsterAffixId id{MonsterAffixId::mighty};
    MonsterAffixTier tier{MonsterAffixTier::m1};

    friend bool operator==(
        MonsterAffixInstance left, MonsterAffixInstance right) noexcept {
        return left.id == right.id && left.tier == right.tier;
    }
};
struct MonsterAffixSet final {
    std::array<MonsterAffixInstance, 3> values{};
    std::uint8_t count{};

    friend bool operator==(
        const MonsterAffixSet& left, const MonsterAffixSet& right) noexcept {
        return left.count == right.count && left.values == right.values;
    }
};
```

目录等级参数使用单一 POD `MonsterAffixTierValues`，明确包含 `primary_bp`、`secondary_bp`、`interval_ticks`、`duration_ticks`、`damage`、`radius`、`projectile_count`；无关字段必须为零。`MonsterAffixDefinition` 包含稳定 ID、ASCII 姓名/简称、危险级、权重、`required_tags`、`forbidden_tags`、`conflict_mask` 和三档参数。

- [ ] **Step 4: 运行 GREEN 并提交**

```powershell
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure
git diff --check
git add src/combat tests/combat
git commit -m "feat: add monster affix catalog"
```

Expected: combat suite 通过，目录 12 条完整且无 raylib 依赖。

---

## Task 2: 深度生成与遭遇导演集成

**Files:**
- Create: `src/combat/monster_affix_generation.hpp`
- Create: `src/combat/monster_affix_generation.cpp`
- Create: `tests/combat/monster_affix_generation_tests.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/CMakeLists.txt`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `src/dungeon/encounter_director.cpp`
- Modify: `tests/dungeon/encounter_director_tests.cpp`

**Interfaces:**
- Consumes: Task 1 catalog和 `MonsterDefinition::tags`。
- Produces: `affix_count_weights(depth)`, `affix_tier_weights(depth)`, `generate_monster_affixes(...)`, `monster_affix_danger_score(set)`。
- Produces: `MonsterSpawnSpec::affixes` 与 `MonsterSpawnSpec::spawn_ordinal`。

- [ ] **Step 1: 写深度、适用性和确定性失败测试**

覆盖 `3/4/9/10/19/20/39/40`，验证权重精确命中；扫描固定种子，验证同输入字节等价、单怪无重复、三高危可出现、support 不获得命中附加词缀、hazard caster 不获得多重投射：

```cpp
ARPG_REQUIRE(affix_count_weights(3U) == std::array<std::uint16_t, 4>{80,20,0,0});
ARPG_REQUIRE(affix_count_weights(40U) == std::array<std::uint16_t, 4>{5,20,40,35});
const auto a = generate_monster_affixes(0xA11CEULL, 40U, 1U, 7U,
    *monster_definition(MonsterId::lightning_shooter));
const auto b = generate_monster_affixes(0xA11CEULL, 40U, 1U, 7U,
    *monster_definition(MonsterId::lightning_shooter));
ARPG_REQUIRE(a.has_value() && b.has_value());
ARPG_REQUIRE(*a == *b);
```

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_combat_tests arpg_dungeon_tests
ctest --test-dir build-release -R "combat.units|dungeon.units" --output-on-failure
```

Expected: 新生成接口和 `MonsterSpawnSpec` 字段缺失。

- [ ] **Step 3: 实现独立随机域和导演后置注入**

公开签名固定为：

```cpp
[[nodiscard]] std::optional<MonsterAffixSet> generate_monster_affixes(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index,
    const MonsterDefinition& monster) noexcept;
[[nodiscard]] std::uint16_t monster_affix_danger_score(
    const MonsterAffixSet& set) noexcept;
```

使用独立常量域 `count/selection/tier`。遭遇导演必须先完成现有 `fill_wave()`，随后遍历稳定出生项写入：

```cpp
spawn.spawn_ordinal = static_cast<std::uint16_t>(wave_index * 96U + spawn_index);
const auto generated = generate_monster_affixes(
    room_seed, depth, wave_index, spawn_index, *definition);
if (!generated.has_value()) {
    result.fault = DungeonFault::invalid_rules;
    result.plan = {};
    return result;
}
spawn.affixes = *generated;
```

`encounter_plan_legal()` 同时验证序号、词缀集合、标签适用性和危险分可计算。基础怪物 ID 与位置测试必须证明增加词缀前后的旧固定 trace 不漂移。

- [ ] **Step 4: 运行 GREEN 并提交**

```powershell
cmake --build build-release --target arpg_combat_tests arpg_dungeon_tests
ctest --test-dir build-release -R "combat.units|dungeon.units|architecture.combat" --output-on-failure
git diff --check
git add src/combat src/dungeon tests/combat tests/dungeon
git commit -m "feat: generate deterministic monster affixes"
```

---

## Task 3: 运行时属性投影与快照

**Files:**
- Create: `src/combat/monster_affix_runtime.hpp`
- Create: `src/combat/monster_affix_runtime.cpp`
- Create: `tests/combat/monster_affix_runtime_tests.cpp`
- Modify: `src/combat/monster_pool.hpp`
- Modify: `src/combat/monster_pool.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_snapshot.cpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Consumes: Task 2 的出生词缀与序号。
- Produces: `MonsterAffixProfile evaluate_monster_affixes(...)` 和运行时/快照词缀状态。

- [ ] **Step 1: 写纯投影和波次加载失败测试**

构造单词缀集合，精确断言强壮生命、狂暴伤害/时序、迅捷移动/冷却、护甲 rating、护盾点数和延迟；加载 wave 后断言 runtime 与 snapshot 保留序号和词缀：

```cpp
MonsterAffixSet mighty_m3{};
mighty_m3.values[0] = {
    MonsterAffixId::mighty, MonsterAffixTier::m3};
mighty_m3.count = 1U;
const MonsterAffixProfile profile = evaluate_monster_affixes(
    *monster_definition(MonsterId::water_bulwark),
    mighty_m3);
ARPG_REQUIRE(profile.max_hp == 1400);
ARPG_REQUIRE(profile.horizontal_impulse_bp == 5500);
ARPG_REQUIRE(world.snapshot().monsters[0].affixes.count == 1U);
ARPG_REQUIRE(world.snapshot().monsters[0].spawn_ordinal == 97U);
```

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure
```

- [ ] **Step 3: 实现固定运行时状态**

`MonsterAffixProfile` 包含已求值的 `max_hp`、`armor_rating`、`max_shield`、`shield_recharge_delay_ticks`、`damage_bp`、`attack_timing_bp`、`move_bp`、`cooldown_bp`、`horizontal_impulse_bp`。`MonsterRuntime` 增加词缀集合、序号、profile、护盾恢复计时、燃烧/闪现计时和当前词缀预警；不得保存字符串。

把 `MonsterPool::spawn()` 改为消费完整 `MonsterSpawnSpec`：

```cpp
[[nodiscard]] std::optional<MonsterHandle> spawn(
    const MonsterSpawnSpec& spec) noexcept;
```

保留 `spawn(MonsterId, Vec3)` 作为测试/legacy 包装，内部构造空词缀 spec。`CombatSnapshot` 复制固定集合、序号、预警类型与剩余 tick。

- [ ] **Step 4: 运行 GREEN 并提交**

```powershell
cmake --build build-release --target arpg_combat_tests arpg_dungeon_tests
ctest --test-dir build-release -R "combat.units|dungeon.units" --output-on-failure
git diff --check
git add src/combat tests/combat
git commit -m "feat: project affixes into monster runtime"
```

---

## Task 4: 强壮、狂暴、迅捷、坚甲与护盾行为

**Files:**
- Modify: `src/combat/hit_resolution.cpp`
- Modify: `src/combat/target_simulation.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/monster_ai_melee.cpp`
- Modify: `src/combat/monster_ai_ranged.cpp`
- Modify: `src/combat/monster_ai_special.cpp`
- Modify: `src/combat/monster_affix_runtime.hpp`
- Modify: `src/combat/monster_affix_runtime.cpp`
- Modify: `tests/combat/monster_affix_runtime_tests.cpp`

**Interfaces:**
- Consumes: Task 3 profile。
- Produces: `scaled_monster_damage()`, `scaled_monster_ticks()`, `monster_move_step()` 和 `tick_monster_affix_resources()`。

- [ ] **Step 1: 写五个被动词缀失败测试**

精确验证：M3 强壮的上挑 `velocity.z` 与普通怪相同但 `velocity.x` 为 55%；狂暴按 15/30/50% 放大伤害并缩短非 active 阶段；迅捷只缩放移动和 cooldown；坚甲只减物理 HP 伤害不减元素/破韧；护盾受击后按 180/150/120 tick 一次补满且再次受击重置。

```cpp
ARPG_REQUIRE(strong_after.velocity.z == normal_after.velocity.z);
ARPG_REQUIRE(arpg::test::near(std::fabs(strong_after.velocity.x),
    std::fabs(normal_after.velocity.x) * 0.55F, 1.0e-4));
ARPG_REQUIRE(armored_after.break_value == normal_after.break_value);
ARPG_REQUIRE(armored_after.hp > normal_after.hp);
```

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure
```

- [ ] **Step 3: 实现通用缩放和护盾恢复**

所有缩放使用整数 basis points 和至少 1 tick：

```cpp
scaled = std::max<std::uint16_t>(1U,
    static_cast<std::uint16_t>((base * timing_bp + 9999U) / 10000U));
physical_after = ceil_div(physical_before *
    (10000 - rating_to_basis_points(armor_rating)), 10000);
```

狂暴缩放 telegraph/recovery/cooldown，不改变 active；迅捷缩放 move 和 cooldown，不改变攻击有效帧。玩家命中时先只对物理分量应用护甲，再把各分量求和，由怪物护盾吸收减伤后的总伤害，剩余值扣生命。每次实际命中重置护盾恢复计时。

- [ ] **Step 4: 运行 GREEN 并提交**

```powershell
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure
git diff --check
git add src/combat tests/combat
git commit -m "feat: apply passive monster affix behaviors"
```

---

## Task 5: 冰寒减速与混沌腐蚀状态原语

**Files:**
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/player_simulation.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/monster_ai_melee.cpp`
- Modify: `src/combat/monster_ai_ranged.cpp`
- Modify: `src/combat/monster_ai_special.cpp`
- Modify: `tests/combat/monster_affix_runtime_tests.cpp`

**Interfaces:**
- Produces: 固定 `PlayerStatusRuntime`，以及 `apply_monster_direct_hit()` 统一直接攻击入口。

- [ ] **Step 1: 写状态刷新失败测试**

验证冰寒附加水伤、减速 15/25/35%、持续 60/90/120 tick；重复低等级不能降低现有效果，重复高等级替换强度并刷新持续时间。腐蚀每 60 tick 造成 20/30/45 混沌伤，持续 120/180/240 tick，只刷新不叠层，已生效 DoT 不投闪避。

```cpp
ARPG_REQUIRE(snapshot.player.slow_bp == 3500);
ARPG_REQUIRE(snapshot.player.slow_ticks == 120U);
ARPG_REQUIRE(snapshot.player.corrosion_damage_per_second == 45);
ARPG_REQUIRE(snapshot.player.corrosion_ticks == 240U);
```

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure
```

- [ ] **Step 3: 实现固定玩家状态和统一命中入口**

`PlayerStatusRuntime` 只包含一个 slow 和一个 corrosion 槽：

```cpp
struct PlayerStatusRuntime final {
    std::int32_t slow_bp{};
    std::uint16_t slow_ticks{};
    int corrosion_damage_per_second{};
    std::uint16_t corrosion_ticks{};
    std::uint8_t corrosion_tick_phase{};
};
```

所有怪物直接命中通过 `apply_monster_direct_hit(slot, packet, position, feedback)`；该函数读取 owner 词缀，以攻击包所有正原始分量之和为基数，通过 `ceil(raw_total * added_water_bp / 10000)` 构造附加水伤分量，应用直接伤害后再刷新状态。DoT 每 60 tick 调用 `apply_player_damage(..., DamageDelivery::ground_or_environment, ...)`，因此不投闪避但仍应用混沌减伤。

- [ ] **Step 4: 运行 GREEN 并提交**

```powershell
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure
git diff --check
git add src/combat tests/combat
git commit -m "feat: add chilled and corrosion monster hits"
```

---

## Task 6: 多重投射、炽燃地面、连锁闪电与闪现突袭

**Files:**
- Create: `tests/combat/monster_affix_trigger_tests.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_snapshot.cpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/monster_ai_melee.cpp`
- Modify: `src/combat/monster_ai_ranged.cpp`
- Modify: `src/combat/monster_ai_special.cpp`
- Modify: `src/combat/monster_pool.hpp`
- Modify: `src/combat/monster_pool.cpp`
- Modify: `tests/combat/combat_test_support.hpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`

**Interfaces:**
- Produces: `HazardKind::{native,burning,chain_lightning,death_blast}`、固定预警状态、弹体触发来源。
- Produces: `CombatEventKind::affix_blink_warning` 和 `CombatEventKind::affix_chain_warning`；只有预警对象成功创建后才发事件。

- [ ] **Step 1: 写四类主动触发失败测试**

验证多重投射数量/扇形/逐弹伤害；燃烧每 180/150/120 tick 生成对应区域且 30 tick 结算；近战命中和弹体命中/消散各生成一次 42 tick 闪电预警且不递归；闪现按 480/360/240 tick 冷却、42/36/30 tick 预警、边界钳制并强化下一次命中。

```cpp
ARPG_REQUIRE(multishot_world.snapshot().projectile_count == 4U);
ARPG_REQUIRE(burning_hazard.kind == HazardKind::burning);
ARPG_REQUIRE(chain_hazard.telegraph_ticks == 42U);
ARPG_REQUIRE(blink_monster.affix_warning == MonsterAffixWarning::blink);
ARPG_REQUIRE(blink_monster.position.x >= room_bounds::min_x);
ARPG_REQUIRE(blink_monster.position.x <= room_bounds::max_x);
```

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure
```

- [ ] **Step 3: 实现固定触发原语**

多重投射使用固定 `std::array<Vec3,4>`，以目标方向旋转对称角度，不创建 vector。`ProjectileRuntime` 增加 `trigger_chain_on_end`，弹体结束前缓存 owner 词缀并生成一次 chain hazard。`HazardRuntime` 与 `HazardSnapshot` 增加 `kind`，快照同时暴露既有 `persists_after_owner_death`。燃烧在怪物通用 tick 中按计时生成 `DamageType::fire` 区域。闪现使用运行时预警字段暂停基础 AI，预警结束后传送到玩家相反朝向 1.0 世界单位处并钳制边界；`blink_empowered` 与狂暴伤害乘算，只在下一次实际直接命中后清除。

区域生成接口增加明确 kind 和死亡存续参数：

```cpp
bool spawn_hazard(MonsterHandle owner, HazardKind kind, Vec3 center,
    float radius, std::uint16_t telegraph_ticks,
    std::uint16_t active_ticks, std::uint16_t damage_interval_ticks,
    DamagePacket damage, bool persists_after_owner_death = false) noexcept;
```

- [ ] **Step 4: 验证池耗尽安全降级**

先填满 384 弹体和 96 区域，再触发四类机制；断言旧对象未覆盖、数量不增加、饱和计数只增加一次且没有残留假预警。

- [ ] **Step 5: 运行 GREEN 并提交**

```powershell
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure
git diff --check
git add src/combat tests/combat
git commit -m "feat: execute active monster affix triggers"
```

---

## Task 7: 死亡爆破与 owner 生命周期

**Files:**
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/target_simulation.cpp`
- Modify: `src/combat/monster_ai.cpp`
- Modify: `src/combat/monster_ai_special.cpp`
- Modify: `src/combat/monster_pool.cpp`
- Modify: `tests/combat/monster_affix_trigger_tests.cpp`

**Interfaces:**
- Produces: `defeat_monster(slot, attack, reward_eligible)` 唯一死亡入口和完整击杀事件负载。
- Produces: `CombatEventKind::affix_death_warning`；只有死亡爆破区域成功创建后才发事件。

- [ ] **Step 1: 写死亡与清理失败测试**

验证普通死亡清除 owner 弹体、燃烧和 chain 区域；死亡爆破保留并按 66/54/45 tick、半径和物伤生效；死亡爆破不递归；自爆怪走相同生命周期但 `reward_eligible=false`，不能伪造玩家击杀奖励。

```cpp
ARPG_REQUIRE(after.projectile_count == 0U);
const auto find_hazard = [](const CombatSnapshot& state,
        HazardKind kind) noexcept -> const HazardSnapshot* {
    for (const HazardSnapshot& hazard : state.hazards) {
        if (hazard.active && hazard.kind == kind) return &hazard;
    }
    return nullptr;
};
ARPG_REQUIRE(find_hazard(after, HazardKind::burning) == nullptr);
const auto* death = find_hazard(after, HazardKind::death_blast);
ARPG_REQUIRE(death != nullptr);
ARPG_REQUIRE(death->persists_after_owner_death);
```

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure
```

- [ ] **Step 3: 集中死亡入口**

`defeat_monster()` 必须按顺序缓存 ID/序号/危险分/位置，生成死亡爆破，清除非死亡对象，设置 defeated 状态，最后发事件：

```cpp
struct DefeatPayload final {
    MonsterId monster_id;
    std::uint16_t spawn_ordinal;
    std::uint16_t affix_score;
    bool reward_eligible;
};
```

扩展 `CombatEvent` 为同名稳定字段，保留 `target_index` 供视觉反馈。玩家击杀 `reward_eligible=true`；fire bomber 自爆为 false。非 legacy 怪保持 defeated runtime，房间仍通过 `hp == 0` 判断清场。

- [ ] **Step 4: 运行 GREEN 并提交**

```powershell
cmake --build build-release --target arpg_combat_tests arpg_dungeon_tests
ctest --test-dir build-release -R "combat.units|dungeon.units" --output-on-failure
git diff --check
git add src/combat tests/combat
git commit -m "feat: add affix death lifecycle"
```

---

## Task 8: 危险分经验与直接装备掉落

**Files:**
- Create: `tests/dungeon/dungeon_affix_reward_tests.cpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `tests/dungeon/dungeon_test_support.hpp`
- Modify: `tests/dungeon/dungeon_loot_drop_tests.cpp`
- Modify: `tests/dungeon/dungeon_progression_reward_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Interfaces:**
- Consumes: Task 7 击杀事件的 `monster_id/spawn_ordinal/affix_score/reward_eligible`。
- Produces: `affix_drop_chance_bp()`, `affix_item_level()`, `affix_experience()` 纯函数。

- [ ] **Step 1: 写公式和旧 trace 失败测试**

逐分数断言 `0,1,9,27` 的掉落基点、经验倍率和等级奖励；验证分数 0 仍命中 Stage 8 同一固定种子掉落，分数 27 为 41.5% 掉率、+270% 经验和 +9 物品等级；无奖励资格事件不抽掉落也不加经验。

```cpp
ARPG_REQUIRE(affix_drop_chance_bp(0U) == 100U);
ARPG_REQUIRE(affix_drop_chance_bp(27U) == 4150U);
ARPG_REQUIRE(affix_item_level(95U, 27U) == 100U);
ARPG_REQUIRE(affix_experience(40U, 27U) == 148U);
```

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_dungeon_tests
ctest --test-dir build-release -R dungeon.units --output-on-failure
```

- [ ] **Step 3: 改为事件自包含奖励**

`relay_combat_events()` 不再从已死亡 snapshot 反查怪物 ID。分数 0 使用旧 `next_bounded(100)==0`；分数大于 0 使用同一 drop chance 域的 `next_bounded(10000) < chance_bp`。物品部位、内容和 ID 域保持不变，只把生成请求 `item_level` 替换为冻结公式结果。掉落 ordinal 直接使用事件值并验证 `<192`。

经验计算使用 checked 64 位中间值后饱和加入 `pending_room_experience_`：

```cpp
bonus_xp = base_xp * (100U + score * 10U) / 100U;
```

- [ ] **Step 4: 验证领取位与重载**

同一事件重复 relay 只抽一次；已领取位重载后不再掉落；未领取的同一稳定怪在重建后产生相同词缀、掉落判定和物品内容。

- [ ] **Step 5: 运行 GREEN 并提交**

```powershell
cmake --build build-release --target arpg_dungeon_tests arpg_persistence_tests
ctest --test-dir build-release -R "dungeon.units|persistence" --output-on-failure
git diff --check
git add src/dungeon tests/dungeon
git commit -m "feat: reward dangerous monster affixes"
```

---

## Task 9: raylib 徽标、预警与提示音

**Files:**
- Modify: `src/platform/raylib/combat_view_math.hpp`
- Modify: `src/platform/raylib/combat_view_math.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/combat_audio.hpp`
- Modify: `src/platform/raylib/combat_audio.cpp`
- Modify: `tests/platform/monster_view_tests.cpp`

**Interfaces:**
- Consumes: Task 3/6/7 快照和预警事件。
- Produces: `monster_affix_badge()`, `monster_affix_outline()`, `hazard_color()` 和新的音频 cue 路由。

- [ ] **Step 1: 写纯视图和音频路由失败测试**

断言 12 个简称、M1～M3 文本、基础/防御/火/水/电/混沌颜色、高危红色脉冲；四种 hazard kind 颜色不同；blink/chain/death 事件分别路由独立音色，同类声音 12 tick 内只允许一次。

```cpp
const AffixBadge badge = monster_affix_badge(
    {MonsterAffixId::death_blast, MonsterAffixTier::m3});
ARPG_REQUIRE(std::strcmp(badge.short_name, "DEATH") == 0);
ARPG_REQUIRE(badge.danger == MonsterAffixDanger::high);
ARPG_REQUIRE(std::strcmp(badge.tier_text, "M3") == 0);
ARPG_REQUIRE(route_audio_cues(blink_event)
    == audio_cue_mask(AudioCue::blink_warning));
```

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_platform_tests
ctest --test-dir build-release -R platform.units --output-on-failure
```

- [ ] **Step 3: 实现无规则判断的表现层**

血条下方固定最多绘制 3 个 `SHORT M#`；按 `snapshot.tick % 30` 计算高危脉冲透明度。怪物轮廓按词缀类别叠加，不改生态本体色。burning/chain/death 使用 `HazardKind` 选择橙红/黄/红，native 保持原紫色。

`AudioCue` 增加 `blink_warning/chain_warning/death_warning`；`CombatAudio` 为三类各生成一个程序化短波形并保存最后播放 tick，满足：

```cpp
if (event.tick >= last_tick + 12U || event.tick < last_tick) {
    PlaySound(cue);
    last_tick = event.tick;
}
```

同帧最多播放每类一次，限流只影响声音、不影响规则事件。

- [ ] **Step 4: 运行 GREEN 并提交**

```powershell
cmake --build build-release --target arpg_platform_tests arpg_game
ctest --test-dir build-release -R "platform.units|architecture.combat_no_raylib" --output-on-failure
git diff --check
git add src/platform tests/platform
git commit -m "feat: render and announce monster affixes"
```

---

## Task 10: 压力、双配置全测与窗口验收

**Files:**
- Create: `tests/dungeon/dungeon_affix_stress_tests.cpp`
- Create: `tests/dungeon/stage9_validation_fixture.cpp`
- Create: `tests/dungeon/stage9_validation_game.cpp`
- Create: `docs/validation/stage9-monster-affixes.md`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Interfaces:**
- Consumes: Tasks 1～9 完整系统。
- Produces: Stage 9 自动化与窗口验收证据，不产生 Stage 10 代码。

- [ ] **Step 1: 写 1,000 房确定性压力测试**

两个独立 session 使用相同根种子逐房比较：房间种子、怪物 ID/位置、词缀 ID/等级、危险分、击杀经验、掉落 ordinal、物品 ID/等级/稀有度和领取位；每 37 房编码/解码 V4 后继续。额外构造 96 怪、384 弹体、96 区域与三高危组合，使用 allocation probe 包围 600 tick：

```cpp
arpg::test::AllocationProbe probe;
probe.begin();
for (int tick = 0; tick < 600; ++tick) {
    world.tick(arpg::combat::MovementInput{});
}
const std::size_t allocations = probe.end();
ARPG_REQUIRE(allocations == 0U);
const Stage9Trace left_trace = run_stage9_trace(left, 1000U, 37U);
const Stage9Trace right_trace = run_stage9_trace(right, 1000U, 37U);
ARPG_REQUIRE(left_trace == right_trace);
```

测试文件内定义 `Stage9Trace`，字段是房间 seed、怪物/词缀摘要、累计经验、掉落/领取摘要和最终物品摘要的固定数组或固定哈希，并提供逐字段 `operator==`；`run_stage9_trace(session, room_count, restart_interval)` 每 37 房调用现有 V4 encode/decode，并对每个稳定字段显式比较。

- [ ] **Step 2: 运行单模块 GREEN**

```powershell
cmake --build build-release --target arpg_combat_tests arpg_dungeon_tests
ctest --test-dir build-release -R "combat.units|dungeon.units" --output-on-failure
```

- [ ] **Step 3: 增加测试专用验收入口**

`arpg_stage9_validation_fixture` 输出固定浅层与深度 40 的词缀 trace、三高危样本、奖励公式和重载一致性；返回非零表示失败。`arpg_stage9_validation_game` 使用测试专用固定房间直接展示 12 条词缀及 M1/M2/M3，不改生产存档和正式地下城规则。

- [ ] **Step 4: 完整 Debug 验证**

```powershell
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DARPG_FETCH_RAYLIB=OFF -DCMAKE_PREFIX_PATH=E:/game/.deps/raylib-6.0
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure
```

Expected: 全部测试通过，无 CRT assert、崩溃或超时。

- [ ] **Step 5: 完整 Release 验证**

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DARPG_FETCH_RAYLIB=OFF -DCMAKE_PREFIX_PATH=E:/game/.deps/raylib-6.0
cmake --build build-release
ctest --test-dir build-release --output-on-failure
git diff --check
git diff --exit-code 7b54370 -- src/persistence
```

Expected: 全部测试通过，架构测试确认 combat/dungeon/persistence 不依赖 raylib，且持久化源码相对 Stage 8 基线没有变化。

- [ ] **Step 6: raylib 窗口验收**

启动 `build-release/bin/arpg_stage9_validation_game.exe` 和正式 `build-release/bin/arpg_game.exe`，逐项验证：

1. 浅层少量 M1 与血条旁简称/M 等级。
2. 深度 40 的 2～3 词缀和 M2/M3 组合。
3. 多重投射、燃烧、连锁闪电、闪现和死亡爆破预警。
4. L 上挑强壮怪仍浮空 0.5 秒，仅水平击退减弱。
5. 高危险怪死亡位置直接出现装备，危险分奖励符合 fixture 输出。
6. 相同房间重启后词缀与未领取掉落一致，已领取掉落不复制。
7. 极端组合与池压力下不崩溃，诊断计数增加。

把构建提交、命令、测试数、fixture 输出、每项 pass/fail 和截图路径写入 `docs/validation/stage9-monster-affixes.md`。

- [ ] **Step 7: 最终范围审查与提交**

```powershell
rg -n "abyss_affix|summon_affix|aura_affix|Stage 10" src tests
git status --short
git diff --check
git add tests/dungeon docs/validation/stage9-monster-affixes.md
git commit -m "test: validate stage 9 monster affixes"
```

Expected: 搜索不发现已实现的深渊/召唤/光环词缀；工作树只包含预期 Stage 9 文件。提交后停止，等待用户验收，不进入 Stage 10。
