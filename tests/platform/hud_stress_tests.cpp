#include "allocation_probe.hpp"

#include "combat/combat_types.hpp"
#include "debug_overlay_renderer.hpp"
#include "hud_font.hpp"
#include "hud_layout.hpp"
#include "hud_notice_state.hpp"
#include "hud_renderer.hpp"
#include "hud_view_model.hpp"

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

namespace combat = arpg::combat;
namespace dungeon = arpg::dungeon;
namespace platform = arpg::platform;

constexpr std::size_t kScenarioCount = 5U;
constexpr std::size_t kIterations = 100000U;
constexpr std::uint64_t kHashOffset = 1469598103934665603ULL;
constexpr std::uint64_t kHashPrime = 1099511628211ULL;

struct StressScenario final {
    dungeon::DungeonSnapshot previous{};
    dungeon::DungeonSnapshot current{};
    dungeon::DungeonSnapshot alternate{};
    platform::DungeonRenderStatus status{};
    platform::ControlHints hints{};
    combat::MonsterSnapshot monster{};
    combat::CombatEvent last_event{};
    int width{1280};
    int height{720};
    bool debug_visible{};
    bool recovery_required{};
    bool has_last_event{};
    bool cjk_font_ready{};
};

std::array<StressScenario, kScenarioCount> g_scenarios{};

void fold_bytes(std::uint64_t& hash, const void* data, std::size_t size) noexcept {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t index{}; index < size; ++index) {
        hash = (hash ^ bytes[index]) * kHashPrime;
    }
}

template <typename Value>
void fold_object(std::uint64_t& hash, const Value& value) noexcept {
    fold_bytes(hash, &value, sizeof(value));
}

[[nodiscard]] std::uint64_t input_hash() noexcept {
    std::uint64_t hash = kHashOffset;
    for (const StressScenario& scenario : g_scenarios) {
        fold_object(hash, scenario.previous);
        fold_object(hash, scenario.current);
        fold_object(hash, scenario.alternate);
        fold_object(hash, scenario.status);
        fold_object(hash, scenario.hints);
        fold_object(hash, scenario.monster);
        fold_object(hash, scenario.last_event);
        fold_object(hash, scenario.width);
        fold_object(hash, scenario.height);
        fold_object(hash, scenario.debug_visible);
        fold_object(hash, scenario.recovery_required);
        fold_object(hash, scenario.has_last_event);
        fold_object(hash, scenario.cjk_font_ready);
    }
    return hash;
}

[[nodiscard]] platform::ControlHints stress_hints() noexcept {
    platform::ControlHints hints{};
    static constexpr char kPrimary[] = "W Move Up  S Move Down";
    static constexpr char kSecondary[] =
        "J Light Attack  K Jump  L Launcher  Q Interact  O Inventory  "
        "T Passive Tree";
    std::memcpy(hints.primary.data(), kPrimary, sizeof(kPrimary));
    std::memcpy(hints.secondary.data(), kSecondary, sizeof(kSecondary));
    hints.revision = 23U;
    return hints;
}

[[nodiscard]] dungeon::DungeonSnapshot base_snapshot() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.session_tick = 17U;
    snapshot.commit_generation = 3U;
    snapshot.room_index = 4U;
    snapshot.depth = 7U;
    snapshot.floor_room_index = 2U;
    snapshot.biases = {{11U, 22U, 33U, 44U}};
    snapshot.phase = dungeon::RoomPhase::combat;
    snapshot.has_active_room = true;
    snapshot.wave_index = 0U;
    snapshot.wave_count = 2U;
    snapshot.remaining_targets = 3U;
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.progression = {4U, 40U, 3U, 2U};
    snapshot.pending_room_experience = 25U;
    snapshot.combat.emplace();
    snapshot.combat->player.hp = 80;
    snapshot.combat->player.max_hp = 100;
    snapshot.combat->player.barrier = 20;
    snapshot.combat->player.max_barrier = 40;
    snapshot.combat->monsters[0].active = true;
    snapshot.combat->monsters[0].hp = 75;
    snapshot.combat->monsters[0].max_hp = 100;
    snapshot.combat->monsters[0].shield = 15;
    snapshot.combat->monsters[0].max_shield = 30;
    snapshot.combat->monsters[0].break_value = 20;
    snapshot.combat->monsters[0].max_break = 50;
    snapshot.combat->monster_count = 1U;
    return snapshot;
}

