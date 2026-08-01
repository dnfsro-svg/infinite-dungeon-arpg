# Stage 10 人口缩放验证预算设计

日期：2026-07-31

## 1. 问题与证据

`stage10_validation_fixture.cpp` 的 `12,000` tick 清房上限来自 2026-07-16
最初的 Stage 10 验证夹具。当时深渊遭遇至多约 6 只怪；后续百倍正方形房间把
合法深渊人口扩展为 450～1125，但只同步了房间边界，没有重新标定清房预算。

当前真实 600 怪运行在 12,000 tick 内击杀 184 只，用时约 3.874 秒。按已观测
吞吐线性估算，600 怪约需 39,131 tick，1125 怪约需 73,370 tick。因此
`12,000` 是过期的测试内部常量，不是玩法、正式 Stage 10 设计或证据护栏的硬约束。

当前夹具还会把同一个 600 怪房间完整清理两次：第一次验证奖励领取，第二次只为
建立奖励放弃分支。第二次清房不提供新的行为证据，却把最大人口下的预计耗时翻倍。

## 2. 方案比较

### A. 保留双清房，只放大预算和超时

改动最少，但最大人口预计需要约 60 秒清房时间，CTest 至少要放宽到 90 秒；重复
执行相同清房浪费 CPU。

### B. 一次真实清房后从稳定存档分叉（采用）

第一次清房仍完整经过生产 combat、`abyss_clear` 保存事务和回执发布。在任何奖励
生成或领取前读取已提交的 `cleared` checkpoint：原 session 继续奖励生成、领取和
重载；另一个 SaveStore 从该 checkpoint 启动，走生产奖励重建、离房警告、二次确认、
放弃保存和重载摘要。

这同时验证 cleared checkpoint 的真实重载路径，且不需要第二次清理数百只怪。

### C. 只击杀 25% 或继续压榨固定 12,000 tick 驱动

25% 只证明出口解锁，不能证明完整清房后的深渊奖励事务；固定 12,000 tick 则要求
当前驱动至少再提升 3.26 倍，且仍无法覆盖 1125 的合法人口。两者均不采用。

## 3. 清房预算合同

预算只属于测试夹具，不改变游戏固定步长、怪物属性、伤害、房间人口或生产超时。

在 `drive_clear()` 开始时，从首次真实 snapshot 读取不可变的
`initial_monster_count`，拒绝 0 或超过 `kRoomMonsterCapacity` 的非法值。预算为：

```text
observed_kills = 184
observed_ticks = 12000
headroom = 5 / 4
scaled_budget(N) = ceil(N * observed_ticks * 5 / (observed_kills * 4))
budget(N) = max(12000, scaled_budget(N))
```

等价整数式为 `ceil(N * 60000 / 736)`：600 怪为 48,914 tick，1125 怪为
91,712 tick。中间值使用 `uint64_t`，结果验证后缩窄为 `uint32_t`。失败诊断必须输出
实际预算和初始人口，不再硬编码打印 12,000。

每个驱动迭代仍恰好调用一次 `session.tick(movement)`；不得使用私有测试入口、直接
改写怪物状态、缩减真实人口或绕过保存事务。

## 4. 稳定存档分叉合同

第一次 `drive_clear()` 成功后、`wait_for_rewards()` 前立即 `store.load()`，并验证：

- load 状态为 ready；
- abyss lifecycle 为 cleared，rule 与 room seed 与入口一致；
- commit generation 等于 `drive_clear()` 返回的 clear generation；
- reward total 合法，且 generated、claimed、abandoned mask 均为 0。

原 session 保持现有奖励生成、领取和重载断言。放弃分支把这份 checkpoint 提交到
独立 `abandon_store`，用其 verified state 构造 `DungeonSession`，随后保持现有真实
warning、arming、neutral release、二次触门、resolution 字段和数量断言。

不得从领取后的 checkpoint 分叉，也不得调用私有 restore/defeat 辅助接口。

## 5. 超时与资源边界

将 `stage10.validation_fixture.real_abyss_transactions` 的 CTest `TIMEOUT` 从 30 秒
调整为 60 秒。按当前测量，最大 1125 怪单次清房预算约 29.61 秒；60 秒保留约
30 秒给尾部稀疏导航、SaveStore I/O、奖励/放弃事务、环境探针和机器波动。

验证只构建目标夹具，使用单任务构建；失败的直接运行后不执行 CTest。不得启动游戏、
正式图形验收或全量回归。

## 6. 精确验收

1. 目标构建成功。
2. 直接运行退出码为 0，输出证明真实 started、clear、rewards、reload、abandon 和
   environment 路径。
3. 精确 CTest `^stage10\.validation_fixture\.real_abyss_transactions$` 通过且不超时。
4. Stage 10 私有注入证据护栏通过。
5. `git diff --check` 通过；改动仅限夹具、CTest 超时、本设计和对应实施计划。
6. 独立审查确认没有削弱完整清房、奖励领取、奖励放弃和环境伤害证据。
