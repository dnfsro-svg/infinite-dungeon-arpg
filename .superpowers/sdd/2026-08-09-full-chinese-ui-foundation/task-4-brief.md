### Task 4: 单实例共享字体生命周期

**Files:**
- Modify: `src/platform/raylib/shared_ui_font.hpp/.cpp`
- Modify: `src/platform/raylib/hud_renderer.hpp/.cpp`
- Modify: `src/platform/raylib/death_overlay_renderer.hpp/.cpp`
- Modify: `src/platform/raylib/pause_menu_renderer.hpp/.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp/.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/hud_font_tests.cpp`
- Modify: `tests/platform/hud_host_integration_tests.cpp`
- Modify: `tests/platform/stage12_material_formal_game_validation.cpp`
- Modify: `tests/platform/stage12_material_validator.ps1`
- Modify: `tests/platform/stage12_material_validator_self_test.ps1`
- Modify: `tests/platform/stage11d_renderer_integration_guard_test.cmake`

**Interfaces:**
- Consumes: `UiGlyphPlan`。
- Produces: 一个宿主持有的 `SharedUiFont`；renderer 的 `initialize(const SharedUiFont&)` 仅建立借用。

- [ ] **Step 1: 把现有字体测试改成单实例契约并观察 RED**

Task 4 在实现宿主唯一所有权的同一变更中，把 Task 3 的阶段性
`kUiFontAtlasInstanceCount == 3U` 原子切换为最终 `== 1U`；不得在 Task 3
提前低报仍由三个 renderer 独立加载的资源。

新增/更新断言：

```cpp
ARPG_REQUIRE(platform::kUiFontAtlasInstanceCount == 1U);
ARPG_REQUIRE(runtime.bundled_font_instance_count == 1U);
ARPG_REQUIRE(runtime.bundled_font_total_atlas_bytes
    == runtime.bundled_font_atlas_bytes);
```

源守卫还必须拒绝 `HudRenderer`、`DeathOverlayRenderer`、`PauseMenuRenderer` 中出现 `LoadFontEx` 或 `UnloadFont`。

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2`

Expected: 测试失败，显示当前实例数为 3 或 renderer 仍独立拥有字体。

- [ ] **Step 2: 实现唯一字体所有者**

`SharedUiFont` 精确公开：

```cpp
class SharedUiFont final {
public:
    ~SharedUiFont() noexcept;
    [[nodiscard]] bool initialize() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] Font font() const noexcept;
    [[nodiscard]] std::size_t atlas_bytes() const noexcept;
    [[nodiscard]] const UiGlyphPlan& plan() const noexcept;
private:
    Font font_{};
    UiGlyphPlan plan_{};
    bool ready_{};
};
```

`run_raylib_host()` 在窗口和应用目录初始化成功后创建并初始化它；依次调用：

```cpp
SharedUiFont shared_ui_font;
if (!shared_ui_font.initialize()) {
    TraceLog(LOG_ERROR, "Bundled Chinese UI font initialization failed");
    return HostExitCode::window_initialization_failed;
}
renderer.initialize_resources(shared_ui_font);
pause_menu_renderer.initialize(shared_ui_font);
```

三个 renderer 只保存 `Font` 值和 `ready` 标志，不调用字体加载/卸载。关闭顺序固定为暂停 renderer、战斗 renderer、共享字体、窗口。

- [ ] **Step 3: 更新正式材质验证的真实资源统计**

`Stage12MaterialRuntimeStatus` 增加 `std::uint8_t bundled_font_instance_count{}`；记录真实 `atlas_bytes`、相同的 `total_atlas_bytes` 和实例数 1。PowerShell validator 期望统一改为容量 640、实例 1、总字节等于单图集字节。

- [ ] **Step 4: 运行定向和相关正式测试**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY -ErrorAction SilentlyContinue }
ctest --preset windows-msvc-debug --output-on-failure -R 'platform.units|stage12.material_formal'
```

Expected: foundation 15/15；相关平台和 Stage 12 字体验证全部通过；运行时实例数为 1，图集不超过 32 MiB。

- [ ] **Step 5: 提交共享生命周期**

```powershell
git add src/platform/raylib/shared_ui_font.hpp src/platform/raylib/shared_ui_font.cpp src/platform/raylib/hud_renderer.hpp src/platform/raylib/hud_renderer.cpp src/platform/raylib/death_overlay_renderer.hpp src/platform/raylib/death_overlay_renderer.cpp src/platform/raylib/pause_menu_renderer.hpp src/platform/raylib/pause_menu_renderer.cpp src/platform/raylib/combat_renderer.hpp src/platform/raylib/combat_renderer.cpp src/platform/raylib/raylib_host.cpp tests/platform/hud_font_tests.cpp tests/platform/hud_host_integration_tests.cpp tests/platform/stage12_material_formal_game_validation.cpp tests/platform/stage12_material_validator.ps1 tests/platform/stage12_material_validator_self_test.ps1 tests/platform/stage11d_renderer_integration_guard_test.cmake
git diff --cached --check
git commit -m "refactor: share one Chinese UI font atlas"
```

---

## Plan Completion Gate

Run:

```powershell
git diff --check
$env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY -ErrorAction SilentlyContinue }
```

Expected: 工作树无非预期改动，foundation 15 cases / 0 failures；共享字体只有一个资源所有者。随后进入 `2026-08-09-full-chinese-ui-menus-hud.md`。
