# Stage 11-D Ground Loot Filter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add persistent three-level ground-loot filtering, fixed-capacity world labels, filter-aware automatic pickup, and transactional pickup feedback without changing loot generation or the character save.

**Architecture:** Settings owns the persisted user preset and migrates its 44-byte file from V1 to V2. The host maps committed settings to a small dungeon-owned `AutoPickupPolicy` passed on every fixed tick, while a raylib-free view builder projects the snapshot into a fixed-capacity label plan. A presentation-only observer turns confirmed pickup commits into one-shot HUD notices.

**Tech Stack:** C++17, raylib 6.0.0 static, CMake/Ninja, MSVC 19.44, the repository's allocation probe and custom test framework.

## Global Constraints

- Windows x64 remains the only required platform for Stage 11-D.
- raylib remains exactly 6.0.0 and is linked statically.
- The character V6 checkpoint format and all item generation/drop probabilities remain byte-for-byte unchanged.
- Filter presets are exactly `show_all`, `magic_or_better`, and `rare_only`.
- Abyss chest rewards are always visible and always eligible for automatic pickup.
- Filtered ordinary drops remain in the production ground pool and reappear when the preset is relaxed.
- No manual pickup binding, custom rule language, external filter file, item deletion, or Stage 12 work.
- New label, policy, and feedback hot paths use fixed capacity and allocate zero heap memory.

---

### Task 1: Settings Model and V1-to-V2 Codec

**Files:**
- Modify: `src/platform/settings/settings_types.hpp`
- Modify: `src/platform/settings/settings_types.cpp`
- Modify: `src/platform/settings/settings_codec.cpp`
- Modify: `tests/settings/settings_types_tests.cpp`
- Modify: `tests/settings/settings_codec_tests.cpp`
- Modify: `tests/settings/settings_store_tests.cpp`
- Modify: `tests/settings/settings_stress_tests.cpp`

**Interfaces:**
- Produces: `enum class LootFilterMode : std::uint8_t { show_all, magic_or_better, rare_only }`.
- Produces: `SettingsData::loot_filter_mode` defaulting to `show_all`.
- Produces: `const char* loot_filter_label(LootFilterMode) noexcept`.
- Preserves: `kSettingsEncodedSize == 44U`; V2 uses byte 23 and V1 decodes to `show_all`.

- [ ] **Step 1: Write failing model and codec tests**

Add tests that require all three modes to validate, require an out-of-range mode to return a new `SettingsValidationError::loot_filter_mode`, require defaults to use `show_all`, require encoded bytes 8-9 to hold format `2`, and require byte 23 to round-trip `rare_only`. Build a real V1 record by changing the format bytes to `1`, setting byte 23 to zero, and refreshing CRC; assert it decodes to `show_all`.

```cpp
SettingsData values = arpg::settings::default_settings();
ARPG_REQUIRE(values.loot_filter_mode == LootFilterMode::show_all);
values.loot_filter_mode = LootFilterMode::rare_only;
const auto encoded = arpg::settings::encode_settings(values);
ARPG_REQUIRE(encoded[8] == 2U && encoded[9] == 0U);
ARPG_REQUIRE(encoded[23] == static_cast<std::uint8_t>(LootFilterMode::rare_only));
const auto decoded = arpg::settings::decode_settings(encoded.data(), encoded.size());
ARPG_REQUIRE(decoded.error == SettingsCodecError::none);
ARPG_REQUIRE(decoded.settings.loot_filter_mode == LootFilterMode::rare_only);
```

- [ ] **Step 2: Run the settings suite and verify RED**

Run:

```powershell
cmake --build out/build/windows-msvc-debug --target arpg_settings_tests arpg_settings_stress_tests
$env:ARPG_TEST_FILTER='settings.'; .\out\build\windows-msvc-debug\bin\arpg_settings_tests.exe
```

Expected: compilation fails because `LootFilterMode` and `loot_filter_mode` do not exist.

- [ ] **Step 3: Implement the model and dual-format codec**

