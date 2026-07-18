#include "allocation_probe.hpp"

#include "combat/combat_types.hpp"
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
    platform::DungeonRenderStatus status{};
    platform::ControlHints hints{};
    combat::MonsterSnapshot monster{};
    int width{1280};
    int height{720};
    bool debug_visible{};
    bool recovery_required{};
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
        fold_object(hash, scenario.status);
        fold_object(hash, scenario.hints);
        fold_object(hash, scenario.monster);
        fold_object(hash, scenario.width);
        fold_object(hash, scenario.height);
        fold_object(hash, scenario.debug_visible);
        fold_object(hash, scenario.recovery_required);
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

    dungeon::DungeonSnapshot& overflow = g_scenarios[4].current;
    overflow.phase = dungeon::RoomPhase::cleared;
    overflow.remaining_targets = 0U;
    overflow.last_room_experience = 999U;
    overflow.progression.level = 5U;
    overflow.progression.unspent_passive_points = 1U;
    overflow.inventory_count = 1U;
}

float fixed_measure(const char* text, float font_size, void*) noexcept {
    std::size_t length{};
    while (text != nullptr && text[length] != '\0') ++length;
    return static_cast<float>(length) * font_size * 0.6F;
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

    // Warm every static and formatting path before taking the allocation
    // baseline. The measured loop still visits all five production snapshots.
    for (const StressScenario& scenario : g_scenarios) {
        platform::HudNoticeState notices{};
        notices.observe(scenario.previous, scenario.current, scenario.status,
            scenario.hints, scenario.recovery_required);
        notices.update(1.0F / 60.0F, false);
        platform::HudViewModel model{};
        platform::build_hud_view_model(model, scenario.current,
            scenario.status, scenario.hints);
        platform::attach_notice_view(model, notices.view());
        const platform::HudLayout layout = platform::make_hud_layout(
            scenario.width, scenario.height, scenario.debug_visible);
        static_cast<void>(platform::make_player_panel_plan(
            model.player, layout, 0.0F));
        static_cast<void>(platform::make_monster_bar_visual_plan(
            scenario.monster));
        static_cast<void>(platform::make_objective_panel_plan(
            model.room, layout));
        static_cast<void>(platform::make_navigation_panel_plan(
            model.navigation, layout));
        static_cast<void>(platform::make_context_panel_plan(
            model.context, layout));
        static_cast<void>(platform::make_hud_text_draw_plan(
            model.navigation.primary, layout.navigation_panel.width,
            16.0F, 11.0F, &fixed_measure, nullptr));
        static_cast<void>(platform::make_hud_font_selection_plan(true));
    }

    const std::uint64_t inputs_before = input_hash();
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    std::array<std::size_t, kScenarioCount> visits{};
    std::uint64_t output_checksum = kHashOffset;
    std::uint32_t overflow_drops{};

    for (std::size_t iteration{}; iteration < kIterations; ++iteration) {
        const std::size_t scenario_index = iteration % kScenarioCount;
        const StressScenario& scenario = g_scenarios[scenario_index];
        ++visits[scenario_index];

        platform::HudNoticeState notices{};
        notices.observe(scenario.previous, scenario.current, scenario.status,
            scenario.hints, scenario.recovery_required);
        notices.update(1.0F / 60.0F, false);
        const platform::HudNoticeView notice_view = notices.view();

        platform::HudViewModel model{};
        platform::build_hud_view_model(model, scenario.current,
            scenario.status, scenario.hints);
        platform::attach_notice_view(model, notice_view);
        const platform::HudLayout layout = platform::make_hud_layout(
            scenario.width, scenario.height, scenario.debug_visible);
        const platform::PlayerPanelPlan player =
            platform::make_player_panel_plan(
                model.player, layout, static_cast<float>(iteration) / 60.0F);
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
            platform::make_hud_font_selection_plan((iteration & 1U) != 0U);

        output_checksum ^= static_cast<std::uint64_t>(player.bar_count)
            | (static_cast<std::uint64_t>(player.tag_count) << 8U)
            | (static_cast<std::uint64_t>(monster.bars[0].visible) << 16U)
            | (static_cast<std::uint64_t>(objective.visible) << 17U)
            | (static_cast<std::uint64_t>(navigation.visible) << 18U)
            | (static_cast<std::uint64_t>(context.primary_visible) << 19U)
            | (static_cast<std::uint64_t>(text.visible) << 20U)
            | (static_cast<std::uint64_t>(font.use_default_font) << 21U);
        output_checksum *= kHashPrime;
        if (scenario_index == 4U) {
            overflow_drops += notices.dropped_count();
        }
    }

    const std::uint64_t allocation_delta = arpg::test::allocation_count()
        - allocations_before;
    const std::uint64_t inputs_after = input_hash();
    for (const std::size_t count : visits) {
        if (count != kIterations / kScenarioCount) {
            return fail("five-snapshot traversal count changed");
        }
    }
    if (allocation_delta != 0U) {
        return fail("measured loop allocated heap memory");
    }
    if (inputs_before == 0U || inputs_before != inputs_after) {
        return fail("production input snapshot hash changed");
    }
    if (overflow_drops != visits[4] || output_checksum == 0U) {
        return fail("priority-overflow or pure-plan coverage changed");
    }

    std::printf(
        "[stage11c-hud-stress] iterations=%zu scenarios=5 allocations=%llu "
        "input_hash=0x%016llx output=0x%016llx result=pass\n",
        kIterations, static_cast<unsigned long long>(allocation_delta),
        static_cast<unsigned long long>(inputs_after),
        static_cast<unsigned long long>(output_checksum));
    return 0;
}
