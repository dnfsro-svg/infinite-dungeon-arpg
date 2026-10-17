# Stage 11-A Task 8 独立复审约束

复审 `42e36b2..Task8 HEAD`，完整读取设计、计划 Task 8、brief、实现报告和 full diff。不要改代码。

重点检查：

1. prepare 前置条件严格；invalid target/phase/pending/Combat 不得发布部分 pending 或改变 stable。
2. next 只复制已保存 target，不调用 RNG；generation +1、death sequence 不变、DeathCheckpoint 全清零、永久状态与 abyss history 保留。
3. floor-room 0、depth 1 clamp、deep N-1、target seed/ecology/hole 跨 retry/reload 完全一致。
4. committed receipt 绑定 kind/generation/full state/exact cache；duplicate、late、indeterminate、任意字段漂移 fail closed。
5. not_committed 返回同一 death_pending，snapshot 仍可继续且可见失败诊断；第二次请求 next 逐字段相同。
6. 事件 requested/continued/save_failed 的容量预留、顺序、次数和边界 30/32、31/32、32/32 是否原子，无 Task6 事件回归。
7. committed 后先 phase transitioning，再由下一 tick 构建保存 target 的普通房 Combat；不得在 receipt 前创建 Combat。
8. 真实 SaveStore 双槽测试必须落盘两代并重建 Session，故障 winner 不跳过 pending death；不能用伪造 receipt 代替磁盘证据。
9. 测试不得直接注入 private pending、stable death、death snapshot；fixture/旧断言不得弱化。
10. 普通/深渊 retreat、Stage10、Persistence/Platform/Architecture 无退化；无 `/STACK`、padding `memcmp`，工作树干净、报告一致。

结论必须为 Approved 或 Needs fixes，并按 Critical / Important / Minor 给出文件行号。
