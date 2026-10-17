# Stage 9 分支审查修复报告

- 基线：`a173723`。
- 范围：仅修复词缀生成的输出位置 RNG 隔离，以及正式游戏截图的陈旧文件误通过；未修改 `src/persistence` 或 `.superpowers`。

## RNG 子流

- `count` 保持独立的 count domain。
- 每个 selection/tier 位置均从同一 context seed 经各自 named domain，再经 `output_index` 派生独立 `DeterministicRng`；不会消费 Director 的基础 RNG。
- TDD RED：只加入测试接口和位置独立断言后构建失败，链接器报告两个 output seed 符号未定义。
- GREEN：`combat.units`：132 cases, 0 failures；`ARPG_STAGE9_AFFIX_STRESS_ONLY=1 arpg_dungeon_tests.exe`：2 cases, 0 failures。

## 正式截图

- TDD RED：预写满足原像素阈值的陈旧 PNG，并用零退出但不写截图的 `whoami.exe` 运行旧脚本；旧脚本实际输出 `formal_capture=PASS`。
- GREEN：脚本启动前仅删除指定的文件 CapturePath，并记录 `runStartedUtc`，要求运行后文件存在且 `LastWriteTimeUtc` 不早于本轮启动。
- 回归：修复后同一 no-op 宿主退出 1 且陈旧文件已删除；向正式路径预写哨兵后运行正式 host，输出 `formal_capture=PASS colors=35 non_background=6339 bright=69`，且新时间戳晚于哨兵。

## 最终验证

- Release 全量构建：通过。
- `ctest --test-dir build-release -R "^stage9\\." --output-on-failure`：4/4 通过。
- `ctest --test-dir build-release -R "^(platform\\.units|platform\\.input_latency_source|platform\\.module_boundary)$" --output-on-failure`：3/3 通过。
- `git diff --check`：通过。
