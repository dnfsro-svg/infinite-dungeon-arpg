### Task 8: 游戏窗口标题与启动器正式文案审计

**Controller-resolved requirements:**
- 游戏 `RaylibHostConfig` 默认窗口标题精确为 `无限地城`；启动器窗口标题继续精确为 `无限地下城启动器`，不要把两者擅自统一。
- 启动器当前已经是中文时只补行为测试/审计，不为制造 diff 重写实现。
- `CreateTextLayout`、其他 DirectWrite/Win32 API 名和 `HRESULT 0x...` 只可出现在中文错误主消息后的括号技术详情；文件名 `arpg_game.exe`、资源路径、物理键名和技术日志可保留。
- 不改变 `make_launch_request()`：仍只启动带引号的 `arpg_game.exe`，不得新增 `--` 参数；默认存档仍为 `%LOCALAPPDATA%\InfiniteDungeon\save`。
- 不修改部署 staging/备份/发布/回滚事务，不重命名 exe、assets、manifest identity、窗口类或默认目录；快捷方式目标、工作目录和图标保持不变。
- 必须在 Task6/7 完成后的真实玩家展示路径做窄英语残留守卫，白名单逐项说明，禁止整文件白名单；既有 full ecology 与 case-count +1 基线继续可见。
- Windows smoke 可运行；Release 构建、部署和安装版视觉验收留给后续发布计划，不在本任务提前部署。

**Files:**
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/launcher/launcher_main.cpp`
- Modify: `src/launcher/launcher_renderer.cpp`
- Modify: `tests/platform/host_window_lifetime_tests.cpp`
- Modify: `tests/launcher/launcher_core_tests.cpp`
- Modify: `tests/launcher/launcher_window_smoke_test.ps1`

**Interfaces:**
- Consumes: 生产宿主默认窗口标题和启动器现有 DirectWrite 中文路径。
- Produces: 默认标题“无限地城”和无玩家英文的启动器；API 阶段名仅保留在错误诊断括号内。

- [ ] **Step 1: 写默认标题和启动器可见文本失败测试**

断言 `RaylibHostConfig{}.window_title` 为 UTF-8“无限地城”；启动器窗口标题、按钮、状态和错误前缀均为中文。`CreateTextLayout`、`HRESULT` 只允许作为技术详情，不能成为主消息。

- [ ] **Step 2: 更新生产标题并运行 launcher 测试**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_launcher_tests --parallel 2
& '.\out\build\windows-msvc-debug\bin\arpg_launcher_tests.exe'
ctest --preset windows-msvc-debug --output-on-failure -R 'launcher.window_smoke'
```

Expected: launcher 单测和窗口 smoke 全部通过，窗口标题为“无限地城”。

- [ ] **Step 3: 提交窗口/启动器汉化**

```powershell
git add src/platform/raylib/raylib_host.hpp src/launcher/launcher_main.cpp src/launcher/launcher_renderer.cpp tests/platform/host_window_lifetime_tests.cpp tests/launcher/launcher_core_tests.cpp tests/launcher/launcher_window_smoke_test.ps1
git diff --cached --check
git commit -m "feat: localize game and launcher titles"
```

---

## Plan Completion Gate

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe'; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE } } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY -ErrorAction SilentlyContinue }
ctest --preset windows-msvc-debug --output-on-failure -R 'platform.pause_menu_state_boundary|platform.pause_menu_host_layout_source|platform.hole_prompt_binding_source'
git diff --check
```

Expected: 菜单/HUD 定向 suite 和三个 source guard 全部通过；玩家路径不再包含本计划覆盖的英文文案。随后进入 `2026-08-09-full-chinese-ui-inventory-progression.md`。
