# Task 5 独立审查约束

- 确认 death 分流发生在任何 abyss/encounter/Combat/build 之前，非法状态 fail closed。
- 确认重生成使用 `commit_generation-1` 与当前 death_sequence，且构造局部 pre-commit state 的 previous death sequence，不存在 underflow/重骰。
- 确认 current_room 只是死亡锚点，稳定 is_abyss=false；只在局部校验 copy 恢复 death_was_abyss。
- 确认 catalog validation 使用真实 enum 上界/目录，不把任意 u8/u16 当合法。
- 确认 abyss failed 与 LastAbyssResolution 关系完整，普通死亡不会被过度限制而丢失既有历史 resolution。
- 确认 snapshot 只复制 stable/pending next checkpoint，不读取 Combat death 临时对象。
- 确认所有 gameplay request 在 death_pending fail closed，tick 不推进，reset 不能绕过。
- 确认 TransitionKind 新值只做稳定格式支持，没有提前实现 Task6 transaction。
- 确认 same_run_state 全字段、无私有测试注入、无无关改动、默认栈回归保持。
