# Task 2 实施报告：冻结固定时间步语义

## 结论

- 状态：PASS
- 基线提交：`2d7fab4fc12f37f5fb136e4e9bd4f32462c376c3`
- 冻结提交：`54475508b7a8dd96ff322b1b795438473ae08932`
- 提交信息：`feat: harden fixed timestep runtime`
- 公开接口未改：`FixedStepRunner::advance(double) noexcept`、`FixedStepFrame`、`kStepSeconds = 1.0 / 60.0`、`kMaxStepsPerFrame = 8` 均保持原声明。

## TDD RED

先只替换 `tests/core/fixed_step_tests.cpp`，生产文件仍为基线实现，然后运行：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
```

第一次沙箱内运行在 CMake 配置阶段因既有构建目录写权限失败：

```text
CMake Error: Could not open file for write ... Permission denied
```

该次没有进入编译/测试，因此未作为 RED 证据。授权 CMake 写入本工作树构建目录后，用同一命令重新运行，配置、编译和链接均成功，得到有效 RED（进程退出码 1，CTest 退出码 8）：

```text
[FAIL] fixed_step.frame partitioning: thirty.total_ticks == 60
[FAIL] fixed_step.cap and fraction: eight.steps == 8
[FAIL] fixed_step.one second regression: frame.steps == 8
[FAIL] fixed_step.invalid input: invalid.invalid_input_count == 4
[FAIL] fixed_step.huge finite input: frame.steps == FixedStepRunner::kMaxStepsPerFrame
7 cases, 5 failures
0% tests passed, 1 tests failed out of 1
```

失败原因与简报一致：基线实现每次最多只执行一步，不累计非法输入，也不记录被丢弃的整步时间；不是编译错误、测试拼写错误或环境错误。

## GREEN

写入简报指定的最小实现后运行 core-only：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
```

关键输出（退出码 0）：

```text
1/2 Test #1: core.units .......................   Passed
2/2 Test #2: architecture.core_no_raylib ......   Passed
100% tests passed, 0 tests failed out of 2
```

