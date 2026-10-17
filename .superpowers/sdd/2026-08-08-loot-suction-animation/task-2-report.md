# Task 2 Report: CombatRenderer 吸入背包绘制接入

## RED

- 新增 `combat_renderer_commits_and_clears_loot_suction` 后，定向编译按预期失败：`CombatRenderer` 缺少 `loot_suction_active_count()`。
- 新增品质光晕断言后，定向编译按预期失败：`LootSuctionFlight` 缺少 `rarity_sprite`。
- 首次未加载项目配置的增量构建缺少 MSVC 标准库 `<cstddef>`；重新执行 `Configure.ps1` 后排除为环境问题，得到上述精确 RED。

## GREEN

- `CombatRenderer::observe_hud()` 在安全帧先推进状态、建立无回执基线，再观察已提交拾取；错误、恢复、故障和跨房间时清空并重置基线。
- `clear_combat_transients()` 清除飞行动画。
- actors 阶段后、仍在 `BeginMode2D` 内绘制品质光晕/装备槽位图标或材料图标，以及角色腰侧到达脉冲；使用一次投影后的屏幕坐标，继承现有 camera offset，不重复投影或叠加震屏偏移。
- 固定 12 槽状态不变；不改掉率、拾取事务、存档、药水规则或 Stage11D `GroundLootView` / 渲染计划。
- 为正式接入增加生命周期用例，并将 CPLAY-021 定向期望数从 6 同步为 7。

## 验证

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 4
$env:ARPG_CPLAY021_LOOT_SUCTION_ONLY='1'
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

结果：`7 cases, 0 failures`。

```powershell
git diff --check -- src/platform/raylib/combat_renderer.hpp src/platform/raylib/combat_renderer.cpp src/platform/raylib/loot_suction_animation.hpp src/platform/raylib/loot_suction_animation.cpp tests/platform/loot_suction_animation_tests.cpp tests/platform/platform_test_main.cpp
```

结果：退出码 0（仅现有 CRLF 工作树提示）。

## Commit

`138668d feat: render loot flying into backpack`

## Concerns

- 本任务按范围未运行 Release 或启动游戏；Windows 视觉验收留给 Task 3。

## 审查修复：同 fixed-step 来源保全与品质回退

### RED

- 新增“previous/current 均无地面源、但已提交装备回执带来源坐标”用例后，精确编译失败：`LootPickupReceipt` 缺少 `position` 与 `slot`。
- 新增等价材料回执来源用例后，精确编译失败：`MaterialPickupReceipt` 缺少 `representative_origins` 与 `origin_valid_mask`。
- 新增稀有装备与深渊宝箱回退颜色区分用例后，现有实现固定使用白色，不能满足断言。

### GREEN

- `LootPickupReceipt` 在 `pickup_candidate` 创建时捕获地面位置和槽位；有效性校验同步约束有限坐标与非 `count` 槽位。
- `MaterialPickupReceipt` 仅作为瞬态 `DungeonSnapshot` 数据，增加固定 `array<Vec3, 14>` 代表来源及 `uint16_t` 掩码。`commit_pending_save` 在删除地面材料前写入：直接拾取使用该序号来源；房间吸取每种材料保留最低序号的一个来源。
- 吸入状态优先使用 previous 的精确地面项；仅在 current 不再拥有同一来源时才回退到已提交回执来源。因此同 fixed-step 生成并拾取仍会播放，跨房间、错误、恢复和故障路径仍清空动画。
- 装备回退颜色改为复用 `GroundLootView` 的纯品质颜色辅助函数；稀有物品与深渊宝箱（当前项目最高的紫色来源，未定义 `legendary` 稀有度枚举）不会退化为同一白色。
- 未改掉率、拾取事务结果、存档/编解码、药水或动态容器；飞行状态仍固定 12 槽。压力快照相等性覆盖新增瞬态字段。

### 定向验证

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 4
$env:ARPG_CPLAY021_LOOT_SUCTION_RUNTIME_ONLY='1'
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
Remove-Item Env:ARPG_CPLAY021_LOOT_SUCTION_RUNTIME_ONLY
$env:ARPG_CPLAY021_LOOT_SUCTION_ONLY='1'
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
Remove-Item Env:ARPG_CPLAY021_LOOT_SUCTION_ONLY
git diff --check
```

结果：runtime 来源发布 `1 cases, 0 failures`；CPLAY-021 吸入动画 `10 cases, 0 failures`；`git diff --check` 退出码 0（仅 CRLF 工作树提示）。未运行全套、Release 或游戏。

### 审查修复 Commit

`11a2650 fix: preserve loot suction pickup origins`

## 发布者回归测试补充

- 新增单一专用 DungeonSession 用例，覆盖直接材料拾取在 pending 与 `not_committed` 时不发布、提交后才发布精确来源；第二次 `not_committed` 保持上一回执不变。
- 同一用例覆盖房间吸取：两个 chaos（ordinal 6/8）与一个 coupon（ordinal 7）提交后，chaos 来源固定取最低 ordinal 6，coupon 保留独立来源。夹具先用真实合法槽位登记，再仅修改地面快照坐标以让代表选择可观察。
- 敏感性：临时移除 `dungeon_transition.cpp` 的来源填充循环后，专用入口精确 RED 于 direct 回执的 `origin_valid_mask`；已立即恢复，未提交该变异。

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests --parallel 1
$env:ARPG_LOOT_SUCTION_MATERIAL_ORIGIN_ONLY='1'
.\out\build\windows-msvc-debug\bin\arpg_dungeon_tests.exe
Remove-Item Env:ARPG_LOOT_SUCTION_MATERIAL_ORIGIN_ONLY
git diff --check
```

结果：恢复后 `1 cases, 0 failures`。本轮始终单线程构建；未运行全套、压力、Release 或游戏。测试追加提交：`cdee48e test: cover material loot origin publication`。
