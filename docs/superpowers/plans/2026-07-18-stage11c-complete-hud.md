# Stage 11-C Complete HUD Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the permanent debug text wall with a production, Chinese-capable, combat-first HUD that closes the player information loop without changing gameplay state.

**Architecture:** A pure fixed-capacity `HudViewModel` validates and formats committed snapshots, a bounded `HudNoticeState` owns presentation-only notices, and `HudLayout` maps these models into three resolution-safe panel groups. `HudRenderer` owns font and drawing resources while a separate `DebugOverlayRenderer` receives all developer diagnostics; the raylib host observes production state once per presented frame and never exposes HUD setters to formal validation.

**Tech Stack:** C++17, raylib 6.0.0 static, CMake/Ninja, CTest, MSVC 19.44, Windows SDK 10.0.26100.0, fixed-capacity arrays, PowerShell evidence validator.

## Global Constraints

- Base commit is the accepted Stage 11-B `bd8c8be6dd02614c85a117d869520c57fa7aca1f`.
- Work only on `codex/stage11c-complete-hud` in `E:\game\.worktrees\stage11c-complete-hud`; do not merge `main`.
- Core, combat, dungeon, items, progression, passives, persistence, abyss, modifiers and settings remain raylib-free.
- HUD code is read-only presentation: no Session/Store calls, RNG, physical input sampling or fixed ticks.
- Normal HUD hot paths use fixed buffers and fixed arrays; 100,000 unchanged iterations allocate zero heap memory.
- Player-visible text is Chinese-first with `HP`, `XP`, `F1` retained; NotoSansSC/fallback coverage is formally verified.
- Validate 1024x576, 1280x720 and 1920x1080; 1280x720 is the formal screenshot size.
- Do not implement ground-item filtering, main menu, controller support, resolution/quality settings, macOS or Stage 11-D.
- Every task follows RED -> GREEN -> focused regression -> review -> commit. Fix every Critical, Important and Minor review finding before the next task.

---

### Task 1: Fixed-Capacity HUD View Model

**Files:**
- Create: `src/platform/raylib/hud_view_model.hpp`
- Create: `src/platform/raylib/hud_view_model.cpp`
- Create: `tests/platform/hud_view_model_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes: `dungeon::DungeonSnapshot`, `DungeonRenderStatus`, `ControlHints`.
- Produces:

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

- [ ] **Step 1: Write ViewModel RED tests**

Add cases for normal combat, no combat snapshot, HP/barrier over-range clamping, level 100, pending XP, three status tags, four biases at `UINT32_MAX`, text NUL termination and Chinese objective text. Register `hud_view_model_suite()` and update the exact platform case count.

```cpp
arpg::test::Failure clamps_invalid_resources_without_mutating_snapshot() noexcept {
    auto snapshot = hud_fixture();
    snapshot.combat->player = {};
    snapshot.combat->player.hp = 150;
    snapshot.combat->player.max_hp = 100;
    HudViewModel model{};
    build_hud_view_model(model, snapshot, {}, default_control_hints());
    ARPG_REQUIRE(model.player.hp_ratio == 1.0F);
    ARPG_REQUIRE(snapshot.combat->player.hp == 150);
    ARPG_REQUIRE(model.diagnostics.clamped_values == 1U);
    return {};
}
```

- [ ] **Step 2: Run RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
```

Expected: compile fails because `hud_view_model.hpp` and `build_hud_view_model` do not exist.

- [ ] **Step 3: Implement fixed data projection**

Implement saturating ratio helpers and `std::snprintf` formatting. Derive player status tags only when the corresponding snapshot ticks/value are non-zero. Use `progression::default_progression_rules()` only to read the level threshold; do not duplicate the XP curve.

```cpp
float safe_ratio(int value, int maximum, HudBuildDiagnostics& diagnostics) noexcept {
    if (maximum <= 0) return 0.0F;
    const int clamped = std::clamp(value, 0, maximum);
    if (clamped != value) ++diagnostics.clamped_values;
    return static_cast<float>(clamped) / static_cast<float>(maximum);
}
```

- [ ] **Step 4: Run GREEN and regression**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.module_boundary)$" --output-on-failure
```

Expected: 2/2 CTest pass and all platform cases pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib/hud_view_model.* src/platform/raylib/CMakeLists.txt tests/platform
git commit -m "feat: project production hud view model"
```

