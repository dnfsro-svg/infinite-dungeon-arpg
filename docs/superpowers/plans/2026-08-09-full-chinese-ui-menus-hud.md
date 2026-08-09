# Full Chinese Menus and HUD Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将暂停、设置、HUD、房间/深渊交互、存档恢复和 F1 调试层全部汉化，并实现选项居中、高对比度背板和合理分页。

**Architecture:** `PauseMenuState` 负责零分配分页状态，`PauseMenuView` 只生成中文展示模型，renderer 使用共享 CJK 字体和同一布局几何。控制提示改为结构化动作绑定，HUD/通知/房间提示不再解析英文显示字符串。

**Tech Stack:** C++17、raylib 6.0、静态 `ui_text_catalog`、`ui_utf8`、共享 `SharedUiFont`、项目平台单测。

## Global Constraints

- 依赖 `2026-08-09-full-chinese-ui-foundation.md` 全部完成。
- 所有选项、页签和按钮在完整交互矩形内水平、垂直居中。
- 数据型 HUD 保留现有眼动分区，不为追求居中破坏信息扫描。
- 正文与实际背板对比度不低于 4.5:1；颜色不是唯一状态信号。
- 设置草稿、revision、键位含义、保存调用和设置文件格式不变。
- 不允许任何逻辑从中文或英文显示文本反向解析动作。
- 不新增逐帧分配；固定缓冲统一走 `ui_utf8`。

---

## File Structure

- Modify: `src/platform/raylib/pause_menu_state.hpp/.cpp` — 设置分页状态和导航语义。
- Modify: `src/platform/raylib/pause_menu_view.hpp/.cpp` — 中文行模型、主值/已保存辅助值。
- Modify: `src/platform/raylib/pause_menu_renderer.cpp` — 整行背板和居中文字。
- Modify: `src/platform/raylib/control_hints.hpp/.cpp` — 结构化动作绑定缓存。
- Modify: `src/platform/raylib/hud_view_model.hpp/.cpp` — 中文 HUD 与 UTF-8 安全控制提示。
- Modify: `src/platform/raylib/hud_notice_state.cpp` — 直接按 `SettingAction` 取键。
- Modify: `src/platform/raylib/dungeon_view_math.hpp/.cpp` — 生态、深渊、洞口、保存和房间状态中文映射。
- Modify: `src/platform/raylib/room_renderer.cpp` — 中文交互标签、共享字体和高对比度背板。
- Modify: `src/platform/raylib/raylib_host.cpp` — 恢复页共享字体与设置/暂停接线。
- Modify: `src/platform/raylib/debug_overlay_renderer.hpp/.cpp` — 中文 F1 ViewModel 和两列布局。
- Modify: `src/platform/raylib/debug_renderer.cpp`、`actor_renderer.cpp` — 中文世界调试标签。
- Create: `tests/platform/debug_overlay_renderer_tests.cpp`。
- Modify: 对应 pause/control/HUD/dungeon/host 测试及源守卫。

---

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

### Task 2: 中文菜单 ViewModel 和居中布局

**Files:**
- Modify: `src/platform/raylib/pause_menu_view.hpp`
- Modify: `src/platform/raylib/pause_menu_view.cpp`
- Modify: `src/platform/raylib/pause_menu_renderer.cpp`
- Modify: `tests/platform/pause_menu_view_tests.cpp`
- Create: `tests/platform/pause_menu_host_layout_source_test.cmake`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- Consumes: `PauseMenuState.settings_page`、`ui_text_catalog`、`ui_utf8_format()`。
- Produces: 中文 `PauseMenuView`，每行 `primary` 和可选 `saved` 辅助文本；统一 `pause_menu_layout()`。

- [ ] **Step 1: 写中文和居中失败测试**

对根页、四个设置页、捕获页和退出确认逐一断言中文。对 800×450、1280×720、1920×1080 计算：

```cpp
const auto centered = platform::centered_pause_text_layout(
    layout.rows[0], 180.0F, 24.0F, 12.0F);
ARPG_REQUIRE(arpg::test::near(
    centered.text.x + centered.text.width * 0.5F,
    layout.rows[0].x + layout.rows[0].width * 0.5F));
ARPG_REQUIRE(arpg::test::near(
    centered.text.y + centered.text.height * 0.5F,
    layout.rows[0].y + layout.rows[0].height * 0.5F));
ARPG_REQUIRE(centered.backing.x == layout.rows[0].x);
ARPG_REQUIRE(centered.backing.width == layout.rows[0].width);
```