Define the enum and field, add range validation and stable labels. Encode format 2 with byte 23. Decode formats 1 and 2: V1 still rejects nonzero byte 23 and migrates to `show_all`; V2 interprets byte 23 and validates it. Keep bytes 34-39 reserved and keep the same CRC coverage.

```cpp
enum class LootFilterMode : std::uint8_t {
    show_all,
    magic_or_better,
    rare_only,
};

struct SettingsData final {
    std::uint8_t master_sfx_percent{100};
    WindowMode window_mode{WindowMode::windowed};
    bool vsync_enabled{true};
    LootFilterMode loot_filter_mode{LootFilterMode::show_all};
    std::array<StableKey, static_cast<std::size_t>(SettingAction::count)> bindings{};
    std::uint64_t revision{};
};
```

- [ ] **Step 4: Update store/stress equality helpers and verify GREEN**

Every test helper comparing `SettingsData` must include `loot_filter_mode`. Extend the 1000-cycle stress trace to rotate through all three modes and verify mixed V1/V2 slots choose the highest legal revision. Rebuild and run both settings executables; expected all tests pass and allocation counts remain zero.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/settings tests/settings
git commit -m "feat: persist loot filter settings"
```

### Task 2: Pause Settings Draft Semantics

**Files:**
- Modify: `src/platform/raylib/pause_menu_state.cpp`
- Modify: `src/platform/raylib/pause_menu_view.hpp`
- Modify: `src/platform/raylib/pause_menu_view.cpp`
- Modify: `tests/platform/pause_menu_state_tests.cpp`
- Modify: `tests/platform/pause_menu_view_tests.cpp`

**Interfaces:**
- Settings row 3 becomes `Loot Filter`; binding rows move to 4-13.
- Reset, Apply, Cancel move to rows 14, 15, 16.
- Produces: `kPauseMenuRowCapacity == 17U`.

- [ ] **Step 1: Write failing state/view tests**

Require navigation to wrap 17 rows. On row 3, left cycles `show_all -> rare_only`, right/Enter cycles forward, and the command is `preview`. Require reset to restore `show_all`, cancel to restore committed mode, and view text to include both draft and saved filter labels.

```cpp
state.screen = platform::PauseScreen::settings;
state.selected_row = 3U;
ARPG_REQUIRE(update(state, input_for(InputKind::right)) ==
    platform::PauseCommand::preview);
ARPG_REQUIRE(state.draft.loot_filter_mode ==
    settings::LootFilterMode::magic_or_better);
```

- [ ] **Step 2: Build platform tests and verify RED**

Run `cmake --build out/build/windows-msvc-debug --target arpg_platform_tests`; expected failures for the old 16-row layout.

- [ ] **Step 3: Implement row mapping and text**

Add `update_loot_filter`, update row constants, binding offsets, reset/apply/cancel row numbers, layout capacity, and `build_settings_rows`. Do not publish settings inside state logic; keep the existing preview/apply/rollback command boundary.

- [ ] **Step 4: Run focused tests and verify GREEN**

Run:

```powershell
$env:ARPG_TEST_FILTER='platform.pause'; .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

Expected: pause state, view, render-plan, host-gate, and zero-allocation tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib/pause_menu_* tests/platform/pause_menu_*
git commit -m "feat: add loot filter setting control"
```

### Task 3: Dungeon-Owned Automatic Pickup Policy

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `tests/dungeon/dungeon_loot_drop_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_reward_tests.cpp`
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`

**Interfaces:**
- Produces: `struct AutoPickupPolicy { items::ItemRarity minimum_rarity{items::ItemRarity::normal}; }`.
- Produces: `bool auto_pickup_eligible(const GroundItem&, AutoPickupPolicy) noexcept`.
- Changes: `DungeonSession::tick(combat::MovementInput, AutoPickupPolicy = {}) noexcept`.
- Changes: `request_nearby_pickups(combat::Vec3, AutoPickupPolicy = {}) noexcept`.
- Preserves: `request_pickup(std::uint16_t)` ignores policy.

- [ ] **Step 1: Write failing policy and transaction tests**

