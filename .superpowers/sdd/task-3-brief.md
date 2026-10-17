### Task 3: 增加稳定 DeathCheckpoint 与确定性退层生成器

**Files:**

- Create: `src/dungeon/death_checkpoint.hpp`
- Create: `src/dungeon/death_checkpoint.cpp`
- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `src/dungeon/dungeon_progression.cpp`
- Modify: `src/dungeon/room_generation.hpp`
- Modify: `src/dungeon/room_generation.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Create: `tests/dungeon/death_checkpoint_tests.cpp`
- Modify: `tests/dungeon/dungeon_progression_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`

**Step 1: 写稳定状态和随机域失败测试**

在 `dungeon_checkpoint.hpp` 的 `dungeon::checkpoint` 命名空间定义稳定类型；`death_checkpoint.hpp/.cpp` 只承载 Combat→checkpoint 转换和需要 Dungeon/目录语义的校验，避免 Persistence 反向链接 `arpg_dungeon`：

```cpp
enum class DeathLifecycle : std::uint8_t { none, pending_continue };
enum class DeathSourceKind : std::uint8_t {
    monster_attack, projectile, ground_hazard,
    monster_affix, abyss_environment, unknown,
};
enum class DeathDamageType : std::uint8_t {
    physical, fire, water, lightning, chaos,
};

struct DeathCheckpoint final {
    DeathLifecycle lifecycle{DeathLifecycle::none};
    std::uint8_t data_version{};
    std::uint64_t death_depth{};
    std::uint64_t death_floor_room_index{};
    DungeonElement death_ecology{DungeonElement::fire};
    bool death_was_abyss{};
    DeathSourceKind source_kind{DeathSourceKind::unknown};
    std::uint8_t source_monster_id{0xFFU};
    std::uint16_t source_detail_id{};
    DeathDamageType damage_type{DeathDamageType::physical};
    std::uint64_t raw_damage{};
    std::uint64_t barrier_loss{};
    std::uint64_t health_loss{};
    std::uint64_t final_damage{};
    std::array<std::uint64_t, 5> recent_damage{};
    std::int32_t hp{};
    std::int32_t max_hp{};
    std::int32_t barrier{};
    std::int32_t max_barrier{};
    std::int64_t armor{};
    std::int64_t evasion{};
    std::int32_t armor_reduction_bp{};
    std::int32_t evasion_rate_bp{};
    std::array<std::int32_t, 4> damage_reduction{};
    std::array<std::int32_t, 4> damage_reduction_cap{};
    RoomDescriptor target_room{0U, 0U, 0U, 0U,
        EntrySide::initial, DungeonElement::fire, false, false};
};
```

在 `DungeonRunState` 追加 `std::uint64_t death_sequence{}` 和 `DeathCheckpoint death{}`。测试要求 `same_run_state()` 比较每个新增字段；初始 run 为序号 0、规范 `none`。`none` 的 target 深度/层内编号必须显式为 0，不能继承普通 `RoomDescriptor` 的 depth/floor 默认 1；枚举使用各自规定的 canonical 默认，其余数值和布尔为 0。

退层生成接口固定为：

```cpp
struct DeathRetreatTargetResult final {
    DungeonFault fault{DungeonFault::none};
    checkpoint::RoomDescriptor room{};
};

[[nodiscard]] DeathRetreatTargetResult make_death_retreat_target(
    const checkpoint::DungeonRunState& current,
    std::uint64_t next_death_sequence,
    const DungeonRules& rules) noexcept;
```

测试覆盖深度夹紧、`floor_room_index=0`、初始入口、非深渊、零偏向生成、普通洞口规则、同输入逐字段相同、改变任一输入改变命名流、非零 seed、最大序号/代数 fault，且不改变门/下降/深渊/掉落流的既有 golden 值。

**Step 2: 确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
```

Expected: 新稳定类型、生成接口或 suite 未定义导致失败。

**Step 3: 实现命名域与规则无关结构校验**

- 在 `room_generation.cpp` 增加唯一常量 `kDeathRetreatDomain`，依次混入当前房 seed、当前 `commit_generation`、下一 `death_sequence`、目标深度；若结果为 0，固定映射为 `0xD34D5EEDULL`，不二次抽样。
- 用 `generate_room_descriptor(seed, current.index + 1, target_depth, 0, initial, {0,0,0,0}, rules)` 生成生态和洞口，再强制 `is_abyss=false`。
- 对 `current.index`、`commit_generation`、`death_sequence` 的最大值先返回明确 overflow fault；新增 fault 枚举必须有测试和可读诊断。
- `dungeon_checkpoint.hpp` 提供 `constexpr/noexcept` 的 header-only `valid_death_checkpoint_structural()`，检查规范零值、枚举范围、最后一击和防御字段，因此 Persistence 只依赖稳定类型且无需链接 `arpg_dungeon`。Combat snapshot 转换、目录 ID 和需要 `DungeonRules` 的重生成校验放在 `death_checkpoint.hpp/.cpp` 的 Dungeon 层。

**Step 4: 运行 GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
ctest --preset windows-msvc-debug -R '^dungeon\.units$' --output-on-failure
```

Expected: Dungeon 既有 golden 和新死亡生成测试全部通过。

**Step 5: 提交**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: add deterministic death checkpoints"
```

---

