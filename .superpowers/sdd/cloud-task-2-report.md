# Cloud Task 2 报告：CMake 平台边界与 Linux 核心 preset

## 完成内容

- 根 `CMakeLists.txt` 现在以 `WIN32` 分支完整包住原有 MSVC 19.44、x64、Windows SDK 10.0.26100.0 门禁。
- 非 Windows 仅允许 GNU 或 Clang；当 `ARPG_BUILD_GRAPHICS=ON` 时，配置立即失败并提示设为 `OFF`。
- `arpg_enable_project_warnings` 保持 MSVC 的 `/W4 /permissive-`，并为其他编译器增加 `-Wall -Wextra -Wpedantic`。
- 新增 `linux-gcc-core-debug` 的 configure、build、test preset；configure 使用 Ninja、Debug、测试开启、compile commands 导出、图形关闭，build/test 均引用该 configure preset，test 开启 `outputOnFailure` 与 `stopOnFailure`。

## RED 证据

```powershell
python tests/cloud/cloud_readiness_test.py
```

变更前退出码为 `1`，四项测试均失败：

```text
FFFF
Missing configurePresets preset: linux-gcc-core-debug
Missing buildPresets preset: linux-gcc-core-debug
Missing required cloud file: scripts/cloud/setup.sh
Missing required cloud file: .github/workflows/build-and-test.yml
```

## GREEN 与自检

```powershell
python tests/cloud/cloud_readiness_test.py
Get-Content -Raw CMakePresets.json | ConvertFrom-Json | Out-Null
cmake --list-presets
git -c safe.directory=E:/game/.worktrees/stage19-large-density-rooms diff --check
```

结果：

```text
FF..
Ran 4 tests
FAILED (failures=2)
CMAKE_PRESETS_JSON=valid
Available configure presets:
  "windows-msvc-debug"      - Windows MSVC Debug
  "windows-msvc-release"    - Windows MSVC Release
  "windows-msvc-core-debug" - Windows MSVC Core-only Debug
  "linux-gcc-core-debug"    - Linux GCC Core-only Debug
CMAKE_LIST_PRESETS_EXIT=0
DIFF_CHECK_EXIT=0
```

保留的两项失败是 `scripts/cloud/setup.sh`/`maintenance.sh` 和 `.github/workflows/build-and-test.yml` 缺失，属于本任务简报明确排除的后续云脚本与 CI 工作；Linux preset 相关的两项契约测试已通过。

本机无 Linux 容器，因此未实际执行 GCC/Clang configure、build 或 ctest；这符合任务简报的验证范围。

## 提交范围

提交仅包含：

- `CMakeLists.txt`
- `CMakePresets.json`

本报告未加入提交。未修改 `.superpowers/sdd/task-5-report.md`。

---

## 独立审查修复

### 新增契约与 RED

扩展 `tests/cloud/cloud_readiness_test.py`，新增两项：

- 非 Windows 单一 CMake 条件必须同时要求 `CMAKE_C_COMPILER_ID` 和 `CMAKE_CXX_COMPILER_ID` 都为 GNU 或 Clang，并具有明确的 C/CXX 错误信息。
- `linux-gcc-core-debug` test preset 的 `filter.exclude.name` 必须精确等于：
  `^(stage14\.audio_sources|stage15\.audio_sources|stage15\.audio_sources_self_test)$`。

新增测试、尚未修复配置时运行：

```powershell
python tests/cloud/cloud_readiness_test.py
```

输出为 `FF.F.F`（6 项中 4 项失败）：两项新测试分别因 C compiler ID 未纳入同一非 Windows 门禁、test preset 尚未设置排除 filter 而失败；云脚本和 CI 两项既有失败仍在。

### 修复

- 将非 Windows 门禁改为一个 `if(... OR ...)`，同时验证 C 与 CXX compiler ID；失败信息为 `Non-Windows core builds require GNU or Clang C and CXX compilers`。
- 为 Linux test preset 添加精确的 `filter.exclude.name` 正则，仅排除三项无条件调用 `powershell.exe` 的 Windows 专属音频测试。

首次 GREEN 后发现新测试的 Python 正则未将 CMake 字面量中的圆括号和 `$` 转义，产生误报；修正测试字面匹配后重新运行。

### 最终验证

