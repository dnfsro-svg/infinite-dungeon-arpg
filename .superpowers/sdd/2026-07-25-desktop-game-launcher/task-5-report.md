# Task 5 验收阻塞报告

日期：2026-07-25

工作树：`E:\game\.worktrees\desktop-game-launcher`

分支 / 基线：`codex/desktop-game-launcher` / `b4dbcbe9553d333a81a28cea04582095c4186139`

状态：**阻塞，未通过。**

## 已执行

1. 开始前确认没有已有 `arpg_game.exe`、`无限地下城启动器.exe` 或 `arpg_launcher.exe` 进程。
2. 设置 `CMAKE_BUILD_PARALLEL_LEVEL=1` 后运行：

   ```powershell
   ./scripts/Test.ps1 -Preset windows-msvc-debug
   ```

   外层命令执行器在 124 秒超时，但没有重启构建链；其已经启动的单个 `ctest.exe --preset windows-msvc-debug --no-tests=error` 继续正常执行。测试日志表明其执行了完整的正式验证链，包含 `launcher.units`、`launcher.icon_asset`、`launcher.manifest`、`launcher.window_smoke` 和 `launcher.deploy_self_test`，这些启动器测试均通过。
3. Debug CTest 于第 **99/114** 项失败，因而按任务要求停止；未运行 Release、`dumpbin`、部署、桌面快捷方式、Computer Use 视觉验收、真实启动流或 DPI 验收。

## 失败证据

失败测试：`stage14.audio_integration_guard`

日志：`out/build/windows-msvc-debug/Testing/Temporary/LastTest.log` 第 18159–18174 行。

原始错误：

```text
CMake Error at E:/game/.worktrees/desktop-game-launcher/tests/platform/stage14_audio_integration_guard.cmake:58 (message):
  Stage14 production source must not launch a process:
  E:/game/.worktrees/desktop-game-launcher/src/launcher/launcher_platform_win32.cpp
```

## 诊断

`tests/platform/stage14_audio_integration_guard.cmake` 对 `src/**/*.c*` 和 `src/**/*.h*` 做全局扫描，并在第 58 行拒绝 `CreateProcess*`、`ShellExecute*` 等进程 API。

命中的 `src/launcher/launcher_platform_win32.cpp` 中：

- 第 99 行：`CreateProcessW`，是启动器“开始游戏”按设计启动同目录 `arpg_game.exe` 的实现；
- 第 126 行：`ShellExecuteW`，是“打开存档目录”按设计打开默认存档目录的实现。

因此，这是既有 Stage14 音频整合防护的扫描范围与新启动器功能的冲突，而不是 Task 5 可通过放宽、跳过或修改源码掩盖的测试失败。Task 5 不修改源代码，故未尝试修复。

## 资源与进程摘要

| 时点 | CPU | 可用内存 | 物理磁盘吞吐 |
| --- | ---: | ---: | ---: |
| 开始前 | 43.5% | 7862.9 MiB | 243489 B/s |
| Debug 后 | 34.6% | 7437.9 MiB | 42733 B/s |

Debug 期间，Stage12 变异证据自测创建约 66 个临时目录、约 4.8 GB 测试副本，并持续进行磁盘验证；未发现重复构建链、`taskkill.exe` 或 `conhost.exe` 风暴。Debug 结束后无 `cmake.exe`、`ctest.exe`、`cl.exe`、`link.exe` 或 `taskkill.exe` 残留。

## 未改变的现场

- 未执行 Release 验证，也没有 Release 资源采样。
- 未执行部署；未创建或修改桌面快捷方式。
- 未启动已部署游戏；未触碰 `%LOCALAPPDATA%\InfiniteDungeon\save`。
- 未创建 Task 5 的两张视觉 PNG 或 `launcher-validation.txt`，因此未提交证据提交。
- 未修改任何源代码、测试、防护规则或构建脚本。
- 开始前工作树干净；Debug CTest 写入了 8 个既有、非 Task 5 的受跟踪证据文件：`docs/validation/evidence/stage11d/{magic-plus.png,pickup-feedback.png,preview-cancel.png,rare-abyss.png,rare-only.png,show-all.png,stage11d-loot-evidence.txt}` 与 `docs/validation/evidence/stage15-audio-mix/stage15-audio-evidence.txt`。为避免重置或覆盖无关改动，已保留该现场，未纳入任何提交。

