# Stage 11-C Task 1 Brief — Fixed-Capacity HUD View Model

Worktree: `E:\game\.worktrees\stage11c-complete-hud`
Base before task: `9d8e5cd`

## Binding global constraints

- C++17, raylib 6.0.0, MSVC 19.44 / SDK 26100.
- HUD is read-only presentation: no Session/Store calls, RNG, physical input sampling or fixed tick.
- Hot paths use fixed arrays and fixed buffers; no dynamic containers.
- Core/combat/dungeon/items/progression/passives/persistence/settings remain raylib-free.
- Do not implement Task 2+, item filtering, menu/controller/quality/macOS/Stage 11-D.
- Strict TDD: add tests and record expected RED before production code.

## Files

- Create `src/platform/raylib/hud_view_model.hpp`
- Create `src/platform/raylib/hud_view_model.cpp`
- Create `tests/platform/hud_view_model_tests.cpp`
- Modify `src/platform/raylib/CMakeLists.txt`
- Modify `tests/platform/CMakeLists.txt`
- Modify `tests/platform/platform_test_main.cpp`

## Exact interface

```cpp
enum class HudStatusTagKind : std::uint8_t { slow, corrosion, invulnerable };

struct HudText96 final {
    std::array<char, 96> bytes{};
    bool truncated{};
};

struct PlayerHudModel final {
    bool visible{};
    int hp{};
    int max_hp{};
    int barrier{};
    int max_barrier{};
    float hp_ratio{};
    float barrier_ratio{};
    std::uint8_t level{};
    std::uint64_t experience{};
    std::uint64_t required_experience{};
    float experience_ratio{};
    std::uint8_t unspent_passive_points{};
    std::array<HudStatusTagKind, 3> status_tags{};
    std::uint8_t status_tag_count{};
};

struct RoomHudModel final {
    HudText96 objective{};
    HudText96 secondary{};
    bool abyss{};
    std::uint8_t remaining_targets{};
};

struct NavigationHudModel final {
    std::uint64_t depth{};
    std::uint64_t floor_room{};
    dungeon::DungeonElement ecology{};
    std::array<std::uint32_t, 4> biases{};
};

struct HudBuildDiagnostics final {
    std::uint32_t clamped_values{};
    std::uint32_t truncated_texts{};
    bool combat_snapshot_missing{};
};

struct HudViewModel final {
    PlayerHudModel player{};
    RoomHudModel room{};
    NavigationHudModel navigation{};
    HudBuildDiagnostics diagnostics{};
};

void build_hud_view_model(HudViewModel& output,
    const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& hints) noexcept;
```

## Required TDD cycle

1. Add tests for normal combat, absent combat snapshot, HP/barrier over-range clamping without mutating the snapshot, level 100, pending XP, slow/corrosion/invulnerability tags, tag capacity three, all four `UINT32_MAX` biases, Chinese objective formatting, truncation and NUL termination.
2. Register `hud_view_model_suite()` and update the exact `platform_test_main.cpp` expected case count.
3. Build before production files. Record RED caused by missing HUD API, not by a typo.
4. Implement fixed projection. Use `std::snprintf`, saturating ratios and `progression::default_progression_rules()` for thresholds. Derive tags only from non-zero snapshot values/ticks. Do not use `TextFormat`, raylib drawing, dynamic strings or duplicated progression curves.
5. Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.module_boundary)$" --output-on-failure
git diff --check
```

6. Self-review scope and API. Commit exactly the Task 1 files with message `feat: project production hud view model`.

## Report contract

Write `.superpowers/sdd/stage11c-task-1-report.md` containing:

- Status: DONE / DONE_WITH_CONCERNS / NEEDS_CONTEXT / BLOCKED
- RED command and expected failure excerpt
- changed files and behavior
- GREEN commands with exact counts/results
- commit hash
- self-review findings and any concerns