---

### Task 2: Bounded HUD Notice State and Priority

**Files:**
- Create: `src/platform/raylib/hud_notice_state.hpp`
- Create: `src/platform/raylib/hud_notice_state.cpp`
- Create: `tests/platform/hud_notice_state_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes: previous/current `DungeonSnapshot`, `DungeonRenderStatus`, committed `ControlHints`, `frame_seconds`, `paused`.
- Produces:

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

- [ ] **Step 1: Write notice RED tests**

Test the complete priority order, two-line maximum, pause timer freeze, duplicate commit/room/level signal suppression, room transition clearing, queue overflow dropping the lowest priority, save error persistence, explicit recovery ownership and rebound E/I/P labels.

```cpp
state.observe(previous, current_with_save_error(), error_status(), hints, false);
state.observe(current, current_with_hole_ready(), saved_status(), hints, false);
ARPG_REQUIRE(state.view().primary.kind == HudNoticeKind::save_error);
state.update(10.0F, true);
ARPG_REQUIRE(state.view().primary.kind == HudNoticeKind::save_error);
```

- [ ] **Step 2: Run RED**

Run the platform target. Expected: missing `hud_notice_state.hpp` compile failure.

- [ ] **Step 3: Implement deterministic priority queue**

Use insertion sort over four slots, stable order for equal priority, and saturating real-time subtraction only when `paused == false`. Persistent save/recovery notices use `seconds_left = infinity` and are removed only when status changes.

- [ ] **Step 4: Run GREEN**

Run `platform.units`, `platform.input_latency_source`, and `stage11b.settings_formal`. Expected: 3/3 CTest pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib/hud_notice_state.* src/platform/raylib/CMakeLists.txt tests/platform
git commit -m "feat: prioritize bounded hud notices"
```

---

### Task 3: Resolution-Safe HUD Layout

**Files:**
- Create: `src/platform/raylib/hud_layout.hpp`
- Create: `src/platform/raylib/hud_layout.cpp`
- Create: `tests/platform/hud_layout_tests.cpp`
- Modify: `src/platform/raylib/render_layout.hpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

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

- [ ] **Step 1: Write layout RED tests**

For 1024x576, 1280x720 and 1920x1080 assert all panels are inside the safe area, player/objective/navigation panels do not overlap, notice panels stay above the bottom abyss confirmation area, and the combat exclusion rectangle remains free. Reject zero/negative viewport sizes with an empty safe layout.

- [ ] **Step 2: Run RED**

Expected: missing `make_hud_layout` compile failure.

- [ ] **Step 3: Implement clamped 16:9 layout**

Use 1280x720 logical coordinates, `scale = clamp(min(width/1280, height/720), 0.8, 1.5)`, a clamped 16-pixel logical margin and anchored panels. Do not scale bar aspect ratios independently.

- [ ] **Step 4: Run GREEN**

Run `platform.units`. Expected: all three resolution matrices pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib/hud_layout.* src/platform/raylib/render_layout.hpp src/platform/raylib/CMakeLists.txt tests/platform
git commit -m "feat: define responsive hud safe layout"
```

---

### Task 4: HUD Font, Palette and Drawing Resources

**Files:**
- Create: `src/platform/raylib/hud_font.hpp`
- Create: `src/platform/raylib/hud_font.cpp`
- Create: `src/platform/raylib/hud_palette.hpp`
- Create: `src/platform/raylib/hud_renderer.hpp`
- Create: `src/platform/raylib/hud_renderer.cpp`
- Create: `tests/platform/hud_font_tests.cpp`
- Modify: `src/platform/raylib/death_overlay_font.hpp`
- Modify: `src/platform/raylib/death_overlay_font.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

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

- [ ] **Step 1: Write font/palette RED tests**

Assert required Chinese strings (`生命`, `护盾`, `剩余`, `出口已开放`, `保存失败`, `未分配点`, all four elements and status labels) are covered, codepoints are unique and within fixed capacity, and palette colors are distinct with alpha 255.

- [ ] **Step 2: Run RED**

Expected: missing HUD font and palette APIs.

- [ ] **Step 3: Implement shared font plan and renderer lifecycle**

Extend the shared fixed codepoint plan rather than creating a second UTF-8 parser. Load candidates with `LoadFontEx`, use the default font only as runtime fallback, and expose `font_ready()` so formal validation fails when Chinese coverage is unavailable.

- [ ] **Step 4: Run GREEN and resource lifecycle smoke**

Build `arpg_platform_tests` and `arpg_game`; run `platform.units` and Stage 11-B corrupt-notice formal evidence to prove shared font behavior remains valid.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib/hud_font.* src/platform/raylib/hud_palette.hpp src/platform/raylib/hud_renderer.* src/platform/raylib/death_overlay_font.* src/platform/raylib/CMakeLists.txt tests/platform
git commit -m "feat: load chinese hud drawing resources"
```

