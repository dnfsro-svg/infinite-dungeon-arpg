# Stage 9 validation fixture exit 2：最小修复设计

基线：`ab9f888b96df2b5edc117dfbf6ddbd67b7c20f9e`。本设计只修复独立夹具对当前真实人口/驻留/战斗/掉落契约的适配；不修改生产代码、玩法、黄金值、CTest timeout 或 B--G 范围，也不使用 `relay_defeated()` 或手工构造/注入 `CombatEvent`。

## 结论与推荐文件范围

推荐只改两个测试文件：

1. `tests/dungeon/stage9_validation_fixture.cpp`
2. `tests/dungeon/dungeon_test_support.hpp`

不能把“只改 `stage9_validation_fixture.cpp`”作为可靠方案。夹具虽然能通过现有 `mutable_combat_world()` 和公开的 `RoomMonsterField::synchronize_active_region()` 把目标 cell 驻留，再调用 `defeat_next_live_monster()`，但后者只杀 active pool 中遇到的第一个活槽（`dungeon_test_support.hpp:249-265`），不能按 room ordinal 指定目标。若为此挑选某 cell 中的最低 ordinal，并依赖 `MonsterPool::spawn()` 的当前装槽顺序，测试就会把非契约化的槽顺序当作证据。`CombatWorld::defeat_monster()` 又是 private（`src/combat/combat_world.hpp:150-154`），因此最小且可审查的做法是在已经被 `CombatWorld` 与 `DungeonSession` 设为 friend 的 `DungeonSessionTestAccess` 中增加一个按 ordinal 的真实击杀 helper。

不改：生产源文件、CMake、`dungeon_affix_stress_tests.cpp`、timeout、容量常量、黄金输出及玩法阈值。

## 精确数据流

### 1. 从真实人口选择一个确定会尝试产出装备掉落的 affix 目标

把 `deep40_state_with_affix()` 改为只负责构造合法 deep-40 `DungeonRunState`（建议改名 `deep40_state()`）：

- 保留 `make_initial_run_state()` fault 检查和当前房间 index/depth/floor index/seed 设置（当前 `stage9_validation_fixture.cpp:121-129`）。
- 删除旧 `build_encounter_plan()` 与 `has_affix(RoomEncounterPlan)` 资格判断（当前 130-134）。旧 plan 不再是生产人口。

在 `run_real_deep40_fixture()` 中创建 `DungeonSession` 后：

1. 取 `const RoomMonsterPlan* plan = arpg::test::room_monster_plan(session)`（访问器现有实现为 `dungeon_test_support.hpp:54-60`、wrapper 为 797-800）。要求非空，且 `monster_count > 0`。
2. 只扫描 `ordinal < min(plan->monster_count, kGroundDropCapacity)`；当前地面装备容量是 192（`src/dungeon/dungeon_types.hpp:140`），claim bits 正好是 3 x 64（`src/items/item_types.hpp:74-81`）。
3. 每个候选必须同时满足：
   - `source.spawn_ordinal == ordinal`；sealed plan 已保证该不变量（`src/combat/room_monster_field.cpp:54-59`），夹具仍显式检查；
   - `source.affixes.count > 0`；
   - `monster_affix_danger_score(source.affixes) > 0`；
   - 使用生产相同的 chance domain/RNG 派生判定此次 affix roll 会命中：`roll < affix_drop_chance_bp(score)`。
4. drop-roll selector 只用于缩小候选，不作为掉落证据。可最小借用当前可信实现 `tests/dungeon/dungeon_affix_reward_tests.cpp:18-36` 的 `kDropChanceDomain`、`drop_roll()`、`affix_drop_hits()`；它与生产 `dungeon_session.cpp:33-44, 1933-1939` 一致。不要复制 slot/content/item-id 生成逻辑，最终必须检查生产实际生成的 `ground_items()`。
5. 记录一个紧凑的 `MonsterTrace`，不要复制 `RoomMonsterPlan`：`ordinal/spawn_ordinal`、`id`、`initial_position`、`home_cell`、`roaming_leash_cells`、完整 `MonsterAffixSet` 和 `danger_score`。另记录 plan 的 `monster_count/threat_total/generator_version/blueprint_hash` 供输出。

