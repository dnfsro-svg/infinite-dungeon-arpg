# Stage 8 Equipment and Loot MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在已验收 Stage 7 基线上交付可实际游玩的装备闭环：确定性掉落、自动拾取、暂停式三栏背包、即时换装、三合一、战斗属性生效以及 V4 原子存档。

**Architecture:** 新增无图形 `arpg_items` 规则层；`modifiers` 负责通用属性求值，`combat` 只消费最终 build，`dungeon` 拥有物品稳定状态与事务，`persistence` 编解码完整检查点，raylib 只负责输入和展示。背包不进入战斗热路径；物品变更仅在原子存档 committed 后发布到稳定状态。

**Tech Stack:** C++17, raylib 6.0.0, CMake 3.25+, Ninja, MSVC 19.44, Windows SDK 10.0.26100.0, CTest

## Global Constraints

- 只实现 Stage 8；不得引入怪物词条、史诗装备、鉴定、工艺、材料、单件销毁、物品过滤器或 Stage 9 内容。
- `core`、`modifiers`、`items`、`combat`、`dungeon`、`persistence` 不得依赖 raylib。
- 所有随机选择使用现有 `core::DeterministicRng::next_bounded()` 与 `derive_stream()` 独立域种子；禁止标准库分布和隐式重骰。
- 战斗固定步热路径不遍历背包、不分配堆内存；装备汇总只在加载或 committed 的物品事务后重算。
- 存档解码最多接受 65,535 件物品；任何长度、算术、ID 或 Modifier 溢出均拒绝加载或进入 faulted。
- 当前生命和护盾在换装时保持绝对值，仅向下截断；增加上限不得回复。
- 每个生产代码任务必须先增加失败测试，再做最小实现；每项提交前运行该模块测试及 `git diff --check`。
- 保留工作树中所有既有改动和文件，不重置、不删除无关内容。

---

## Frozen Data Constants

实现不得重新解释本节数值。tier 数组统一按 `T8,T7,T6,T5,T4,T3,T2,T1` 排列：

```cpp
inline constexpr std::array<std::uint8_t, 8> kTierMinimumLevel{
    1, 16, 30, 45, 60, 75, 88, 95};
inline constexpr std::array<std::uint32_t, 8> kTierBaseWeight{
    64, 48, 36, 27, 20, 15, 11, 8};
```

固定基底：

| ID | Base | Slot | inherent T8→T1 |
|---:|---|---|---|
| 1 | Iron Blade | weapon | physical flat `2,3,4,5,7,9,12,16` |
| 2 | Guard Helm | helmet | max health `3,5,8,12,17,23,30,38` |
| 3 | Ward Coat | chest | max barrier `4,7,11,16,22,29,37,46` |
| 4 | Striker Gloves | gloves | global attack speed bp `100,200,300,400,500,600,700,800` |
| 5 | Runner Boots | boots | move speed bp `100,200,300,400,500,600,700,800` |
| 6 | Element Charm | accessory | each element reduction bp `100,200,300,400,500,600,800,1000` |

固定前缀：

| ID | Group/effect | Slots | values T8→T1 |
|---:|---|---|---|
| 1 | local physical flat | weapon | `1,2,3,4,5,7,9,12` |
| 2 | local physical increased bp | weapon | `500,800,1200,1600,2100,2700,3400,4200` |
| 3 | fire flat | weapon,gloves,accessory | `1,2,3,4,5,6,8,10` |
| 4 | water flat | weapon,gloves,accessory | `1,2,3,4,5,6,8,10` |
| 5 | lightning flat | weapon,gloves,accessory | `1,2,3,4,5,6,8,10` |
| 6 | chaos flat | weapon,gloves,accessory | `1,2,3,4,5,6,8,10` |
| 7 | fire increased bp | all slots | `500,800,1200,1600,2100,2700,3400,4200` |
| 8 | water increased bp | all slots | `500,800,1200,1600,2100,2700,3400,4200` |
| 9 | lightning increased bp | all slots | `500,800,1200,1600,2100,2700,3400,4200` |
| 10 | chaos increased bp | all slots | `500,800,1200,1600,2100,2700,3400,4200` |
| 11 | max health flat | helmet,chest,gloves,boots,accessory | `4,7,11,16,22,29,37,46` |
| 12 | max barrier flat | helmet,chest,gloves,boots,accessory | `3,5,8,12,17,23,30,38` |

固定后缀：

