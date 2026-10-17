# Task 6 独立审查约束

- 检查同 tick death 优先发生在 clear/settle/reward 前，不能仅靠最终 phase 掩盖已提交奖励。
- 检查 next 构造先局部后 emplace；overflow/异常/事件容量错误无 partial stable/pending publication。
- 检查 stable sequence 提交前不变，not_committed 重试 next/target/death 全字段相同。
- 检查 death snapshot cache 与 next checkpoint 双向一致，不能重读会变化的 Combat 数据。
- 检查 receipt exact 绑定 generation+kind+full state；Runtime 实际回显 kind，不是只在单测手填。
- 检查 committed 才销毁 Combat/ground/transient，not_committed 保留冻结 Combat 供确定性重试。
- 检查普通死亡保留永久状态和历史 abyss resolution；深渊仍走旧路径，未提前 Task7。
- 检查 SaveStore 同代比较涵盖 death 全字段且无 memcmp padding；冲突测试使用真实编码/CRC。
- 检查事件 exactly once、duplicate receipt、队列溢出、默认栈、无私有 death/pending 注入。
