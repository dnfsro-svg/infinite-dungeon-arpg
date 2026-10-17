# Task 11：Raylib 白屏调查（系统化调试 Phase 1/2）

日期：2026-07-12。范围仅限观察、复现和临时未提交探针；未改任何生产代码，探针已全部回滚。

## 结论

桌面截图中的纯白窗口**不是游戏渲染失败**。临时 Debug 探针证明宿主每帧进入
`BeginDrawing → ClearBackground(13,17,27,255) → renderer.draw → EndDrawing`，并从
present 调用正常返回；第 0 帧 Raylib 帧缓冲截图包含完整深色房间、可读 HUD、HP、
Wave 1/2、三个怪物、生态门洞与 UI。白屏仅发生在本机桌面/自动化捕获的窗口表面。

## 启动矩阵

| 组合 | 命令与工作目录 | 等待 | 结果 |
| --- | --- | --- | --- |
| Debug + isolated | `...\\windows-msvc-debug\\bin\\arpg_game.exe --seed 0x8 --save-dir E:\\game\\.worktrees\\m04-final-validation\\out\\acceptance\\task11-debug-seed-8`；工作目录为同一 `bin` | 3 s | 进程 PID 176172、`Responding=True`；窗口内容区纯白；Esc 关闭。 |
| Release + default | `...\\windows-msvc-release\\bin\\arpg_game.exe`；工作目录为同一 `bin`；无参数 | 3 s | PID 200788、`Responding=True`；纯白；F1/F12 无可见结果；Esc 关闭。该比较按验收指令执行，初次启动创建了 `%LOCALAPPDATA%\\InfiniteDungeon\\save\\run_a.sav`（96 B）。 |
| Release + isolated | `...\\windows-msvc-release\\bin\\arpg_game.exe --seed 0x8 --save-dir E:\\game\\.worktrees\\m04-final-validation\\out\\acceptance\\task11-release-isolated-log-seed8`；工作目录为同一 `bin` | 3 s 后截图；F1 后约 0.5 s、F12 后约 1 s | PID 201340、`Responding=True`，窗口客户区截图为纯白；Esc 后退出且无残留 `arpg_game`。 |

窗口配置源码默认值是 1280x720（`RaylibHostConfig`），桌面自动化读取到实际窗口截图区域为 1026x607；这不是 0 尺寸窗口。Release 隔离截图证据：
`E:\codex1\Temp\codex-shot-2026-07-12_08-25-28.png`。

## 与 Task 9 成功记录的对比

Task 9 记录的成功 Debug 启动为 `arpg_game.exe --save-dir ... --seed 8`，Raylib 6 / OpenGL 3.3 / 音频初始化后，通过 F12 生成 `stage3-dungeon-rules.png`，并显示可读 HUD。其日志位于
`E:\game\.superpowers\sdd\task9-game.stdout.log`。本次 Debug 隔离测试与其使用相同的 `bin` 工作目录、同样的隔离存档和同等确定性 seed（本次写为 `0x8`，解析值仍为 8），但复现白屏；因此并非 Release 专属，也不能归因于 save-dir 参数或 seed 形式。

## 宿主渲染链路源码检查

`src/platform/raylib/raylib_host.cpp` 的正常路径是：

1. `InitWindow`、`IsWindowReady`、`SetTargetFPS(60)`；
2. 每帧 `WindowShouldClose` 循环读取 F1/F12，更新 runtime/session；
3. 无条件执行 `BeginDrawing()`、`ClearBackground(Color{13,17,27,255})`、`renderer.draw(...)`、`EndDrawing()`；
4. 若读到 F12，再执行 `TakeScreenshot("stage3-dungeon-rules.png")`。

恢复分支也会执行 `BeginDrawing/ClearBackground/EndDrawing`。`CombatRenderer::draw` 自身先画深色房间和 HUD，不存在正常路径下整窗白色填充。

## Phase 1 临时 instrumentation（已回滚）

仅在 `src/platform/raylib/raylib_host.cpp` 的未提交工作区临时加入低频 stderr
探针：首帧和每 120 帧记录 `IsWindowReady`、`WindowShouldClose`、窗口尺寸、session/
combat/recovery 状态、F1/F12，以及 `BeginDrawing`、clear、renderer、`EndDrawing` 前后；
首帧额外执行一次临时 `TakeScreenshot("task11-diag-frame.png")`。使用 Debug 隔离命令
启动 1.5 秒后，日志记录：

```text
DIAG frame=0 enter ready=1 close=0 size=1280x720 session=1 combat=1 recovery=0 f1=0 f12=0
DIAG frame=0 after-begin clear=13,17,27,255
DIAG frame=0 before-renderer combat=1 alpha=0.000
DIAG frame=0 after-renderer before-end
DIAG frame=0 after-end present-returned
DIAG frame=0 screenshot-requested
```

日志持续到 frame 240，全部阶段完整通过。证据文件：

- `E:\game\.worktrees\m04-final-validation\out\acceptance\task11-debug-framebuffer.stderr.log`
- `E:\game\.worktrees\m04-final-validation\out\build\windows-msvc-debug\bin\task11-diag-frame.png`

第二个文件直接验证 framebuffer 为正确场景，而非白色。探针源代码已用 `apply_patch`
完整移除；`git status --short` 恢复为空。

## 启动日志、进程与图形证据

Release 隔离日志：
`E:\game\.worktrees\m04-final-validation\out\acceptance\task11-release-isolated.stdout.log`。

- Raylib 6.0、GLFW Win32、Intel UHD OpenGL 3.3、默认 shader、字体、VRAM render batch 和 WASAPI 音频均报告初始化成功。
- 日志和 stderr 中未出现 `error`、`warning`、`fail`、`assert` 或 `gl_` 错误关键字。
- 进程在白屏期间 `Responding=True`，约 0.77 s CPU、144 MB working set；Esc 后日志正常记录音频/纹理/shader 释放与 `Window closed successfully`，并确认无 `arpg_game` 残留。
- F12 后没有在 build 输出树找到 `stage3-dungeon-rules.png` 或 `screenshot*.png`，日志亦没有截图条目。

## 单一根因假设（基于 instrumentation）

**非代码根因：本机 Windows 桌面合成/远程自动化捕获链没有显示 Raylib 已成功提交的
OpenGL 帧。**

该假设与三种桌面白屏组合、完整 draw/present 探针和正确的 Raylib framebuffer 截图
同时吻合；“game loop 未进入 renderer”这一代码级假设已被证伪。F1/F12 的自动化键入
也始终记录为 0，故它不能再作为应用输入故障的证据。没有发现需要修改正式逻辑的代码级根因。