```powershell
python tests/cloud/cloud_readiness_test.py
Get-Content -Raw CMakePresets.json | ConvertFrom-Json | Out-Null
cmake --list-presets
ctest --list-presets
git -c safe.directory=E:/game/.worktrees/stage19-large-density-rooms diff --check
```

结果：

```text
FF....
Ran 6 tests
FAILED (failures=2)
CMAKE_PRESETS_JSON=valid
CMAKE_LIST_PRESETS_EXIT=0
CTEST_LIST_PRESETS_EXIT=0
DIFF_CHECK_EXIT=0
```

剩余两项 RED 仍仅为 `scripts/cloud/setup.sh`/`maintenance.sh` 与 `.github/workflows/build-and-test.yml` 缺失，属于后续云脚本/CI 任务。本机无 Linux 容器，未实际运行 GCC/Clang configure、build 或 ctest。

本次修复提交仅包含：

- `CMakeLists.txt`
- `CMakePresets.json`
- `tests/cloud/cloud_readiness_test.py`

---

## 生产配置审查：结构化 CMake 回归测试修复

本轮只修改 `tests/cloud/cloud_readiness_test.py`，消除原测试可在真实非 Windows 分支之外误匹配文本的问题。

### 结构解析范围

- 首先逐行移除 `#` 行内或整行注释。
- 若 `#` 出现在未闭合引号内，解析器直接报错；不尝试猜测引号内 `#`，以 fail-closed 防止注释诱饵绕过。
- 用大小写无关、空白容忍的正则识别顶层 `if ( WIN32 )`，按嵌套 `if/endif` 深度寻找其 depth=1 的 `else()`，仅从该分支提取并规范化 `if` 条件。
- 同一候选门禁必须同时匹配 C 和 CXX compiler ID 的 GNU/Clang 正则，且带 `OR`；错误 message 也仅在该真实非 Windows 分支中验证。

辅助函数 docstring 记录了支持的 line-oriented CMake 范围与 fail-closed 限制。

### Mutation/self-test

在运行主契约前，用临时内联 Python 验证：

```text
STRUCTURE_MUTATION_SELF_TEST=pass
```

覆盖结论：

- 只有注释中的匹配文本会因缺少真实 `if(WIN32)` 而失败；
- C/CXX 条件调换顺序、调整大小写和空白仍能通过；
- 把联合门禁移到真实 `else()` 分支之外时不会被接受。

### 最终验证

```powershell
python tests/cloud/cloud_readiness_test.py
git -c safe.directory=E:/game/.worktrees/stage19-large-density-rooms diff --check
```

结果为 `FF....`，共 6 项测试中仅保留 `scripts/cloud/setup.sh`/`maintenance.sh` 和 `.github/workflows/build-and-test.yml` 缺失造成的两项后续任务 RED；`DIFF_CHECK_EXIT=0`。

本轮修复提交仅包含 `tests/cloud/cloud_readiness_test.py`。未修改或提交 `.superpowers/sdd/task-5-report.md`，本报告也不加入提交。

---

## 架构复核：删除手写 CMake 源码解析

架构复核后，仅从 `tests/cloud/cloud_readiness_test.py` 移除了专为 CMake `WIN32/else` 源码结构解析而添加的内容：

- `#` 注释剥离与 fail-closed 逻辑；
- 手写的 top-level `if(WIN32)` / depth=1 `else()` / `endif()` 分支解析；
- `if` 条件归一化与 compiler gate 源码断言；
- 相关 mutation/self-test 路径和 `re` 依赖。

保留生产 C/CXX 联合门禁，不改 `CMakeLists.txt`；保留 `linux-gcc-core-debug` test preset 的精确 JSON 排除正则测试。

```powershell
python tests/cloud/cloud_readiness_test.py
git -c safe.directory=E:/game/.worktrees/stage19-large-density-rooms diff --check
```

最终结果：

```text
FF...
Ran 5 tests
FAILED (failures=2)
DIFF_CHECK_EXIT=0
```

两项 RED 仍仅为后续任务缺失的云脚本和 CI workflow。编译器门禁的权威回归验证不再由 Python 部分解释 CMake 源码承担，将由 Task 3/4 在 Ubuntu 实际执行 `cmake --preset linux-gcc-core-debug --fresh` 完成。

本次提交仅包含 `tests/cloud/cloud_readiness_test.py`；未修改或提交 `.superpowers/sdd/task-5-report.md`，本报告不加入提交。