断言主文本不再包含英文 `(saved ...)`，仅值变化时 `saved` 显示“已保存：…”；所有固定缓冲合法 UTF-8。

- [ ] **Step 2: 运行并确认 RED**

Expected: 新 View 字段/纯布局 helper 缺失，或当前行文本仍左对齐。

- [ ] **Step 3: 实现中文 View 和整行居中**

把行模型改为：

```cpp
struct PauseMenuRowView final {
    std::array<char, 96U> primary{};
    std::array<char, 64U> saved{};
};
```

主页输出“音频设置、显示设置、按键设置、恢复默认设置、应用、取消”；各子页按规格输出。`pause_menu_renderer.cpp` 删除 `bounds.x + 14` 和按文字宽度绘制的 backing，先对完整 `bounds` 绘制深色整行内底，再以 `MeasureTextEx` 计算共同中心并绘制主文本。`saved` 仅在有差异时靠右、使用辅助色且不侵入主文本安全区。

- [ ] **Step 4: 恢复布局同源性守卫并运行 GREEN**

新建已在 CMake 注册但当前缺失的 `pause_menu_host_layout_source_test.cmake`，断言宿主鼠标命中和 renderer 都以 `make_pause_menu_view(state).row_count` 调用三参 `pause_menu_layout()`；不得删除该 CTest。

Run:

```powershell
ctest --preset windows-msvc-debug --output-on-failure -R 'platform.pause_menu_state_boundary|platform.pause_menu_host_layout_source'
$env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY -ErrorAction SilentlyContinue }
```

Expected: source guards 和菜单定向 suite 全部通过。

- [ ] **Step 5: 提交菜单 View 和布局**

```powershell
git add src/platform/raylib/pause_menu_view.hpp src/platform/raylib/pause_menu_view.cpp src/platform/raylib/pause_menu_renderer.cpp tests/platform/pause_menu_view_tests.cpp tests/platform/pause_menu_host_layout_source_test.cmake tests/platform/CMakeLists.txt
git diff --cached --check
git commit -m "feat: center fully Chinese pause menus"
```

---

### Task 3: 设置和暂停状态消息汉化

**Files:**
- Modify: `src/platform/raylib/pause_menu_state.cpp`
- Modify: `src/platform/raylib/host_settings_runtime.cpp`
- Modify: `src/platform/raylib/stable_key_raylib.cpp`
- Modify: `tests/platform/pause_menu_state_tests.cpp`
- Modify: `tests/platform/host_settings_runtime_tests.cpp`
- Modify: `tests/platform/host_input_tests.cpp`

**Interfaces:**
- Consumes: `ui_text_catalog`、物理按键标签。
- Produces: 所有设置成功/失败/冲突消息和可绑定键显示中文说明。

- [ ] **Step 1: 把现有英文精确断言改为中文并观察 RED**

至少断言：该按键不可绑定、设置无效、版本号已达上限、设置已保存、另一实例已修改并重新加载、保存繁忙、存储冲突、回滚失败。字母/功能键本体仍为 `W`、`Esc`、`F12`。

- [ ] **Step 2: 实现静态目录消息并运行 GREEN**

状态层只保存 `ui_text()` 返回的静态指针；不得创建临时 `std::string`。`stable_key_label()` 对方向键和修饰键输出“上方向键”“左 Shift”等玩家展示名，字母键保持单字母。

Run: 使用菜单/HUD 定向入口并单独运行 host settings suite。

- [ ] **Step 3: 提交状态消息汉化**

```powershell
git add src/platform/raylib/pause_menu_state.cpp src/platform/raylib/host_settings_runtime.cpp src/platform/raylib/stable_key_raylib.cpp tests/platform/pause_menu_state_tests.cpp tests/platform/host_settings_runtime_tests.cpp tests/platform/host_input_tests.cpp
git diff --cached --check
git commit -m "feat: localize settings feedback"
```

---

### Task 4: 结构化控制绑定与 HUD 文案

**Files:**
- Modify: `src/platform/raylib/control_hints.hpp/.cpp`
- Modify: `src/platform/raylib/hud_view_model.hpp/.cpp`
- Modify: `src/platform/raylib/hud_notice_state.cpp`
- Modify: `src/platform/raylib/active_skill_view.cpp`
- Modify: `src/platform/raylib/active_skill_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/control_hints_tests.cpp`
- Modify: `tests/platform/hud_view_model_tests.cpp`
- Modify: `tests/platform/hud_notice_state_tests.cpp`
- Modify: `tests/platform/active_skill_view_tests.cpp`

