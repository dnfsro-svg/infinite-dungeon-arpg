# Stage 9 分支 RNG 测试边界修复

基线：`414d87e`。

## 修复内容

- 删除生产静态库中的仅测试 output-seed 导出：
  `monster_affix_selection_output_seed` 与
  `monster_affix_tier_output_seed`。
- 同时删除 `tests/combat/monster_affix_test_support.hpp` 对应声明；生产
  public header 没有、也未新增这些测试 API。
- 将位置隔离测试改为真实 `generate_monster_affixes` 行为回归。测试目录内
  独立实现 context 与按输出位置的 reference 子流，并完整复现候选筛选，逐项
  比较真实生成的 affix 输出。
- 测试会寻找深度 40 的双 affix 真实输入：indexed reference 与 shared
  sequential reference 必须不同。额外消费 position 0 的 selection/tier
  reference 随机值后，position 1 预测保持不变；真实生成必须等于 indexed
  reference 且 position 1 不得等于 shared sequential 结果。因此共享顺序流
  实现会失败，而现有 indexed 实现通过。

现有生产实现已经使用按输出位置派生的子流，所以新增行为测试在基线即通过；
本次没有修改玩法或随机算法，只收紧测试边界和移除测试符号。

## 验证

- `cmake --build build-release --parallel 4`：通过。
- `build-release/bin/arpg_combat_tests.exe`：132 cases, 0 failures。
- `ARPG_STAGE9_AFFIX_STRESS_ONLY=1 build-release/bin/arpg_dungeon_tests.exe`：
  2 cases, 0 failures。
- `ctest --test-dir build-release -R '^combat\\.units$' --output-on-failure`：
  1/1 通过。
- `ctest --test-dir build-release -R '^stage9\\.' --output-on-failure`：4/4
  通过，含 fixture、evidence guard、validation capture 与 formal game capture。
- `git diff --check`：无空白错误（仅 Git 的 LF-to-CRLF 工作区警告）。
- `rg` 审计生产 `.hpp`：output-seed API 不存在；
  `dumpbin /linkermember:1 build-release/lib/arpg_combat.lib`：output-seed
  符号不存在。