---

### Task 5: Player Panel and Monster Bar Visual Language

**Files:**
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Create: `tests/platform/hud_render_plan_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes Task 1 `PlayerHudModel`, Task 3 `HudLayout`, Task 4 palette/font.
- Produces pure draw plans for headless verification:

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

- [ ] **Step 1: Write render-plan RED tests**

Cover health only, barrier conditional visibility, XP MAX, low-health frequency bounds, three tags, bar ratios and stable layout. Assert actor monster HP/shield/break bars use the same palette IDs and remain world-space.

- [ ] **Step 2: Run RED**

Expected: missing `make_player_panel_plan`.

- [ ] **Step 3: Implement player panel and palette reuse**

Draw Chinese labels with `DrawTextEx`, bars with bounded rectangles and low-health border pulse at <= 2 Hz. Add the missing monster break bar only from existing `break_value/max_break`; do not add target locking or alter monster snapshots.

- [ ] **Step 4: Run GREEN**

Run `platform.units`, `combat.units` and `architecture.combat_no_raylib`. Expected: 3/3 pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib/hud_renderer.cpp src/platform/raylib/actor_renderer.cpp tests/platform
git commit -m "feat: render player and monster combat bars"
```

---

### Task 6: Objective, Navigation and Context Panels

**Files:**
- Modify: `src/platform/raylib/hud_view_model.hpp`
- Modify: `src/platform/raylib/hud_view_model.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/hud_renderer.hpp`
- Modify: `tests/platform/hud_view_model_tests.cpp`
- Modify: `tests/platform/hud_render_plan_tests.cpp`

**Interfaces:**

```cpp
struct ContextHudModel final {
    HudText96 primary{};
    HudText96 secondary{};
    HudNoticeKind primary_kind{HudNoticeKind::none};
    HudNoticeKind secondary_kind{HudNoticeKind::none};
};

void attach_notice_view(HudViewModel&, const HudNoticeView&) noexcept;
```

- [ ] **Step 1: Write panel RED tests**

Test combat/wave delay/cleared/committing/abyss objectives; depth and floor room extremes; element names and colors; all notice priorities; rebound E/I/P text; and normal HUD absence of `Budget`, `Saturation`, `Invalid owner` and seed text.

- [ ] **Step 2: Run RED**

Expected: panel model assertions fail because the normal HUD still contains old debug text.

- [ ] **Step 3: Implement objective/navigation/context draw plans**

Move the existing abyss confirmation panel into context priority without changing `abyss_hud_values` semantics. Keep door/hole hidden-result rules: only render READY/interaction state already present in the committed snapshot.

- [ ] **Step 4: Run GREEN and Stage 10 regression**