| ID | Group/effect | Slots | values T8→T1 |
|---:|---|---|---|
| 101 | attack speed bp | weapon,gloves,accessory | `200,300,400,500,600,800,1000,1200` |
| 102 | move speed bp | boots,accessory | `200,300,400,500,600,700,800,1000` |
| 103 | evasion | helmet,chest,gloves,boots,accessory | `25,60,150,400,1000,3000,10000,30000` |
| 104 | armor | helmet,chest,gloves,boots | `25,60,150,400,1000,3000,10000,30000` |
| 105 | impulse scale bp | weapon,gloves,accessory | `400,600,800,1000,1300,1600,2000,2500` |
| 106 | melee increased bp | weapon,gloves,accessory | `400,600,900,1200,1500,1900,2400,3000` |
| 107 | fire reduction bp | helmet,chest,boots,accessory | `300,500,700,900,1100,1400,1700,2000` |
| 108 | water reduction bp | helmet,chest,boots,accessory | `300,500,700,900,1100,1400,1700,2000` |
| 109 | lightning reduction bp | helmet,chest,boots,accessory | `300,500,700,900,1100,1400,1700,2000` |
| 110 | chaos reduction bp | helmet,chest,boots,accessory | `300,500,700,900,1100,1400,1700,2000` |
| 111 | max health increased bp | helmet,chest,accessory | `300,500,700,900,1200,1500,1900,2400` |
| 112 | one-element reduction cap bp | helmet,chest,accessory | `100,100,200,200,300,400,500,600` |

词缀 ID 即唯一词缀组 ID；同一物品不得重复。词缀 112 的 variant 为 fire=0、water=1、lightning=2、chaos=3 且四种等权；其他已使用词缀 variant 固定 `0xFF`。

生成公式：

```cpp
x = item_level - 1;
normal_weight = 70 - (30 * x / 99);
magic_weight = 25 + (15 * x / 99);
rare_weight = 100 - normal_weight - magic_weight;
highest_eligible_tier_weight += item_level / 5;
second_highest_eligible_tier_weight += item_level / 10;
```

普通固定 0 词缀；魔法 1/2 条权重 50/50；稀有 3/4/5/6 条权重 40/30/20/10。required level 为最强已生成 tier 的门槛，普通固定 1。

防御曲线锚点为 `(0,0),(100,2000),(1000,4000),(10000,6000),(100000,8000),(1000000,9900),(10000000,9990)`；尾部 `gap_bp=max(1,ceil(100000000/value))`、`result_bp=10000-gap_bp`。

V4 固定布局：magic=`IARPGS05`、format=4、header=32 bytes；payload 前 88 bytes 沿用 V3，offset 88=`item_count:u32`、92=`next_item_sequence:u64`、100=`claimed_drop_bits[3]:u64`、124=`equipped_ids[6]:u64`、172 起为 40-byte item records。payload=`172+40*N`，file=`204+40*N`。item record offset 0=`id:u64`、8=`base_id:u8`、9=`rarity:u8`、10=`item_level:u8`、11=`required_level:u8`、12=`affix_count:u8`、13..15=zero reserved、16..39=六个 4-byte rolls；roll 为 `affix_id:u16,tier:u8,variant:u8` 小端编码，未使用 roll 四字节全零。

---

## File Map

### New files

- `src/items/item_types.hpp` — 物品、词缀、装备、地面掉落和所有权状态的稳定类型。
- `src/items/item_catalog.hpp/.cpp` — 6 个基底、24 条词缀、8 阶门槛和目录校验。
- `src/items/item_generation.hpp/.cpp` — 稀有度、阶级、词缀无放回生成、ID 派生与三合一产物。
- `src/items/item_modifiers.hpp/.cpp` — 基底/词缀向通用 Modifier 的投影与装备 build 收集。
- `src/items/CMakeLists.txt` — `arpg_items` 静态库。
- `tests/items/item_test_main.cpp` — items 测试入口。
- `tests/items/item_catalog_tests.cpp` — 目录 golden 测试。
- `tests/items/item_generation_tests.cpp` — 确定性生成与统计边界测试。
- `tests/items/item_modifier_tests.cpp` — 局部武器与装备 Modifier 投影测试。
- `tests/items/item_recipe_tests.cpp` — 三合一纯规则测试。
- `tests/items/CMakeLists.txt` — `items.units`。
- `src/platform/raylib/inventory_view_math.hpp/.cpp` — 三栏布局、虚拟化、命中、筛选和双击纯函数。
- `src/platform/raylib/inventory_renderer.hpp/.cpp` — 背包和地面装备绘制。
- `tests/platform/inventory_view_math_tests.cpp` — 不开窗口的视图与输入语义测试。

### Modified files