void initialize_scenarios() noexcept {
    const dungeon::DungeonSnapshot base = base_snapshot();
    const platform::ControlHints hints = stress_hints();
    for (StressScenario& scenario : g_scenarios) {
        scenario.previous = base;
        scenario.current = base;
        scenario.alternate = base;
        scenario.hints = hints;
        scenario.monster = base.combat->monsters[0];
    }

    g_scenarios[1].current.combat->player.hp = 20;
    g_scenarios[1].width = 1024;
    g_scenarios[1].height = 576;

    combat::PlayerSnapshot& status_player =
        g_scenarios[2].current.combat->player;
    status_player.slow_bp = 2500;
    status_player.slow_ticks = 30U;
    status_player.corrosion_damage_per_second = 12;
    status_player.corrosion_ticks = 90U;
    status_player.invulnerability_ticks = 5U;
    g_scenarios[2].current.encounter.total_budget = 91U;
    g_scenarios[2].current.encounter.current_wave_budget = 37U;
    g_scenarios[2].current.combat->monster_count = 7U;
    g_scenarios[2].current.combat->projectile_count = 8U;
    g_scenarios[2].current.combat->hazard_count = 9U;
    g_scenarios[2].current.combat->diagnostics.projectile_saturation_count = 11U;
    g_scenarios[2].current.combat->diagnostics.projectile_invalid_owner_count = 12U;
    g_scenarios[2].current.combat->diagnostics.hazard_saturation_count = 13U;
    g_scenarios[2].current.combat->diagnostics.hazard_invalid_owner_count = 14U;
    g_scenarios[2].current.diagnostics.ground_saturation_count = 15U;
    g_scenarios[2].current.diagnostics.room_index_overflow = true;
    g_scenarios[2].alternate = g_scenarios[2].current;
    g_scenarios[2].last_event.kind = combat::CombatEventKind::hit;
    g_scenarios[2].last_event.tick = 123U;
    g_scenarios[2].last_event.target_index = 3U;
    g_scenarios[2].last_event.hit_count = 2U;
    g_scenarios[2].has_last_event = true;
    g_scenarios[2].cjk_font_ready = true;
    g_scenarios[2].debug_visible = true;

    dungeon::DungeonSnapshot& maximum = g_scenarios[3].current;
    maximum.depth = (std::numeric_limits<std::uint64_t>::max)();
    maximum.floor_room_index = (std::numeric_limits<std::uint64_t>::max)();
    maximum.biases.fill((std::numeric_limits<std::uint32_t>::max)());
    maximum.pending_room_experience =
        (std::numeric_limits<std::uint64_t>::max)();
    maximum.progression.level = 99U;
    maximum.progression.experience =
        (std::numeric_limits<std::uint64_t>::max)();
    maximum.combat->player.hp = INT_MAX;
    maximum.combat->player.max_hp = INT_MAX;
    maximum.combat->player.barrier = INT_MAX;
    maximum.combat->player.max_barrier = INT_MAX;
    g_scenarios[3].width = 1920;
    g_scenarios[3].height = 1080;
    g_scenarios[3].alternate = maximum;

    dungeon::DungeonSnapshot& overflow = g_scenarios[4].current;
    overflow.phase = dungeon::RoomPhase::cleared;
    overflow.remaining_targets = 0U;
    overflow.last_room_experience = 999U;
    overflow.progression.level = 5U;
    overflow.progression.unspent_passive_points = 1U;
    overflow.inventory_count = 1U;
    g_scenarios[4].alternate = overflow;
    ++g_scenarios[4].alternate.commit_generation;
    ++g_scenarios[4].alternate.room_index;
    ++g_scenarios[4].alternate.last_room_experience;
    ++g_scenarios[4].alternate.progression.level;
}

