# Stage 11-B Task 6 实现报告

## 状态

- 分支：`codex/stage11b-settings-input`。
- 功能提交：`3c7eb3b8f3dcc3dc11856a9a05f297609e8d0ac1`。
- 提交标题：`feat: render bounded pause settings overlay`。
- 范围：仅固定容量 pause/settings view、布局、命中测试与薄 raylib renderer；未实现 Task 7，也未接入 host。

## TDD 证据

### RED：接口缺失

先新增并接入 `pause_menu_view_tests.cpp`，未创建生产接口，运行：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
```

结果：exit `1`，MSVC `C1083`，无法打开 `pause_menu_view.hpp`。失败原因是 Task 6 view 接口不存在，属于预期 RED，不是测试拼写或构建环境故障。

### GREEN：最小实现

新增 `pause_menu_view.hpp/.cpp`、`pause_menu_renderer.hpp/.cpp` 并接入 `arpg_raylib` 后运行：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R "^platform\.units$" --output-on-failure
```

结果：平台测试构建成功，`platform.units` 通过；平台二进制由 `143` 增至 `150 cases, 0 failures`，Task 6 新增 `7` cases。

## 已实现契约

- view 使用 `16 x 64` 固定字符数组和 `std::snprintf`，每行强制在索引 63 终止；同一状态重复生成逐字节一致。
- root 固定显示 `PAUSED` 与 `Continue / Settings / Quit Game`；settings 固定显示 `SETTINGS` 与 Task 5 的 16 行顺序；capture 固定显示 `BIND KEY`、捕获动作和当前 draft key；quit 固定显示 `QUIT GAME?` 与 `Quit Game / Back`。
- settings 前三行同时显示 draft 和 saved/committed 的 Master SFX、Window Mode、VSync；第 3..12 行同时显示十个动作的 draft 与 committed key；第 13..15 行固定为 `Reset Defaults / Apply / Cancel`。
- 布局为居中的固定 760x544 上限面板；在 1024x576、1280x720、1920x1080 均满足至少 32px 水平外边距、16px 垂直外边距。16 行为固定 22px 高、27px stride，形成 5px 显式间隙；footer 与所有行分离。
- hit test 对 16 个行矩形采用左/上闭、右/下开语义；间隙、panel padding、footer 和 panel 外均返回空。
- renderer 只读取 `PauseMenuState` 生成 view 并绘制最终全屏暗层、panel、title、`view.row_count` 行、选中高亮与 message/footer；没有状态转换、存储、Combat、Dungeon 或文件 I/O。
- 1000 次循环覆盖 closed/root/settings/capture/quit 的 view，以及三档分辨率的 layout/hit；allocation probe 保持不变。

## 最终验证

最终门禁命令：

```powershell
$ErrorActionPreference = 'Stop'
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(platform\.units|platform\.pause_menu_state_boundary|architecture\.core_no_raylib|platform\.input_latency_source|platform\.host_input_source|platform\.module_boundary)$" --output-on-failure
git diff --check
```

结果：

- `arpg_platform_tests` 与 `arpg_game` 构建/链接成功。
- 平台二进制：`150 cases, 0 failures`。
- pause/core/module/input 相关 CTest：`6/6`，0 失败。
- `git diff --check`：0 错误；仅显示仓库现有 LF/CRLF 转换提示。

曾在未先运行配置脚本的新 PowerShell 进程内单独链接 `arpg_game`，得到 `LNK1104 opengl32.lib`。诊断读回为脚本前 `cl=False` 且 SDK `LIB=False`，脚本后两者均为 True；同一进程重新链接成功。未为该环境问题修改源码。

## 残余风险与停止边界

- 本 Task 只验证纯 view/layout/hit 的 headless 行为与 renderer 编译/链接；renderer 尚未被生产 host 调用，真实窗口视觉和最终绘制顺序由后续已规划 host 集成任务验证。
- `hit_test_pause_row` 的接口没有 `row_count` 参数，因此按合同只判断 16 个布局行；后续调用方必须再以当前 `PauseMenuView::row_count` 限定可操作行。本 Task 未擅自扩展接口。
- 文本当前使用 raylib 默认 ASCII 字体和固定 16px 字号；三档边界已由几何测试锁定，但真实像素截图属于后续正式路径证据，不在 Task 6 范围。
- 未修改 `raylib_host`、Combat、Dungeon、Persistence 或设置 store，未开始 Task 7。

## Review 修复：selected clamp 与共享绘制计划

- 修复提交：`38db8aab0d0ec2582b956514f03d2faf9dbb9205`。
- 提交标题：`fix: clamp and verify pause overlay rendering`。
- 范围仍限定 Task 6；未修改 host，未开始 Task 7。

### Review RED/GREEN

1. selected 越界 RED：先将 root/settings/quit/capture/closed 的 `selected_row` 设为 `size_t::max()`；平台二进制结果为 `151 cases, 1 failures`，失败点是 root 未 clamp 到 2。加入最小 clamp 后为 `151 cases, 0 failures`；最终 root/settings/quit 分别限制为 2/15/1，capture/closed 固定为 0。
2. render-plan RED：先增加 `PauseMenuRenderPlan`、op kind/order/count/selected/message 测试，构建因 `PauseMenuRenderPlan`、`PauseMenuRenderOpKind` 和 `make_pause_menu_render_plan` 不存在而失败。加入固定容量生产计划并让 renderer 单一 dispatch 消费后为 `154 cases, 0 failures`。

### Review 修复内容

- 新增最多 21 项的纯 `PauseMenuRenderPlan`：`dim / panel / title / 最多16 row / optional message / footer`。
- closed plan 为 0；四个可见 screen 严格按 `dim -> panel -> title -> view.row_count rows -> optional message -> footer` 生成；每个 row op 记录 row index 与 selected，所有界面恰好一个 selected。
- `draw_pause_menu` 删除原有独立绘制顺序和独立行循环，所有 raylib 绘制只由上述 plan 的单一 switch dispatch 驱动。
- 1024x576 黄金几何精确锁定 panel、header 空间、row0、row15、27px stride、5px gap 和 footer；1024x576、1280x720、1920x1080 同时验证 panel/rows/footer 的相对偏移、宽高和居中不漂移。
- 1000 次零分配循环新增 render-plan，并同时覆盖 message 与 no-message 分支。

### Review 最终验证

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(platform\.units|platform\.pause_menu_state_boundary|architecture\.core_no_raylib|platform\.input_latency_source|platform\.host_input_source|platform\.module_boundary)$" --output-on-failure
git diff --check
```

结果：platform/game 构建成功；平台 `154 cases, 0 failures`；相关 CTest `6/6`、0 失败；diff check 0 错误，仅有既有 LF/CRLF 提示。

残余风险仅保留真实 host 尚未接入、因此无生产窗口截图这一既定 Task 6 边界；绘制 op 顺序、行数、message/footer 与唯一 selected 已可 headless 验证。
