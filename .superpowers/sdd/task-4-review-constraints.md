# Task 4 独立审查约束

- 精确核对全部绝对偏移、224/428/460 大小、little-endian、reserved zero、CRC 覆盖范围。
- 确认 V5 常量/fixture 未被 V6 语义覆盖，V5 item offset 236 与 V6 item offset 460 均正确。
- 确认 encoder 仅 V6，decoder V1-V6，`migrated == (format != 6)`，所有旧版本 canonical death none。
- 确认 pending/none 全字段真正 round-trip，测试不是只比较部分字段或靠 fault/default value 假通过。
- 确认错误矩阵先重算 CRC，再断言预期 CodecError；截断/尾随与 payload length 的错误优先级稳定。
- 确认 count/size 算术在 allocation 前防溢出，最大 item count 回归未弱化。
- 确认 V5/V6 item 解码共享逻辑，没有复制实现或改变 40-byte item record。
- 确认 Persistence 只使用 header-only structural validator，未依赖 DungeonRules/room generation/Combat/raylib。
- 确认无 Task 5+ Session/UI 行为，工作树无无关改动。
