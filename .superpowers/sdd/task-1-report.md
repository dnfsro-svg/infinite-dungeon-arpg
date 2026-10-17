# Task 1 实施报告：Buildable Integration Baseline

## 状态

完成。Task 1 的 20 个阶段 0 基线文件已提交到
`milestone/m00-foundation`，未进入任何战斗、地下城或其他玩法实现。

## 提交

- 提交：`2d7fab4fc12f37f5fb136e4e9bd4f32462c376c3`
- 父提交：`f8bbe5ec21e5f36f88dd18924d5962d8f2d26817`
- 主题：`feat: establish stage 0 build baseline`
- 提交范围：仅任务简报列出的 20 个基线文件。

## 实现内容

### 构建与依赖

- 根 CMake 固定 C++17、CMake 3.25+、MSVC 19.44、x64 和 Windows SDK
  10.0.26100.0。
- 使用 Ninja 单配置 Debug/Release presets，并增加可独立配置的
  `windows-msvc-core-debug`。
- 通过 `FetchContent` 获取官方 raylib 提交
  `dbc56a87da87d973a9c5baa4e7438a9d20121d28`。
- 固定归档 SHA256：
  `81b06ce7c19cf3b634b0271c23c361ba6ad8bf45fb8b036abbfeb4260ec1e126`。
- 强制 raylib 为静态库，并在配置期和编译期双重检查 raylib 6.0.0。
- 项目警告 `/W4 /permissive-` 仅逐目标 PRIVATE 添加，未传播给 raylib。

### 入口脚本

- `scripts/Configure.ps1`：通过 vswhere 定位 VS 2022 Build Tools，
  导入 DevShell，强制 x64/x64、SDK 26100，并检查 MSVC/CMake/Ninja。
- `scripts/Build.ps1`：复用配置脚本后按 preset 构建。
- `scripts/Test.ps1`：复用构建脚本后运行 CTest，并保留
  `--no-tests=error`。

### 目标与接口

- `arpg_core`：纯 C++17 静态库。
- `arpg_raylib`：PRIVATE 链接 `arpg_core` 与静态 `raylib`。
- `arpg_game`：只装配默认 `RaylibHostConfig` 并调用 host。
- `arpg_core_tests`：无窗口轻量测试程序。
- 冻结 `arpg::core::FixedStepFrame` 和
  `FixedStepRunner::advance(double) noexcept` 接口。
- 冻结 `RaylibHostConfig` 和
  `run_raylib_host(const RaylibHostConfig&) noexcept` 接口。
- 最小 raylib host 创建 1280×720 窗口、驱动 FixedStep，并只绘制阶段 0
  标题与 tick；没有玩家、怪物、战斗或房间玩法。

### 测试与架构守卫

- 自带静态测试 harness，无 Catch2/GoogleTest 等额外依赖。
- `core.units` 覆盖一个完整步和两个半步的真实行为。
- `architecture.core_no_raylib` 扫描核心源码，并在配置期检查
  `arpg_core` 的链接和包含属性，拒绝 raylib 泄漏。

## TDD RED 证据

先创建获批准的 CMake/脚本/接口/最小窗口脚手架和测试，
`src/core/fixed_step.cpp` 此时只有：

```cpp
#include "core/fixed_step.hpp"
```

首次命令：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
```

结果：退出码 1。配置与编译先成功，关键环境输出为：

```text
The C compiler identification is MSVC 19.44.35228.0
The CXX compiler identification is MSVC 19.44.35228.0
Building raylib static library
Generated build type: Debug
Configuring done
Generating done
```

首次输出包含大量 MSVC include trace，因此在完全相同、仍未实现
`advance` 的 RED 源码状态下增量重跑：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
```

结果仍为退出码 1，失败原因精确符合预期：

```text
Linking CXX executable bin\arpg_core_tests.exe
fixed_step_tests.cpp.obj : error LNK2019:
  unresolved external symbol
  arpg::core::FixedStepRunner::advance(double)
bin\arpg_core_tests.exe : fatal error LNK1120:
  1 unresolved externals
```

该 RED 证明测试真实调用冻结接口，并且失败不是配置、下载、编译错误或
测试拼写错误。

## GREEN 实现

RED 后才按简报补入最小行为：

- 仅接受有限且非负的帧间隔。
- 累加时间后在达到一步时执行一个固定步并增加总 tick。
- 返回夹紧到 `[0, 1]` 的插值值及当前诊断字段。

没有实现测试之外的额外 FixedStep 行为。

## GREEN 与最终测试结果

完整 Debug 图形模式：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
```

```text
[4/4] Linking CXX executable bin\arpg_game.exe
1/2 Test #1: core.units ....................... Passed
2/2 Test #2: architecture.core_no_raylib ...... Passed
100% tests passed, 0 tests failed out of 2
```

全新 Core-only Debug 模式：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

```text
Build files have been written to:
  .../out/build/windows-msvc-core-debug
1/2 Test #1: core.units ....................... Passed
2/2 Test #2: architecture.core_no_raylib ...... Passed
100% tests passed, 0 tests failed out of 2
```

聚焦核心测试：

```powershell
.\out\build\windows-msvc-debug\bin\arpg_core_tests.exe
```

```text
2 cases, 0 failures
```

## 自审

- 用脚本从任务简报提取 20 个最终代码块并逐文件规范化换行比较：
  `20/20` 完全一致。
- PowerShell 三个脚本全部通过 AST 解析，`CMakePresets.json` 通过 JSON
  解析；端到端测试同时验证嵌套 dot-source 调用链。
- 本地归档大小为 52,562,578 字节，重新计算 SHA256 与简报固定值一致。
- Debug `compile_commands.json`：
  - 项目编译单元 5 个，全部具有 `/W4 /permissive-`；
  - raylib 编译单元 27 个，均不具有这两个项目警告选项；
  - `fixed_step.cpp` 编译命令不含 `raylib` 或 `_deps` 路径。
- CMake cache 确认：
  - `CMAKE_BUILD_TYPE:STRING=Debug`
  - `BUILD_SHARED_LIBS:BOOL=OFF`
  - `PLATFORM:STRING=Desktop`
  - `CMAKE_GENERATOR:INTERNAL=Ninja`
  - Core-only 中 `ARPG_BUILD_GRAPHICS:BOOL=OFF`
- `out/build/.../CMakeCache.txt` 经 `git check-ignore` 确认被忽略。
- `git diff --check` 与暂存后的 `git diff --cached --check` 均通过。
- 提交只含任务简报列出的 20 个文件。
- 独立只读代码审查结论：
  - Critical：none
  - Important：none
  - Minor：none
  - Ready to commit：Yes

## Step 1 结束复核

从 `E:\game` 复核：

```text
git -c safe.directory=E:/game check-ignore .worktrees/probe
=> .worktrees/probe

git -c safe.directory=E:/game/.worktrees/m00-foundation
    -C .worktrees/m00-foundation status --short --branch
=> ## milestone/m00-foundation
```

工作树处于正确分支且无 tracked/untracked 改动；当前提交父节点为控制器给定
基线 `f8bbe5ec21e5f36f88dd18924d5962d8f2d26817`。

## 疑虑与明确未覆盖项

- 无阻断疑虑。
- 本任务严格按简报只建立阶段 0 可构建基线；完整的最多 8 步追帧、丢弃时间、
  无效输入计数等行为仍由后续阶段 0 核心运行时任务实现。
- 本 Task 1 未执行 Release 构建或人工启动窗口；它们属于后续阶段 0 完整集成
  与验收，不是本任务 Step 8 的要求。
