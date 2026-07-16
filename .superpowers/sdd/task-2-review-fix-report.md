# Stage 9 Task 2 审查修复报告

基线：`f1d0ac3`。

## 修复内容

- `encounter_plan_legal()` 现通过目录双向检查任意已选词缀对的
  `conflict_mask`；测试以单向目录掩码和两种词缀顺序验证两侧检查。
- 词缀生成在计数抽样前验证完整固定目录：12 个稳定 ID、固定权重 100、
  danger、required/forbidden tag、conflict bit、自冲突以及每档 tier 字段。
  无效目录和候选数不足均显式返回 `nullopt`，包括抽到 0 个词缀的路径。
- 上下文随机种子改为 room -> domain -> depth -> wave -> spawn 的逐层
  `derive_stream`。count/selection/tier 继续使用独立命名域，导演基础选择和
  位置 RNG 未改动。
- 词缀生成测试不再因找到三高危样本提前退出，整个种子范围都会继续验证
  support/hazard 适用性断言。

## RED

新增测试后，`arpg_combat_tests` 编译按预期失败：缺少目录注入、目录校验和
上下文种子测试接口。加载 VS 开发环境后，失败仅为这些尚未实现的标识符。

## GREEN 与回归

- `build-release/bin/arpg_combat_tests.exe`：109 cases, 0 failures。
- `build-release/bin/arpg_dungeon_tests.exe`：122 cases, 0 failures。
- `ctest -R "combat.units|dungeon.units|architecture.combat"`：5/5 通过。
  - `combat.units` 0.69s
  - `dungeon.units` 139.94s
  - `architecture.combat_no_raylib` 10.14s
  - `architecture.combat_no_dungeon` 4.78s
  - `architecture.combat_no_persistence` 5.00s
- `git diff --check`：通过。
