# Stage 1 Task 2 实施报告：Eight-Tick Bounded Input Buffer

## 状态

- Status: COMPLETE
- 工作树：`E:\game\.worktrees\m01-combat-core`
- 分支：`task/m01-combat-core`
- 基线：`6cd16bd2b60b7b9dc979b9336c65e801b5459bfb`
- 提交：`e901d65` (`feat: add bounded combat input buffer`)

## TDD 证据

### RED

先只添加四个行为测试及 Combat CMake/test-main 接线，没有添加生产头文件或实现。

命令（仅清理当前进程继承的重复 `Path`/`PATH` 环境项，不改系统配置）：

```powershell
$savedPath = $env:Path
[System.Environment]::SetEnvironmentVariable('PATH', $null, [System.EnvironmentVariableTarget]::Process)
[System.Environment]::SetEnvironmentVariable('Path', $savedPath, [System.EnvironmentVariableTarget]::Process)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\scripts\Test.ps1' -Preset windows-msvc-core-debug
```

结果：退出码 `1`，失败原因符合预期：

```text
tests\combat\input_buffer_tests.cpp(3): fatal error C1083:
无法打开包括文件: “combat/input_buffer.hpp”: No such file or directory
ninja: build stopped: subcommand failed.
```

说明：第一次直接调用脚本被本机 PowerShell execution policy 拦截，第二次调用暴露继承环境中重复的 `Path`/`PATH`；二者都不是有效行为 RED。使用进程级 Bypass 与进程级环境归一化后，才得到上述有效缺失头文件 RED。未修改执行策略、系统环境或仓库脚本。

### GREEN

添加最小固定存储实现后运行同一完整命令，结果退出码 `0`：

```text
1/4 Test #1: core.units ....................... Passed
2/4 Test #2: combat.units ..................... Passed
3/4 Test #3: architecture.core_no_raylib ...... Passed
4/4 Test #4: architecture.combat_no_raylib .... Passed
100% tests passed, 0 tests failed out of 4
```

Focused runner：

```powershell
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
```

```text
7 cases, 0 failures
```

固定存储扫描：

```powershell
rg -n "malloc|calloc|realloc|new " src/combat/input_buffer.hpp src/combat/input_buffer.cpp
```

结果：无匹配；实现未使用堆分配或动态容器。

## 实现文件

- `src/combat/input_buffer.hpp`：`Action`、容量/寿命常量、`InputBuffer` 公共接口与 32 槽 `std::array` 存储。
- `src/combat/input_buffer.cpp`：push、最老匹配 consume、active aging/稳定压缩、clear 与 diagnostics。
- `tests/combat/input_buffer_tests.cpp`：四个真实行为测试。
- `src/combat/CMakeLists.txt`：接入 `input_buffer.cpp`。
- `tests/combat/CMakeLists.txt`：接入 `input_buffer_tests.cpp`。
- `tests/combat/combat_test_main.cpp`：注册 suite，精确 case 门禁从 3 调整为 7。

## 需求逐项核对

- 固定容量 `32`；第 33 次 push 返回 false、保持现有条目并递增 overflow 计数。
- 每条输入初始寿命 `8`；7 次 active age 后仍可消费，第 8 次 active age 时过期。
- `age(true)` 立即返回，不改变条目、寿命、大小或 diagnostics。
- consume 从索引 0 起查找，删除最老匹配动作，不受不匹配队首阻塞，并稳定移动余项。
- `age(false)` 每个 active tick 只递减一次，并用 read/write 索引稳定压缩未过期项。
- clear 只清空待处理输入并保留 diagnostics；reset_diagnostics 独立清零 expired/overflow。
- 未修改攻击目录语义、架构守卫、Core、Host 或 Stage 2。

## 自审

- 四个测试分别覆盖：最老匹配与无队首阻塞；精确 8-tick 与暂停；32/33 容量边界；稳定过期压缩、clear 与 diagnostics。
- 稳定压缩测试用两条不同年龄的 `heavy` 夹在会过期条目之后，若压缩换序，后续 expired 计数和第二次 consume 会失败；不是只检查 size 的弱测试。
- 所有公开方法与 brief 一致并为 `noexcept`；活动条目寿命始终大于零，因此 age 中的减法无下溢路径。
- `git diff --check` 无空白错误；MSVC `/W4` 构建未产生新的编译器 warning；只出现 Git 的 LF/CRLF 工作树提示。
- Concerns: 无代码或规格疑虑。环境仅需进程级 execution-policy bypass、重复 PATH 归一化及工作树构建/索引写入审批。
