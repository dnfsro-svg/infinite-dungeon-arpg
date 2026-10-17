# M01 Launcher Remap — Task 4 验收报告

- 状态：`DONE_WITH_CONCERNS`
- 任务分支：`task/m01-launcher-remap`
- 验证 HEAD：`7f568aee3914f431788d9f7204025edc6de59e03`
- 验证日期：2026-07-11
- 范围：Task 4 Steps 1–3、Step 5。未执行 Step 4 交叉审查派发；未执行 Step 6 合并。
- 唯一关注点：桌面自动化在取得窗口状态前被中断，GUI 键盘矩阵需用户实体键盘复验。

## 1. Fresh 门禁

使用 MSVC `19.44.35228.0`，三套 Fresh 门禁均从本任务工作树执行：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
ctest --test-dir .\out\build\windows-msvc-debug -N
ctest --test-dir .\out\build\windows-msvc-release -N
```

结果：

| 配置 | CTest | 失败 |
|---|---:|---:|
| windows-msvc-core-debug | 4/4 | 0 |
| windows-msvc-debug | 5/5 | 0 |
| windows-msvc-release | 5/5 | 0 |

Debug 与 Release 的测试名均恰好为：

1. `core.units`
2. `combat.units`
3. `platform.view_math`
4. `architecture.core_no_raylib`
5. `architecture.combat_no_raylib`

说明：普通沙箱首次执行时因构建目录 ACL 无法更新 CMake/CTest 临时文件；在获准的提升环境中以相同命令重跑后通过。Full Fresh 使用工作树已有的 FetchContent raylib 源，没有触发网络失败，因此无需启用 `FETCHCONTENT_SOURCE_DIR_RAYLIB` fallback，也未修改项目脚本或 CMakePreset。

## 2. 直接测试与静态边界

执行：

```powershell
& .\out\build\windows-msvc-debug\bin\arpg_core_tests.exe
& .\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
& .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
& .\out\build\windows-msvc-release\bin\arpg_core_tests.exe
& .\out\build\windows-msvc-release\bin\arpg_combat_tests.exe
& .\out\build\windows-msvc-release\bin\arpg_platform_tests.exe
rg -n "raylib|raymath|rlgl" src/core src/combat
Get-ChildItem .\out\build\windows-msvc-release -Recurse -Filter 'raylib*.dll'
```

结果：

| 配置 | Core | Combat | Platform |
|---|---:|---:|---:|
| Debug | 22 cases / 0 failures | 30 / 0 | 7 / 0 |
| Release | 22 cases / 0 failures | 30 / 0 | 7 / 0 |

- `src/core`、`src/combat` 的 `raylib|raymath|rlgl` 命中数：0。
- Release 构建树中的 `raylib*.dll` 数：0（静态链接形态）。
- 平台键位测试 `combat_keys_are_j_k_and_l_without_u` 已自动验证：绑定数组恰好 3 项，`J -> light`、`K -> jump`、`L -> launcher`，且所有绑定均不为 `KEY_U`。

## 3. Release 产物

| 产物 | 字节数 |
|---|---:|
| `out/build/windows-msvc-release/bin/arpg_game.exe` | 810,496 |
| `out/build/windows-msvc-debug/bin/arpg_game.exe` | 3,240,960 |
| `out/build/windows-msvc-release/bin/arpg_core_tests.exe` | 27,648 |
| `out/build/windows-msvc-release/bin/arpg_combat_tests.exe` | 96,256 |
| `out/build/windows-msvc-release/bin/arpg_platform_tests.exe` | 532,992 |

## 4. GUI / 键位矩阵

实际尝试：通过 Windows 桌面控制按 Release EXE 完整路径启动，轮询目标窗口，计划激活并取得窗口截图后发送按键。工具调用在返回任何可用窗口、截图或输入结果前被中断；依任务指令停止重复尝试。

| 验收项 | 自动化证据 | 真实窗口观察 | 结论 |
|---|---|---|---|
| HUD 仅显示 `L Launcher` | 源码静态核对 HUD 字符串；不含 Heavy / U Launcher | 未取得窗口截图 | 未自动可见验证，需实体复验 |
| `L` 触发 7/4/17 上挑与自然浮空 | Combat 30/0 + 平台 `L -> launcher` 映射测试 | 未取得 GLFW 输入结果 | 映射与模拟自动验证；实体键盘需复验 |
| `U` 无动作、缓冲或木桩变化 | 平台测试明确排除 `KEY_U` | 未取得 GLFW 输入结果 | 绑定表自动验证；实体键盘需复验 |
| `J` 三段、`K`、`R`、`F1` | Combat/Platform 测试通过；源码静态核对运行时路径 | 未取得 GLFW 输入结果 | 需实体键盘复验 |
| `F12` 截图 | 源码静态核对 `TakeScreenshot("combat-lab.png")` | 本次未生成可归因截图；截图中“四个对象”“状态条”“新 HUD”三项均未观察 | 四个对象、状态条、新 HUD 均未自动验证，需实体键盘复验 |
| `Esc` 退出 | 源码静态核对 `SetExitKey(KEY_ESCAPE)` | 未取得输入结果 | 未自动验证，需实体复验 |
| 关闭按钮退出 | `WindowShouldClose()` / `CloseWindow()` 静态核对 | 未取得窗口操作结果 | 未自动验证，需实体复验 |

截图路径：无。本次 Release/Debug `bin` 目录均未发现 `combat-lab.png`，不得将旧文件或静态实现当作截图证据。

退出清理：按完整 Release EXE 路径检查时匹配数为 0；最终 `@(Get-Process arpg_game -ErrorAction SilentlyContinue).Count` 为 0。没有终止无关进程。

## 5. 分支与工作树

执行：

```powershell
git diff --check 3c75fc3..HEAD
git status --short --branch
git log --graph --decorate --oneline 3c75fc3..HEAD
git worktree list
```

结果：

- `git diff --check 3c75fc3..HEAD`：通过，无 whitespace 错误。
- 写报告前 `git status --porcelain=v1`：空；tracked/untracked 状态干净。
- 当前提交链：`b88ce06 -> aaaa5b5 -> 2df2c37 -> 7f568ae`。
- 任务工作树 `E:/game/.worktrees/m01-launcher-remap` 仍保留在 `task/m01-launcher-remap`。
- Task 4 证据阶段未产生新的 tracked commit；分支已有 Tasks 1–3 四个提交链。未合并，未修改 `main`、里程碑或任何 Stage 2 分支。

## 6. 结论与限制

Fresh 门禁、直接测试、静态架构边界、静态链接形态和平台键位表自动测试均通过，未暴露 Critical/Important 缺陷。按分工未执行 Step 4 最终交叉审查，因此不能在本报告中宣称最终审查 `C0/I0`。最终状态为 `DONE_WITH_CONCERNS`，关注点仅为 GUI 自动化工具中断；必须用实体键盘完成 L/U/J/K/R/F1/F12/Esc、关闭按钮和 HUD/截图的可见复验。

## 7. 最终审查与里程碑集成补充

- 最终全分支审查初次发现 2 个 Important 测试证据缺口；修复提交 `2c5f2aa` 只增强测试，没有修改生产数值。
- 修复后复审结论：Critical 0、Important 0、Minor 0，Ready to merge。
- 任务分支最终 HEAD：`2c5f2aa81f787860ad78f2f109f630625626b5cd`。
- 里程碑合并提交：`265fcc2e5900bcb7601168ef6797530626af50cf`（`merge: replace heavy attack with L launcher`）。
- 合并后 Fresh Debug CTest 5/5，直接测试 Core 22/0、Combat 30/0、Platform 7/0。
- 合并后 Fresh Release CTest 5/5，直接测试 Core 22/0、Combat 30/0、Platform 7/0。
- Release EXE：`E:\game\.worktrees\m01-combat-lab\out\build\windows-msvc-release\bin\arpg_game.exe`，810,496 bytes。
- raylib 静态库：5,918,934 bytes；raylib DLL 数量 0；最终 `arpg_game` 进程数 0。
- `main` 保持 `f8bbe5e`，Stage 2 分支数量 0；里程碑和任务工作树均保留。
- GUI 实体键盘复验限制仍然有效，不因代码审查和自动测试通过而视为已完成。
