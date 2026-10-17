# Task 1 Independent Code Review

## Verdict

APPROVED

审查范围：`ab9f888b96df2b5edc117dfbf6ddbd67b7c20f9e..ccc1ed01c00b3eb626722cb109ca13f2e50f49a9`。

## Critical

无发现。

## Important

无发现。

## Minor

无发现。

## Review basis

- 提交范围严格限于 `tests/dungeon/dungeon_test_support.hpp`、`tests/dungeon/stage9_validation_fixture.cpp` 和 Stage 9 计划。
- 新 helper 通过 `RoomMonsterField::synchronize_active_region()`、`resident_handle()` 和 `active_runtime()` 获得真实 resident runtime，只将该 runtime 的 HP 置零，随后调用真实 `CombatWorld::defeat_monster()` 和 `DungeonSession::relay_combat_events()`，并返回从 session 队列取出的真实 defeated event。
- 本次新增代码未使用 `relay_defeated`、伪造或直接 push 事件、直接 ledger 写入、ground item 注入或 ownership/claim 直接修改。
- 选靶只扫描 `min(plan.monster_count, kGroundDropCapacity)`，RNG 的 ordinal stream、chance domain 和 10000 基点比较与生产 `roll_ground_drop()` 一致；`RoomMonsterPlan` 仅以 session-owned pointer/reference 访问，没有栈复制。
- 事件位置假设成立：首次 `session.tick({})` 仅将 phase 从 locked 切换到 combat；helper 随后把目标 home cell 单独同步为 active region，因此目标 runtime 的 defeat position 仍是 blueprint initial position。两次 fixture 采用相同初态和相同操作序列，tick 比较也是确定的。
- 目标 ordinal 为 `uint16_t`，候选上限为 192；home cell 在转换 row/column 前验证小于 400，bit word 在移位前验证容量，未发现数组越界或整数截断风险。
- 证据链将同一 target ordinal 贯穿真实 defeated event、实际 ground slot、公开 `request_pickup()`、真实 pending-save commit、claim bit、唯一新增 owned item以及 codec reload；reload 后比较完整 plan fields、目标 trace、全部 claim words和拥有物品的 id/level/rarity。

按任务约束，本审查未运行构建或测试，也未审查 Stage 10 未提交改动。
