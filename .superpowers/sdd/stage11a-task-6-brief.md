# Stage 11-A Task 6：普通房 death_retreat 原子事务

## 基线与范围

- 基线：`20403a76181817c4c7f7407fbea043fce8a20233`。
- 主要修改：`src/dungeon/dungeon_types.hpp`、`dungeon_session.hpp/.cpp`、`dungeon_transition.cpp`、`tests/dungeon/dungeon_death_lifecycle_tests.cpp`、`dungeon_transaction_tests.cpp`。
- 原子 receipt 必需前置：允许最小修改 `src/platform/raylib/dungeon_runtime.cpp` 以回显 pending kind，并补相关 platform test。
- 双槽 death 比较必需前置：允许修改 `src/persistence/save_slot.cpp` 与 persistence tests，使同代比较覆盖 death_sequence 和 DeathCheckpoint 全字段。
- 只实现普通房死亡的 `death_retreat`。深渊死亡继续保留 Stage10 `abyss_fail` 路径，Task7 才合并；不得实现 death_continue/UI。

## 新稳定/事务类型

- `PendingSaveKind::death_retreat`。
- `DungeonEventKind::death_detected`、`death_retreat_committed`。
- `PendingSave` 保存 `std::optional<combat::CombatDeathSnapshot>`（或等价固定缓存）；非 death kind 必须为空，death_retreat 必须存在且与 next_state.death 逐字段一致。
- `PendingSaveResult` 在末尾增加可选 kind echo，保持既有三字段 aggregate 调用源兼容；death receipt（committed/not_committed/indeterminate）必须带 kind 且等于 death_retreat，否则 fault。Runtime 在 commit 前冻结 pending kind，并回填 result。

## 检测与同 tick 优先级

- 测试必须通过实际 `CombatWorld::apply_player_damage` 生产路径生成 `CombatDeathSnapshot`；可使用既有测试 helper 取得 CombatWorld，但禁止直接写 death snapshot、stable death 或 pending save。
- `tick()` 顺序保持：Combat tick -> relay -> durable defeat detection -> room clear/wave/reward 判断。
- 玩家与最后怪物同 tick 死亡时，death 优先：不得 settle 当前房经验、不得发布 room_cleared/exits/reward；任何本 tick transient drop/rolled bit/pending exp 最终丢弃。
- `handle_player_defeat()`：普通房只调用 `prepare_death_retreat()`，不再 reset；已开始深渊仍走既有 abyss_fail，留给 Task7。

## prepare_death_retreat 原子构造

前置：非 fault、无 pending save、普通 combat 房、Combat death snapshot exactly once、stable death none。

1. 先检查 `commit_generation`、`death_sequence`、current room index 上溢；任何 fault 在 emplace 前发生，stable/transient 不部分修改。
2. 冻结 CombatDeathSnapshot 到局部。
3. `next = stable_state`（异常/分配失败 fail closed）；只在局部：
   - generation +1；death_sequence +1；
   - 保留等级/经验/技能点/passive/equipment/inventory/已提交 abyss history；
   - biases 全零；current_room 保持死亡锚点 index/seed/depth/floor/entry/ecology/hole，但 `is_abyss=false`；
   - `last_transition=death_retreat`、`last_direction=none`；
   - 用 Task3 `make_death_retreat_target(stable_state, next death_sequence, rules)` 生成并冻结完整 target；
   - 用 production Combat snapshot + death room + target 构造 DeathCheckpoint。
4. 用 Task5 session/dungeon validation 等价关系自检 next，全部成功后一次性 emplace pending save/cache；stable death_sequence 和 stable death 在提交前不变。
5. phase=committing，Combat 保留且冻结；snapshot 显示 saving=true/can_continue=false，只来自 pending next death。
6. 发 `death_detected` exactly once。事件队列容量不足时 fail closed，不发布可保存 pending。

## receipt 状态机

- 所有 death receipt 先校验 phase/pending kind/cache consistency/result kind echo。
- `not_committed`：必须验证 kind；不 fault，发 save_failed，清 pending/cache，stable 不变，phase 回到可自动重试的 frozen combat phase。Combat death snapshot、target 输入和 transient 保持到下 tick；下 tick重建的 next/death/target逐字段相同。不得增 death_sequence。
- `indeterminate`：fault，保持不可玩。
- `committed`：generation、kind、完整 verified_state 必须 exact；否则 fault。成功时发布 next stable，清 pending/cache、Combat、ground、rolled bits、pending room exp/临时奖励，phase=death_pending，发 `death_retreat_committed` exactly once。Snapshot saving=false/can_continue=true。
- duplicate/late receipt fault。

## 双槽与 exact state

- Dungeon `same_run_state` 已覆盖完整 death，回归 mutation matrix。
- Persistence `detail::same_state` 必须新增 death_sequence 与 DeathCheckpoint 全字段比较；不能用 memcmp/padding。
- 新测试构造两个 CRC 正确、同 generation、其他字段相同但 death_sequence 或单个 death 字段不同的槽，必须被识别为冲突/损坏，不能当 identical winner。

## TDD 测试矩阵

先写 RED 并真实运行，至少覆盖：

- 生产 Combat 普通死亡进入 committing，不 reset。
- pending next 保留永久状态，丢弃当前房 transient；完整 snapshot/target/sequence/anchor。
- 同 tick last monster + player death 优先且无结算。
- committed exact、duplicate、not_committed deterministic retry、indeterminate、wrong generation、missing/wrong kind、每个 verified death field mismatch。
- commit generation/death sequence/room index max fault 在 pending 创建前。
- death cache absent/mismatch fail closed；非death pending带 cache fail closed（不直接私有注入，若只能静态/构造覆盖，需说明）。
- deep abyss Stage10 现有失败回归仍通过，未提前切换。
- Runtime real pending kind echo 测试。
- SaveStore same-generation death conflict tests。

## 验证交付

- build Dungeon/Persistence/Platform 相关目标。
- 直跑 Dungeon 默认栈、Persistence、Platform。
- CTest：`dungeon.units|persistence.units|platform.units` 与相关 architecture、Stage10 regression fixture。
- `git diff --check`，范围审计，无 `/STACK`。
- unique report `.superpowers/sdd/stage11a-task-6-report.md`。
- 提交建议：`feat: commit atomic death retreats`。

