### Task 1: 建立 Combat 承伤解析与 300 tick 固定账本

**Files:**

- Create: `src/combat/player_damage_history.hpp`
- Create: `src/combat/player_damage_history.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Create: `tests/combat/player_damage_history_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `tests/combat/CMakeLists.txt`

**Step 1: 写解析与滚动窗口失败测试**

新增 `player_damage_history_suite()`，覆盖：

- `resolve_player_damage_packet()` 保留物理/火/水/电/混沌逐类型最终值，并让 `total` 等于五项饱和和。
- 护甲只处理物理，四元素分别使用现有减伤与上限，`damage_taken` 最后应用；结果与现有总伤害测试一致。
- 逐类型解析值是减伤后的“可结算伤害”；真正写入承伤历史前还必须受当前护盾+生命上限裁剪，过量伤害不得计入“实际承伤”。
- 同 tick 多次 `record()` 逐类型累加。
- 第 300 tick 仍在窗口，第 301 tick 精确淘汰最旧桶。
- `INT_MAX` 输入和重复累计饱和而不回绕。
- 构造、推进、记录、汇总期间 `allocation_probe` 为 0。

在 `combat_types.hpp` 先声明预期接口：

```cpp
struct ResolvedPlayerDamage final {
    std::array<std::uint64_t, modifiers::kDamageTypeCount> by_type{};
    std::uint64_t total{};
};

[[nodiscard]] std::optional<ResolvedPlayerDamage>
resolve_player_damage_packet(
    DamagePacket packet, const PlayerCombatBuild& build) noexcept;
```

在新头文件声明：

```cpp
inline constexpr std::size_t kPlayerDamageHistoryTicks = 300U;

class PlayerDamageHistory final {
public:
    void begin_tick(std::uint64_t tick) noexcept;
    void record(const ResolvedPlayerDamage& damage) noexcept;
    [[nodiscard]] std::array<std::uint64_t,
        modifiers::kDamageTypeCount> totals() const noexcept;
private:
    std::array<std::array<std::uint64_t, modifiers::kDamageTypeCount>,
        kPlayerDamageHistoryTicks> buckets_{};
    std::uint64_t active_tick_{};
    bool initialized_{};
};
```

**Step 2: 运行测试并确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
```

Expected: 编译或链接失败，明确缺少 `resolve_player_damage_packet` / `PlayerDamageHistory` 实现；不得通过删测试规避。

**Step 3: 实现逐类型解析和固定账本**

- 把现有 `resolve_player_damage()` 的计算拆成逐类型结果；保留旧函数作为 `total` 的范围检查兼容包装，避免一次改坏既有调用者。
- `PlayerDamageHistory::begin_tick()` 对上次 tick 与新 tick 之间每个被跨过的 `tick % 300` 桶逐一清零；若倒退或跳跃达到 300 tick，清空全部桶；同 tick 重复调用不得清除已记内容。
- 使用本项目已有饱和工具或局部无异常 `saturating_add_u64`；不使用 `vector`、`deque`、字符串或异常路径。
- 在 `src/combat/CMakeLists.txt` 注册新 `.cpp`。

**Step 4: 运行 GREEN 并回归 Combat**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
ctest --preset windows-msvc-debug -R '^combat\.units$' --output-on-failure
```

Expected: `combat.units` 通过，旧伤害/防御测试无变化，账本零分配断言通过。

**Step 5: 提交**

```powershell
git add src/combat tests/combat
git commit -m "feat: add fixed player damage history"
```

---