**Interfaces:**
- Consumes: `SettingsData.bindings/revision`。
- Produces: `control_hint_binding()`、`control_hint_key_label()` 和中文 HUD/通知。

- [ ] **Step 1: 写移除英文反解析的失败测试**

把 `ControlHints` fixture 改为结构化绑定，断言交互改成 Q 后：

```cpp
ARPG_REQUIRE(platform::control_hint_binding(
    hints, settings::SettingAction::interact) == settings::StableKey::q);
ARPG_REQUIRE(std::strstr(notice.text.bytes.data(), "Q") != nullptr);
ARPG_REQUIRE(std::strstr(notice.text.bytes.data(), u8"交互") != nullptr);
ARPG_REQUIRE(std::strstr(notice.text.bytes.data(), "Interact") == nullptr);
```

再覆盖背包、被动树和 unchanged revision 的 10000 次零分配。

- [ ] **Step 2: 运行并确认 RED**

Expected: `ControlHints` 仍是英文字符串或 `hud_notice_state` 找不到中文提示中的动作名。

- [ ] **Step 3: 实现结构化绑定缓存**

```cpp
struct ControlHints final {
    std::array<settings::StableKey,
        static_cast<std::size_t>(settings::SettingAction::count)> bindings{};
    std::uint64_t revision{};
};

[[nodiscard]] settings::StableKey control_hint_binding(
    const ControlHints&, settings::SettingAction) noexcept;
[[nodiscard]] const char* control_hint_key_label(
    const ControlHints&, settings::SettingAction) noexcept;
```

`refresh_control_hints()` 只复制结构化值。HUD 通过动作列表生成三行中文，`hud_notice_state` 直接传 `SettingAction`，彻底删除对 `Interact/Inventory/Passive Tree` 的 `strstr`/子串扫描。

- [ ] **Step 4: 汉化 HUD 数值和状态**

将 `HP/XP/LV/XP MAX` 改为“生命/经验/等级/经验已满”；主动技能键帽从 `Num1` 至 `Num5` 改为“小键盘1”至“小键盘5”，冷却 `%.0fs` 改为 `%.0f秒`。控制提示和动态消息均使用 `ui_utf8_format()`。加长键名时输出仍合法 UTF-8，常规 96/160 字节缓冲不得截断。

- [ ] **Step 5: 运行定向测试并提交**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY -ErrorAction SilentlyContinue }
git add src/platform/raylib/control_hints.hpp src/platform/raylib/control_hints.cpp src/platform/raylib/hud_view_model.hpp src/platform/raylib/hud_view_model.cpp src/platform/raylib/hud_notice_state.cpp src/platform/raylib/active_skill_view.cpp src/platform/raylib/active_skill_renderer.cpp src/platform/raylib/combat_renderer.cpp src/platform/raylib/raylib_host.cpp tests/platform/control_hints_tests.cpp tests/platform/hud_view_model_tests.cpp tests/platform/hud_notice_state_tests.cpp tests/platform/active_skill_view_tests.cpp
git diff --cached --check
git commit -m "feat: localize structured HUD controls"
```

---

### Task 5: 房间、深渊、洞口和保存提示

**Files:**
- Modify: `src/platform/raylib/dungeon_view_math.hpp/.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp/.cpp`
- Modify: `tests/platform/dungeon_view_math_tests.cpp`
- Modify: `tests/platform/hud_layout_tests.cpp`
- Modify: `tests/platform/hud_render_plan_tests.cpp`
- Modify: `tests/platform/hole_prompt_binding_source_test.cmake`

**Interfaces:**
- Consumes: 统一中文目录、共享字体、结构化交互键。
- Produces: 中文生态/深渊/房间/保存枚举和带高对比背板的世界提示。

- [ ] **Step 1: 写全部枚举和动态键失败测试**

逐一覆盖四生态、深渊三危险等级、九条规则、九条效果说明、洞口四状态、保存四状态、房间阶段和出口方向。交互键 Q 的提示必须是 `Q：下潜` 或 `Q：进入下一层`，不得包含硬编码 E。

- [ ] **Step 2: 运行并确认 RED**

Expected: 现有 `FIRE/ABYSS/SEALED/DESCEND` 等英文断言失败。

- [ ] **Step 3: 替换展示映射并统一 CJK 绘制**

`dungeon_view_math` 只返回中文目录字符串。`room_renderer` 的门、洞口和深渊提示接收共享 `Font`，以 `MeasureTextEx` 测量，先绘制不透明 `#0B1019` 内底，再用 `draw_crisp_ui_text()` 绘制。更新 source guard 为“交互键来自宿主参数且源码中没有 `E: DESCEND`”。

