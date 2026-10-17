# Task 1 report — 设置分页状态机

## 实施结果

- 新增 `PauseSettingsPage::{home,audio,display,bindings}`，默认主页。
- 设置页拆为主页 6 行、音频 6 行、显示 4 行、按键 11 行，并分别环绕导航。
- 根菜单进入设置强制回主页；子页 Esc 或返回行回主页并保留 draft 与对应主页焦点。
- 主页恢复默认、应用、取消继续沿用原 preview/apply/rollback、revision 和校验语义。
- 按键捕获成功、Esc、失焦取消均回按键页原行；保存格式和键位分配/交换逻辑未改。
- 未开始 Task 2 的文案、ViewModel 或 renderer 工作。

## TDD 证据

### RED

测试先行加入四页导航、进入/返回、草稿保留和捕获回页断言。首次直接构建因当前 PowerShell 未加载 MSVC 标准库环境而提前失败，不计为有效 RED。加载项目的 VS 开发环境后重跑：

```powershell
. '.\scripts\Configure.ps1' -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

有效 RED：编译失败于 `PauseSettingsPage` 不存在及 `PauseMenuState::settings_page` 缺失，符合计划预期。

### GREEN

同一 target 以 `--parallel 2` 构建成功。随后运行：

```powershell
$env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY -ErrorAction SilentlyContinue }
```

结果：`32 cases, 0 failures`。

## 文件

- `src/platform/raylib/pause_menu_state.hpp`
- `src/platform/raylib/pause_menu_state.cpp`
- `tests/platform/pause_menu_state_tests.cpp`
- `tests/platform/platform_test_main.cpp`
- `.superpowers/sdd/2026-08-09-full-chinese-ui-menus-hud/task-1-report.md`

提交消息：`feat: paginate Chinese settings menu`

## Fix round 1 — 测试总数对齐

现状 RED：

- `ARPG_CPLAY028_PAUSE_FOCUS_ONLY=1`：`actual 32 != expected 30`。
- 默认 full platform：`actual 657 != expected 654`；同时仍有既有 `ecology_material_coverage` 的 `alpha_bounds.x` 失败。

Task 1 前本分支已有 `actual 655 / expected 654` 的一例既有计数漂移。Task 1 只新增 2 个 pause-state case，因此精确增量为：

- CPLAY028 expected：`30 -> 32`。
- full-platform expected：`654 -> 656`，保留既有 `actual 657 / expected 656` 漂移，不把它误算为 Task 1 新增。

本轮只修改测试入口 expected，不修改生产代码，也不处理或放宽既有 ecology failure。

修复后证据：

- `ARPG_CPLAY028_PAUSE_FOCUS_ONLY=1`：`32 cases, 0 failures`。
- `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1`：`32 cases, 0 failures`。
- 默认 full platform：`657 cases, 2 failures`；失败分别为既有 ecology `alpha_bounds.x` 和保留可见的 `actual 657 != expected 656` 既有计数漂移。

Fix commit：`fix: align pause menu test counts`。