如果当前 root 没有符合条件的真实目标，丢弃该 session 并继续下一个 root；不要降级到无 affix、ordinal >= 192 或旧 encounter plan。

### 2. 让目标成为 resident，并通过真实 CombatWorld 产生 event

在 `DungeonSessionTestAccess` 内、现有 `defeat_next_live_monster()` 附近增加：

```cpp
static std::optional<combat::CombatEvent> defeat_room_monster_by_ordinal(
    dungeon::DungeonSession& session,
    combat::MonsterOrdinal ordinal) noexcept;
```

并在 namespace wrapper 区（现有 `defeat_next_live_monster` wrapper 为 741-743）增加同签名 inline wrapper。若采用该签名，应在 test-support header 显式包含 `<optional>`。

helper 的唯一语义：把真实 plan 中指定 ordinal 的怪物切入 active residency，调用真实 `CombatWorld::defeat_monster()`，让 `DungeonSession::relay_combat_events()` 执行生产的 defeat-ledger、掉落和事件转发，然后返回从 session combat-event queue 取出的真实 defeated event。helper 不得接收 monster id、affix score、位置或 reward payload 参数，避免调用者伪造 payload。

精确步骤：

1. 要求 `session.phase_ == RoomPhase::combat`、`session.combat_` 存在、world/field 无 fault、ordinal `< field->plan().monster_count`。
2. 从 `field->plan().monsters[ordinal]` 读取 blueprint；要求 `spawn_ordinal == ordinal` 且 `home_cell < 400`。
3. 从 `home_cell` 算出 `column = home_cell % room_spatial::columns`、`row = home_cell / room_spatial::columns`，调用一格区域：
   `{column, 1U, row, 1U}` 的 `field->synchronize_active_region()`。
   一格最多 3 只（`room_spatial_grid.hpp:12-30`），远低于 128 active slots，且包含目标的 home cell。
4. 用 `field->resident_handle(ordinal)` / `field->active_runtime(ordinal)` 取得目标；要求 handle 有效、runtime active、hp > 0、reaction 非 defeated、`monster_ordinal/id/spawn_ordinal/affixes` 与 blueprint 一致。
5. 延续现有 test helper 的真实击杀方式（`dungeon_test_support.hpp:249-263`）：把该 runtime 的 hp 置 0，然后调用 `world.defeat_monster(handle->index, AttackId::j1, true)`。不要调用 `relay_defeated()`，不要向 `world.events_`/`session.combat_events_` push，也不要构造一个 defeated `CombatEvent`。
6. 调用 `session.relay_combat_events()`。这会先消费真实 defeat ledger、更新 room defeated bits/count（`dungeon_session.cpp:1542-1592`），再对真实 defeated event 执行 `claim_defeat_reward()`/`roll_ground_drop()` 并转发原 event（1595-1614）。
7. 从 `session.combat_events_` 逐个 pop。允许并丢弃同次死亡产生的非-defeated affix warning；只接受恰好一个与目标匹配的 defeated event。将该真实 event 放进 `std::optional` 返回；不生成替代 event。

helper 成功前必须验证：

- `world.fault() == CombatFault::none`，session 未 fault；
- field defeated count 和 session room defeated count 均恰好 +1；相应 room defeat bit 已置位；
- event 的 `kind == defeated`；
- `target_ordinal == ordinal`；
- `spawn_ordinal == blueprint.spawn_ordinal == ordinal`；
- `monster_id == blueprint.id`；
- `affix_score == monster_affix_danger_score(blueprint.affixes) > 0`；
- `reward_eligible == true`；
- 不存在第二个 defeated event。

`CombatWorld::defeat_monster()` 当前确实从 runtime 形成 payload、标记 field defeated、写 defeat ledger，并发出真实 event（`src/combat/combat_world.cpp:1332-1396`）；因此返回值不是伪造证据。

### 3. 生产掉落、精确领取、提交

`run_real_deep40_fixture()` 对选中的 target 执行：