- `CMakeLists.txt` — 注册 `src/items` 与 `tests/items`。
- `src/modifiers/modifier_types.hpp`、`player_modifier_values.hpp/.cpp`、`modifier_math.cpp` — 新 Stat、减免重命名、256 Modifier 容量和防御曲线。
- `src/passives/passive_tree_rules.hpp/.cpp` — 提供原始星盘 Modifier 收集接口。
- `src/combat/combat_types.hpp`、`combat_world.hpp/.cpp`、`combat_snapshot.cpp`、`player_simulation.cpp`、`monster_ai*.cpp` — 闪避随机流、伤害交付类型、护甲/元素减伤和热更新 build。
- `src/dungeon/dungeon_checkpoint.hpp`、`dungeon_types.hpp`、`dungeon_session.hpp/.cpp`、`dungeon_snapshot.cpp`、`dungeon_transition.cpp`、`room_combat_template.hpp/.cpp` — 物品稳定状态、掉落池和原子事务。
- `src/persistence/checkpoint_codec.hpp/.cpp`、`save_store.hpp/.cpp`、`save_store_detail.hpp`、`save_transaction.cpp`、`crc32.hpp/.cpp` — 可变长度 V4 编码、校验、CRC 和 V1–V3 迁移。
- `src/platform/raylib/dungeon_runtime.hpp/.cpp`、`raylib_host.cpp`、`room_renderer.cpp`、`hud_renderer.cpp`、`CMakeLists.txt` — 保存服务、背包门控、暂停与绘制。
- 对应 `tests/modifiers`、`tests/passives`、`tests/combat`、`tests/dungeon`、`tests/persistence`、`tests/platform` 文件和 CMake 清单 — 回归及新增行为测试。

---

## Task 1: Modifier 与防御数值基础

**Files:**
- Modify: `src/modifiers/modifier_types.hpp`
- Modify: `src/modifiers/player_modifier_values.hpp`
- Modify: `src/modifiers/player_modifier_values.cpp`
- Modify: `src/modifiers/modifier_math.cpp`
- Test: `tests/modifiers/player_modifier_values_tests.cpp`
- Test: `tests/modifiers/modifier_math_tests.cpp`

- [ ] **Step 1: 写下新 Stat 与曲线的失败测试**

在 `player_modifier_values_tests.cpp` 增加精确断言：物理固定伤害、护甲、闪避、四元素减免、四元素减免上限奖励均能独立求值；旧四个 resistance 测试改用 damage reduction 命名。增加 7 个锚点、每段首尾与中点单调、`10,000,001`、`INT64_MAX` 的曲线测试：

```cpp
CHECK_EQ(rating_to_basis_points(0), 0);
CHECK_EQ(rating_to_basis_points(100), 2000);
CHECK_EQ(rating_to_basis_points(1000), 4000);
CHECK_EQ(rating_to_basis_points(10000), 6000);
CHECK_EQ(rating_to_basis_points(100000), 8000);
CHECK_EQ(rating_to_basis_points(1000000), 9900);
CHECK_EQ(rating_to_basis_points(10000000), 9990);
CHECK(rating_to_basis_points(INT64_MAX) < 10000);
```

- [ ] **Step 2: 运行 RED**

Run:

```powershell
cmake --build build-release --target arpg_modifier_tests
ctest --test-dir build-release -R modifiers.units --output-on-failure
```

Expected: 编译失败，指出 `rating_to_basis_points`、新 Stat 或重命名字段尚不存在。

- [ ] **Step 3: 扩展稳定接口**

在 `modifier_types.hpp` 将旧枚举项改名并加入下列 Stat；保持枚举序列只在源代码内部使用，不进入存档：

```cpp
physical_flat_damage,
fire_damage_reduction,
water_damage_reduction,
lightning_damage_reduction,
chaos_damage_reduction,
fire_damage_reduction_cap,
water_damage_reduction_cap,
lightning_damage_reduction_cap,
chaos_damage_reduction_cap,
armor,
evasion,
```

`PlayerModifierValues` 使用下列字段，所有百分比仍为 basis points：

```cpp
std::array<std::int64_t, 5> flat_damage{};
std::array<std::int32_t, 5> damage_increased{};
std::array<std::int32_t, 4> damage_reduction{};
std::array<std::int32_t, 4> damage_reduction_cap_bonus{};
std::int64_t armor{};
std::int64_t evasion{};
```

公开纯函数：

```cpp
[[nodiscard]] std::int32_t rating_to_basis_points(std::int64_t value) noexcept;
```

负输入返回 0；锚点间使用 checked 64 位分段插值；尾部按设计公式计算且最大 9999。

- [ ] **Step 4: 提高组合容量并保证溢出失效**

把 `modifier_math.cpp` 的固定活动 Modifier 上限从 128 提到：

```cpp
inline constexpr std::size_t kPlayerModifierCapacity = 256;
```

超过容量、加法溢出或非法负 armor/evasion 时，`PlayerModifierValues::valid` 为 false，禁止饱和成看似合法 build。

- [ ] **Step 5: 运行 GREEN 与回归**

Run:

```powershell
cmake --build build-release --target arpg_modifier_tests arpg_passive_tests arpg_combat_tests
ctest --test-dir build-release -R "modifiers.units|passives.units|combat.units" --output-on-failure
git diff --check
```

Expected: 三组测试通过，旧抗性行为仅重命名、数值不漂移。

- [ ] **Step 6: 提交**

```powershell
git add src/modifiers tests/modifiers
git commit -m "feat: add equipment defense modifier foundation"
```

---

## Task 2: Item 类型、目录与构建边界

