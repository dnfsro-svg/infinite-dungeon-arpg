## Task 1: 建立纯 `arpg_abyss` 规则核

**Files:**

- Create: `src/abyss/CMakeLists.txt`
- Create: `src/abyss/abyss_types.hpp`
- Create: `src/abyss/abyss_rules.hpp`
- Create: `src/abyss/abyss_rules.cpp`
- Create: `src/abyss/abyss_rewards.hpp`
- Create: `src/abyss/abyss_rewards.cpp`
- Create: `tests/abyss/CMakeLists.txt`
- Create: `tests/abyss/abyss_test_main.cpp`
- Create: `tests/abyss/abyss_rules_tests.cpp`
- Create: `tests/abyss/abyss_rewards_tests.cpp`
- Modify: `CMakeLists.txt`

**Produces:** `AbyssDanger`, `AbyssRuleId`, `AbyssLifecycle`, `AbyssCombatConfig`、固定深度权重、规则抽取、比例生命映射、环境伤害原始值、宝箱件数/物品等级/稀有度偏移。

**Consumes:** `core::DeterministicRng` 和 `modifiers::DamageType`；不得包含 `dungeon/`、`persistence/` 或 raylib 头文件。

`CMakeLists.txt` 必须在 `src/core` 之后、`src/combat` 之前加入 `src/abyss`；`arpg_abyss` 只链接 `arpg_core`。后续由 `arpg_combat` PUBLIC 链接 `arpg_abyss`，不能形成 `abyss → combat → abyss` 环。

- [ ] 写 28 个纯函数单测，覆盖四段危险权重、每段边界、每个危险等级三条规则的可达性、同种子稳定性、不同命名域互不漂移、`ceil(1.5x)`、深度词缀下限、整数生命比例映射、三档宝箱和稀有度重分配。

危险权重必须固化为 `1～9: 70/25/5`、`10～19: 45/40/15`、`20～39: 25/45/30`、`40+: 10/35/55`（低/中/高）；选中危险后在该档三条规则中等权选择。低危为 Thunderstorm/Swift Pursuit/Heavy Steps，中危为 Hunting Flames/Abyss Bulwark/Exhausted Recovery，高危为 Chaos Expansion/Abyss Fury/Life Sacrifice。

核心公共类型按以下稳定枚举值创建；这些值会进入 V5 存档，后续不得重排：

```cpp
inline constexpr std::uint32_t kAbyssRulesVersion = 1U;

enum class AbyssDanger : std::uint8_t { low = 0, medium = 1, high = 2 };

enum class AbyssRuleId : std::uint8_t {
    thunderstorm = 0,
    hunting_flames = 1,
    chaos_expansion = 2,
    swift_pursuit = 3,
    abyss_bulwark = 4,
    abyss_fury = 5,
    heavy_steps = 6,
    exhausted_recovery = 7,
    life_sacrifice = 8,
    none = 0xFF,
};

enum class AbyssLifecycle : std::uint8_t {
    none = 0, available = 1, started = 2, cleared = 3, failed = 4,
};

struct AbyssCombatConfig final {
    AbyssRuleId rule{AbyssRuleId::none};
    std::uint16_t player_ground_move_bp{10000};
    std::uint16_t player_resource_restore_bp{10000};
    std::uint16_t player_max_health_bp{10000};
    std::uint16_t monster_move_bp{10000};
    std::uint16_t monster_cooldown_bp{10000};
    std::uint16_t monster_armor_bp{10000};
    std::uint16_t monster_extra_shield_bp{};
    std::uint16_t monster_damage_bp{10000};
    std::uint16_t monster_attack_speed_bp{10000};
};

struct AbyssSelection final {
    AbyssDanger danger{AbyssDanger::low};
    AbyssRuleId rule{AbyssRuleId::thunderstorm};
    std::uint32_t rules_version{kAbyssRulesVersion};
};
```

- [ ] 先只声明接口并接入 CMake，运行：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests
```

预期：链接失败，缺少 `select_abyss_rule`、`map_resource_ratio`、`shift_abyss_rarity_weights` 等实现，证明测试先红。

- [ ] 实现下列无状态接口，所有乘法先提升到 `std::uint64_t` 或已检查的 `std::int64_t`：

```cpp
[[nodiscard]] bool is_abyss_roll(std::uint64_t room_seed) noexcept;
[[nodiscard]] std::optional<AbyssSelection> select_abyss_rule(
    std::uint64_t room_seed, std::uint64_t depth) noexcept;
[[nodiscard]] std::uint8_t abyss_encounter_budget(
    std::uint8_t normal_budget) noexcept;
[[nodiscard]] std::uint8_t minimum_abyss_affixes(
    std::uint64_t depth) noexcept;
[[nodiscard]] std::optional<int> map_resource_ratio(
    int current, int old_max, int new_max, bool alive) noexcept;
[[nodiscard]] std::optional<int> percent_of_actual_max_hp(
    int actual_max_hp, std::uint16_t basis_points) noexcept;
[[nodiscard]] AbyssCombatConfig combat_config_for(
    AbyssRuleId rule) noexcept;
[[nodiscard]] AbyssRewardProfile reward_profile_for(
    AbyssDanger danger, std::uint8_t base_item_level) noexcept;
[[nodiscard]] std::optional<AbyssRarityWeights> shift_abyss_rarity_weights(
    AbyssRarityWeights base, AbyssDanger danger) noexcept;
```

`map_resource_ratio` 必须精确计算 `floor((current * new_max + old_max / 2) / old_max)`，存活角色最少为 1；稀有度重分配必须保持总和 100，并以最大余数法处理普通权重不足。

奖励 profile 固定为低危 `1 件/+1 ilvl/白蓝黄 -10/+5/+5`，中危 `2 件/+3/-20/+10/+10`，高危 `3 件/+5/-30/+10/+20`，最终 ilvl 上限 100。

- [ ] 运行局部测试，预期 `28 cases, 0 failures`：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests
& .\out\build\windows-msvc-debug\bin\arpg_abyss_tests.exe
```

- [ ] 提交：

```powershell
git add CMakeLists.txt src/abyss tests/abyss
git commit -m "feat: add deterministic abyss rules core"
```

---