1. 先 `session.tick({})` 一次，把 locked 变为 combat（`dungeon_session.cpp:326-331`），并清空 room-enter/combat-started dungeon events。此后调用新 helper；不需要为了击杀再跑 4096 tick。
2. 严格比较 helper 返回的真实 event 与 `MonsterTrace`。
3. 读取 `arpg::test::ground_items(session)[target.spawn_ordinal]`（访问器现有定义 716-720、wrapper 1080-1084），要求：active、`drop_ordinal` 等于目标 ordinal、source 是 `monster_drop`、item 合法。selector 命中但生产 ground item 不存在时，该 root 失败；绝不能注入 ground item。
4. 保存实际 `DropTrace`。用现有 `set_player_position(session, drop.position)`（396-400、880-884）移动玩家。
5. 推荐调用公开的 `session.request_pickup(target.spawn_ordinal)`，并要求结果是 `accepted`，而不是泛化的 `request_nearby_pickups()`。原因是 nearby 流程先扫描所有 health potion（`dungeon_transition.cpp:1622-1645`），同次真实击杀若掉了血瓶，第一次 nearby 请求可能提交的是血瓶而不是目标装备。精确 `request_pickup()` 仍走真实距离、item 校验和 pending-save 流程（1244-1293），只是消除无关拾取顺序。
6. 用现有夹具 `commit_pending()`（当前 `stage9_validation_fixture.cpp:137-143`）提交 pending save；额外要求 pending kind 是装备 pickup、提交后 phase 恢复且非 fault。
7. 要求 `claimed_drop_bits[target.ordinal]` 已置位、ownership item count 恰好增加 1、按 id 找到的 owned item level/rarity 与实际 ground drop 一致，再保存 `claimed_state`。

整个路径只击杀一个目标，因此不会触及“一季度击杀开门”的 room unlock 阈值，也无需改变或特殊提交 room-unlock/room-clear；玩法 B--G 保持原样。

### 4. 重放与 reload 验证

保留当前 main 的“第二次运行 + encode/decode + reloaded session”结构（`stage9_validation_fixture.cpp:251-274`），但改为真实 plan：

1. `replay = run_real_deep40_fixture(rules)` 仍应选到同 root/target，并产生相同实际 drop；比较 target ordinal、blueprint trace、event payload 及 DropTrace。
2. `original{rules, fixture->claimed_state}`，对其 `stable_state` 进行 `encode_checkpoint()`/`decode_checkpoint()`，再构造 `reloaded{rules, decoded.state}`。
3. `state_equal` 保持使用 `same_run_state()`。
4. 从 original 与 reloaded 取非空 `room_monster_plan()`；用生产已有 `arpg::dungeon::room_monster_plan_equal_fields()` 做完整 plan 比较。该函数比较 cell offsets/counts、monster count、threat、generator version、blueprint hash，以及每个 blueprint（`room_monster_plan_builder.cpp:655-672`）；夹具需包含 `dungeon/room_monster_plan_builder.hpp`。
5. 另外用 target ordinal 在 original/reloaded 的 plan 中取 blueprint，与保存的 `MonsterTrace` 逐字段比较：id、spawn ordinal、position、home cell、leash、affix set、danger score。
6. `drop_equal` 比较 replay 的实际 drop，并确认 reloaded ownership 中存在同 id/item level/rarity；`claim_equal` 确认 3-word claimed bits 完全相等且目标 bit 为 1。

注意：普通 `DungeonRunState` v4 checkpoint 持久化的是 ownership/claim，不是当前 active residency 或地面瞬态。重载后 plan 由同一 room descriptor/generator 重新生成；不要错误断言“目标仍处于 defeated runtime”或“ground item 仍 active”。本夹具应验证的是真实 blueprint 可重建、已领取 item 和 claim bit 持久化。

## `stage9_validation_fixture.cpp` 的精确函数改动

