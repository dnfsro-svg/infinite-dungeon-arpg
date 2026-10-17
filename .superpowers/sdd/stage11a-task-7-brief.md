# Stage 11-A Task 7 实现简报

## 目标

仅实现计划 Task 7：把正式深渊中的玩家死亡并入唯一的 `death_retreat` 原子事务，正式替代 Stage 10“深渊失败后留在原房”的阶段行为。不要实现 `death_continue`、死亡 UI 或 Task 8+。

基线：`763ff5e`。

## 必须保持的行为

- 测试必须通过生产 `request_abyss_start`、真实 save receipt、Combat 生产伤害/死亡路径进入正式 started abyss；不得直接注入 death snapshot、stable death、pending save 或伪造正式深渊状态。
- 深渊死亡只产生一个 `PendingSaveKind::death_retreat`；不得先提交旧 `abyss_fail` 再提交死亡。
- 同一个 next state 原子包含：递增 generation/death sequence、`DeathCheckpoint.death_was_abyss=true`、死亡房摘要、确定性退层目标、`current_room.is_abyss=false`、`abyss.lifecycle=failed` 和规范 `LastAbyssResolution`。
- failed resolution 的 room seed/rule/total 必须来自死亡前已开始挑战；generated=0、claimed=0、abandoned=total，不得发布/物化奖励。
- committed 后才销毁 Combat、地面宝箱/奖励、pending reward、rolled bits 和房间临时经验；not_committed 保留冻结的深渊 Combat 和完整 death snapshot，重试得到逐字段相同的 pending next。
- 首次死亡预留两个事件槽、retry 预留一个终结槽的 Task 6 修复不得退化。
- 旧 started save 的启动期修复仍只处理遗留 started 状态；合法 V6 pending death 重启不得二次写 failed、增加 generation 或重骰目标。
- 普通死亡、手动深渊失败/放弃、成功清理/奖励以及 Stage 10 其余深渊不变量不得退化。

## 实现约束

- 先写 RED 并真实运行，记录准确失败；再改生产代码。
- 优先抽取/复用纯函数 `apply_abyss_failure_resolution(next, previous)`，让死亡事务在局部 next 上一次完成 failed lifecycle/resolution；不得通过第二次保存补写。
- pending cache consistency 必须能从 frozen death snapshot + stable started abyss 重建完整 exact next，任何字段漂移 fail closed。
- 避免 `memcmp`、padding 比较、测试私有状态注入、扩大栈链接选项或 `/STACK`。
- 仅改 Task 7 所需文件；保留用户/其他任务改动。

## 验证与提交

- 至少运行 Dungeon 全套 247+ cases、`stage10.validation_fixture.real_abyss_transactions`、Persistence/Platform 受影响回归、`git diff --check`、`rg '/STACK'`。
- Dungeon Debug 全套历史耗时约 8.5 分钟，不得因慢误判卡死；每分钟回报状态。
- 自审后提交，建议消息 `feat: retreat after abyss deaths`；更新 Task 7 报告，列出 RED、实现、测试、SHA 和已知疑虑。
