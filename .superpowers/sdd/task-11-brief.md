## Task 11: 完成 raylib 6.0 门标、规则警告与奖励提示

**Files:**

- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/dungeon_view_math.hpp`
- Modify: `src/platform/raylib/dungeon_view_math.cpp`
- Modify: `tests/platform/dungeon_view_math_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Produces:** 四向门深渊图标、房内危险/规则 HUD、环境预警范围、待领取/待生成计数和二次确认文案。

**Consumes:** `DungeonSnapshot`/`CombatSnapshot`；不得在 renderer 重新推导 seed、规则、伤害或奖励。

- [ ] 先写纯 view-math 测试：普通门无图标、四方向各自图标、门仍 locked/open 原样式、三个环境规则 warning/active 颜色与半径、确认提示计数与方向、没有规则详情泄漏到未进入的门。

- [ ] `draw_doors` 读取 `snapshot.abyss_doors[index]`，在箭头旁绘制统一紫红菱形图标，只表示目标房是深渊。

- [ ] `draw_abyss` 保留现有房间紫色脉冲，并增加当前危险等级/规则名称；环境 warning 使用 snapshot 的 locked center/radius 绘制地面圈，不调用 `GetRandomValue`。

- [ ] HUD 显示 `ABYSS LOW/MED/HIGH`、规则名、`Reward pending X / unpicked Y`；第一次触门显示 `再次进入同一扇门将放弃全部剩余奖励`，洞口确认则显示 `再次按 E 将放弃剩余奖励并下层`，位置、方向/transition 或 revision 失效后立即消失。

- [ ] 运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
ctest --preset windows-msvc-debug -R "platform.units|architecture" --output-on-failure
```

预期：platform 与所有 architecture 边界测试通过；core/combat/dungeon/persistence 均无 raylib 依赖。

- [ ] 提交：

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render abyss combat and reward state"
```

---

