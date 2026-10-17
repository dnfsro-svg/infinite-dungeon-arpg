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

