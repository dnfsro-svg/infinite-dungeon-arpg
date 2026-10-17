# Task 6 实现报告：raylib 6.0 灰盒宿主与诊断 HUD

## 提交

- 分支：`task/m00-raylib-host`
- 基线 SHA：`effb6bd4c8423c02fafadd97286ea4735e9fe2ea`
- 实现提交 SHA：`3b9f92566d01b1de2d06694164e5fa2f3fcb493d`
- 提交信息：`feat: render stage 0 raylib graybox`

提交唯一改动文件：

- `src/platform/raylib/raylib_host.cpp`

提交统计为 `1 file changed, 168 insertions(+), 10 deletions(-)`；未修改前置架构修复、CMake、core 或其他工作树。

## 视觉 RED 证据

视觉 RED 由主代理在基线 `effb6bd4c8423c02fafadd97286ea4735e9fe2ea` 上完成并提供：

- Release 基线成功启动 1280x720 窗口。
- Esc 可以正常退出进程。
- 基线源码和可见窗口只有平面深色背景、`Stage 0 integration baseline` 标题与 tick。
- 基线不包含低成本 2.5D 灰盒房间，也不包含完整 7 项诊断 HUD。

本实现代理未重复桌面自动化；系统窗口捕获无法读取 OpenGL framebuffer。

## GREEN 实现

严格采用任务简报 Step 2 的实现：

- 绘制随窗口尺寸变化的 2.5D 灰盒房间、透视地面和网格。
- 设置 `FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE`、最小窗口 800x450、Esc 退出和 60 FPS 目标。
- 继续使用 `FixedStepRunner` 的 60 Hz 固定更新数据。
- HUD 显示 7 项诊断：raylib 版本、root seed、total tick、fixed steps、alpha、dropped seconds、invalid-dt count。
- 保留 raylib 6.0.0 的 major/minor/patch 编译期断言。

## 双配置测试

首次在受限沙箱运行两个预设时，现有 `out/build` 写入被拒；该环境失败不是编译或测试失败。按权限流程以相同命令重跑，并在提交前再次进行新鲜增量验证。

### Debug

命令：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
```

最终结果：退出码 `0`。

- `core.units`：通过。
- `architecture.core_no_raylib`：通过。
- `100% tests passed, 0 tests failed out of 2`。
- `out/build/windows-msvc-debug/bin/arpg_game.exe` 成功构建。

### Release

命令：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-release
```

最终结果：退出码 `0`。

- `core.units`：通过。
- `architecture.core_no_raylib`：通过。
- `100% tests passed, 0 tests failed out of 2`。
- `out/build/windows-msvc-release/bin/arpg_game.exe` 成功构建。

两套 MSVC 构建均未报告项目 `/W4 /permissive-` 编译警告。

## 自检

- `git diff --check`：提交前退出码 `0`；仅有 Git 的 LF/CRLF 工作树提示，无 diff 错误。
- `git diff HEAD^ HEAD --check`：提交后退出码 `0`。
- `git show --name-status HEAD`：只列出 `M src/platform/raylib/raylib_host.cpp`。
- `git status --short`：提交后无输出。
- 实现提交的父提交为指定基线 `effb6bd4c8423c02fafadd97286ea4735e9fe2ea`。

## 待主代理视觉 GREEN

视觉 GREEN 尚未由本实现代理宣称完成。按任务分工，主代理将使用可读取 OpenGL framebuffer 的验证路径确认灰盒房间、完整 7 项 HUD、动态 tick/alpha、窗口缩放及 Esc 正常退出。

## 审查修复：侧墙三角形绕序

独立审查指出 `draw_graybox_room()` 的四个侧墙三角形与两个地板三角形绕序相反。在 raylib 2D 投影与默认 GL_CCW 背面剔除条件下，侧墙可能被剔除。

### RED 与根因

以归一化屏幕坐标计算二维有向面积：

- 修复前四个侧墙：`+0.1752`、`+0.0400`、`+0.1752`、`+0.0400`。
- 两个正确地板三角形：`-0.6440`、`-0.4200`。

四个侧墙均与地板符号相反，根因是每个侧墙 `DrawTriangle` 的第二、第三顶点顺序颠倒。

### 最小修复

- 修复提交 SHA：`f76bb66e441a4d714ef01bc70bea259840411bd8`
- 最终 HEAD：`f76bb66e441a4d714ef01bc70bea259840411bd8`
- 提交信息：`fix: correct graybox wall winding`
- 父提交：`3b9f92566d01b1de2d06694164e5fa2f3fcb493d`

仅交换四个侧墙三角形各自的第二、第三顶点；没有修改颜色、坐标、几何范围、HUD 或其他文件，也没有 amend 原实现提交。

### GREEN 规则验证

修复后四个侧墙有向面积变为：

- `-0.1752`、`-0.0400`、`-0.1752`、`-0.0400`。

四者均与两个地板三角形的负号绕序一致。

### 修复后双配置验证

- `.\scripts\Test.ps1 -Preset windows-msvc-debug`：退出码 `0`，`core.units` 与 `architecture.core_no_raylib` 均通过，2/2。
- `.\scripts\Test.ps1 -Preset windows-msvc-release`：退出码 `0`，`core.units` 与 `architecture.core_no_raylib` 均通过，2/2。
- 两套配置均重新编译 `raylib_host.cpp` 并成功链接 `arpg_game.exe`，未报告项目编译警告。
- 提交前新鲜增量复验：两配置均 `100% tests passed, 0 tests failed out of 2`。
- 修复提交 `git diff HEAD^ HEAD --check`：退出码 `0`。
- 修复提交后 `git status --short`：无输出。

视觉 GREEN 仍按原分工由主代理执行帧缓冲验证。
