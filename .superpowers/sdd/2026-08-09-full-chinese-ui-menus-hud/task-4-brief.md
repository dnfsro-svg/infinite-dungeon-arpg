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