Run `platform.units`, `stage10.formal_game.capture_content_validator`, `stage11b.settings_formal` and `platform.input_latency_source`. Expected: 4/4 pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib/hud_view_model.* src/platform/raylib/hud_renderer.* tests/platform
git commit -m "feat: render room navigation and context hud"
```

---

### Task 7: Debug Overlay Extraction and Host Integration

**Files:**
- Create: `src/platform/raylib/debug_overlay_renderer.hpp`
- Create: `src/platform/raylib/debug_overlay_renderer.cpp`
- Modify: `src/platform/raylib/debug_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Create: `tests/platform/hud_host_integration_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

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

- [ ] **Step 1: Write integration RED tests and source guard**

Assert one HUD observation per presented frame including frames owned by recovery/death overlays, no gameplay state changes from that observation, notification timers freeze while paused, settings Apply refreshes committed hints, and `CombatRenderer::draw` consumes prebuilt models without Session/input calls. Add a CMake source test rejecting old normal HUD strings and direct input/RNG/Store/fixed_tick tokens in HUD files.

- [ ] **Step 2: Run RED**

Run `platform.units` and the new `stage11c.architecture.hud_boundaries`; expected missing test/API failure.

- [ ] **Step 3: Integrate lifecycle and move diagnostics**

Initialize/shutdown `HudRenderer` beside `DeathOverlayRenderer`; observe snapshots before drawing; draw normal HUD before overlays; draw debug overlay only when F1 is true. Preserve the single physical snapshot and existing pause/death priorities.

- [ ] **Step 4: Run GREEN and input/death regression**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.host_input_source|platform.input_latency_source|stage11.death_formal.five_paths|stage11b.settings_formal|stage11c.architecture.hud_boundaries)$" --output-on-failure
```

Expected: 6/6 pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: integrate production hud and debug overlay"
```

---

### Task 8: Allocation Stress and Architecture Mutation Guards

**Files:**
- Create: `tests/platform/hud_stress_tests.cpp`
- Create: `tests/platform/stage11c_hud_architecture_guard_test.cmake`
- Create: `tests/platform/stage11c_hud_architecture_guard_self_test.cmake`
- Create: `tests/platform/stage11c_hud_bad_source.txt`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- Produces CTest `stage11c.hud_stress.zero_alloc_100k`.
- Produces CTest `stage11c.architecture.hud_boundaries` and mutation self-test.

- [ ] **Step 1: Write RED stress and guard registrations**

Stress 100,000 iterations across unchanged, low-health, status-heavy, max-counter and priority-overflow snapshots. Probe ViewModel build, notice refresh and layout; renderer GPU calls are excluded but draw-plan creation is included.

Guard mutations must independently inject: `GetKeyPressed`, `DungeonSession`, `fixed_tick`, `SettingsStore`, `std::string`, `std::vector`, old `Budget` text in normal HUD, and a fourth visible status tag.

- [ ] **Step 2: Run RED**

Run the two new names before implementation. Expected: missing target/guard fixture failure.

- [ ] **Step 3: Implement stress filter and mutation self-checks**

Use `ARPG_STAGE11C_HUD_STRESS=1` as the dedicated environment filter and a 300-second timeout. The self-test copies production HUD sources, applies one mutation at a time, invokes the main guard, and checks the named rejection reason.

- [ ] **Step 4: Run focused tests**

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11c\.(hud_stress|architecture)" --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|combat.units|dungeon.units)$" --output-on-failure
```

Expected: new guard/stress tests pass and 3/3 regressions pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/platform
git commit -m "test: stress stage 11c hud boundaries"
```

---

### Task 9: Real Raylib HUD Evidence and Anti-Injection Guard

**Files:**
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Create: `tests/platform/stage11c_hud_formal_game_validation.cpp`
- Create: `tests/platform/stage11c_hud_formal_validator.ps1`
- Create: `tests/platform/stage11c_hud_evidence_guard_test.cmake`
- Create: `tests/platform/stage11c_hud_evidence_guard_self_test.cmake`
- Create: `tests/platform/stage11c_hud_bad_formal_input.txt`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**

```cpp
enum class Stage11CHudValidationScenario : std::uint8_t {
    none, normal_combat, low_health_status, cleared_exit,
    abyss_abandon, level_up_points, debug_overlay
};
```

Validation config may select a deterministic scenario and post-Present capture path. It may inject only physical edges and production fixture seeds; it cannot set `HudViewModel`, `HudNoticeState`, combat snapshots, room phases or logical actions.

- [ ] **Step 1: Register formal RED harness and validator**

Require six fresh 1280x720 PNGs: `combat.png`, `low-health.png`, `cleared.png`, `abyss-warning.png`, `level-up.png`, `debug.png`. Summary fields include scenario, HUD rectangles, player values, status tags, objective, notice kinds, navigation values, font readiness, F1 state, production snapshot hash and `result=pass`.

- [ ] **Step 2: Run RED**

Run `ctest -R '^stage11c.hud_(formal|evidence)'`. Expected: missing formal target or evidence report failure.

- [ ] **Step 3: Implement production-only scenarios**

Drive the existing host through fixed seeds, physical input and real Session actions. Capture only through the existing unique helper after `EndDrawing()`. Use independent child processes per scenario to avoid raylib reinitialization leakage.

- [ ] **Step 4: Implement validator and mutation guards**

PowerShell validates PNG signature, exact dimensions, freshness, report fields and a deterministic sample of region pixels. The guard requires `run_raylib_host`, physical sample->map->pause gate->submit order, production snapshot hashes and post-Present capture. Mutations reject direct ViewModel assignment, TestAccess, logical action queueing, pre-Present capture, fake font-ready, fake `result=pass` and bypassed Session progression.

- [ ] **Step 5: Run formal evidence and inspect images**

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11c\.hud_(formal|evidence)" --output-on-failure
```