- [ ] **Step 4: 运行测试并提交**

```powershell
ctest --preset windows-msvc-debug --output-on-failure -R 'platform.hole_prompt_binding_source'
$env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY -ErrorAction SilentlyContinue }
git add src/platform/raylib/dungeon_view_math.hpp src/platform/raylib/dungeon_view_math.cpp src/platform/raylib/room_renderer.cpp src/platform/raylib/combat_renderer.hpp src/platform/raylib/combat_renderer.cpp tests/platform/dungeon_view_math_tests.cpp tests/platform/hud_layout_tests.cpp tests/platform/hud_render_plan_tests.cpp tests/platform/hole_prompt_binding_source_test.cmake
git diff --cached --check
git commit -m "feat: localize dungeon interaction views"
```

---

### Task 6: 中文存档恢复页

**Files:**
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Modify: `tests/platform/hud_host_integration_tests.cpp`

**Interfaces:**
- Consumes: `SharedUiFont` 和 `DungeonRenderStatus`。
- Produces: `make_recovery_screen_view()` 纯模型与共享字体绘制。

- [ ] **Step 1: 写恢复页纯模型失败测试**

断言标题“需要恢复存档”、双槽说明、`存档错误 7`、800/1280/1920 居中面板和对比色。测试不得依赖打开 raylib 窗口。

- [ ] **Step 2: 抽取模型并替换默认 `DrawText`**

`draw_recovery_screen()` 接收 `Font`，使用共享 CJK 字体、深色全屏和居中文本；移除玩家路径中的 `SAVE RECOVERY REQUIRED`、`Both save slots...`、`Save error`。

- [ ] **Step 3: 运行测试并提交**

```powershell
git add src/platform/raylib/raylib_host.cpp src/platform/raylib/raylib_host.hpp tests/platform/dungeon_runtime_tests.cpp tests/platform/hud_host_integration_tests.cpp
git diff --cached --check
git commit -m "feat: localize save recovery screen"
```

---

### Task 7: 中文 F1 调试面板

**Files:**
- Modify: `src/platform/raylib/debug_overlay_renderer.hpp`
- Modify: `src/platform/raylib/debug_overlay_renderer.cpp`
- Modify: `src/platform/raylib/debug_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Create: `tests/platform/debug_overlay_renderer_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/stage11c_hud_formal_game_validation.cpp`

**Interfaces:**
- Consumes: snapshot、runtime status、combat feedback、共享字体。
- Produces: 固定 12 行 `DebugOverlayView` 和响应式 `DebugOverlayLayout`。

- [ ] **Step 1: 写 5 个 F1 View/Layout 失败测试**

定义期望结构：

```cpp
struct DebugOverlayLine final {
    std::array<char, 192U> text{};
    Color color{};
};
struct DebugOverlayView final {
    std::array<DebugOverlayLine, 12U> lines{};
    std::size_t line_count{};
};
```

测试全部行 UTF-8 合法、无玩家英文标签；800 宽单列、1280/1920 两列；所有矩形在屏幕内；10000 次构建零分配；状态枚举“起手/生效/后摇/结束”等全覆盖。

- [ ] **Step 2: 运行并确认 RED**

Expected: 纯 View/Layout 接口不存在，当前 renderer 仍直接 `DrawText(TextFormat(...))`。

- [ ] **Step 3: 实现中文模型和共享字体渲染**

把数值格式化移入 `make_debug_overlay_view()`，通过 `ui_utf8_format()` 写固定行。`debug_overlay_layout()` 在宽度小于 1280 时单列，否则两列。renderer 先绘制半透明深色列背板，再用共享字体逐行绘制。世界盒标签改为“辅助判定、攻击判定、受击判定”。

- [ ] **Step 4: 运行定向测试并提交**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY -ErrorAction SilentlyContinue }
git add src/platform/raylib/debug_overlay_renderer.hpp src/platform/raylib/debug_overlay_renderer.cpp src/platform/raylib/debug_renderer.cpp src/platform/raylib/actor_renderer.cpp src/platform/raylib/combat_renderer.cpp tests/platform/debug_overlay_renderer_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp tests/platform/stage11c_hud_formal_game_validation.cpp
git diff --cached --check
git commit -m "feat: localize responsive debug overlay"
```

---

### Task 8: 游戏窗口标题与启动器正式文案审计

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
