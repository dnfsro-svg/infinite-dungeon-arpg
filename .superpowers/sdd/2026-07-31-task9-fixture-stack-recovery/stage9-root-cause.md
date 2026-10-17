# Stage 9 独立验证夹具：exit 2 根因调查

调查范围仅限 `E:\game\.worktrees\task9-gate-recovery`，基线为 `ab9f888`。未修改源码、测试、CMake、Git/index 或 HEAD；未运行全套 CTest，也没有重复已知的约 48 秒夹具执行。

## 结论

`arpg_stage9_validation_fixture.exe` 的 exit `2` 是一个**已证实的夹具数据模型失配**，不是新的栈溢出、timeout、房间清理阈值或玩法失败。

`main()` 只有在 `run_real_deep40_fixture(rules)` 返回空值时返回 `2`（`tests/dungeon/stage9_validation_fixture.cpp:224-227`）。该函数对每个候选 root 在创建 `DungeonSession` 后执行：

```cpp
evidence.plan = arpg::test::encounter_plan(session);
if (!has_affix(evidence.plan)) continue;
```

（`stage9_validation_fixture.cpp:150-154`）。但当前生产会话已不再用这个旧的 `RoomEncounterPlan` 保存真实怪物人口：

1. `DungeonSession::stage_current_room_population()` 用 `build_room_monster_plan()` 构建真实的 `RoomMonsterPlan`（`src/dungeon/dungeon_session.cpp:1251-1295`）。
2. `activate_staged_room_population()` 把真实人口放入 `RoomMonsterField`，随后明确把 `encounter_plan_` 清零，只写 `wave_count = 1`（`src/dungeon/dungeon_session.cpp:1309-1338`）。它没有任何 spawn。
3. 测试访问器 `arpg::test::encounter_plan()` 正是返回这个 `session.encounter_plan_`（`tests/dungeon/dungeon_test_support.hpp:42-45`）。
4. 因此 `has_affix(evidence.plan)` 的唯一 wave 的 `spawn_count == 0`，必为 false；每一个 root 都在 154 行 `continue`，不会进入 156 行之后的击杀、掉落、领取或重载检查。最终 194 行返回空值，主函数 227 行返回 `2`。

这也精确解释了已知现象：没有 stdout（所有输出均在 227 行之后），直接运行约 47.57 秒后退出 2，以及 CTest 以 0/1 失败但没有 Application Error 1000。MSVC `/STACK:4194304` 只消除了先前的 0xC00000FD；它不改变这个更靠后的逻辑出口。

## 所有 exit 2 的内部路径

`run_real_deep40_fixture()` 只在下面任一情况后返回空值；没有其他 `return 2` 路径：

| 阶段 | 路径 | 当前判定 |
|---|---|---|
| 候选 root | `make_initial_run_state` fault，或旧 `build_encounter_plan` fault/无 affix（121-134） | 可能跳过，但不是全量失败的必要条件 |
| 会话刚创建 | 旧 `encounter_plan_` 无 affix，154 行 `continue` | **已证实：对所有可创建会话恒成立** |
| 战斗循环 | fault、当前驻留区无活怪、未出现相符掉落、提交 fault，或 4096 tick 耗尽（156-192） | 当前不可达；不需要用于解释这次 exit 2 |

## 与近期契约变化的关系

### 已证实

- `cadbb83` 把房间上限扩到 `kRoomMonsterCapacity = 1152`；当前密度定义允许深渊 1125 只（`src/core/gameplay_limits.hpp:7-18`、`src/dungeon/room_affix.cpp:23-24`）。
- `062173b` 引入真实的 `RoomMonsterField` 分区驻留。空间是 20x20 cell，视口加 halo 的活跃集合小于 128，且 `CombatWorld` 每 tick 按玩家位置同步该集合（`src/combat/room_spatial_grid.hpp:12-39`、`src/combat/combat_world.cpp:642-648`）。
- 当前仓库中已经有适配这一契约的工作范例：Stage 9 千房压力测试用 `arpg::test::room_monster_plan(session)` 读取真实人口，而不是 `encounter_plan()`（`tests/dungeon/dungeon_affix_stress_tests.cpp:221-245`）。

### 已证实但尚未触发的第二层风险