运行完整 Debug：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
```

关键输出（退出码 0）：

```text
[5/5] Linking CXX executable bin\arpg_game.exe
1/2 Test #1: core.units .......................   Passed
2/2 Test #2: architecture.core_no_raylib ......   Passed
100% tests passed, 0 tests failed out of 2
```

提交和两个派生工作树创建完成后，又重新运行以上两条命令；两者仍均为 `2/2`、`100% tests passed`。另外直接运行核心测试二进制：

```powershell
& '.\out\build\windows-msvc-core-debug\bin\arpg_core_tests.exe'
```

输出：

```text
7 cases, 0 failures
```

## 冻结语义

- 有效 `dt` 按固定 `1/60` 秒步长累计，每帧最多执行 8 步。
- 使用基于机器 epsilon 的容差和近整数吸附，保证 60 Hz、30 Hz 和混合分区在 1 秒回归中都得到 60 tick。
- 超过 8 步时只保留不足一步的余数，整步部分累计到 `dropped_seconds`；1 秒输入执行 8 步并丢弃 52 步。
- `NaN`、正负无穷和负数输入计为非法且不改变累加器；`-0.0` 作为合法零输入。
- `interpolation_alpha` 始终限制在 `[0, 1)`。
- `DBL_MAX` 有限输入被有界处理；丢弃时间使用饱和加法，保持有限且不溢出。

## 修改范围与自审

提交只包含：

```text
M src/core/fixed_step.cpp
M tests/core/fixed_step_tests.cpp
```

- `git diff --check`：退出码 0；仅有 Git 的 LF/CRLF 行尾提示，无空白错误。
- `git show --name-status HEAD`：仅上述两个允许文件。
- 未修改公开头文件、根 CMake、预设、`cmake/`、`scripts/`、宿主或计划/设计文档。
- `architecture.core_no_raylib` 在 core-only 与 full Debug 中均通过，`arpg_core` 保持无 raylib。
- 逐项复核非法输入保持累加器、浮点吸附、8 步上限、1 秒丢 52 步、alpha 上界和 `DBL_MAX` 饱和路径；未发现未覆盖的简报约束。
- 疑虑：无。沙箱写权限问题只影响首次 CMake 启动，授权后同一命令得到有效 RED 和重复 GREEN；不涉及代码变更。

## 工作树冻结状态

独立运行 `status --short --branch` 与 `rev-parse HEAD`，结果如下：

| 工作树 | 分支 | HEAD | 状态 |
|---|---|---|---|
| `E:\game\.worktrees\m00-foundation` | `milestone/m00-foundation` | `54475508b7a8dd96ff322b1b795438473ae08932` | 干净 |
| `E:\game\.worktrees\m00-core-runtime` | `task/m00-core-runtime` | `54475508b7a8dd96ff322b1b795438473ae08932` | 干净 |
| `E:\game\.worktrees\m00-raylib-host` | `task/m00-raylib-host` | `54475508b7a8dd96ff322b1b795438473ae08932` | 干净 |

三个工作树均指向同一个固定时间步冻结提交；两个任务分支可从该接口并行继续。

## 复审修复：覆盖已有 dropped 上的饱和累加

审查发现原 `huge_finite_input_is_bounded` 只在新 runner 上调用一次 `DBL_MAX`，只能覆盖首次设置 `dropped_seconds`，不能区分第二次累加时的饱和保护与普通浮点加法。

测试现连续两次调用 `runner.advance(std::numeric_limits<double>::max())`，并明确断言：

- 第一次和第二次 `dropped_seconds` 都精确等于 `std::numeric_limits<double>::max()` 且均为 finite。
- 两次 `invalid_input_count` 都为 0。
- 第二次 `interpolation_alpha` 仍在 `[0, 1)`；同时保留第一次 alpha 边界和两次 8 步上限断言。

### 变异 RED

由于正确实现会直接通过新测试，先使用未提交的临时错误变体，把 `saturating_add` 的饱和保护替换为普通 `current + addition`，再运行：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
```

关键输出（进程退出码 1，CTest 退出码 8）：

```text
[FAIL] fixed_step.huge finite input: second.dropped_seconds == maximum
7 cases, 1 failures
0% tests passed, 1 tests failed out of 1
```

该失败准确证明第二次 DBL_MAX 会使普通加法产生 `inf`，新增测试能够覆盖 `current > maximum - addition` 的饱和分支。取得 RED 后立即恢复正确实现；`git diff --exit-code -- src/core/fixed_step.cpp` 退出 0，最终差异和提交均不含临时变体。

### 恢复后的 GREEN

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\scripts\Test.ps1 -Preset windows-msvc-debug
```

两条命令均退出 0，关键输出均为：

```text
1/2 Test #1: core.units .......................   Passed
2/2 Test #2: architecture.core_no_raylib ......   Passed
100% tests passed, 0 tests failed out of 2
```

直接运行 core 测试二进制输出：

```text
7 cases, 0 failures
```

### 复审提交与范围

- 提交：`cc4b2da224388d8f6d73cdafa425a78c46e18018`
- 提交信息：`test: cover fixed-step dropped saturation`
- 提交统计：`1 file changed, 17 insertions(+), 7 deletions(-)`
- 唯一提交文件：`tests/core/fixed_step_tests.cpp`
- `src/core/fixed_step.cpp` 与父提交完全一致；`git diff --check` 和 staged `diff --check` 均退出 0。
- `milestone/m00-foundation` 当前位于复审提交并保持干净。
- 上面的工作树冻结表记录的是初次 Step 6 完成时的状态。本次按要求未更新 `m00-core-runtime` 或 `m00-raylib-host`；它们将由主代理在复审通过后统一 fast-forward。
