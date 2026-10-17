# Stage 11-C Task 2 Brief — Bounded HUD Notice State and Priority

## Scope

只实现计划 Task 2。不要开始 HUD 布局、绘制、宿主集成、正式截图、物品过滤或 Stage 11-D。

## Files

- Create: `src/platform/raylib/hud_notice_state.hpp`
- Create: `src/platform/raylib/hud_notice_state.cpp`
- Create: `tests/platform/hud_notice_state_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

## Required interface

```cpp
enum class HudNoticeKind : std::uint8_t {
    none, save_error, recovery_required, abyss_abandon,
    hole_interact, exit_ready, room_clear, reward, level_up,
    passive_points, inventory, passive_tree
};

struct HudNotice final {
    HudNoticeKind kind{HudNoticeKind::none};
    std::uint8_t priority{};
    float seconds_left{};
    HudText96 text{};
};

struct HudNoticeView final {
    HudNotice primary{};
    HudNotice secondary{};
};

class HudNoticeState final {
public:
    void observe(const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& status,
        const ControlHints& hints,
        bool recovery_required) noexcept;
    void update(float frame_seconds, bool paused) noexcept;
    void clear_room_context() noexcept;
    [[nodiscard]] HudNoticeView view() const noexcept;
    [[nodiscard]] std::uint32_t dropped_count() const noexcept;
private:
    std::array<HudNotice, 4> notices_{};
    std::uint32_t dropped_count_{};
    std::uint64_t last_commit_generation_{};
    std::uint64_t last_room_index_{};
    std::uint64_t last_room_experience_{};
    std::uint8_t last_level_{};
};
```

## Required behavior and tests

- 先写完整 RED：完整优先级顺序、最多两行、暂停时计时冻结、重复 commit/room/level 信号抑制、换房清理、4 槽溢出丢最低优先级、存档错误持久、恢复提示由显式参数拥有、E/I/P 文案使用已重绑的 `ControlHints`。
- 使用 4 个固定槽和插入排序；同优先级稳定。
- 仅在 `paused == false` 时对真实帧时间做饱和减法。
- 存档错误/恢复提示的 `seconds_left` 为 infinity，只有状态变化才移除。
- 所有提示文本写入固定缓冲，必须有边界且 NUL 终止；不得动态分配。
- 不修改 Session/Store、RNG、输入采样、fixed tick；不加入任何 raylib 绘制。

## Required GREEN

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.input_latency_source|stage11b.settings_formal)$" --output-on-failure
git diff --check
```

## Commit and report

- Commit message: `feat: prioritize bounded hud notices`
- Write `.superpowers/sdd/stage11c-task-2-report.md` with Status, RED evidence, changed files/behavior, exact GREEN evidence, commit hash, self-review, concerns.
- 不得把 `.superpowers/sdd` 报告加入提交。