**Files:**
- Create: `src/items/item_types.hpp`
- Create: `src/items/item_catalog.hpp`
- Create: `src/items/item_catalog.cpp`
- Create: `src/items/CMakeLists.txt`
- Create: `tests/items/item_test_main.cpp`
- Create: `tests/items/item_catalog_tests.cpp`
- Create: `tests/items/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Modify: `tests/platform/module_boundary_test.cmake`

- [ ] **Step 1: 注册空 items 测试目标并写目录 golden 测试**

测试逐项比较设计规格中的 6 个基底、ID 1–12/101–112、T8→T1 数组、部位掩码、前后缀、词缀组和 variant 规则；再断言所有 6 部位至少存在 3 个合法前缀与 3 个合法后缀候选。

- [ ] **Step 2: 运行 RED**

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DARPG_FETCH_RAYLIB=OFF -DCMAKE_PREFIX_PATH=E:/game/.deps/raylib-6.0
cmake --build build-release --target arpg_item_tests
```

Expected: 配置或编译失败，因为 `src/items`、类型和目录接口尚不存在。

- [ ] **Step 3: 定义稳定类型**

`item_types.hpp` 必须包含：

```cpp
enum class ItemSlot : std::uint8_t { weapon, helmet, chest, gloves, boots, accessory, count };
enum class ItemRarity : std::uint8_t { normal, magic, rare };
enum class AffixKind : std::uint8_t { prefix, suffix };

struct AffixRoll final {
    std::uint16_t affix_id{};
    std::uint8_t tier{};
    std::uint8_t variant{};
};

struct ItemInstance final {
    std::uint64_t id{};
    std::uint8_t base_id{};
    ItemRarity rarity{};
    std::uint8_t item_level{};
    std::uint8_t required_level{};
    std::array<AffixRoll, 6> affixes{};
    std::uint8_t affix_count{};
};

struct EquipmentState final {
    std::array<std::uint64_t, 6> equipped_ids{};
};

struct ItemOwnershipState final {
    std::vector<ItemInstance> items{};
    EquipmentState equipment{};
    std::array<std::uint64_t, 3> claimed_drop_bits{};
    std::uint64_t next_item_sequence{1};
};
```

- [ ] **Step 4: 实现只读目录与完整合法性校验**

公开接口：

```cpp
[[nodiscard]] const BaseDefinition* base_definition(std::uint8_t id) noexcept;
[[nodiscard]] const AffixDefinition* affix_definition(std::uint16_t id) noexcept;
[[nodiscard]] std::uint8_t tier_minimum_level(std::uint8_t tier) noexcept;
[[nodiscard]] bool validate_catalog() noexcept;
[[nodiscard]] bool validate_item(const ItemInstance& item) noexcept;
[[nodiscard]] bool validate_ownership(const ItemOwnershipState& state) noexcept;
```

`validate_item` 完整执行 V4 章节的词缀数、前后缀上限、组重复、slot、tier、variant、需求等级约束；`validate_ownership` 检查 65,535 边界、ID、装备引用与槽位。

- [ ] **Step 5: 注册模块并检查无 raylib 依赖**

`arpg_items` 只链接 `arpg_modifiers`；根 CMake 在 modifiers 后注册 items。扩展模块边界测试，禁止 `src/items` 包含 raylib 头。

- [ ] **Step 6: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_item_tests
ctest --test-dir build-release -R "items.units|platform.module_boundary" --output-on-failure
git diff --check
```

- [ ] **Step 7: 提交**

```powershell
git add CMakeLists.txt src/items tests/items tests/platform/module_boundary_test.cmake
git commit -m "feat: add equipment item catalog"
```

---

## Task 3: 确定性生成、局部武器属性与组合 Modifier

**Files:**
- Create: `src/items/item_generation.hpp`
- Create: `src/items/item_generation.cpp`
- Create: `src/items/item_modifiers.hpp`
- Create: `src/items/item_modifiers.cpp`
- Create: `tests/items/item_generation_tests.cpp`
- Create: `tests/items/item_modifier_tests.cpp`
- Create: `tests/items/item_recipe_tests.cpp`
- Modify: `src/items/CMakeLists.txt`
- Modify: `tests/items/CMakeLists.txt`
- Modify: `src/passives/passive_tree_rules.hpp`
- Modify: `src/passives/passive_tree_rules.cpp`
- Modify: `tests/passives/passive_tree_rules_tests.cpp`

- [ ] **Step 1: 写生成与组合失败测试**

覆盖：ilvl 1/100 稀有度权重、所有 tier 门槛、最高两档加权、普通/魔法/稀有词缀数、前后缀最多 3、所有部位可生成 6 词缀稀有、固定 seed 字节等价、需求等级、ID 非零与冲突拒绝。三合一测试固定三材料 ID，断言排序无关、平均 ilvl 向下取整、同 rarity/slot、完整重骰与固定产物。

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_item_tests arpg_passive_tests
```

Expected: 新生成器、投影和星盘收集接口均未定义。

- [ ] **Step 3: 实现生成接口**

