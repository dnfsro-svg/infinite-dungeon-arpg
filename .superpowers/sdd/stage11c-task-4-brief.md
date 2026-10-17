# Stage 11-C Task 4 Brief — HUD Font, Palette and Drawing Resources

## Scope

只实现计划 Task 4：共享中文字体计划、HUD 调色板和 `HudRenderer` 资源生命周期。不要开始 Task 5 面板视觉、Task 6 上下文面板、Task 7 宿主集成或正式截图。

## Files

- Create: `src/platform/raylib/hud_font.hpp/.cpp`
- Create: `src/platform/raylib/hud_palette.hpp`
- Create: `src/platform/raylib/hud_renderer.hpp/.cpp`
- Create: `tests/platform/hud_font_tests.cpp`
- Modify: `src/platform/raylib/death_overlay_font.hpp/.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

## Required interface

```cpp
struct HudFontPlan final {
    DeathOverlayFontPlan shared{};
    bool covers_required_text{};
};
[[nodiscard]] HudFontPlan hud_font_plan() noexcept;

class HudRenderer final {
public:
    [[nodiscard]] bool initialize() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool font_ready() const noexcept;
    void draw(const HudViewModel&, const HudLayout&) const noexcept;
private:
    Font font_{};
    bool font_ready_{};
};
```

## Required behavior and TDD

- 先写 RED：必需中文 `生命`、`护盾`、`剩余`、`出口已开放`、`保存失败`、`未分配点`、四元素名、三个状态标签均被覆盖；codepoint 唯一且在固定容量内；调色板关键颜色互异且 alpha=255。
- 扩展现有 death overlay 的固定 codepoint/UTF-8 计划，不创建第二套 UTF-8 解析器，不动态分配。
- 候选字体用 `LoadFontEx`；默认字体仅允许运行时 fallback。
- `font_ready()` 必须区分真正具有中文覆盖的加载成功，便于正式验证在缺字体时失败。
- `initialize/shutdown` 幂等、安全，无资源泄漏；Task 4 的 `draw` 只提供后续任务可扩展的安全入口，不提前实现 Task5/6 面板。
- 不改 Session/Store/RNG/input/fixed tick、质量设置、macOS、物品过滤。

## Required GREEN

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|stage11b.settings_formal)$" --output-on-failure
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
git diff --check
```

同时确认 Stage 11-B corrupt-notice 正式证据仍通过；若正式测试名称不同，以现有 CTest 列表中对应 corrupt/settings formal 项为准并在报告写明准确名称。

## Commit and report

- Commit: `feat: load chinese hud drawing resources`
- Write ignored `.superpowers/sdd/stage11c-task-4-report.md` with RED, changes, exact GREEN, resource lifecycle evidence, commit, self-review, concerns.
- 不提交 `.superpowers/sdd`。