Expected: formal, validator, guard and self-test all pass; manual inspection confirms Chinese text and all six scenarios.

- [ ] **Step 6: Commit**

```powershell
git add src/platform/raylib/raylib_host.* tests/platform
git commit -m "test: validate stage 11c hud in raylib"
```

---

### Task 10: Documentation, Full Gates and Milestone Stop

**Files:**
- Modify: `README.md`
- Create: `docs/validation/stage11c-complete-hud.md`

**Interfaces:**
- Produces the final player-facing controls/HUD description and requirement-to-CTest evidence map.

- [ ] **Step 1: Document exact visible behavior and exclusions**

README describes player panel, room objective, navigation, context priority and F1 diagnostics. Validation maps every design section to exact tests, lists formal evidence paths and explicitly excludes item filtering, main menu, controller, quality/resolution, macOS and Stage 11-D.

- [ ] **Step 2: Fresh Debug full gate**

```powershell
.\scripts\Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug --clean-first
ctest --preset windows-msvc-debug --output-on-failure
```

Expected: configure/build succeed and every CTest passes, including Stage 11-C stress/formal/evidence.

- [ ] **Step 3: Fresh Release full gate**

```powershell
.\scripts\Configure.ps1 -Preset windows-msvc-release -Fresh
cmake --build --preset windows-msvc-release --clean-first
ctest --preset windows-msvc-release --output-on-failure
```

Expected: all tests pass and Release evidence is freshly generated with raylib 6.0.0 static linkage.

- [ ] **Step 4: Audit branch boundaries**

```powershell
git diff --check bd8c8be6dd02614c85a117d869520c57fa7aca1f..HEAD
git status --short
git diff --stat bd8c8be6dd02614c85a117d869520c57fa7aca1f..HEAD
git ls-files | rg "(^|/)(out|build)/|\.png$|\.log$|\.sav$|settings-[ab]\.bin$"
```

Expected: clean status, no generated Stage 11-C artifact tracked, and only HUD source/tests/docs in scope.

- [ ] **Step 5: Two independent full-diff reviews**

Both reviewers inspect `bd8c8be..HEAD`, the design, ViewModel data boundary, notice priority, three resolutions, zero allocation, input/death/settings regression, Chinese font, formal screenshots and mutation coverage. Fix every Critical/Important/Minor finding in separate commits, rerun focused tests, then rerun complete Debug CTest after the final fix.

- [ ] **Step 6: Commit docs and stop**

```powershell
git add README.md docs/validation/stage11c-complete-hud.md
git commit -m "docs: complete stage 11c hud milestone"
```

Expected: worktree clean on `codex/stage11c-complete-hud`; do not merge `main` or start Stage 11-D.

---

## Requirement Coverage

| Requirement | Tasks |
| --- | --- |
| Read-only fixed-capacity production HUD model | 1 |
| Priority, dedupe, pause-frozen notices | 2 |
| 1024/1280/1920 safe layout | 3 |
| Chinese font, palette and resource lifecycle | 4 |
| Player bars, status tags and monster break bar | 5 |
| Room objective, navigation and context prompts | 6 |
| F1-only diagnostics and host integration | 7 |
| 100k zero allocation and architecture mutations | 8 |
| Real raylib screenshots and anti-injection evidence | 9 |
| Fresh Debug/Release, docs and full reviews | 10 |
