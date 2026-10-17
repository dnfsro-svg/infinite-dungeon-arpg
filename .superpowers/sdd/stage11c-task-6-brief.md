# Stage 11-C Task 6 Brief — Objective, Navigation and Context Panels

## Scope

只实现 Task 6：目标、导航、上下文通知面板与纯 plan/model。不要做 Task 7 宿主接入/F1诊断抽取、Task8守卫、Task9正式截图或物品过滤。

## Files

- Modify: `src/platform/raylib/hud_view_model.hpp/.cpp`
- Modify: `src/platform/raylib/hud_renderer.hpp/.cpp`
- Modify: `tests/platform/hud_view_model_tests.cpp`
- Modify: `tests/platform/hud_render_plan_tests.cpp`
- Modify platform test registration/count only if new cases require it.

## Required interface

```cpp
struct ContextHudModel final {
    HudText96 primary{};
    HudText96 secondary{};
    HudNoticeKind primary_kind{HudNoticeKind::none};
    HudNoticeKind secondary_kind{HudNoticeKind::none};
};

void attach_notice_view(HudViewModel&, const HudNoticeView&) noexcept;
```

如避免头文件循环依赖需要前置声明或最小公共类型拆分，可做最小结构调整，但不得改变公开语义或扩展功能。

## Required behavior and TDD

- 先 RED：combat / wave delay / cleared / committing / abyss 目标文案；depth、floor room极值；四元素名称与共享颜色；完整通知优先级；重绑 E/I/P 文案；正常 HUD 不出现 `Budget`、`Saturation`、`Invalid owner`、seed。
- 实现目标/导航/上下文纯 draw plans 与实际绘制，固定缓冲/数组、中文优先、所有文本有界。
- 把既有 abyss confirmation 面板纳入 context priority，但不得改变 `abyss_hud_values` 语义。
- 门/洞隐藏结果规则不变：只渲染 committed snapshot 已公开的 READY/交互状态，不提前泄漏隐藏结果。
- 正常 HUD 移除调试字段；Task7 才把完整诊断内容接到 F1 debug panel，不要提前宿主接入。
- 无动态分配，不改 Session/Store/RNG/input/fixed tick/战斗规则。

## Required GREEN

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|stage10.formal_game.capture_content_validator|stage11b.settings_formal|platform.input_latency_source)$" --output-on-failure
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
git diff --check
```

## Commit and report

- Commit: `feat: render room navigation and context hud`
- Write ignored `.superpowers/sdd/stage11c-task-6-report.md` with RED, changes, GREEN, commit, self-review, concerns.
- 不提交 `.superpowers/sdd`。
