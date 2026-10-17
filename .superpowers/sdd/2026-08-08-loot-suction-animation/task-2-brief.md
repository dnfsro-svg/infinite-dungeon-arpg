### Task 2: CombatRenderer 绘制接入

**Files:**
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `tests/platform/loot_suction_animation_tests.cpp`

**Interfaces:**
- Consumes: Task 1 的 `LootSuctionState` 和 `LootSuctionPlan`。
- Produces: 成功拾取后由正式 `observe_hud -> draw` 路径显示的装备/材料飞行图标与角色腰侧收纳光环。

- [ ] **Step 1: 添加渲染接入测试并确认 RED**

```cpp
CombatRenderer renderer{};
renderer.observe_hud(previous_with_item, current_without_item,
    committed_status, hints, 0.0F, false);
ARPG_REQUIRE(renderer.loot_suction_active_count() == 1U);
renderer.clear_combat_transients();
ARPG_REQUIRE(renderer.loot_suction_active_count() == 0U);
```

Run Task 1 的定向命令。Expected: 因 `CombatRenderer` 尚未接入或无查询接口而失败。

- [ ] **Step 2: 接入观察、更新、清理和绘制**

在 `observe_hud()` 先按 `frame_seconds/paused` 前进动画，再调用 `loot_suction_.observe(previous, current, runtime_status)`；在 `clear_combat_transients()` 清空。`draw()` 的 actors stage 之后、结束 world mode 之前绘制计划：装备同时画品质光晕与槽位图标，材料画材料图标；目标取插值后的角色位置并上移到腰后，到达脉冲放大/提亮。

```cpp
const LootSuctionPlan suction = loot_suction_.build_plan(
    camera, interpolated_player, width, height);
for (std::size_t i = 0; i < suction.count; ++i) {
    const auto& flight = suction.flights[i];
    static_cast<void>(material_pack_.draw(
        flight.sprite, flight.center, false, flight.scale));
}
```

- [ ] **Step 3: 运行定向 GREEN 和静态自检**

Run Task 1 定向命令，再运行：
```powershell
git diff --check -- src/platform/raylib/combat_renderer.hpp src/platform/raylib/combat_renderer.cpp src/platform/raylib/loot_suction_animation.hpp src/platform/raylib/loot_suction_animation.cpp tests/platform/loot_suction_animation_tests.cpp
```

Expected: 定向用例全绿，diff check 退出码 0。

- [ ] **Step 4: 提交 Task 2**

```powershell
git add -- src/platform/raylib/combat_renderer.hpp src/platform/raylib/combat_renderer.cpp tests/platform/loot_suction_animation_tests.cpp
git commit -m "feat: render loot flying into backpack"
```

