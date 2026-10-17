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

