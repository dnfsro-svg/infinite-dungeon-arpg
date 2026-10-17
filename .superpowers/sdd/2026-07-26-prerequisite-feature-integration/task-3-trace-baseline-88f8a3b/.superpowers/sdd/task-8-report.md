# Stage 10 Task 8 实现报告

## 状态

已实现深渊清场原子事务：最后一波清零只准备 `abyss_clear` 保存；精确成功 receipt 后才发布 cleared 状态、还原资源/规则并开放门和洞。普通房仍沿用同步清场路径。

## TDD：RED

- 先在 `dungeon_lifecycle_tests.cpp` 和 `dungeon_transaction_tests.cpp` 增加清场测试，再改生产代码。
- 测试先成功编译；旧实现明确失败在 `drive_to_abyss_clear_pending(...)`：最后一波仍直接进入 `cleared`，没有 `PendingSaveKind::abyss_clear`。
- RED 失败原因与 Task 8 缺失行为一致，不是夹具、语法或构建错误。

## TDD：GREEN / Refactor

- 将末波清场拆为 `prepare_room_clear()` 与 `publish_room_clear()`。
- 普通房：`settle_room_experience()` 后同步 `publish_room_clear()`，保留原事件顺序。
- started 深渊房：
  - 立即进入 `committing`；
  - 保留当前 combat world、深渊规则、environment hazard 和玩家资源用于冻结诊断；
  - 构造 `abyss_clear` next state；
  - `current_room.is_abyss` 和 `has_hole` 保持不变；
  - lifecycle 写为 `cleared`，保留 danger/rule/rules_version；
  - reward_total 按 danger 固定为低/中/高 1/2/3；四个 reward mask/revision 均从 0 开始；
  - 只保存奖励总数，不生成宝箱物品、不分配 ID、不推进 item sequence。
- 更新千房间压力驱动器，使 `abyss_clear` 被识别为合法 save boundary，并验证成功 receipt 后停在 `cleared`。

## 事务、事件与 XP 时序

1. 最后一波清零时，怪物 XP 仍只累计在 `pending_room_experience_`。
2. prepare 阶段用 pending XP + room-clear XP 计算 next-state progression 投影，但不改 stable state，不清空 live pending XP。
3. `not_committed`、`indeterminate`、generation mismatch、verified-state mismatch 均直接 fault；stable generation/progression、live pending XP、深渊规则与资源保持原值。
4. 精确 committed receipt 后才采用已核验的等值 next state，并调用 `settle_room_experience()` 一次；随后同步 live progression 到 committed progression。
5. 再调用 `clear_abyss_rule_preserving_resources()`，然后 phase=`cleared`，依次发 `room_cleared`、`exits_opened`，各一次。
6. 下一 tick 沿原路径从 `cleared` 转到 `awaiting_exit`。

失败 receipt 不发 `room_cleared`/`exits_opened`，不开放门/洞，不清规则；因此 XP 不会提前落稳定状态，也不会重复结算。

## 资源数据与规则清理

- Life Sacrifice 集成测试：
  - 深渊最大生命 550，装备提供最大护盾 46；
  - 受到 146 点伤害后护盾 0、生命 450；
  - receipt 前仍为 450/550；
  - receipt 成功后按比例映回 818/1000；
  - 护盾保持 0/46，没有补满。
- Chaos Expansion 集成测试：receipt 前 environment hazard 数量大于 0；成功后为 0。
- 成功后 active abyss config 的 rule=`none`，monster damage/armor 等倍率恢复普通配置；清场时活怪已为 0。

## 洞口与快照

- 深渊与洞同房时，prepare/committing 阶段 `has_hole=true`，但 phase 不是 `awaiting_exit`，`request_descent(true)` 被拒绝，所有 exits 均关闭。
- 精确 receipt 后 phase=`cleared`，门/洞可用状态与 snapshot 一致；下一 tick 进入 `awaiting_exit` 后可发起下降事务。

## 测试

最终指定命令：

```powershell
ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
```

结果：2/2 通过，0 失败；`dungeon.units` 147.44 秒，`persistence.units` 0.25 秒，总计 147.70 秒。

覆盖内容：

- 普通房清场事件顺序回归；
- abyss_clear prepare/commit 生命周期；
- danger 低/中/高 reward_total 1/2/3；
- not_committed / indeterminate / generation mismatch / state mismatch；
- XP 原子结算；
- Life 比例映射与 barrier 不 refill；
- environment hazard 删除与 monster 配置恢复；
- 洞口提交前封闭、成功后开放；
- 千房间、被动树、持久化 codec 回归。

## 疑虑 / 后续边界

- Task 8 只保存 cleared/reward_total，不实现 cleared checkpoint 重载后的宝箱物化或领取；该生命周期继续由 Task 9 接管。
- 本任务没有生成任何深渊宝箱 ground item，也没有改变奖励 ID/sequence；Task 9 必须继续保持 exact-receipt 和幂等语义。

## Important 复审修复：清场事件容量预留

### 根因

原实现的 exact receipt 分支会先采用已持久化的 cleared state、结算 XP、清除深渊规则，再逐个发送 `room_cleared` 与 `exits_opened`。如果 dungeon event queue 只余 0 或 1 个槽，发布会全部或部分失败，造成持久化已 cleared、运行时却 faulted 且事件不完整。

### RED

- 新增安全预填助手，直接向测试会话的 dungeon event queue 放入无副作用的 `room_reset` 占位事件，不触发 Debug assert。
- 先写两个边界测试：
  - 只剩 0/1 槽：必须在创建 `abyss_clear` pending 前 fault；
  - 恰剩 2 槽：允许 pending，exact receipt 后两个事件必须按顺序完整发布。
- 旧实现下第一个测试在 `faulted.phase == RoomPhase::faulted` 明确失败，实际仍进入 `abyss_clear` committing；第二个测试通过，证明边界正好位于两个槽。

### GREEN 与不变量

- 增加 `can_emit(count)` 容量检查。
- started 深渊的 `prepare_room_clear()` 在构造 next state、投影 XP、创建 pending 之前先要求至少两个空槽。
- 只剩 0/1 槽时：记录 event overflow 并进入 `DungeonFault::event_overflow`；不创建 pending，stable lifecycle 保持 `started`，stable XP 不变，pending XP、规则和 environment hazard 保留，门/洞继续封闭。
- 恰剩 2 槽时：允许进入 committing。committing 期间 `tick()` 冻结，且没有其他 dungeon-event producer，因此两个槽一直保留到 receipt；`publish_room_clear()` 仍保留原有 defensive fault 行为。
- 普通房同步清场路径和队列容量均未修改。

### Fresh 验证

```powershell
ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
```

最终提交前复跑结果：2/2 通过，0 失败；`dungeon.units` 145.27 秒，`persistence.units` 0.25 秒，总计 145.52 秒。