```cpp
struct ItemGenerationRequest final {
    std::uint64_t seed{};
    ItemSlot slot{};
    std::uint8_t item_level{};
    std::uint64_t item_id{};
    std::optional<ItemRarity> forced_rarity{};
};

[[nodiscard]] std::optional<ItemInstance> generate_item(
    const ItemGenerationRequest& request) noexcept;

[[nodiscard]] std::optional<ItemInstance> generate_recipe_item(
    std::uint64_t root_seed,
    std::uint64_t next_sequence,
    const ItemInstance& a,
    const ItemInstance& b,
    const ItemInstance& c) noexcept;
```

先构造全部合法候选，再用固定数组按组、前后缀容量无放回抽取；生成目标条数前验证候选足够。稀有度和 tier 严格使用 Frozen Data Constants 的整数公式与权重。

- [ ] **Step 4: 实现装备投影与局部武器计算**

```cpp
struct EquipmentProjection final {
    std::array<modifiers::Modifier, 128> modifiers{};
    std::size_t modifier_count{};
    std::int64_t weapon_physical{};
    std::int32_t local_attack_speed_bp{};
    bool valid{};
};

[[nodiscard]] EquipmentProjection project_equipment(
    const ItemOwnershipState& state) noexcept;
```

武器物理严格执行 `floor((base + local_flat) * (10000 + local_inc) / 10000)`；武器词缀 101 写入 local attack speed，手套/饰品 101 写入全局 attack speed。固有属性也通过同一投影路径进入 build。

- [ ] **Step 5: 暴露星盘原始 Modifier 收集**

新增：

```cpp
[[nodiscard]] bool append_passive_modifiers(
    const PassiveTreeState& tree,
    modifiers::Modifier* output,
    std::size_t capacity,
    std::size_t& count) noexcept;
```

现有 `evaluate_passive_tree()` 保留，内部改为调用 append 后再 evaluate，确保兼容旧调用者。后续 dungeon 把星盘和装备投影装入 256 固定缓冲，只调用一次 Modifier evaluator。

- [ ] **Step 6: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_item_tests arpg_passive_tests arpg_modifier_tests
ctest --test-dir build-release -R "items.units|passives.units|modifiers.units" --output-on-failure
git diff --check
```

- [ ] **Step 7: 提交**

```powershell
git add src/items tests/items src/passives tests/passives
git commit -m "feat: generate and project deterministic equipment"
```

---

## Task 4: 战斗闪避、护甲、元素减伤与热换 build

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

## Task 5: V4 可变长度检查点与迁移

**Files:**
- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/crc32.hpp`
- Modify: `src/persistence/crc32.cpp`
- Modify: `src/persistence/save_store.hpp`
- Modify: `src/persistence/save_store.cpp`
- Modify: `src/persistence/save_store_detail.hpp`
- Modify: `src/persistence/save_transaction.cpp`
- Test: `tests/persistence/checkpoint_codec_tests.cpp`
- Test: `tests/persistence/passive_tree_checkpoint_tests.cpp`
- Test: `tests/persistence/save_store_tests.cpp`
- Test: `tests/persistence/save_store_fault_tests.cpp`

- [ ] **Step 1: 写 V4 布局和迁移失败测试**

逐字节断言 magic `IARPGS05`、format 4、payload 172+40N、文件 204+40N、偏移 88/92/100/124/172 和 40 字节记录。固定样本至少含普通、魔法、稀有及词缀 112 variant。V1–V3 fixture 解码后断言空物品/装备/领取位及 sequence=1。

- [ ] **Step 2: 写损坏输入失败测试**

覆盖超 65,535、乘加溢出、截断、尾随字节、CRC、reserved、未用 roll 非零、重复/零 ID、非法目录字段、需求等级、词缀组、装备悬空/错槽/重复引用及超 95% cap。

- [ ] **Step 3: 运行 RED**

```powershell
cmake --build build-release --target arpg_persistence_tests
ctest --test-dir build-release -R persistence.units --output-on-failure
```

Expected: V4 magic、动态编码结果和物品状态字段不存在。

- [ ] **Step 4: 改为有界动态编码**

接口统一为：

```cpp
using EncodedCheckpoint = std::vector<std::uint8_t>;

[[nodiscard]] std::optional<EncodedCheckpoint> encode_checkpoint(
    const dungeon::checkpoint::DungeonRunState& state) noexcept;
```

编码前 checked 计算长度并一次 `resize`；捕获 `std::bad_alloc` 返回 `nullopt`。CRC 增加增量接口：

```cpp
[[nodiscard]] std::uint32_t crc32_update(
    std::uint32_t state, const std::uint8_t* data, std::size_t size) noexcept;
```

事务写入接受 `const std::vector<std::uint8_t>&`，保留临时文件、flush、轮换和故障注入语义。

- [ ] **Step 5: 解码先验边界后再分配**

先验证 magic/version/header/payload 长度与 `item_count <= 65535`，checked 得到精确期望长度且无尾随字节，再 reserve/resize。读取完成后只调用 `items::validate_ownership()` 作为统一语义门。

