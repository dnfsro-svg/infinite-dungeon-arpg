# Stage 11-A Task 8 实现报告

## 范围

- 基线：`42e36b2a42f550d34da717383f1daaf2e3d727f0`。
- 仅实现 Session `death_continue` 第二次原子事务和真实 SaveStore 跨重启闭环；未接 Runtime 输入或死亡 UI。
- 测试未直接写 private pending、stable death 或 death snapshot；磁盘集成通过真实 Combat 致死、真实 V6 双槽 commit/load 和新 Session 重建推进。

## RED

先新增 Dungeon 与 Persistence 测试，生产代码未改时在 MSVC DevShell 中真实构建，明确因以下缺失而失败：

```text
error C2039: "request_death_continue": 不是 "arpg::dungeon::DungeonSession" 的成员
error C2065: "death_continue": 未声明的标识符
error C2065: "death_continue_requested": 未声明的标识符
error C2065: "death_continued": 未声明的标识符
```

自审发现 deep abyss 的 failed checkpoint 若跨房保留，会污染下一次普通死亡。新增正式链路回归后，当前实现再次明确 RED：

```text
[FAIL] dungeon_death_lifecycle.abyss continue permits next real death:
continued.next_state.abyss.lifecycle == abyss::AbyssLifecycle::none
```

随后才修改生产实现。

## 实现

- 新增公开 `request_death_continue()`、`PendingSaveKind::death_continue`、`death_continue_requested` 和 `death_continued`。
- `build_death_continue_next()` 是 prepare 与 receipt cache 共用的唯一 exact-next 投影：局部复制稳定状态、generation +1、直接发布已保存 target、清 DeathCheckpoint、保持 death sequence、零 biases 和永久角色/历史状态。
- 继续进入新房时清空房间绑定的 active abyss checkpoint，但保留 `last_abyss_resolution`；因此深渊死亡继续后，新房再次真实死亡仍能合法生成并重建第二个 checkpoint。
- request 只在 `death_pending`、无 pending、无 Combat、合法确定性 target 时 accepted；代数溢出或事件容量不足在 pending/stable 发布前 fail closed。
- accepted 前为 requested + terminal/save_failed 预留两个事件槽；30/32 可完整发布，31/32 与 32/32 不发布 pending。
- committed receipt 必须匹配 kind、generation、完整 verified state 和 exact cache；成功后清 transient、进入 `transitioning`，下一 tick 才构建目标普通房 Combat。
- not_committed 回到同一 death screen，保留 checkpoint/target 并累加保存失败诊断；再次请求产生逐字段相同 next。
- Snapshot 在 continue 保存期间保留死亡摘要但标记 `saving=true`、`can_continue=false`。

## 测试覆盖

- exact target、深度 1 夹紧、深层 N-1、seed 非零且不同于死亡房、DeathCheckpoint 清零、永久状态与历史保留。
- committed / not_committed / indeterminate、kind/generation/full-state 漂移、duplicate receipt、generation overflow。
- requested / continued / save_failed 次数、顺序和 30/31/32 容量边界；retry requested 与 exact next。
- 正式深渊死亡 → retreat commit → continue commit → 目标房构建 → 新房真实 Combat 再死亡 → 第二次 retreat commit + Session 重建。
- 真实临时目录中 generation 1 初始房、generation 2 death retreat、generation 3 death continue 的双槽闭环；两次进程重建分别保持死亡回顾和进入目标房。
- continue pre-publish fault 保持 generation 2 死亡代；损坏 generation 3 槽后最高合法 winner 回退到 generation 2，不跳过死亡屏。

## 验证

- 有效 RED：缺失 API/枚举构建失败；deep abyss 跨周期回归 31 cases / 1 failure。
- Focused death lifecycle：31/31 passed。
- Dungeon Debug 全套：254/254 passed。
- Persistence Debug 全套：83/83 passed。
- Stage10 real abyss fixture + Persistence + Platform：3/3 CTest passed。
- Architecture：18/18 passed，97.60 秒。
- `git diff --check`：clean。
- `/STACK`：无匹配。

## 自审结论与疑虑

- exact-next 不调用任何 room RNG 或生成器，只复制已保存 target；确定性验证仍在加载/request 前执行。
- SaveStore 不持有事务 kind，因此测试把真实 `SaveCommitResult` 与原 pending kind 映射为 Session receipt；没有用伪造 receipt 替代磁盘 commit/load。
- Task 9 的 Runtime 输入转发和死亡 UI 本任务未实现，保持计划边界。