- 原夹具若只替换 154 行而仍保留 `defeat_next_live_monster()`，也不够稳健：该 helper 只杀当前 128-slot active pool 的怪（`dungeon_test_support.hpp:249-265`），不会遍历 `RoomMonsterField` 的其它区域。
- 生产掉落只接受 `spawn_ordinal < kGroundDropCapacity`，而该容量仍是 192（`src/dungeon/dungeon_types.hpp:140`；`dungeon_session.cpp:1916-1939`）。大房间人口可达 1125。因此夹具必须针对真实人口选择一个可掉落 ordinal，或按分区遍历；不能假定任意带 affix 的旧 encounter spawn 都可以领取。
- `f132f27` 的“击杀四分之一即可开门”不会触发本次 exit 2，因为当前代码在首次击杀前就 154 行跳过。不过修复后，夹具不能把 `committing(room_unlock)` 误判为 fault，并应在需要完整清理时明确处理 unlock/clear 的两段提交。

### 未证实、且非本次根因的假设

- 某个具体 root 的掉落 RNG 没命中、玩家在密集怪物中死亡、或某个分区驻留容量 fault，均可能在修复 154 行后暴露；现有 exit 2 无法支持这些推断，因为战斗循环根本未执行。
- 因此本报告不把房间尺寸、怪物数量或 1/4 清理阈值描述为本次直接故障；直接故障是夹具读取了已废弃的缓存平面。

## 最小修复建议（不改玩法）

范围仅限测试夹具，首选修改：

- `tests/dungeon/stage9_validation_fixture.cpp`

具体做法：

1. 删除 `deep40_state_with_affix()` 中对旧 `build_encounter_plan()`/`has_affix(RoomEncounterPlan)` 的候选资格依赖；创建 `DungeonSession` 后改用 `arpg::test::room_monster_plan(session)`。
2. 从真实 `RoomMonsterPlan::monsters` 中选择 `affixes.count > 0` 且 `spawn_ordinal < arpg::dungeon::kGroundDropCapacity` 的目标，并把该真实 blueprint（ordinal、monster id、位置、affix set/score）作为重载前后的比较数据。
3. 用真实 `CombatWorld::defeat_monster` 产生 defeated event，并以分区/目标方式驱动 `RoomMonsterField`；不要重建或伪造 `CombatEvent`。已有 `force_defeat_current_wave()` 展示了分区遍历的正确模型（`dungeon_test_support.hpp:183-229`），但夹具应保留并校验它自己的真实 defeated event。
4. 从实际 `ground_items()` 中选与这个真实 affix ordinal 相符的已生成掉落，再按现有 `request_nearby_pickups` + `commit_pending` 流程领取，最后对 reload 后的真实 `RoomMonsterPlan` 目标与持久化掉落/claim bits 作比较。

若第 3 步没有可复用的“按真实 ordinal 击杀并返回 defeated event”的访问器，唯一合理的附带改动是：

- `tests/dungeon/dungeon_test_support.hpp`

增加一个很小的 test-only helper：在包含目标 ordinal 的 `RoomMonsterField` resident region 中调用真实 `CombatWorld::defeat_monster`，随后返回从 `session.try_pop_combat_event()` 取得的真实 event。它不应注入 `relay_defeated()`，也不应改生产代码。

这可以在**不改玩法规则、黄金值、CTest timeout、B--G 范围或生产源文件**的前提下修复。推荐不要扩大 `kGroundDropCapacity`：那是玩法/持久化容量契约，不是本故障的最小修复。

## 回归选择器与风险

主回归：

```powershell
ctest --test-dir E:\game\.worktrees\task9-gate-recovery\out\build\windows-msvc-debug --output-on-failure -R '^stage9\.validation_fixture\.real_v4_reward_reload$'
```

辅助、只在主回归通过后选择运行：

```powershell
ctest --test-dir E:\game\.worktrees\task9-gate-recovery\out\build\windows-msvc-debug --output-on-failure -R '^stage9\.evidence\.no_injected_defeat_trace$'
```

风险主要在夹具错误地把“真实人口计划”退化为旧 encounter 缓存，或为省事注入 defeat 事件，从而失去 Stage 9 所要求的真实战斗/掉落证据。目标选择限制在 `< 192` 与按分区驱动可避免容量契约和驻留契约造成的偶发失败；不要以增加 timeout 或放宽黄金值掩盖它。

## 调查执行说明

未再执行 47--48 秒夹具或全套 CTest：源码数据流已给出一个无条件路径（`activate_staged_room_population` 清空 `encounter_plan_`，随后 fixture 无条件 `continue`），额外复现不能区分任何候选根因。未运行其他聚焦命令，因为不存在比该静态不变量更有区分价值、同时不重复已知失败的运行。