- [ ] **Step 6: 扩展状态相等与双槽恢复**

`same_state` 比较获得顺序、全部 roll、equipment、claimed bits、sequence。增加一大一小两个 V4 轮换，验证恢复选择最新有效 generation。

- [ ] **Step 7: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_persistence_tests
ctest --test-dir build-release -R persistence --output-on-failure
git diff --check
```

- [ ] **Step 8: 提交**

```powershell
git add src/dungeon/dungeon_checkpoint.hpp src/persistence tests/persistence
git commit -m "feat: persist variable length equipment checkpoints"
```

---

## Task 6: Dungeon 背包、换装与三合一原子事务

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/room_combat_template.hpp`
- Modify: `src/dungeon/room_combat_template.cpp`
- Create: `tests/dungeon/dungeon_item_transaction_tests.cpp`
- Modify: `tests/dungeon/dungeon_passive_tree_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`

- [ ] **Step 1: 写事务失败测试**

覆盖装备等级不足、错槽、不存在 ID、重复装备、卸下；committed 才发布 build；not_committed 保持旧状态/build；indeterminate faulted。生命/护盾不回复。三合一覆盖三 ID 唯一、未装备、同 slot/rarity、材料删除+产物追加+sequence 增加原子性和失败重试等价。

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_dungeon_tests
ctest --test-dir build-release -R dungeon.units --output-on-failure
```

- [ ] **Step 3: 增加请求和稳定查询接口**

```cpp
enum class PendingSaveKind : std::uint8_t {
    transition, passive_tree, loot_pickup, equipment, recipe
};

[[nodiscard]] RequestResult request_equip(std::uint64_t item_id) noexcept;
[[nodiscard]] RequestResult request_unequip(items::ItemSlot slot) noexcept;
[[nodiscard]] RequestResult request_recipe(
    const std::array<std::uint64_t, 3>& item_ids) noexcept;
[[nodiscard]] const items::ItemOwnershipState& item_state() const noexcept;
```

`DungeonSnapshot` 只增加 `inventory_count`、6 个 equipped IDs、ground drop 固定快照和 pending kind；禁止每帧复制 vector。

- [ ] **Step 4: 实现唯一组合 build 路径**

增加私有函数：

```cpp
[[nodiscard]] std::optional<combat::PlayerCombatBuild> build_for(
    const checkpoint::DungeonRunState& state) const noexcept;
```

它把 `append_passive_modifiers` 与 `project_equipment` 合并进 256 固定缓冲并只 evaluate 一次。房间加载和 equipment/recipe committed 后都使用该函数；无 combat 的阶段只更新稳定状态。

- [ ] **Step 5: 实现 pending checkpoint 事务**

所有 request 先拒绝 faulted、pending、非法等级/引用/材料，再复制一次稳定状态构造 `PendingSave`；捕获分配失败并返回 rejected。只有 `on_save_committed()` 交换稳定状态和应用 build；not_committed 丢弃 next state；indeterminate 调用既有 faulted 路径。

- [ ] **Step 6: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_dungeon_tests
ctest --test-dir build-release -R "dungeon.units|combat.units|items.units" --output-on-failure
git diff --check
```

- [ ] **Step 7: 提交**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: add atomic equipment and recipe transactions"
```

---

## Task 7: 怪物独立掉落、地面池与自动拾取

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Create: `tests/dungeon/dungeon_loot_drop_tests.cpp`
- Modify: `tests/dungeon/dungeon_wave_tests.cpp`
- Modify: `tests/dungeon/dungeon_lifecycle_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`

- [ ] **Step 1: 写固定 trace 与领取事务失败测试**

用可控击杀事件覆盖 2×96 序号、`wave*96+target_index`、每怪仅一次 1% roll、无额外房间上限、192 池容量、固定位置与固定内容。覆盖 claimed 后重载不再掉、未 claimed 重杀同序号得到同物品、新房 bits 清零、离房未拾取永久消失。

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_dungeon_tests
ctest --test-dir build-release -R dungeon.units --output-on-failure
```

- [ ] **Step 3: 定义固定地面池**

```cpp
inline constexpr std::size_t kGroundDropCapacity = 192;
inline constexpr float kPickupRadius = 1.5F;

struct GroundItem final {
    bool active{};
    std::uint16_t drop_ordinal{};
    combat::Vec3 position{};
    ItemInstance item{};
};
```

`DungeonSession` 使用 `std::array<GroundItem, 192>`，不使用 vector；快照只复制 active 项的有界视图或整个固定数组。

- [ ] **Step 4: 从击杀事件派生一次掉落**

在 relay `CombatEventKind::defeated` 时读取当前 wave index 和 `target_index`；检查 0..191、claimed、已生成 ordinal。由 room seed + ordinal + drop 域调用 `DeterministicRng::derive_stream()` 创建独立 RNG，只有 `next_bounded(100).value()==0` 才生成物品；slot 六等权，ilvl=`min(100, depth)`。

- [ ] **Step 5: 实现拾取请求**

