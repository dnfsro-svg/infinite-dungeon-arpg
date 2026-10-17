## Task 3: 确定性生成、局部武器属性与组合 Modifier

### Frozen generation and integration decisions

- `item_level` 只接受 1..100，`item_id` 必须非零，slot 必须小于 `ItemSlot::count`。`generate_item` 负责这些局部输入；与整个物品表的 ID 冲突由 Task 2 `validate_ownership` 和后续 dungeon 事务拒绝。
- 公开可直接测试的权重接口，避免用大样本统计猜公式：

```cpp
struct RarityWeights final {
    std::uint32_t normal{};
    std::uint32_t magic{};
    std::uint32_t rare{};
};

[[nodiscard]] std::optional<RarityWeights> rarity_weights(
    std::uint8_t item_level) noexcept;
[[nodiscard]] std::optional<std::array<std::uint32_t, 8>> tier_weights(
    std::uint8_t item_level) noexcept; // T8..T1；未解锁 tier 为 0
```

- 令 `x=item_level-1`：normal=`70-floor(30*x/99)`，magic=`25+floor(15*x/99)`，rare=`100-normal-magic`。ilvl1 为 70/25/5，ilvl100 为 40/40/20。
- tier minimum 复用 Task 2 `tier_minimum_level()`；基础权重必须复用 `tier_base_weight()`，不得复制数组。最高可用 tier 加 `floor(item_level/5)`，第二高可用 tier 加 `floor(item_level/10)`；仅一个 tier 时只加第一项。
- 普通固定 0 词缀；魔法 1/2 条权重 50/50；稀有 3/4/5/6 条权重 40/30/20/10。prefix/suffix 各最多 3、group 不重复、候选无放回；词缀值按 `index=8-tier` 读取目录。
- 每个随机决策使用 `DeterministicRng::derive_stream(seed, named_domain)` 和 `next_bounded()`；域常量必须是命名的非零 `uint64_t`，drop/item generation 与 recipe ID、recipe affix 彼此隔离。测试锁定同 seed 请求的 `ItemInstance` 字节等价，不锁定某个实现偶然产生的统计分布。
- `generate_recipe_item` 必须先 `validate_item` 三材料，拒绝零/重复材料 ID、不同 slot、不同 rarity、`next_sequence==0`。output slot/rarity 不变，ilvl 为三者平均向下取整，base 由 slot 唯一基底决定。output ID 由 `root_seed + next_sequence + recipe-id domain` 派生并将 0 固定重映射为非零；若等于任一材料 ID 则返回 `nullopt`。词缀 seed 只由排序后三材料 ID + recipe-affix domain 派生，因此材料输入顺序不影响完整产物。普通产物 0 词缀，魔法/稀有重新决定目标条数并完整重骰。
- `project_equipment` 必须先验证所有权，且只投影 6 个 equipped 引用。Modifier ID 使用命名的装备专用高位域并由 slot/来源位置派生，必须非零、唯一且不与当前星盘目录 ID 冲突；任何 checked 算术、容量或 int32 local attack speed 超界令 `valid=false`。
- `append_passive_modifiers` 的 `count` 是已有前缀长度。成功时在 `[count,new_count)` 追加并更新 count；容量不足、空 output 与非零 capacity 或非法 tree 时返回 false，且 count 和已有输出内容均不改变。现有 `evaluate_passive_tree` 必须通过该接口收集后再调用一次 evaluator。

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
