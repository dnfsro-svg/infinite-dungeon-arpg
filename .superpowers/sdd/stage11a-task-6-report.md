# Stage 11-A Task 6 实现报告

## 范围

- 基线：`20403a76181817c4c7f7407fbea043fce8a20233`
- 仅实现普通房 `death_retreat` 第一阶段事务；深渊死亡仍保持 Stage10 `abyss_fail`，未实现 `death_continue` 或 UI。
- 测试中的玩家死亡全部通过 `CombatWorld::apply_player_damage` 生产结算路径产生，未直接写 death snapshot、稳定 death 或 pending save。

## RED

在未修改生产实现时新增普通死亡事务测试，并在 MSVC DevShell 中真实构建。预期失败为：

- `PendingSaveKind::death_retreat` 未声明；
- `PendingSave` 不含 `death_snapshot`；
- `PendingSaveResult` 不含 receipt kind echo。

## 实现

- 新增 `death_retreat` pending kind 与 `death_detected` / `death_retreat_committed` 事件。
- 普通房死亡冻结 Combat snapshot，局部构造并完整校验 next state 后一次性发布 pending；generation、death sequence、room index 与事件容量在 pending 创建前 fail closed。
- next state 保留永久角色/物品/星盘/历史深渊状态，清零 biases，固定死亡锚点与 deterministic retreat target；stable state 提交前不变。
- death receipt 绑定 generation、kind echo、完整 verified state、Combat cache 和重建出的完整 expected next state。
- `not_committed` 保留冻结 Combat 并确定性重试；`death_detected` 在整个死亡重试周期只发一次。
- committed 后才清 Combat、ground、rolled bits、pending room exp 和临时奖励，进入 `death_pending`。
- Runtime 在持久化前冻结 pending kind，并在真实 SaveStore receipt 中回显。
- SaveStore 同代比较显式覆盖 `death_sequence`、DeathCheckpoint 全字段和 target room，无 `memcmp`/padding。

## 验证

- `dungeon.units`: 1/1 passed，244 cases，508.63s（独立复审修复后重跑）。
- `persistence.units`: 81/81。
- `platform.units`: 86/86。
- `stage10.validation_fixture.real_abyss_transactions`: passed。
- `architecture.*`: 18/18 passed，72.66s。
- `git diff --check`: clean。
- `/STACK`: 无新增或现存匹配。

## 独立复审修复

- RED：事件队列已占 31/32 时，旧实现仍发布 `death_retreat` pending；新增测试在 `first death reserves two event slots` 明确失败。
- GREEN：首次死亡在 emplace 前保留 `death_detected + terminal` 两个槽；`death_detected` 已发后的 retry 保留一个 terminal 槽；receipt 阶段原有 `can_emit(1)` 防御保持不变。
- 新增真实 30/32→31/32→32/32 committed、not_committed 和 deterministic retry 覆盖；not_committed 不 fault，首次容量不足无 pending/stable partial publication。
- SaveStore 真实双槽测试拆为“仅 death_sequence 不同”和“仅 raw_damage 一个 DeathCheckpoint 字段不同”，均得到 `conflicting_slots`。

## 说明

- death cache absent/mismatch 的生产防御位于 `pending_death_cache_consistent()`；public API 无法注入 pending/cache，故未增加违反证据约束的私有注入测试。覆盖了生产构造、Runtime 真实 receipt、完整 next exact compare、non-death cache invariant 的实现路径。
- 修复了 Platform 旧 V4 测试 helper 在 V6 后仍按旧布局裁剪/magic 的回归，仅影响测试 fixture 构造。