float fixed_measure(const char* text, float font_size, void*) noexcept {
    std::size_t length{};
    while (text != nullptr && text[length] != '\0') ++length;
    return static_cast<float>(length) * font_size * 0.6F;
}

[[nodiscard]] bool same_notice(const platform::HudNotice& lhs,
    const platform::HudNotice& rhs) noexcept {
    return lhs.kind == rhs.kind
        && lhs.priority == rhs.priority
        && lhs.seconds_left == rhs.seconds_left
        && lhs.text.bytes == rhs.text.bytes
        && lhs.text.truncated == rhs.text.truncated
        && lhs.abyss == rhs.abyss;
}

[[nodiscard]] bool same_notice_view(const platform::HudNoticeView& lhs,
    const platform::HudNoticeView& rhs) noexcept {
    return same_notice(lhs.primary, rhs.primary)
        && same_notice(lhs.secondary, rhs.secondary);
}

void fold_output(std::uint64_t& checksum, std::uint64_t value) noexcept {
    checksum = (checksum ^ value) * kHashPrime;
}

struct ExerciseResult final {
    bool unchanged_refresh_preserved{true};
    bool unchanged_static_formatting_preserved{true};
    bool overflowed_once{true};
    bool debug_counters_valid{true};
};

[[nodiscard]] ExerciseResult exercise_all_pure_paths(
    const StressScenario& scenario,
    const dungeon::DungeonSnapshot& current,
    platform::HudNoticeState& notices,
    platform::HudViewModelProjector& projector,
    float presentation_seconds,
    bool require_unchanged_refresh,
    bool require_overflow,
    std::uint64_t& output_checksum) noexcept {
    ExerciseResult result{};
    const platform::HudNoticeView before_view = notices.view();
    const std::uint32_t drops_before = notices.dropped_count();
    notices.observe(scenario.previous, current, scenario.status,
        scenario.hints, scenario.recovery_required);
    const platform::HudNoticeView observed_view = notices.view();
    if (require_unchanged_refresh) {
        result.unchanged_refresh_preserved =
            drops_before == notices.dropped_count()
            && same_notice_view(before_view, observed_view);
    }
    if (require_overflow) {
        result.overflowed_once = notices.dropped_count() == drops_before + 1U;
    }
    notices.update(1.0F / 60.0F, false);
    const platform::HudNoticeView notice_view = notices.view();

    const platform::HudStaticFormattingDiagnostics formatting_before =
        projector.static_formatting_diagnostics();
    platform::HudViewModel model{};
    projector.build(model, current, scenario.status, scenario.hints);
    const platform::HudStaticFormattingDiagnostics formatting_after =
        projector.static_formatting_diagnostics();
    if (require_unchanged_refresh) {
        result.unchanged_static_formatting_preserved =
            formatting_after == formatting_before;
    }
    platform::attach_notice_view(model, notice_view);
    const platform::HudLayout layout = platform::make_hud_layout(
        scenario.width, scenario.height, scenario.debug_visible);
    const platform::PlayerPanelPlan player =
        platform::make_player_panel_plan(
            model.player, layout, presentation_seconds);
    const platform::MonsterBarVisualPlan monster =
        platform::make_monster_bar_visual_plan(scenario.monster);
    const platform::ObjectivePanelPlan objective =
        platform::make_objective_panel_plan(model.room, layout);
    const platform::NavigationPanelPlan navigation =
        platform::make_navigation_panel_plan(model.navigation, layout);
    const platform::ContextPanelPlan context =
        platform::make_context_panel_plan(model.context, layout);
    const platform::HudTextDrawPlan text =
        platform::make_hud_text_draw_plan(model.navigation.primary,
            layout.navigation_panel.width, 16.0F, 11.0F,
            &fixed_measure, nullptr);
    const platform::HudFontSelectionPlan font =
        platform::make_hud_font_selection_plan(scenario.cjk_font_ready);
    const platform::DebugOverlayDiagnosticsPlan debug =
        platform::make_debug_overlay_diagnostics_plan(current,
            model.diagnostics, notices.dropped_count(), scenario.hints.revision,
            scenario.last_event, scenario.has_last_event,
            scenario.cjk_font_ready);

    if (scenario.debug_visible) {
        result.debug_counters_valid = debug.total_budget == 91U
            && debug.current_wave_budget == 37U
            && debug.active_monsters == 7U
            && debug.active_projectiles == 8U
            && debug.active_hazards == 9U
            && debug.projectile_saturation == 11U
            && debug.projectile_invalid_owner == 12U
            && debug.hazard_saturation == 13U
            && debug.hazard_invalid_owner == 14U
            && debug.ground_saturation == 15U
            && debug.room_index_overflow
            && debug.binding_revision == 23U
            && debug.has_last_event
            && debug.last_event.kind == combat::CombatEventKind::hit
            && debug.last_event.tick == 123U
            && debug.last_event.target_index == 3U
            && debug.cjk_font_ready;
    }

    fold_output(output_checksum, player.bar_count);
    fold_output(output_checksum, player.tag_count);
    fold_output(output_checksum, monster.bars[0].visible);
    fold_output(output_checksum, objective.visible);
    fold_output(output_checksum, navigation.visible);
    fold_output(output_checksum, context.primary_visible);
    fold_output(output_checksum, text.visible);
    fold_output(output_checksum, font.use_default_font);
    fold_output(output_checksum, debug.total_budget);
    fold_output(output_checksum, debug.current_wave_budget);
    fold_output(output_checksum, debug.active_monsters);
    fold_output(output_checksum, debug.active_projectiles);
    fold_output(output_checksum, debug.active_hazards);
    fold_output(output_checksum, debug.projectile_saturation);
    fold_output(output_checksum, debug.projectile_invalid_owner);
    fold_output(output_checksum, debug.hazard_saturation);
    fold_output(output_checksum, debug.hazard_invalid_owner);
    fold_output(output_checksum, debug.ground_saturation);
    fold_output(output_checksum, debug.room_index_overflow);
    fold_output(output_checksum, debug.hud.clamped_values);
    fold_output(output_checksum, debug.hud.truncated_texts);
    fold_output(output_checksum, debug.hud.combat_snapshot_missing);
    fold_output(output_checksum, debug.notice_drops);
    fold_output(output_checksum, debug.binding_revision);
    fold_output(output_checksum, static_cast<std::uint8_t>(debug.last_event.kind));
    fold_output(output_checksum, debug.last_event.tick);
    fold_output(output_checksum, debug.last_event.target_index);
    fold_output(output_checksum, debug.last_event.hit_count);
    fold_output(output_checksum, debug.has_last_event);
    fold_output(output_checksum, debug.cjk_font_ready);
    fold_output(output_checksum, formatting_after.objective_rebuilds);
    fold_output(output_checksum, formatting_after.navigation_rebuilds);
    fold_output(output_checksum, formatting_after.control_hint_rebuilds);
    return result;
}

