### Task 1: 设置分页状态机

**Files:**
- Modify: `src/platform/raylib/pause_menu_state.hpp`
- Modify: `src/platform/raylib/pause_menu_state.cpp`
- Modify: `tests/platform/pause_menu_state_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes: `PauseInput`、`PauseContext`、现有 `SettingsData`。
- Produces: `PauseSettingsPage` 和保持原 `PauseCommand` 语义的分页状态机。

- [ ] **Step 1: 写分页行为失败测试**

增加以下精确场景：

```cpp
platform::PauseMenuState state{};
state.screen = platform::PauseScreen::settings;
state.settings_page = platform::PauseSettingsPage::home;
state.selected_row = 0U;
platform::PauseInput enter{};
enter.enter = true;
ARPG_REQUIRE(platform::update_pause_menu(state, {}, enter)
    == platform::PauseCommand::none);
ARPG_REQUIRE(state.settings_page == platform::PauseSettingsPage::audio);
ARPG_REQUIRE(state.selected_row == 0U);

state.draft.master_sfx_percent = 80U;
platform::PauseInput escape{};
escape.escape = true;
ARPG_REQUIRE(platform::update_pause_menu(state, {}, escape)
    == platform::PauseCommand::none);
ARPG_REQUIRE(state.settings_page == platform::PauseSettingsPage::home);
ARPG_REQUIRE(state.draft.master_sfx_percent == 80U);
```

覆盖主页 6 行、音频 6 行、显示 4 行、按键 11 行的环绕导航；子页 Esc 保留草稿，主页 Esc/取消回滚；按键捕获完成或取消回按键页原行。

- [ ] **Step 2: 运行并确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

Expected: 编译失败于 `PauseSettingsPage` 不存在。

- [ ] **Step 3: 实现分页状态且保留设置语义**

新增：

```cpp
enum class PauseSettingsPage : std::uint8_t {
    home,
    audio,
    display,
    bindings,
};
```

`PauseMenuState` 增加 `settings_page{home}`。根页进入设置时重置为主页；设置主页行分别进入三子页、恢复默认、应用、取消。音频页处理五个音量；显示页处理窗口模式、垂直同步和拾取过滤；按键页处理十个动作捕获。所有现有 `PauseCommand::preview/apply/rollback` 返回时机保持不变。

- [ ] **Step 4: 运行 pause state suite**

Run:

```powershell
$env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY -ErrorAction SilentlyContinue }
```

Expected: 当前纳入入口的 pause state case 全部通过，原设置预览/应用/回滚测试保持绿色。

- [ ] **Step 5: 提交设置分页状态机**

```powershell
git add src/platform/raylib/pause_menu_state.hpp src/platform/raylib/pause_menu_state.cpp tests/platform/pause_menu_state_tests.cpp tests/platform/platform_test_main.cpp
git diff --cached --check
git commit -m "feat: paginate Chinese settings menu"
```

---