Inject normal, magic, and rare ordinary ground items inside the pickup radius. Require `magic_or_better` to leave normal active and pick magic first; require `rare_only` to pick rare; require an ordinary normal plus normal abyss reward to pick the abyss reward. Require an explicit normal `request_pickup` to remain accepted.

```cpp
session.request_nearby_pickups(player,
    {items::ItemRarity::magic});
ARPG_REQUIRE(arpg::test::ground_items(session)[normal_ordinal].active);
ARPG_REQUIRE(session.pending_save_view()->pickup_ordinal == magic_ordinal);
```

- [ ] **Step 2: Build dungeon tests and verify RED**

Run `cmake --build out/build/windows-msvc-debug --target arpg_dungeon_tests`; expected compilation failure for the missing policy overload.

- [ ] **Step 3: Implement minimal eligibility and thread policy through tick**

```cpp
bool auto_pickup_eligible(
    const GroundItem& ground, AutoPickupPolicy policy) noexcept {
    if (!ground.active) return false;
    if (ground.source == GroundItemSource::abyss_chest) return true;
    return ground.source == GroundItemSource::monster_drop
        && static_cast<std::uint8_t>(ground.item.rarity)
            >= static_cast<std::uint8_t>(policy.minimum_rarity);
}
```

Call the policy-aware nearby scan at the same point before exit attempts. Do not add policy checks to explicit pickup transactions.

- [ ] **Step 4: Verify focused and deterministic tests GREEN**

Run filtered `dungeon.loot`, `dungeon.abyss_reward`, and `dungeon.stress` suites. Expected: ordering, persistence transactions, room exit behavior, and deterministic traces pass.

- [ ] **Step 5: Commit**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: filter automatic loot pickup"
```

### Task 4: Production Snapshot Metadata and Runtime Mapping

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/platform/raylib/dungeon_runtime.hpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/dungeon/dungeon_loot_drop_tests.cpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`

**Interfaces:**
- Adds to `GroundItemSnapshot`: `std::uint8_t base_id`, `std::uint8_t item_level`.
- Adds to `DungeonSnapshot`: `std::optional<std::uint16_t> pending_pickup_ordinal`.
- Changes: `DungeonRuntime::fixed_tick(combat::MovementInput, dungeon::AutoPickupPolicy = {}) noexcept`.
- Produces: `dungeon::AutoPickupPolicy loot_pickup_policy(settings::LootFilterMode) noexcept` in the host-facing platform boundary.

- [ ] **Step 1: Write failing snapshot/runtime tests**

Require snapshots to match production ground item `base_id`, `item_level`, ordinal, source, rarity, and pending pickup ordinal. Require runtime with rare-only policy to leave a normal drop active while default runtime still picks it.

- [ ] **Step 2: Verify RED**

Build dungeon and platform test targets; expected compile failures for missing fields and runtime overload.

- [ ] **Step 3: Implement snapshot packing and runtime forwarding**

Pack only existing validated production fields. Set `pending_pickup_ordinal` only for `loot_pickup` and `abyss_reward_claim`; otherwise reset it. Forward the host's committed mode on every non-paused fixed tick.

- [ ] **Step 4: Verify GREEN and default compatibility**

Run dungeon snapshot/stress and platform runtime tests. Existing callers omitting policy must retain show-all behavior.

- [ ] **Step 5: Commit**

```powershell
git add src/dungeon src/platform/raylib/dungeon_runtime.* src/platform/raylib/raylib_host.cpp tests/dungeon tests/platform/dungeon_runtime_tests.cpp
git commit -m "feat: expose filtered ground loot snapshots"
```

### Task 5: Fixed-Capacity Ground Loot View

