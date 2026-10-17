# Stage 16 Task 8 最终验收报告

> 本文件是 Stage 16 的最终审计记录；未改动历史 `task-8-report.md`。

## 验收资产与覆盖范围

- `tests/dungeon/stage16_loot_reinforcement_simulation.cpp`：确定性统计与事务验收。材料掉落分别在代表性危险度执行每档一百万次抽样；覆盖普通/深渊材料权重、四种强化券边界、材料与货币边界、强化失败规则、存档重载和饱和。
- 该模拟还以真实 `DungeonSession` 对 +4、+7、+10、+12 各执行 75,000 次强化事务，并冻结 Task 1 房间/装备/深渊流指纹 `15972885342817959559`，验证插入材料随机流不会扰动既有流。
- `tests/platform/stage16_loot_reinforcement_game_validation.cpp`：真实 raylib 6.0 生产路径验收。它构造合法 V7 存档、经 `SaveStore` 写入并由 `DungeonRuntime` 加载；不注入测试专用战斗或拾取状态。

## 真实图形场景

1. 确定性筛选首波最近怪物必掉材料的种子，然后在真实房间战斗中击杀，令材料实际落地。
2. 落地材料由 `CombatRenderer`/房间渲染器在 1280×720 实际画面中截图；再只通过正常移动进入自动拾取范围，确认默认自动拾取提交。
3. 在真实背包界面选择材料，完成混沌工艺、+12 强化券，并打开生产确认界面。
4. 以真实确认点击完成 `+12 -> +13` 的预定失败；严格断言目标装备已从物品所有权中移除（`destroyed=true`）。
5. 重新启动 `DungeonRuntime` 并从 `SaveStore` 读取，断言拾取材料持久化、消耗品状态正确、被销毁装备仍不存在。

运行状态文件位于：
`out/build/windows-msvc-debug/tests/platform/stage16 loot reinforcement evidence/stage16-run/stage16-loot-reinforcement-state.txt`

其中最终结果为 `result=pass`，并记录：

- `ground_material_world_render=pass`；落地距离 `1.7517`；
- `material_pickup=pass generation=2 material=9`；
- `chaos_craft=committed`、`coupon_12=committed`、`production_confirmation_ui=visible`；
- `reinforcement_12_to_13=failed_destroyed`、`save_store_restart=pass`。

截图均为真实 Room/Inventory Renderer 输出，路径为：

- `.../stage16-run/stage16-ground-material-1280x720.png`（75,333 bytes）
- `.../stage16-run/stage16-confirmation-1280x720.png`（63,338 bytes）
- `.../stage16-run/stage16-complete-1280x720.png`（55,968 bytes）

## 最终命令与结果

- `ctest --test-dir out/build/windows-msvc-debug -L stage16 --output-on-failure`：**4/4 通过，0 失败，46.63 秒**；四项分别为 material units 0.09 秒、crafting units 0.01 秒、统计模拟 45.83 秒、真实 raylib 0.80 秒。
- `ctest --test-dir out/build/windows-msvc-debug -R '^dungeon\\.units$' --output-on-failure`：**272 cases，0 failures，196.40 秒**（exit 0）。
- `./scripts/Build.ps1 -Preset windows-msvc-release`：**exit 0**；`out/build/windows-msvc-release/bin/arpg_game.exe` 已生成（1,579,520 bytes，2026-07-20 20:09:20 EDT；增量复跑为 `ninja: no work to do.`）。

用户可见规则与验收入口已写入 `README.md` 和 `docs/validation/stage16-loot-currency-reinforcement.md`；实现与验证提交为 `5a57f7c test: harden stage 16 loot reinforcement validation`。

## 基线例外

Stage 9 `stage9.formal_game.capture_after_present` 的图形栈溢出是先于 Stage 16 的已知基线问题；它未被纳入 Stage16 标签，也没有被误报为通过。