下一步需要负责实现/集成的任务明确调整 Stage14 防护范围（例如仅扫描 Stage14 音频相关生产源，或为启动器的受控本地启动 API 建立有针对性的边界规则），随后从 Debug 验证重新开始。

## 前置集成修复记录（2026-07-25）

按集成审查要求，仅执行了最小的守卫修复与限定验证；没有恢复 Task 5 的全量验收。

### RED

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(stage14|stage15)\.audio_integration_guard$' --output-on-failure
```

结果：2/2 失败。Stage14 报 `Stage14 production source must not launch a process`，Stage15 报 `Stage15 production source launches a process`；两者均命中 `src/launcher/launcher_platform_win32.cpp`。

新增 helper 自测后直接执行：

```powershell
cmake "-DGUARD_TEST_ROOT=out/build/windows-msvc-debug/tests/platform/audio-process-guard-common" -P tests/platform/audio_process_guard_common_self_test.cmake
```

结果：按预期因 `audio_process_guard_common.cmake` 尚不存在而失败。

### 修复范围

- 新增 `tests/platform/audio_process_guard_common.cmake`：仅当源文件的规范化绝对路径精确等于 `src/launcher/launcher_platform_win32.cpp` 时，遮罩精确 bare `CreateProcessW(` 与 `ShellExecuteW(`；所有其他进程 API 与所有其他文件保持检测。
- 新增 `tests/platform/audio_process_guard_common_self_test.cmake`：覆盖两个允许调用、同文件 `system`/`CreateProcessA` 仍拒绝，以及其他启动器文件中两个同名调用仍拒绝。
- `stage14_audio_integration_guard.cmake` 与 `stage15_audio_integration_guard.cmake` 共享该 helper；URL/FTP、Stage14 的 FFmpeg、核心边界与音频集成检查没有变更。
- `tests/platform/CMakeLists.txt` 注册 `audio.process_guard_common_self_test`。

### GREEN

先由 `./scripts/Configure.ps1 -Preset windows-msvc-debug` 载入项目要求的 MSVC/SDK 环境并更新 CTest 注册；随后执行：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(audio\.process_guard_common_self_test|(stage14|stage15)\.audio_integration_guard)$' --output-on-failure
git diff --check
```

结果：3/3 通过；`git diff --check` 退出码 0。未运行 Debug 全量、Release、部署、Computer Use 或 DPI 验收。

## 守卫覆盖审查修复（基于 `0df4d3567c4dcbd0aa1a16433bb5321e4600ac4c`）

独立审查发现首个 helper 将 Stage14 的有限 `_spawn`/`exec` 后缀集合套用到 Stage15，缩窄了 Stage15 原有任意字母后缀检测。

修复将 `audio_process_guard_common.cmake` 限定为只对精确规范化路径 `src/launcher/launcher_platform_win32.cpp` 遮罩精确 bare `CreateProcessW(` 与 `ShellExecuteW(`，并返回扫描文本。Stage14 恢复原有 bare/std/global 三类正则；Stage15 恢复原有 `_spawn[a-z]*`、`_wspawn[a-z]*`、`exec[a-z]*`、`_exec[a-z]*` 宽匹配及 `std::system`。新增自测断言目标启动器的 `popen()`、`WinExec()` 仍拒绝，并验证 `_spawncustom()` 仅由 Stage15 的原宽模式拒绝；其他启动器文件中的两个白名单名称继续拒绝。

RED：修改自测后直接运行其 CMake 脚本，因 `arpg_audio_process_guard_mask_launcher_calls` 尚未定义而失败。

GREEN：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(audio\.process_guard_common_self_test|(stage14|stage15)\.audio_integration_guard)$' --output-on-failure
git diff --check
```

结果：3/3 通过；`git diff --check` 退出码 0。未运行全量 Debug、Release、部署或桌面验收。

## Task 5 重新验收（2026-07-25）

守卫修复获独立审查通过后，从 Step 1 重新执行。

### Debug

命令：

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
./scripts/Test.ps1 -Preset windows-msvc-debug -Fresh
```

外层执行器在 124 秒超时，但没有重新启动命令；同一孤立 `ctest.exe --preset windows-msvc-debug --no-tests=error` 继续运行并真实退出。最终 `LastTest.log` 记录 `115/115 Testing: launcher.deploy_self_test` 与 `End testing`，没有 `Test Failed.` 或 `***Failed` 标记。Debug 通过。

资源样本：开始 CPU 43.2%、可用内存 7326.6 MiB、磁盘 52491 B/s；Debug 后 CPU 31.9%、可用内存 7328.9 MiB、磁盘 351321 B/s。Debug 后无任务构建、测试、游戏或启动器进程残留。

### Release 阻塞

命令：

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
./scripts/Test.ps1 -Preset windows-msvc-release -Fresh
```

fresh Release 配置成功，但单线程 Ninja 在第 `7/378` 项失败：

```text
FAILED: [code=1] src/abyss/CMakeFiles/arpg_abyss.dir/abyss_rewards.cpp.obj
... cl.exe ... /O2 /Ob2 /DNDEBUG ... -c E:\game\.worktrees\desktop-game-launcher\src\abyss\abyss_rewards.cpp
ninja: build stopped: subcommand failed.
```

Ninja/构建目录未保留编译器诊断文本；Application 事件日志中没有对应 `cl.exe`/MSVC 错误，且失败后无 `cmake.exe`、`ctest.exe`、`cl.exe`、`link.exe`、游戏或启动器进程残留。失败后资源为 CPU 29.9%、可用内存 7079.9 MiB、磁盘 3638465 B/s。磁盘 E: 在开始时有 502184751104 字节可用，因此没有空间不足证据。

按验收约束，Release 未通过，故未执行 `dumpbin`、部署、快捷方式、Computer Use 视觉状态、真实启动、DPI 或 Task 5 证据提交。

## Release 重试后与部署/视觉验收（2026-07-25）

对先前不可复现的单对象失败完成独立诊断后，从既有 Release 构建目录继续一次，未再次使用 `-Fresh`，且设置 `CMAKE_BUILD_PARALLEL_LEVEL=1`。外层执行器在构建完成前超时，但没有启动第二个构建；Release `arpg_game.exe` 与 `无限地下城启动器.exe` 随后均存在。仅补跑一次既有构建目录的：

```powershell
ctest --preset windows-msvc-release --no-tests=error
```

同一孤立 CTest 真实退出，最终记录 `115/115 Testing: launcher.deploy_self_test` 与 `End testing`，没有 `Test Failed.` 或 `***Failed` 标记。Release 通过。Release 后资源样本：CPU 45.3%、可用内存 8291.7 MiB、磁盘 292824 B/s。

### 依赖与部署

`dumpbin /dependents` 结果：

- `无限地下城启动器.exe`：`d2d1.dll`、`DWrite.dll`、`SHELL32.dll`、`ole32.dll`、`USER32.dll`、`KERNEL32.dll`；不依赖 raylib 或 .NET。
- `arpg_game.exe`：`WINMM.dll`、`USER32.dll`、`KERNEL32.dll`、`GDI32.dll`、`SHELL32.dll`、`MSVCP140.dll`、`VCRUNTIME140.dll`、`VCRUNTIME140_1.dll` 与 UCRT API 集。三个 VC 运行库均在 `C:\Windows\System32` 存在。

已通过 `DeployLauncher.ps1 -GameBuildDirectory <Release bin>` 真实部署至 `C:\Users\dnfsr\AppData\Local\InfiniteDungeon\app`，并创建 `C:\Users\dnfsr\Desktop\无限地下城.lnk`。COM 读回：Target 为部署启动器 EXE、WorkingDirectory 为部署 app、Icon 为部署启动器 `,0`；三者均不含 `.worktrees`。部署后存档仍为 2 项、1248 字节，目录时间戳未改变。

### 视觉失败（终止条件）

Computer Use 不接受 `.lnk` 作为 `launch_app` 参数；已先以 COM 验证桌面快捷方式目标，再通过该快捷方式指向的已部署 EXE 启动。窗口标题为“无限地下城启动器”，原始截图尺寸为 922×591 像素（920×560 DIP 客户区加系统边框）。

已保存原始 ready 截图：`docs/validation/evidence/desktop-launcher/launcher-ready.png`。该图以及等待 2 秒后的第二次截图均显示标题栏后整个客户区纯白，未绘制标题、状态、路径或任何操作按钮；可访问性树也仅有系统标题栏控件。因此无法确认中文清晰、纯色状态、按钮路径或无重叠，且界面不满足要求。

按 Task 5 规则停止：未临时重命名 `assets/skills`，未生成缺失资源截图，未启动游戏，未执行 DPI，未创建/提交 `launcher-validation.txt`。这次启动的启动器窗口已通过 Computer Use 正常关闭，窗口列表确认无残留。未触碰存档。