**Files:**
- Create: `src/platform/raylib/ground_loot_view.hpp`
- Create: `src/platform/raylib/ground_loot_view.cpp`
- Create: `tests/platform/ground_loot_view_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces: `LootLabelRect`, `GroundLootLabel`, and `GroundLootView` with capacity `dungeon::kGroundDropCapacity`.
- Produces: `build_ground_loot_view(const dungeon::DungeonSnapshot&, settings::LootFilterMode, float width, float height) noexcept`.
- Produces: `bool ground_loot_visible(const GroundItemSnapshot&, LootFilterMode) noexcept`.

- [ ] **Step 1: Write failing visibility, text, and layout tests**

Cover all three modes, abyss bypass, invalid base fallback, rarity palette, NUL termination, deterministic ordinal order, overlapping anchors, and 1024x576/1280x720/1920x1080 safety bounds. Assert the snapshot is not mutated.

```cpp
const GroundLootView view = build_ground_loot_view(
    snapshot, settings::LootFilterMode::magic_or_better, 1280.0F, 720.0F);
ARPG_REQUIRE(view.count == 2U);
ARPG_REQUIRE(view.labels[0].ordinal == magic_ordinal);
ARPG_REQUIRE(view.labels[1].ordinal == rare_ordinal);
ARPG_REQUIRE(view.labels[0].text.back() == '\0');
```

- [ ] **Step 2: Register/build and verify RED**

Register a new `ground_loot_view_suite()` in the platform runner. Build; expected missing-header failure.

- [ ] **Step 3: Implement fixed-capacity builder**

Use `project_combat_position`, `std::snprintf`, catalog base names, and stable insertion by ordinal. Resolve overlaps by moving each later label upward in a bounded loop, then clamp every rectangle to the screen safety inset. Return diagnostics counters for invalid base, truncation, overlap adjustment, and capacity saturation.

- [ ] **Step 4: Run platform tests and allocation probe GREEN**

Add a 100,000-iteration allocation probe around unchanged and alternating filter builds. Expected zero allocations and all view tests pass.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib/ground_loot_view.* src/platform/raylib/CMakeLists.txt tests/platform
git commit -m "feat: build fixed ground loot labels"
```

### Task 6: Renderer and Draft Preview Integration

**Files:**
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/hud_renderer.hpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/hud_font.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/hud_font_tests.cpp`
- Modify: `tests/platform/hud_host_integration_tests.cpp`

**Interfaces:**
- Produces: `CombatRenderer::set_loot_filter_mode(settings::LootFilterMode) noexcept`.
- Changes: `draw_room` consumes a prebuilt `GroundLootView` for icon visibility.
- Produces: `HudRenderer::draw_ground_loot(const GroundLootView&) const noexcept`.

- [ ] **Step 1: Write failing integration/font tests**

Require one view build per presented frame, the exact same visible ordinals in icon and label plans, and CJK font coverage for `普通魔法稀有已拾取`. Require settings screen to send draft mode to the renderer and every other screen to send committed mode.

- [ ] **Step 2: Verify RED**

Build platform tests; expected missing renderer APIs and missing glyph coverage.

- [ ] **Step 3: Integrate one view into the render pipeline**

Build once in `CombatRenderer::draw`, draw eligible shapes in the room pass, then draw label panels after actors and before the normal HUD. Use the HUD-owned font and palette; no `TextFormat` or `std::string` in the label path.

- [ ] **Step 4: Integrate preview/commit selection in host**

When pause screen is `settings`, pass `pause_menu.draft.loot_filter_mode` only to renderer. Always map `live_settings.loot_filter_mode` to `DungeonRuntime::fixed_tick`. On cancel/apply failure, existing rollback automatically restores the rendered mode.

- [ ] **Step 5: Run platform and settings integration tests GREEN**

Expected: preview changes visibility, cancel restores it, committed policy never uses draft, and existing HUD/overlay ordering remains intact.

- [ ] **Step 6: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render filtered ground loot labels"
```

### Task 7: Transactional Pickup HUD Feedback

**Files:**
- Create: `src/platform/raylib/loot_pickup_feedback.hpp`
- Create: `src/platform/raylib/loot_pickup_feedback.cpp`
- Create: `tests/platform/loot_pickup_feedback_tests.cpp`
- Modify: `src/platform/raylib/dungeon_runtime.hpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `src/platform/raylib/hud_view_model.hpp`
- Modify: `src/platform/raylib/hud_notice_state.hpp`
- Modify: `src/platform/raylib/hud_notice_state.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Modify: CMake/test registration files

