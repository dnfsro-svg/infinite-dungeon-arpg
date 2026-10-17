# Task 10 Evidence Fix Report

- 基线：`1acabbb`。
- 真实链：1000 房 Trace 已移除逐怪 `relay_defeated` 注入；改为 `CombatWorld::defeat_monster → DungeonSession::relay_combat_events`，记录真实 defeat payload、经验、掉落与领取，并在每 37 房 V4 重建。
- fixture：深40 root 2，真实 score 17 击杀、掉落 ordinal 2、领取位图 `4,0,0`；V4 state、计划及词缀、掉落、领取比较均为 1。完整输出：[task10-fixture-full.log](evidence/stage9/task10-fixture-full.log)。
- 正式窗口：新增正式 host 的测试入口及第 60 个已提交帧退出开关；捕获在专用 save 目录，截图 [03-formal-game-submitted-frame.png](evidence/stage9/03-formal-game-submitted-frame.png) 经非白画布验证。
- 未完成项：无。未修改 `src/persistence`，未进入 Stage 10。

待提交前完整运行 dungeon、platform、game 相关测试和 `git diff --check`。
