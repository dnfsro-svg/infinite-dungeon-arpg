# Stage 11-C Task 5 Brief — Player Panel and Monster Bar Visual Language

## Scope

只实现 Task 5：玩家面板、纯绘制计划、怪物 HP/护盾/韧性条共享视觉语言。不要实现 Task 6 上下文面板、Task 7 宿主集成、正式截图或物品过滤。

## Files

- Modify: `src/platform/raylib/hud_renderer.hpp/.cpp`（计划的纯 plan 接口必须在头文件公开，允许最小头文件改动）
- Modify: `src/platform/raylib/actor_renderer.cpp`（如测试需要纯 palette plan 声明，可对其现有头文件作最小接口扩展）
- Create: `tests/platform/hud_render_plan_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

## Required pure interface

```cpp
enum class HudBarKind : std::uint8_t { health, barrier, experience };
struct HudBarPlan final { HudRect bounds{}; float ratio{}; HudBarKind kind{}; };
struct PlayerPanelPlan final {
    std::array<HudBarPlan, 3> bars{};
    std::uint8_t bar_count{};
    bool low_health_emphasis{};
    std::array<HudStatusTagKind, 3> tags{};
    std::uint8_t tag_count{};
};
[[nodiscard]] PlayerPanelPlan make_player_panel_plan(
    const PlayerHudModel&, const HudLayout&, float presentation_seconds) noexcept;
```

## Required behavior and TDD

- 先 RED：仅生命；护盾条件显示；XP MAX；低生命强调频率边界；3 状态标签；ratio clamp/稳定布局。
- 怪物 HP/护盾/韧性条使用同一 palette IDs，且仍为 world-space；通过纯 plan/headless 断言，不用像素注入。
- 玩家面板用 `DrawTextEx` 绘制中文标签；矩形严格在布局 bounds 内。
- 低生命边框脉冲频率 <=2Hz，完全由 `presentation_seconds` 驱动，不改变模拟状态。
- 仅从现有 `break_value/max_break` 加缺失怪物韧性条；不得改怪物快照、目标锁定、战斗规则。
- 固定数组、无动态分配；无 Session/Store/RNG/input/fixed tick 改动。

## Required GREEN

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|combat.units|architecture.combat_no_raylib)$" --output-on-failure
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
git diff --check
```

## Commit and report

- Commit: `feat: render player and monster combat bars`
- Write ignored `.superpowers/sdd/stage11c-task-5-report.md` with RED, changes, GREEN, exact commit, self-review, concerns.
- 不提交 `.superpowers/sdd`。
