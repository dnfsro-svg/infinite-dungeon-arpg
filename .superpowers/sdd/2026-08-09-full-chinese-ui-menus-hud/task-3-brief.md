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

