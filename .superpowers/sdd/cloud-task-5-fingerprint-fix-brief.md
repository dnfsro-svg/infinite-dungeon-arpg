# Cloud Task 5 regression fix: Stage 19 encounter fingerprint

工作目录：`E:\game\.worktrees\stage19-large-density-rooms`

## 已确认根因

`stage16.loot_reinforcement.simulation` 的材料 RNG 隔离断言通过，但黄金常量失败。Stage 19 提交 `77f1c5861dac4602f2bec1d6eca56a1f6924f83c` 将指纹中的 encounter 构建从旧参数改成正式 `EncounterBuildRequest`（包含 entry、has_hole、12只怪物），这是批准的 Stage 19 行为改变；该提交遗漏同步黄金指纹。

当前稳定输出：`68296461820532187`。旧常量：`15972885342817959559`。

## 修复

- 只修改 `tests/dungeon/stage16_loot_reinforcement_simulation.cpp`。
- 将黄金常量更新为 `68296461820532187ULL`。
- 将常量名和失败文本从过时的 Task 1 描述改为准确的 Stage 19 room/equipment/abyss/encounter 描述。
- 保留并不得削弱 baseline 与 material-interleaved 指纹相等断言。
- 添加简短注释，说明 Stage 19 正式 encounter request 是此黄金基线的一部分；不得仅删除黄金检查。

## 验证

1. 先以现有失败作为 RED 证据。
2. 增量构建后连续运行模拟可执行文件至少 3 次，每次必须同一指纹且 PASS。
3. 运行：

```powershell
ctest --test-dir out/build/windows-msvc-core-debug -R '^stage16\.loot_reinforcement\.simulation$' --output-on-failure
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
```

完整 core 回归必须最终全部通过，保留 CTest总数、耗时和摘要。

## 交付

- 不修改游戏代码、README、云配置或 `.superpowers/sdd/task-5-report.md`。
- 报告追加到 `.superpowers/sdd/cloud-task-5-report.md`。
- 单独提交测试修复，提交信息：`test: refresh Stage 19 encounter fingerprint`。
