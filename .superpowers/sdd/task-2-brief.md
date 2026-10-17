## Task 2: Item 类型、目录与构建边界

### Frozen catalog constants for this task

Tier 数组按 `T8,T7,T6,T5,T4,T3,T2,T1`：minimum level=`1,16,30,45,60,75,88,95`；base weight=`64,48,36,27,20,15,11,8`。

固定基底：

| ID | Base | Slot | inherent T8→T1 |
|---:|---|---|---|
| 1 | Iron Blade | weapon | physical flat `2,3,4,5,7,9,12,16` |
| 2 | Guard Helm | helmet | max health `3,5,8,12,17,23,30,38` |
| 3 | Ward Coat | chest | max barrier `4,7,11,16,22,29,37,46` |
| 4 | Striker Gloves | gloves | global attack speed bp `100,200,300,400,500,600,700,800` |
| 5 | Runner Boots | boots | move speed bp `100,200,300,400,500,600,700,800` |
| 6 | Element Charm | accessory | each element reduction bp `100,200,300,400,500,600,800,1000` |

前缀：

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

后缀：

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

每个 affix ID 即唯一 group ID。词缀 112 的已使用 roll variant 为 fire=0、water=1、lightning=2、chaos=3；其他已使用词缀 variant 固定 `0xFF`；未使用 roll 四字节全零。普通必须 0 词缀，魔法 1..2，稀有 3..6；prefix/suffix 各最多 3；required level 为最强 tier minimum，普通为 1。所有权最多 65,535 件，item ID 必须非零且唯一，equipped ID 必须存在、slot 匹配且不能被多个槽引用。

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
