## Task 9: 生成确定性深渊宝箱并支持重载重建

**Files:**

- Create: `src/dungeon/abyss_reward.cpp`
- Create: `src/dungeon/abyss_reward.hpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Create: `tests/dungeon/dungeon_abyss_reward_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Produces:** 低/中/高 1/2/3 件、+1/+3/+5 物品等级、稀有度偏移、中心落地、池满重试、重载重建。

**Consumes:** `items::generate_item`、Task 1 奖励 profile、V5 三位掩码。

- [ ] 新增奖励计划结构：

```cpp
struct AbyssRewardSlot final {
    std::uint8_t slot_index{};
    items::ItemSlot item_slot{items::ItemSlot::weapon};
    items::ItemRarity rarity{items::ItemRarity::normal};
    std::uint8_t item_level{1};
    std::uint64_t item_seed{};
    std::uint64_t item_id{};
};

[[nodiscard]] std::optional<AbyssRewardSlot> derive_abyss_reward_slot(
    std::uint64_t room_seed,
    abyss::AbyssDanger danger,
    std::uint8_t base_item_level,
    std::uint8_t reward_ordinal) noexcept;
```

- [ ] 增加测试：三档数量/等级/权重；基础 item level 精确取当前房间 depth 并在加成后 cap100；每个 slot 独立随机且允许重复；同 room seed + ordinal + rules version 得到同内容；时间/build/retry 不参与；地面池空位只生成可放入数量；每 tick 最多尝试 3 次；池满不丢失、不推进 generated mask；清空槽位后继续；reload 后重建 generated 未 claimed 的相同物品。

- [ ] 先运行并确认深渊清场后无奖励。

- [ ] 用 room seed、深渊命名域、reward ordinal 和 rules version 分别派生 item seed、slot、rarity 与 item id；不要把 `next_item_sequence`、清场时间或重试次数混入内容。item id 使用独立深渊 ID 命名域，若与 ownership 现有 ID 碰撞则 fault，不得重投。奖励位置固定在房间中心的三个小偏移点，自动拾取仍复用 `request_nearby_pickups`。

- [ ] GroundItem 增加稳定来源：

```cpp
enum class GroundItemSource : std::uint8_t { monster_drop, abyss_chest };

struct GroundItem final {
    bool active{};
    std::uint16_t drop_ordinal{};
    GroundItemSource source{GroundItemSource::monster_drop};
    std::uint8_t abyss_reward_ordinal{0xFF};
    combat::Vec3 position{};
    items::ItemInstance item{};
};
```

- [ ] 每次成功物化 reward slot 后先创建 `abyss_reward_materialized` 保存请求，同时设置 generated bit 并递增 reward revision；保存成功才把地面物品设 active。这样 crash 不会出现未记录物品。加载 cleared 房时按 masks 重建未 claimed 且已 generated 的地面物品，并继续生成未 generated。

- [ ] 深渊宝箱是本任务唯一新增奖励源；普通怪物经验和掉落继续完全走 Stage 9 的 affix score 逻辑，深渊遭遇预算或房间规则不得额外修改它们。

- [ ] 运行：

```powershell
ctest --preset windows-msvc-debug -R "dungeon.units|items.units|persistence.units" --output-on-failure
```

预期：奖励、物品目录、存档回归全部通过；Stage 9 普通掉落 seed 和概率测试不变。

- [ ] 提交：

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: materialize deterministic abyss chest rewards"
```

---