```cpp
[[nodiscard]] RequestResult request_pickup(std::uint16_t drop_ordinal) noexcept;
void request_nearby_pickups(combat::Vec3 player_position) noexcept;
```

一次只允许一个 pending save；距离使用 X/Y 平方距离 `<= 2.25F`。committed 后移除地面项并设置 claimed；not_committed 保留；indeterminate faulted。新房 committed 后清池并清 bits，旧房未拾取内容不迁移。

- [ ] **Step 6: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_dungeon_tests
ctest --test-dir build-release -R dungeon.units --output-on-failure
git diff --check
```

- [ ] **Step 7: 提交**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: add deterministic ground equipment drops"
```

---

## Task 8: Runtime 保存服务、重启一致性与故障门控

**Files:**
- Modify: `src/platform/raylib/dungeon_runtime.hpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Modify: `tests/persistence/dungeon_save_integration_tests.cpp`
- Modify: `tests/persistence/save_store_fault_tests.cpp`

- [ ] **Step 1: 写 runtime 故障矩阵失败测试**

对 pickup/equip/unequip/recipe 各验证 committed、not_committed、indeterminate；pending 时拒绝新拾取、换装、合成、门、洞。重启后比较 item bytes、equipment、claimed、sequence 和角色 build。

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_platform_tests arpg_persistence_tests
ctest --test-dir build-release -R "platform.units|persistence" --output-on-failure
```

- [ ] **Step 3: 泛化保存服务但保持单事务**

`DungeonRuntime::service_pending_save()` 继续只处理一个 `PendingSave`，不按 kind 分叉编码；把 `SaveStore::commit()` 结果精确映射到 session 的 committed/not_committed/indeterminate。所有物品请求都通过 runtime 的窄接口转发，raylib host 不直接改 session state。

- [ ] **Step 4: 验证大背包不会进入帧快照**

构造 65,535 件合法最小物品状态，调用 snapshot 多次并通过 allocation probe 断言无 vector 复制/分配；UI 使用稳定 `item_state()` const 引用，只在背包打开且 stable generation 改变时重建可见索引。

- [ ] **Step 5: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_platform_tests arpg_persistence_tests
ctest --test-dir build-release -R "platform.units|persistence" --output-on-failure
git diff --check
```

- [ ] **Step 6: 提交**

```powershell
git add src/platform/raylib/dungeon_runtime.* tests/platform/dungeon_runtime_tests.cpp tests/persistence
git commit -m "feat: integrate equipment save transactions"
```

---

## Task 9: raylib 地面表现、三栏背包与零补跑暂停

**Files:**
- Create: `src/platform/raylib/inventory_view_math.hpp`
- Create: `src/platform/raylib/inventory_view_math.cpp`
- Create: `src/platform/raylib/inventory_renderer.hpp`
- Create: `src/platform/raylib/inventory_renderer.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `src/core/fixed_step.hpp`
- Modify: `src/core/fixed_step.cpp`
- Test: `tests/core/fixed_step_tests.cpp`
- Create: `tests/platform/inventory_view_math_tests.cpp`
- Modify: `tests/platform/combat_key_bindings_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`

- [ ] **Step 1: 写暂停与视图失败测试**

固定步测试先 advance 半步、调用 clear accumulator、再 advance 半步，断言 0 tick；不得保留菜单前余量。视图测试覆盖 3 栏矩形、不同窗口缩放、虚拟网格首/末可见索引、筛选、滚动 clamp、单击/双击时间窗、Ctrl 最多 3 件、equipped 命中及 I/P 互斥。

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_core_tests arpg_platform_tests
ctest --test-dir build-release -R "core.units|platform.units" --output-on-failure
```

- [ ] **Step 3: 增加明确 fixed-step 暂停 API**

```cpp
void FixedStepRunner::clear_accumulator() noexcept;
```

只清 `accumulator_seconds_`，不清 total ticks、dropped seconds 或 invalid count。背包打开瞬间和关闭瞬间各调用一次；打开期间完全不调用 `advance()`。

- [ ] **Step 4: 实现纯视图模型**

```cpp
struct InventoryLayout final { Rectangle equipment; Rectangle grid; Rectangle detail; };
struct VisibleGridRange final { std::size_t first{}; std::size_t count{}; };

[[nodiscard]] InventoryLayout inventory_layout(int width, int height) noexcept;
[[nodiscard]] VisibleGridRange visible_grid_range(
    std::size_t filtered_count, int columns, float scroll_rows,
    float viewport_height, float cell_height) noexcept;
