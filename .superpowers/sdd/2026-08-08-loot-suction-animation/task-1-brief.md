### Task 1: 固定容量动画状态与轨迹

**Files:**
- Create: `src/platform/raylib/loot_suction_animation.hpp`
- Create: `src/platform/raylib/loot_suction_animation.cpp`
- Create: `tests/platform/loot_suction_animation_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes: `DungeonSnapshot`、`DungeonRenderStatus`、`CombatCameraView`、`ground_loot_item_sprite()`、`material_loot_sprite()`、`material_color()`。
- Produces: `LootSuctionState::observe(previous, current, status)`、`update(frame_seconds, paused)`、`build_plan(camera, width, height)`、`clear()` 与 `LootSuctionPlan`。

- [ ] **Step 1: 写行为测试**

```cpp
LootSuctionState state{};
state.observe(empty, empty, ok_status); // attach baseline
state.observe(previous_with_item_42, current_without_item,
    committed_receipt_for_item_42);
ARPG_REQUIRE(state.active_count() == 1U);
const LootSuctionPlan start = state.build_plan(
    camera, {0.0F, 0.0F, 0.0F}, 1280.0F, 720.0F);
ARPG_REQUIRE(start.count == 1U);
ARPG_REQUIRE(start.flights[0].center.x > start.destination.x);

state.update(0.35F, false);
const LootSuctionPlan middle = state.build_plan(
    camera, {0.0F, 0.0F, 0.0F}, 1280.0F, 720.0F);
ARPG_REQUIRE(middle.flights[0].center.x < start.flights[0].center.x);
ARPG_REQUIRE(middle.flights[0].center.x > middle.destination.x);
ARPG_REQUIRE(middle.flights[0].center.y <
    start.flights[0].center.y +
        (middle.destination.y - start.flights[0].center.y) * 0.75F);
```

同文件再锁定：保存错误不触发、地面仍存在不触发、同一回执不重复；材料回执只吸入真正消失的 ordinal；暂停冻结；0.45 秒后回收并产生到达脉冲；13 次触发后 `active_count() == 12` 且无堆分配。

- [ ] **Step 2: 运行定向测试确认 RED**

Run:
```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 4
$env:ARPG_CPLAY021_LOOT_SUCTION_ONLY='1'
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Expected: 编译因 `loot_suction_animation.hpp`/`LootSuctionState` 尚不存在而失败，或新增行为用例在旧实现上失败；不能接受无关编译失败作为 RED。

- [ ] **Step 3: 写最小纯状态实现**

```cpp
class LootSuctionState final {
public:
    void observe(const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& status) noexcept;
    void update(float frame_seconds, bool paused) noexcept;
    [[nodiscard]] LootSuctionPlan build_plan(CombatCameraView camera,
        combat::Vec3 player_position, float width, float height) noexcept;
    void clear() noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;
private:
    std::array<LootSuctionFlight, 12U> flights_{};
    std::uint64_t equipment_generation_{};
    std::uint64_t equipment_item_id_{};
    std::uint64_t material_generation_{};
    float destination_pulse_seconds_{};
    bool attached_{};
};
```

装备必须同时满足：新 committed receipt、`previous` 找到同 item_id、`current` 已不存在。材料必须同时满足：新 material receipt、同房间、`previous` ordinal 在 `current` 消失。轨迹使用 `smoothstep` 加速与 `sin(pi*t)` 上拱，目标由当前插值角色位置投影并上移到腰后。

- [ ] **Step 4: 运行同一定向测试确认 GREEN**

Expected: `loot_suction_animation` 全部用例通过，进程退出码 0。

- [ ] **Step 5: 提交 Task 1**

```powershell
git add -- src/platform/raylib/loot_suction_animation.hpp src/platform/raylib/loot_suction_animation.cpp src/platform/raylib/CMakeLists.txt tests/platform/loot_suction_animation_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git commit -m "feat: add bounded loot suction animation state"
```

