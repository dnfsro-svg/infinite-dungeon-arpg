# Stage 9 整卡最终审查

## Verdict

APPROVED

审查范围：`ab9f888b96df2b5edc117dfbf6ddbd67b7c20f9e..ccc1ed01c00b3eb626722cb109ca13f2e50f49a9`。

## Critical

无发现。

## Important

无发现。

## Minor

无发现。

## 最终审查依据

- 规格完成度：`tests/dungeon/dungeon_test_support.hpp:268-365` 实现了按 ordinal 定位真实 resident runtime、调用真实 `CombatWorld::defeat_monster()`、经 `DungeonSession::relay_combat_events()` 处理并返回真实 relayed defeated event 的窄测试 seam；`tests/dungeon/stage9_validation_fixture.cpp:188-303` 将同一目标贯穿真实 blueprint、defeat、ground item、公开 pickup、提交后的 claim/ownership。
- 测试不存在可见假阳性路径：fixture 只有在真实掉落有效、pickup accepted、pending save 为 `loot_pickup` 且提交后回到 combat、claim bit 置位、owned item 恰好新增一个时才返回证据（`tests/dungeon/stage9_validation_fixture.cpp:224-303`）；最终退出 0 还要求重复执行的 plan/target/defeat/drop trace 一致、codec round-trip state 相等、完整重建 plan fields 相等、全部 claim words 相等且 reload 后仍拥有目标物品（`tests/dungeon/stage9_validation_fixture.cpp:357-402`）。
- helper 失败状态不会使本 fixture 误通过：streaming/验证阶段失败直接返回空；真实 defeat 发生后的任何后验失败同样返回空，而唯一调用点随即终止本次 fixture 并销毁 session（`tests/dungeon/stage9_validation_fixture.cpp:265-270`），不存在带着部分变更继续生成成功证据的路径。当前调用前已清空启动事件队列（`tests/dungeon/stage9_validation_fixture.cpp:258-263`），未发现队列复用导致的实际可靠性问题。
- API、边界与生命周期安全：plan 指针均由存活的 session/field 持有且仅在其生命周期内解引用；持久证据只复制紧凑 trace。候选上限为 `min(monster_count, kGroundDropCapacity)`（`tests/dungeon/stage9_validation_fixture.cpp:188-207`）；helper 在 blueprint、home cell、bit word、resident handle/runtime 上均先验证后访问（`tests/dungeon/dungeon_test_support.hpp:275-324`），未发现越界、悬空指针或句柄失效窗口。
- RNG 与生产掉落判定一致：fixture 的 ordinal stream、chance domain、10000 基点比较和 `affix_drop_chance_bp()` 调用（`tests/dungeon/stage9_validation_fixture.cpp:173-185`）与生产 `roll_ground_drop()` 相同；预选只用于寻找确定会掉落的真实目标，后续仍验证实际 production ground slot，未伪造掉落。
- 代码质量与可维护性：新增 helper 不接受 caller-supplied id/position/affix/reward payload；fixture 的 plan/monster/drop/defeat trace 比较职责清楚，未复制大型 `RoomMonsterPlan`，也未保留失效引用。
- 提交范围与禁止项：range 仅包含 Stage 9 计划、`tests/dungeon/dungeon_test_support.hpp`、`tests/dungeon/stage9_validation_fixture.cpp`；新增代码中未发现 `relay_defeated()`、事件直接 push、defeat ledger 直接 append、ground item 注入或 ownership/claim 直接写入。`git diff --check` 通过。Stage10 fixture/plan 未纳入审查或提交；Stage10 fixture 当前 diff 哈希仍为报告记录的 `8bcc00efda5fc9fd4c6dd030800c3875e8853947`。
- 按审查任务要求，本轮未运行构建或测试。执行报告记录的串行目标构建 exit 0 与精确 selector 1/1 通过，和代码中的 CTest 注册/退出条件一致；未把独立已知 RED 或全量 CTest 误报为已验证。