[[nodiscard]] bool environment_enabled() noexcept {
    char* value = nullptr;
    std::size_t length{};
#if defined(_WIN32)
    const errno_t error = _dupenv_s(
        &value, &length, "ARPG_STAGE11C_HUD_STRESS");
    const bool enabled = error == 0 && value != nullptr
        && std::strcmp(value, "1") == 0;
    std::free(value);
    static_cast<void>(length);
    return enabled;
#else
    const char* value = std::getenv("ARPG_STAGE11C_HUD_STRESS");
    return value != nullptr && std::strcmp(value, "1") == 0;
#endif
}

[[nodiscard]] int fail(const char* reason) noexcept {
    std::fprintf(stderr, "[stage11c-hud-stress-fail] %s\n", reason);
    return 1;
}

}  // namespace

int main() {
    if (!environment_enabled()) {
        return fail("requires ARPG_STAGE11C_HUD_STRESS=1");
    }

    initialize_scenarios();
    const std::uint64_t inputs_before = input_hash();
    std::array<platform::HudNoticeState, kScenarioCount> notice_states{};
    std::array<platform::HudViewModelProjector, kScenarioCount> projectors{};
    std::uint64_t output_checksum = kHashOffset;

    // The first allocation baseline is taken before any HUD pure path. This
    // catches one-time initialization allocations instead of warming them away.
    const std::uint64_t cold_allocations_before =
        arpg::test::allocation_count();
    for (std::size_t index{}; index < kScenarioCount; ++index) {
        const ExerciseResult cold = exercise_all_pure_paths(g_scenarios[index],
            g_scenarios[index].current, notice_states[index], projectors[index], 0.0F,
            false, index == 4U, output_checksum);
        if (!cold.overflowed_once || !cold.debug_counters_valid) {
            return fail("first five-scenario pure-path semantics changed");
        }
    }
    const std::uint64_t cold_allocation_delta =
        arpg::test::allocation_count() - cold_allocations_before;
    if (cold_allocation_delta != 0U) {
        return fail("first complete five-scenario pass allocated heap memory");
    }
    if (input_hash() != inputs_before) {
        return fail("first complete five-scenario pass mutated input snapshots");
    }

    // The unchanged notice state now owns its baseline observation. All five
    // states persist across the measured refresh loop.
    const std::uint64_t steady_allocations_before =
        arpg::test::allocation_count();
    std::array<std::size_t, kScenarioCount> visits{};

    for (std::size_t iteration{}; iteration < kIterations; ++iteration) {
        const std::size_t scenario_index = iteration % kScenarioCount;
        const StressScenario& scenario = g_scenarios[scenario_index];
        ++visits[scenario_index];

        const bool overflow_visit = scenario_index == 4U;
        const dungeon::DungeonSnapshot& current = overflow_visit
            && (visits[scenario_index] & 1U) != 0U
            ? scenario.alternate : scenario.current;
        const ExerciseResult result = exercise_all_pure_paths(scenario,
            current, notice_states[scenario_index], projectors[scenario_index],
            static_cast<float>(iteration) / 60.0F,
            scenario_index == 0U, overflow_visit, output_checksum);
        if (!result.unchanged_refresh_preserved) {
            return fail("unchanged same-revision notice refresh requeued or reset state");
        }
        if (!result.unchanged_static_formatting_preserved) {
            return fail("unchanged snapshot rebuilt cached static HUD text");
        }
        if (!result.overflowed_once) {
            return fail("persistent overflow state did not produce one real drop");
        }
        if (!result.debug_counters_valid) {
            return fail("debug diagnostics plan lost F1 counters or combat event");
        }
    }

    const std::uint64_t steady_allocation_delta =
        arpg::test::allocation_count() - steady_allocations_before;
    const std::uint64_t inputs_after = input_hash();
    for (const std::size_t count : visits) {
        if (count != kIterations / kScenarioCount) {
            return fail("five-snapshot traversal count changed");
        }
    }
    if (steady_allocation_delta != 0U) {
        return fail("steady 100k loop allocated heap memory");
    }
    if (inputs_before == 0U || inputs_before != inputs_after) {
        return fail("production input snapshot hash changed");
    }
    if (notice_states[0].dropped_count() != 0U) {
        return fail("unchanged same-revision refresh changed drop diagnostics");
    }
    if (notice_states[4].dropped_count() != visits[4] + 1U
            || output_checksum == 0U) {
        return fail("persistent priority-overflow or pure-plan coverage changed");
    }

    std::printf(
        "[stage11c-hud-stress] iterations=%zu scenarios=5 cold_allocations=%llu "
        "steady_allocations=%llu "
        "input_hash=0x%016llx output=0x%016llx result=pass\n",
        kIterations, static_cast<unsigned long long>(cold_allocation_delta),
        static_cast<unsigned long long>(steady_allocation_delta),
        static_cast<unsigned long long>(inputs_after),
        static_cast<unsigned long long>(output_checksum));
    return 0;
}