```

筛选索引只引用稳定 item vector，不改变获得顺序。比较面板分别求值当前完整 build 和假定替换完整 build，再做字段差。

- [ ] **Step 5: 实现输入门控**

`I` 打开 inventory 仅当 passive overlay 关闭；`P` 仅当 inventory 关闭。inventory 打开时只处理 I/Esc、鼠标、Ctrl、滚轮和请求按钮；不采样/排队 WASD、J/K/L、E、门/洞/reset。pending/faulted 时按钮显示禁用状态。

- [ ] **Step 6: 实现地面与三栏绘制**

地面图标按 slot 形状和 normal/magic/rare 颜色显示，位置由现有世界到屏幕变换；不显示鉴定状态。左栏绘制 6 槽和最终角色属性，中栏只遍历 visible range，右栏显示 ilvl、需求、固有、全部词缀、局部/全局标签与差值。提供全部/slot/rarity 筛选及 Combine 按钮。

- [ ] **Step 7: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_core_tests arpg_platform_tests arpg_game
ctest --test-dir build-release -R "core.units|platform.units" --output-on-failure
git diff --check
```

- [ ] **Step 8: 提交**

```powershell
git add src/core tests/core src/platform/raylib tests/platform
git commit -m "feat: add paused equipment inventory interface"
```

---

## Task 10: 1000 房确定性、全量回归与窗口验收

**Files:**
- Create: `tests/dungeon/dungeon_equipment_stress_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Create: `docs/validation/stage8-equipment-loot-mvp.md`

- [ ] **Step 1: 写 1000 房双 session 压力测试**

两个 session 使用同一 root seed 和固定输入 trace。每房逐只触发击杀，靠近拾取；有合法物品时按固定序列换装和三合一；每 37 房编码、解码并重建其中一个 session。每步比较：room seed、generation、物品记录逐字节、equipment IDs、claimed bits、sequence、完整 build 和 ground drop ordinal/content。

- [ ] **Step 2: 运行 RED 并确认测试能发现扰动**

先在测试 trace 的副本中人为改变一个 room 的 pickup 顺序，断言 comparison helper 报告不等；恢复相同 trace 后，生产实现若仍有随机流耦合则测试失败并给出首个房间/字段。

```powershell
cmake --build build-release --target arpg_dungeon_tests
ctest --test-dir build-release -R dungeon.units --output-on-failure
```

- [ ] **Step 3: 修正仅由测试暴露的确定性缺口**

只允许调整随机域隔离、稳定排序或显式序号；不得通过放宽比较、固定结果常量或跳过重启来使测试通过。修改的生产文件须加入本任务提交并在验证记录列明原因。

- [ ] **Step 4: 完整 Debug 验证**

```powershell
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DARPG_FETCH_RAYLIB=OFF -DCMAKE_PREFIX_PATH=E:/game/.deps/raylib-6.0
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure
```

Expected: 全部测试通过。

- [ ] **Step 5: 完整 Release 验证**

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DARPG_FETCH_RAYLIB=OFF -DCMAKE_PREFIX_PATH=E:/game/.deps/raylib-6.0
cmake --build build-release
ctest --test-dir build-release --output-on-failure
git diff --check
git status --short
```

Expected: 全部测试通过；仅本任务预期文件处于未提交状态。

- [ ] **Step 6: 执行窗口验收并记录证据**

启动 `build-release/src/app/arpg_game.exe`，按设计规格第 15.2 节逐项验证：可见掉落、1.5 距离拾取、I 暂停、三栏、查看/双击换装、实战属性变化、生命护盾只截断、三合一、未拾取离房消失、重启持久化。将日期、构建提交、存档路径、每项 pass/fail 和截图路径写入 `docs/validation/stage8-equipment-loot-mvp.md`；任何 fail 都先修复并重跑受影响测试与窗口步骤。

- [ ] **Step 7: 提交最终验证**

```powershell
git add tests/dungeon docs/validation
git add -u
git commit -m "test: validate stage 8 equipment loot milestone"
```

- [ ] **Step 8: 最终审查边界**

```powershell
git log --oneline 352cc13..HEAD
git diff --stat 352cc13..HEAD
rg -n "epic|identify|craft|corrupt|monster affix|item filter" src tests
ctest --test-dir build-release --output-on-failure
git status --short
```

逐项确认没有实现明确排除功能，工作树干净，并停在 `codex/stage8-equipment-loot-mvp` 等待用户验收。

---

## Spec Coverage Checklist

- [ ] 6 固定基底、24 词缀、T1–T8 门槛和值由 Task 2 golden 测试锁定。
- [ ] 稀有度、词缀数、tier 权重、需求等级、局部武器语义由 Task 3 锁定。
- [ ] 闪避、护甲、元素减伤、75/95% cap 与不回复热换由 Task 1/4 锁定。
- [ ] 1% 独立掉落、192 ordinal/容量、1.5 拾取、离房销毁由 Task 7 锁定。
- [ ] 无限背包查询、即时换装、三合一和提交故障语义由 Task 6/8 锁定。
- [ ] I/P 互斥、战斗暂停、零补跑、三栏虚拟化和比较由 Task 9 锁定。
- [ ] V4 精确布局、65,535 边界、V1–V3 迁移和双槽恢复由 Task 5 锁定。
- [ ] 双 session 1000 房、Debug/Release、窗口验收与停止边界由 Task 10 锁定。