**Interfaces:**
- Produces: `struct LootPickupReceipt` in `DungeonRenderStatus`, containing validity, commit generation, item ID, base ID, item level, rarity, and source.
- Produces: `struct LootPickupFeedback { bool ready; bool abyss; HudText96 text; std::uint64_t item_id; }`.
- Produces: `LootPickupFeedbackState::observe(const DungeonRenderStatus&) noexcept`.
- Produces: `HudNoticeState::publish_loot_pickup(const HudText96&, bool abyss) noexcept`.
- Adds: `HudNoticeKind::loot_pickup` at reward priority.

- [ ] **Step 1: Write failing state-machine tests**

Require the real synchronous runtime to publish a receipt only after a committed ordinary or abyss pickup, using the production pending ordinal and ground item fields. Require save failure, rollback, non-pickup saves, rejected Session receipt, wrong ordinal, or surviving ground item to leave the prior receipt unchanged. For the presentation state, require first observation to establish a baseline, a newer valid receipt to publish once, and repeated/same/older/invalid/error/recovery/fault observations to publish nothing. Error, recovery, and fault must preserve a monotonic generation high-water mark so a later strictly newer committed receipt still publishes without replaying the old one. Require pickup notices to survive a same-frame room change for their remaining lifetime. Cover abyss styling separately.

- [ ] **Step 2: Verify RED**

Build platform tests; expected missing feedback state and notice API.

- [ ] **Step 3: Implement the fixed presentation observer**

Before the synchronous store commit, cache the exact production ground snapshot entry selected by `pending_pickup_ordinal`. After commit and Session resolution, publish `LootPickupReceipt` only if commit generation advanced and the exact item disappeared from the same ground ordinal. Format a newer receipt without heap allocation. Never inspect or mutate `DungeonSession::item_state()` from the presentation observer.

- [ ] **Step 4: Publish through existing HUD priority queue**

Call receipt observation before `HudNoticeState::observe`. Enqueue a newly produced feedback at the existing reward priority with a three-second lifetime; save/recovery/abyss confirmation notices must retain higher priority.

- [ ] **Step 5: Verify GREEN and zero allocation**

Run feedback, notice, host integration, and 100,000-iteration unchanged-observation allocation tests.