- includes：增加 `core/deterministic_rng.hpp`、`dungeon/room_monster_plan_builder.hpp`；移除不再需要的旧 encounter-director include/alias。
- `FixtureEvidence`（当前 33-42）：移除不可再使用的 `RoomEncounterPlan plan`，增加紧凑 `PlanTrace`/`MonsterTrace target`；保留真实 `CombatEvent defeat` 仅作为 helper 返回 event 的值拷贝。
- `same_plan_and_affixes(RoomEncounterPlan, ...)`（66-99）：删除；新增 `trace_matches_blueprint()`。完整 plan equality 交给生产 `room_monster_plan_equal_fields()`。
- `has_affix(RoomEncounterPlan)`（111-119）：删除。
- `deep40_state_with_affix()`（121-135）：改为 `deep40_state()`，不再构建/筛选旧 encounter plan。
- 新增 `drop_roll()` / `affix_drop_hits()`：只借用当前测试中与生产一致的 chance selector。
- 新增 `select_target(real plan, state)`：严格执行 `< kGroundDropCapacity`、affix、正 score、drop-roll 命中条件并生成 trace。
- `run_real_deep40_fixture()`（145-195）：删掉 `encounter_plan()`、`has_affix()`、4096 tick 和 `defeat_next_live_monster()` 循环；改为“真实 plan -> 单目标 selector -> 进入 combat -> 新 helper -> 实际 ground item -> 精确 pickup -> commit”。
- `print_plan(RoomEncounterPlan)`（197-220）：改为输出 plan summary 与唯一真实目标 trace/affix，不遍历或复制 1152-entry plan。
- `main()`（224-280）：输出 target；reload comparison 改用真实 plan full equality + target trace equality；保留 state/drop/claim consistency 最终 gate。

## 可复用代码与边界

### 可直接复用

- 真实 plan 访问：`dungeon_test_support.hpp:54-60, 797-800`。
- 分区同步/真实 defeat 的现成结构：`force_defeat_current_wave()`，`dungeon_test_support.hpp:183-229`；只借用 synchronize + private real defeat + relay 的形态，不借用其“全杀并丢弃事件”行为。
- 当前真实 plan 消费示例：`dungeon_affix_stress_tests.cpp:221-245`。
- 全字段 plan reload equality：`dungeon_wave_tests.cpp:353-382` 和生产 `room_monster_plan_builder.cpp:655-672`。
- drop chance selector：`dungeon_affix_reward_tests.cpp:18-36`；更完整但本修复不应整段复制的参考是 `tests/platform/stage11d_loot_formal_game_validation.cpp:85-114`。
- 真实 event payload 与 plan 对照：`dungeon_affix_stress_tests.cpp:247-272`；该文件后续 `relay_defeated()` 路径不可信，不能借用。
- pickup/commit：当前 fixture 137-143、173-189；精确 request API 在 `dungeon_session.hpp:125-126`。

### 零分配与容量

- `RoomMonsterPlan` 是固定 1152-entry、显式不可 copy/move（`room_monster_plan.hpp:25-42`）。只持有 session-owned `const*`，作用域不越过 session；不得把 plan 塞入 `FixtureEvidence` 或作为局部值复制。紧凑 trace 避免重新制造栈压力。
- 新 helper 本身只使用 fixed/stack 值和 `std::optional`，不分配。生产 session 构造仍用 `new (nothrow)` 创建 field/environment（`dungeon_session.cpp:1251-1261`），装备领取仍可能因 ownership vector 分配失败而 rejected；夹具应失败/换 root，不能伪造成功。
- 一格 region 最多 3 resident；active pool 128。每次只击杀 1 个，defeat ledger 128、combat event queue 512 均有宽裕；helper 每次立即 relay/drain，不能积压。
- ground equipment 与 claimed bits 硬边界都是 192；只接受 `< 192`。不要扩大 `kGroundDropCapacity`。
- plan 总人口最高 1152，room defeat bits 是 18 x 64；目标 ordinal 同时满足 plan/room-progress 和 drop/claim 两套边界。

### 存档边界

- pending pickup 必须通过真实 `request_pickup` 形成并按 expected generation 提交；不得直接改 ownership、claimed bits 或 stable state。
- encode/decode 只针对提交后的 durable stable state。真实 plan 通过 room seed + generator version 重建；claim/item 持久化，resident/defeated/ground transient 不在本次 v4 stable-state reload 断言内。

## 历史/非祖先实现调查

在当前可达 refs、reflog 与 unreachable commits 中检查了对 `stage9_validation_fixture.cpp` / `dungeon_test_support.hpp` 的非祖先修改。`6ffa3cf`、`78da4bf`、`a83531f`、`37a0848`、`2eabc1b`、`77f1c58` 以及 unreachable `d9ecd93`、`dea4071`、`dccc515` 的 Stage 9 fixture 仍使用旧 `encounter_plan()` + `defeat_next_live_monster()`；`d8ac236` 只有更老的 `relay_defeated()` 注入路径。没有发现可信的“真实 RoomMonsterPlan + 按 ordinal resident + CombatWorld real defeat”完整适配可直接移植。

