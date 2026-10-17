# Task 2：确定性预算刷怪导演

## RED

命令：`.\scripts\Build.ps1 -Preset windows-msvc-core-debug`

结果：按预期失败；`tests/dungeon/encounter_director_tests.cpp` 报错 `fatal error C1083: 无法打开包括文件: “dungeon/encounter_director.hpp”`。

## GREEN

- 新增集中式 `EncounterDirectorConfig`、checked budget、固定容量 `RoomEncounterPlan` 与 `EncounterPlanResult`。
- 选择 RNG 使用 `kEncounterDirectorDomain`，位置 RNG 使用独立域；候选按生态权重、预算和标签上限确定性选择。
- 每波从 direct-target 起步；无合法 direct-target 时使用 `chaos_chaser` 后备；位置限制在现有房间边界。
- 未引入 raylib、persistence 或动态热路径分配。

验证命令与结果：

1. `.\scripts\Build.ps1 -Preset windows-msvc-core-debug`：成功。
2. `.\scripts\Test.ps1 -Preset windows-msvc-core-debug`：12/12 通过。
3. `.\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe`：49 cases, 0 failures。

## 关注点

预算两波阈值按 brief 的严格定义执行：`budget > two_wave_threshold`；生态验证使用 matching 权重 1/4 的同种子 A/B 平均偏好生态增量，不比较原始匹配总数。

## 复审修复 RED

新增 3 个回归测试后运行：`.\scripts\Build.ps1 -Preset windows-msvc-core-debug; .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe`。

结果：`52 cases, 3 failures`，分别暴露高预算逐波 high 上限、逐波预算/成本校验和不可分小预算配置校验缺失。

## 复审修复 GREEN

- 高预算语义改为按总预算判断：总预算超过两波阈值时，两波均使用 `high_budget_priority_limit`。
- `encounter_plan_legal` 现在校验波次数边界、first-extra 拆分后的逐波预算、spawn 成本与 `spent_budget` 一致性，并保留总预算上限校验。
- 配置校验拒绝会产生小于最小 direct-target 成本的不可分两波配置（例如 `base=2,max=3,threshold=2`）。

新增 GREEN 验证：

1. `.\scripts\Build.ps1 -Preset windows-msvc-core-debug`：成功。
2. `.\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe`：52 cases, 0 failures。
3. `.\scripts\Test.ps1 -Preset windows-msvc-core-debug`：12/12 通过。
4. `git diff --check`：通过（仅显示 CRLF 转换提示）。

## 阈值校验复审 RED

新增 `base=8,max=24,two_wave_threshold=2` 接受用例后运行 dungeon 测试，预期暴露旧的 `threshold<3` 直接拒绝问题。

RED 输出：`53 cases, 1 failures`，失败点为该配置被错误返回 `invalid_rules`。

## 阈值校验复审 GREEN

- 校验逻辑现在计算从 `base_budget` 按 `budget_per_step` 递增并截断到 `max_budget` 后，首次超过阈值的实际预算；仅当该预算小于两只最小 direct-target 成本时拒绝。
- `base=8,max=24,threshold=2` 首次预算为 8，接受并可生成两波；`base=2,max=3,threshold=2` 首次预算为 3，保持拒绝。
- `.\scripts\Test.ps1 -Preset windows-msvc-core-debug`：12/12 通过。
- 直接 `arpg_dungeon_tests.exe`：53 cases, 0 failures。
- `git diff --check`：通过（仅显示 CRLF 转换提示）。