- [ ] **Step 6: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: report confirmed loot pickups"
```

### Task 8: Architecture Guards and Regression Stress

**Files:**
- Create: `tests/platform/stage11d_loot_architecture_guard_test.cmake`
- Create: `tests/platform/stage11d_loot_architecture_guard_self_test.cmake`
- Create: `tests/platform/stage11d_loot_bad_source.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_equipment_stress_tests.cpp`
- Modify: `tests/settings/settings_stress_tests.cpp`

**Interfaces:**
- Produces CTest gates `stage11d.architecture.loot_boundaries` and `stage11d.architecture.loot_boundaries_self_test`.

- [ ] **Step 1: Register a deliberately red guard**

Register the guard before adding its script and run its named CTest. Expected failure because the script is absent.

- [ ] **Step 2: Implement positive boundary checks**

Reject settings/raylib includes under dungeon, dungeon includes under settings, dynamic strings/containers and input/session/store calls in `ground_loot_view` or feedback files, direct settings-store reads in room renderer, and policy checks inside explicit `request_pickup`.

- [ ] **Step 3: Add mutation self-tests**

The self-test must prove the guard rejects: `std::string`, `IsKeyDown`, `session.tick`, `SettingsStore`, dungeon-to-settings include, a rarity check in explicit pickup, and a filter condition that omits the abyss bypass.

- [ ] **Step 4: Extend long traces**

Rotate filter mode across deterministic equipment runs and settings saves. Confirm filtered items remain identical in the ground pool, relaxing the policy later picks them, two same-seed traces remain equal, and 100,000 hot-path operations allocate zero.

- [ ] **Step 5: Run guard and stress tests GREEN**

Run named CTests and direct stress executables; expected all pass.

- [ ] **Step 6: Commit**

```powershell
git add tests/platform tests/dungeon tests/settings
git commit -m "test: guard stage 11d loot boundaries"
```

### Task 9: Formal Raylib Evidence

**Files:**
- Create: `tests/platform/stage11d_loot_formal_game_validation.cpp`
- Create: `tests/platform/stage11d_loot_formal_validator.ps1`
- Create: `tests/platform/stage11d_loot_evidence_guard_test.cmake`
- Create: `tests/platform/stage11d_loot_evidence_guard_self_test.cmake`
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Generate: `docs/validation/evidence/stage11d/*.png`

**Interfaces:**
- Adds six deterministic `Stage11DLootValidationScenario` values matching the design evidence list.
- Reuses production host, production snapshot, real settings slots, real save transactions, and the post-`EndDrawing()` capture helper.

- [ ] **Step 1: Register formal tests before harness implementation**

Add CTests for the six-path formal game, content validator, architecture evidence guard, and guard self-test. Run the names and confirm RED due to missing harness files.

- [ ] **Step 2: Implement production scenarios**

Drive all/magic+/rare/abyss/preview-cancel/pickup-feedback through public host inputs and stable test launch options. Do not add public snapshot setters, direct view injection, private member access, pre-present screenshots, or a second fake renderer.

- [ ] **Step 3: Implement semantic validator**

Require fresh 1280x720 PNGs, unique hashes, non-empty label regions, manifest summaries, exact production item IDs before/after filtering, hidden items absent from inventory, abyss reward present and claimable, preview rollback, and confirmed pickup notice text.

- [ ] **Step 4: Implement evidence mutation guard**

Reject private injection, fake setting publication, direct pickup completion, snapshot mutation, pre-`EndDrawing()` capture, non-production renderer, stale PNGs, duplicate hashes, and validators that only check file existence.

- [ ] **Step 5: Run formal tests GREEN and commit evidence**

Expected all six scenes and both guard directions pass.

```powershell
git add src/platform/raylib tests/platform docs/validation/evidence/stage11d
git commit -m "test: validate stage 11d loot filter"
```

### Task 10: Documentation and Final Gates

**Files:**
- Modify: `README.md`
- Create: `docs/validation/stage11d-ground-loot-filter.md`

**Interfaces:**
- Documents the three presets, abyss exception, preview/apply behavior, automatic pickup semantics, settings V2 migration, evidence paths, and explicit Stage 12 stop.

- [ ] **Step 1: Update player documentation**

Set the current milestone to Stage 11-D. Explain how to change the preset in pause settings and that filtered ordinary items remain on the ground. Do not claim main menu, controller, resolution/quality settings, macOS, or Stage 12.

- [ ] **Step 2: Run focused verification**

Build and run settings, dungeon, platform, Stage 8 loot, Stage 10 abyss, Stage 11-B settings, Stage 11-C HUD, Stage 11-D guard/stress/formal tests. Record exact counts and durations.

- [ ] **Step 3: Run fresh Debug clean-first gate**

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-debug -CleanFirst
ctest --test-dir out/build/windows-msvc-debug --output-on-failure
```

Expected: every target builds and every CTest passes.

- [ ] **Step 4: Run fresh Release clean-first gate**

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-release -CleanFirst
ctest --test-dir out/build/windows-msvc-release --output-on-failure
```

Expected: every target builds, every CTest passes, and fresh formal PNG evidence is regenerated.

- [ ] **Step 5: Audit and final documentation**

Run `git diff --check`, `git status --short`, inspect tracked binaries and generated files, hash evidence PNGs, and record compiler/raylib versions plus exact gate output in the validation document.

- [ ] **Step 6: Commit final docs**

```powershell
git add README.md docs/validation/stage11d-ground-loot-filter.md
git commit -m "docs: complete stage 11d loot filter"
```

- [ ] **Step 7: Stop at Stage 11-D**

Keep `codex/stage11d-loot-filter` and its worktree clean. Do not merge `main` and do not begin Stage 12.