可借鉴的最小、当前契约一致片段只来自当前 HEAD：

- `force_defeat_current_wave()` 的 region synchronization 与真实 `defeat_monster()` 调用形态；
- `room_monster_plan()` 访问器；
- `dungeon_affix_reward_tests.cpp` 的 drop chance selector；
- `room_monster_plan_equal_fields()` 的 reload comparison。

不要借鉴当前 `dungeon_affix_stress_tests.cpp:301-306` 或历史提交中的 `relay_defeated()`。

## RED / GREEN 命令与预计时长

工作目录均为 `E:\game\.worktrees\task9-gate-recovery`。

### RED（修改前，已知失败，不必重复多跑）

```powershell
cmake --build out/build/windows-msvc-debug --config Debug --target arpg_stage9_validation_fixture --parallel
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R '^stage9\.validation_fixture\.real_v4_reward_reload$'
```

预期：构建若无源码变化通常少于 10 秒；fixture 约 47--48 秒失败，CTest 非零（内部 executable exit 2），且没有 fixture stdout。此 RED 已由根因报告记录；实现者只需在动手前保留一次可审查记录，不应反复消耗 48 秒。

如需直接确认内部 exit：

```powershell
& .\out\build\windows-msvc-debug\bin\arpg_stage9_validation_fixture.exe
$LASTEXITCODE
```

预期输出最后一行 `2`，约 47--48 秒。

### GREEN（实现后）

```powershell
cmake --build out/build/windows-msvc-debug --config Debug --target arpg_stage9_validation_fixture --parallel
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R '^stage9\.validation_fixture\.real_v4_reward_reload$'
```

预期：增量编译约 5--20 秒；测试 exit 0。由于 selector 先挑中会命中的真实 target 且只做一次真实 defeat，预计运行显著低于旧 48 秒，通常应在 1--10 秒量级；第一次实测是权威值，不得为此调整 timeout。

随后做同文件范围的静态反注入检查：

```powershell
rg -n "relay_defeated\s*\(|events_\.try_push\(|CombatEventKind::defeated\s*;" tests/dungeon/stage9_validation_fixture.cpp
```

预期无匹配。新 helper 中允许读取 `CombatEventKind::defeated` 作校验，但不得赋值该 kind 或 push event；该部分需在 diff review 中核对真实调用链。

最后可运行既有证据 guard：

```powershell
ctest --test-dir out/build/windows-msvc-debug -C Debug --output-on-failure -R '^stage9\.evidence\.no_injected_defeat_trace$'
```

耗时约 0.03 秒。重要：在 `ab9f888` 上它当前独立失败，因为 guard 扫描的 `dungeon_affix_stress_tests.cpp:301` 仍调用 `relay_defeated()`；这不是本 exit-2 两文件修复能变绿的项，也不能通过放宽 guard 来掩盖。应把主 fixture GREEN 与该既存 1000-room trace RED 分开记录；若上层任务要求整个 Stage 9 gate 全绿，必须另立任务把 stress trace 改成真实战斗，不应扩大本修复。

## 最大风险与审查要点

最大风险不是 timeout，而是证据再次失真：看似选中真实 blueprint，实际却用 slot 顺序杀了别的怪，或用注入 event/ground item 绕过 `CombatWorld -> defeat ledger -> DungeonSession relay -> roll_ground_drop -> pending save`。审查时从 helper 的唯一入口沿上述链路核对，并要求 event/ground/claim/reload 四处都指向同一个 `spawn_ordinal < 192`、同一 monster id 和同一 affix score。

次要风险：复制 1152-entry `RoomMonsterPlan` 重新引入栈/不可复制问题；把 transient ground/defeat 状态误当 v4 durable state；用 nearby pickup 意外先领取血瓶；以及把既存 stress-trace guard RED 误归因于本 fixture 修复。上述设计分别通过紧凑 trace、明确存档边界、精确 `request_pickup()` 和分离 gate 结果规避。
