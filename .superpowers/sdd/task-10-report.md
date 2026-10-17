# Task 10 Report - Runtime、双槽 Host 与 Stage 3

## TDD RED/GREEN

- RED：先新增 `tests/platform/dungeon_runtime_tests.cpp`、将平台测试守卫从 24 改为 32，并加入 CMake。首次执行 `./scripts/Build.ps1 -Preset windows-msvc-debug` 如预期失败：`fatal error C1083: 无法打开包括文件: dungeon_runtime.hpp`。
- GREEN：实现 `DungeonRuntime` 后，8 个用例覆盖空目录 initial commit、已有存档优先、committed/not_committed/indeterminate 三态桥接、双槽恢复、单槽恢复交替保存和规则拒绝。最终 Debug/Release 平台二进制均为 `32 cases, 0 failures`。

## 修改文件

- `src/platform/raylib/dungeon_runtime.hpp/.cpp`：原地 `optional<DungeonSession>::emplace` 生命周期、SaveStore 启动/恢复和严格三态映射。
- `src/platform/raylib/raylib_host.hpp/.cpp`、`src/app/main.cpp`：CLI 参数、默认存档目录、恢复画面与固定步保存服务。
- `src/platform/raylib/combat_renderer.hpp/.cpp`：Stage 3 ecology/abyss/元素门/洞口/HUD/debug 渲染状态。
- `src/platform/raylib/CMakeLists.txt`：Runtime 编译和 persistence 链接。
- `tests/platform/dungeon_runtime_tests.cpp`、`tests/platform/platform_test_main.cpp`、`tests/platform/CMakeLists.txt`：8 用例与 32 case 守卫。

## 验证命令和结果

```powershell
./scripts/Test.ps1 -Preset windows-msvc-debug -Fresh
./scripts/Test.ps1 -Preset windows-msvc-release -Fresh
```

两种配置均为 13/13 CTest 通过，0 failures。

```powershell
./out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
./out/build/windows-msvc-release/bin/arpg_platform_tests.exe
```

两者均输出 `32 cases, 0 failures`。

```powershell
Get-Item ./out/build/windows-msvc-release/bin/arpg_game.exe
@(Get-ChildItem ./out/build/windows-msvc-release -Recurse -Filter 'raylib*.dll' -File).Count
git diff --check
```

Release EXE 存在（907,776 bytes），raylib DLL 数为 0，差异检查通过。

## 可见启动/重启冒烟

使用 `out/stage3-smoke` 执行：

```powershell
./out/build/windows-msvc-debug/bin/arpg_game.exe --seed 0x8 --save-dir ./out/stage3-smoke
```

- 首次可见启动后，生成 `run_a.sav`；自动 Esc 后进程退出码为 0，`@(Get-Process arpg_game -ErrorAction SilentlyContinue).Count` 为 0。
- 首房 Runtime 断言为 Depth 1、Floor Room 1、WATER、四 bias 为 0、四门关闭。
- 以相同命令重启，存档 SHA-256 保持不变：`9EF5EEFF8E29A8495EB5CC56F543CE9A4B7FFD86DB618585C023CC9D2B09CD32`；退出码 0、进程数 0，证明未用 `--seed` 覆盖已有 checkpoint。

## 自审

- Runtime 对 Session 只使用 `optional.emplace`，未移动/复制含 BoundedQueue 的 Session。
- SaveCommitResult 到 TransitionSaveResult：committed/not_committed 一一对应，其余 indeterminate；generation 和 state 始终来自 verified_state。
- persistence 未依赖 Runtime，保持 checkpoint-only 边界。
- `run_raylib_host` 捕获 Runtime、filesystem 与构造异常；恢复态不会创建 Session。

## Concerns

- 自动 F12 的 SendKeys 在该桌面会话未能生成截图，推测焦点被自动化终端持有；F12 代码路径已在 EndDrawing 后调用 `TakeScreenshot("stage3-dungeon-rules.png")`，但未把这次自动截图当作验证证据。

## Review Fix

- RED：在既有 directional door themes 用例中加入锁门渲染决策断言。首次 `./scripts/Build.ps1 -Preset windows-msvc-debug` 按预期失败，提示 `door_render_decision` 不存在。
- GREEN：锁门门框恢复各方向的 FIRE/WATER/LIGHTNING/CHAOS 元素色，同时始终绘制对应标签与箭头；暗红内层保留锁门表现。新的 `DoorRenderDecision` 令这一视觉元数据可由纯测试覆盖。
- 顺带修复恢复画面：F12 在 `draw_recovery_screen` 返回（其内部已 `EndDrawing`）后也调用 `TakeScreenshot("stage3-dungeon-rules.png")`，不再因 `continue` 漏掉该帧。

### Review Fix Verification

```powershell
./scripts/Test.ps1 -Preset windows-msvc-debug -Fresh
./scripts/Test.ps1 -Preset windows-msvc-release -Fresh
./out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
./out/build/windows-msvc-release/bin/arpg_platform_tests.exe
git diff --check
```

- Debug/Release CTest 均为 13/13 通过。
- Debug/Release platform tests 均为 `32 cases, 0 failures`。
- Release `arpg_game.exe` 存在，raylib DLL 数为 0。
