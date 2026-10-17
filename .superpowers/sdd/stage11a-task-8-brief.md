# Stage 11-A Task 8 实现简报

## 目标

仅实现计划 Task 8：`DungeonSession::request_death_continue()`、`PendingSaveKind::death_continue`、第二次原子保存事务以及真实 SaveStore 跨重启闭环。不要接 raylib 输入/Runtime 转发或死亡 UI（Task 9）。

基线：`42e36b2`。

## 必须保持的状态机

- request 只在 `RoomPhase::death_pending`、无 pending、无 Combat、稳定 death 为合法 `pending_continue` 且 target 可确定性复验时 accepted；其余公开玩法请求继续拒绝。
- next 必须从稳定状态局部复制：generation +1；`current_room = death.target_room`；清空整个 DeathCheckpoint；`death_sequence` 不变；biases 保持零；永久 progression/passives/items/abyss history 原样保留。
- 不调用任何房间 RNG/生成器重新生成 target；不得重骰 seed/ecology/hole。为兼容 floor_room_index=0 的已保存初始房语义，保留规范 `last_transition=death_retreat`、`last_direction=none`，除非现有不变量有更严格已验证方案。
- pending kind 必须是 `death_continue`；stable 在 verified committed receipt 前逐字段不变。
- committed receipt 必须绑定 generation、kind echo、完整 verified state 和从 stable death target 重建的 exact next；之后清 transient、phase=`transitioning`，下一 tick 走既有 `construct_current_room()` 生成目标普通初始房 Combat。
- not_committed 回到同一死亡回顾，保留稳定 checkpoint/target，增加保存失败诊断，再按 E 产生逐字段相同 next；indeterminate、duplicate/late 或任何 receipt mismatch 必须 fault。
- `death_continue_requested` / `death_continued` 事件必须恰好且可证明；pending 发布前为本次请求的 requested+terminal/save_failed 完整链预留容量，事件溢出不得部分发布 stable/pending。
- 第 1 层继续仍到新的第 1 层目标；深度 N 到 N-1；目标 seed 非零、与死亡房不同、跨重试/重启一致。

## 持久化证据

- 集成测试必须使用真实临时目录、真实 V6 编码、双槽 `SaveStore::commit/load` 和新 `DungeonSession`；不得用伪造 `PendingSaveResult` 代替磁盘闭环。
- 覆盖：retreat 保存后进程重建仍显示相同 death/target；continue 保存后再重建直接处于目标房且不再显示死亡；双槽故障/损坏只选择最高合法 committed generation，不得跳过尚未提交的死亡代。
- 单元级 receipt fault matrix 可以使用公开 `resolve_pending_save`，但不得直接注入 private pending、stable death 或伪造 Combat death snapshot。

## TDD 与验证

- 先写 RED，真实构建/运行并记录缺失 API/枚举或行为失败；再改生产代码。
- 完整 exact-next helper 应供 prepare 与 receipt cache 校验复用，避免两套逻辑漂移；不用 `memcmp` 比较含 padding 状态。
- 保持 Task 6/7 普通与深渊 death_retreat、事件容量、not_committed retry、手动 abyss fail、Stage10 fixture 不退化。
- 至少运行 Dungeon 全套、Persistence 全套、Stage10 real abyss fixture、Platform 快速回归、Architecture、`git diff --check`、无 `/STACK`。Dungeon Debug 历史约 520 秒，每分钟回报。
- 自审、更新 `.superpowers/sdd/stage11a-task-8-report.md`、提交建议 `feat: continue from persisted death screens`，回报 SHA/测试/疑虑。
