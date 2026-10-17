# Stage 11-C Task 3 Brief — Resolution-Safe HUD Layout

## Scope

只实现计划 Task 3。不要开始绘制资源、HUD 面板、宿主集成、正式截图、物品过滤或 Stage 11-D。

## Files

- Create: `src/platform/raylib/hud_layout.hpp`
- Create: `src/platform/raylib/hud_layout.cpp`
- Create: `tests/platform/hud_layout_tests.cpp`
- Modify: `src/platform/raylib/render_layout.hpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

## Required interface

```cpp
struct HudRect final { float x{}, y{}, width{}, height{}; };
struct HudLayout final {
    HudRect safe_area{};
    HudRect player_panel{};
    HudRect objective_panel{};
    HudRect navigation_panel{};
    HudRect primary_notice{};
    HudRect secondary_notice{};
    HudRect debug_panel{};
    HudRect combat_exclusion{};
    float scale{1.0F};
};

[[nodiscard]] HudLayout make_hud_layout(
    int screen_width, int screen_height, bool debug_visible) noexcept;
[[nodiscard]] bool hud_rects_overlap(HudRect lhs, HudRect rhs) noexcept;
[[nodiscard]] bool hud_rect_inside(HudRect inner, HudRect outer) noexcept;
```

## Required behavior and tests

- 严格 TDD，先写 RED 并记录缺失 `make_hud_layout`（或等价生产实现缺失）失败。
- 覆盖 1024x576、1280x720、1920x1080：所有可见面板在 safe area 内；player/objective/navigation 不重叠；两条 notice 位于底部深渊确认区上方；combat exclusion 保持空闲。
- 0 或负 viewport 返回空 safe layout。
- 以 1280x720 逻辑坐标布局；`scale = clamp(min(width/1280, height/720), 0.8, 1.5)`；逻辑 16px margin 经 clamp；面板锚定。
- 不独立缩放血条等纵横比。
- 纯数学、`noexcept`、无 raylib 绘制/动态分配/Session/Store/RNG/input/fixed tick。

## Required GREEN

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^platform.units$" --output-on-failure
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
git diff --check
```

## Commit and report

- Commit: `feat: define responsive hud safe layout`
- Write ignored `.superpowers/sdd/stage11c-task-3-report.md` containing Status, RED, changed behavior, exact GREEN, commit, self-review, concerns.
- 不提交 `.superpowers/sdd` 文件。
