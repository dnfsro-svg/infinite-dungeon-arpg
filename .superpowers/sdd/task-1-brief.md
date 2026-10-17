### Task 1: 扩展通用 Modifier 的伤害类型和玩家构筑值

**Files:**
- Create: `src/modifiers/damage_types.hpp`
- Create: `src/modifiers/player_modifier_values.hpp`
- Create: `src/modifiers/player_modifier_values.cpp`
- Modify: `src/modifiers/modifier_types.hpp`
- Modify: `src/modifiers/CMakeLists.txt`
- Modify: `tests/modifiers/CMakeLists.txt`
- Modify: `tests/modifiers/modifier_test_main.cpp`
- Create: `tests/modifiers/player_modifier_values_tests.cpp`

**Interfaces:**
- Consumes: 现有 `Modifier`、`ModifierSpan`、`evaluate_stat()` 和 `kFixedOne`。
- Produces: `DamageType`、`kDamageTypeCount`、`is_elemental()`、`element_index()`、`PlayerModifierValues` 和 `evaluate_player_modifiers(ModifierSpan)`；后续 passives 与 combat 只通过这些类型通信。

- [ ] **Step 1: 写出玩家构筑值的 RED 测试**

在 `player_modifier_values_tests.cpp` 中写入以下测试，先引用尚不存在的头文件和函数：

```cpp
using namespace arpg::modifiers;

arpg::test::Failure fire_and_generic_values_are_independent() noexcept {
    const std::array<Modifier, 4> values{{
        {101U, StatId::fire_flat_damage, ModifierOperation::flat, 30000},
        {102U, StatId::fire_damage, ModifierOperation::increased, 800},
        {103U, StatId::fire_resistance, ModifierOperation::flat, 700},
        {104U, StatId::move_speed, ModifierOperation::increased, 600},
    }};
    const PlayerModifierValues result = evaluate_player_modifiers(values);
    ARPG_REQUIRE(result.flat_damage[damage_index(DamageType::fire)] == 30000);
    ARPG_REQUIRE(result.damage_increased[damage_index(DamageType::fire)] == 10800);
    ARPG_REQUIRE(result.resistance[element_index(DamageType::fire)] == 700);
    ARPG_REQUIRE(result.movement_speed == 10600);
    ARPG_REQUIRE(result.resistance[element_index(DamageType::water)] == 0);
    return {};
}

arpg::test::Failure resistance_and_reduction_are_clamped() noexcept {
    const std::array<Modifier, 2> values{{
        {201U, StatId::lightning_resistance, ModifierOperation::flat, 10000},
        {202U, StatId::damage_taken, ModifierOperation::increased, -9000},
    }};
    const PlayerModifierValues result = evaluate_player_modifiers(values);
    ARPG_REQUIRE(result.resistance[element_index(DamageType::lightning)] == 7500);
    ARPG_REQUIRE(result.damage_taken == 5000);
    return {};
}
```

将 suite 加入 `modifier_test_main.cpp`，临时把期望 case 数从 13 改成 15。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --preset windows-msvc-debug && cmake --build --preset windows-msvc-debug --target arpg_modifier_tests'
```

Expected: 编译失败，提示 `modifiers/damage_types.hpp` 或 `evaluate_player_modifiers` 尚不存在。

- [ ] **Step 3: 实现稳定类型和求值投影**

在 `damage_types.hpp` 定义连续枚举；物理索引为 0，四元素索引为 0–3，避免把物理放进抗性数组：

```cpp
enum class DamageType : std::uint8_t { physical, fire, water, lightning, chaos, count };
inline constexpr std::size_t kDamageTypeCount = static_cast<std::size_t>(DamageType::count);
inline constexpr std::size_t kElementCount = 4U;
[[nodiscard]] constexpr std::size_t damage_index(DamageType type) noexcept {
    return static_cast<std::size_t>(type);
}
[[nodiscard]] constexpr bool is_elemental(DamageType type) noexcept {
    return type != DamageType::physical && type != DamageType::count;
}
[[nodiscard]] constexpr std::size_t element_index(DamageType type) noexcept {
    return static_cast<std::size_t>(type) - 1U;
}
```

把 `StatId` 扩展为：`melee_damage`、`fire_flat_damage`、`water_flat_damage`、`lightning_flat_damage`、`chaos_flat_damage`、四个元素伤害提高、四个元素抗性、`max_health`、`max_barrier`、`damage_taken`、`move_speed`、`attack_speed`、`jump_speed`、`air_control`，保留已有 `impulse_scale` 和怪物 `shield`。`PlayerModifierValues` 的乘数默认 `kFixedOne`、抗性/平坦值默认 0，并以 `evaluate_stat()` 得出值；抗性钳制 `[-6000, 7500]`，`damage_taken` 钳制 `[5000, 20000]`，其余速度/冲量下限为 0。所有固定生命、护盾和附加伤害均以 `kFixedOne == 10000` 为单位：例如 +20 HP 写作 `200000`，+10 barrier 写作 `100000`，+3 火伤写作 `30000`；combat 只在投影时向下转换为整数游戏数值。

```cpp
struct PlayerModifierValues final {
    std::array<FixedValue, kDamageTypeCount> flat_damage{};
    std::array<FixedValue, kDamageTypeCount> damage_increased{{kFixedOne, kFixedOne, kFixedOne, kFixedOne, kFixedOne}};
    std::array<FixedValue, kElementCount> resistance{};
    FixedValue melee_damage{kFixedOne};
    FixedValue max_health{};
    FixedValue max_barrier{};
    FixedValue damage_taken{kFixedOne};
    FixedValue movement_speed{kFixedOne};
    FixedValue attack_speed{kFixedOne};
    FixedValue impulse_scale{kFixedOne};
    FixedValue jump_speed{kFixedOne};
    FixedValue air_control{kFixedOne};
    bool valid{true};
};
```

把 `player_modifier_values.cpp` 加入 `arpg_modifiers`；将测试源加到 `arpg_modifier_tests`。

- [ ] **Step 4: 验证 modifiers 回归**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "modifiers.units|architecture.modifiers" --output-on-failure
git diff --check
```

Expected: `modifiers.units` 与 5 个 `architecture.modifiers_*` 测试全部通过；新 suite 显示 15 cases, 0 failures。

- [ ] **Step 5: 提交 Task 1**

```powershell
git add src/modifiers tests/modifiers
git commit -m "feat: add typed player modifier values"
```

