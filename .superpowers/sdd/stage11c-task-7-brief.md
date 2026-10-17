# Stage 11-C Task 7 Brief — Debug Overlay Extraction and Host Integration

## Scope

只实现 Task 7：生产 HUD 宿主生命周期、每呈现帧观察、旧诊断抽取到 F1 overlay。不要实现 Task8 架构/分配守卫、Task9正式截图、质量设置、控制器或物品过滤。

## Files

- Create: `src/platform/raylib/debug_overlay_renderer.hpp/.cpp`
- Modify: `src/platform/raylib/debug_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp/.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Create: `tests/platform/hud_host_integration_tests.cpp`
- Modify test CMake/main registration/count.

## Required interfaces

```cpp
class DebugOverlayRenderer final {
public:
    void draw(const dungeon::DungeonSnapshot&, const DungeonRenderStatus&,
        const CombatFeedback&, bool audio_ready,
        const HudBuildDiagnostics&, std::uint32_t notice_drops,
        std::uint64_t binding_revision) const noexcept;
};

class CombatRenderer final {
public:
    void observe_hud(const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus&, const ControlHints&,
        float frame_seconds, bool paused) noexcept;
};
```

## Required TDD and behavior

- 先 RED：每个 presented frame 恰观察一次，包括 recovery/death overlay 拥有的帧；观察不改变 gameplay state；暂停冻结通知计时；Settings Apply 后使用 committed hints/revision；`CombatRenderer::draw` 只消费预构建 model，不调用 Session/input。
- Task8 才注册源级架构 guard；本任务不得创建同名/重复 guard。
- `HudRenderer` 与 `DeathOverlayRenderer` 并列 initialize/shutdown，失败路径正确；正式中文缺失时可由后续正式验证失败，但宿主不能泄漏资源。
- 每帧先 observe snapshots，再绘制；normal HUD 在 overlays 之前；debug overlay 仅 F1 true。
- 保持一次物理输入 snapshot、现有 pause/death/recovery 优先级和操作延迟路径，不重复采样键盘。
- 将 Budget/Saturation/Invalid owner/seed 等旧诊断仅放入 `DebugOverlayRenderer`；普通 HUD 永不显示。
- 固定容量、无动态分配；HUD观察不得调用Session/Store/RNG/fixed tick。

## Required GREEN

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.host_input_source|platform.input_latency_source|stage11.death_formal.five_paths|stage11b.settings_formal)$" --output-on-failure
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
git diff --check
```

## Commit and report

- Commit: `feat: integrate production hud and debug overlay`
- Write ignored `.superpowers/sdd/stage11c-task-7-report.md` with RED, changes, lifecycle/order/input evidence, exact GREEN, commit, self-review, concerns.
- 不提交 `.superpowers/sdd`。
