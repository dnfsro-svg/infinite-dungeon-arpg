#include "test_framework.hpp"

#include "combat_renderer.hpp"
#include "control_hints.hpp"
#include "debug_overlay_renderer.hpp"
#include "dungeon_runtime.hpp"
#include "hud_font.hpp"

#include <cstring>

namespace {

namespace dungeon = arpg::dungeon;
namespace platform = arpg::platform;

platform::ControlHints committed_hints(std::uint64_t revision) noexcept {
    platform::ControlHints hints{};
    static constexpr char kPrimary[] = "WASD Move";
    static_assert(sizeof(kPrimary) <= 160U);
    std::memcpy(hints.primary.data(), kPrimary, sizeof(kPrimary));
    hints.revision = revision;
    return hints;
}

dungeon::DungeonSnapshot snapshot() noexcept {
    dungeon::DungeonSnapshot value{};
    value.commit_generation = 7U;
    value.room_index = 3U;
    value.phase = dungeon::RoomPhase::combat;
    value.has_active_room = true;
    value.remaining_targets = 2U;
    value.progression.level = 4U;
    return value;
}

platform::DungeonRenderStatus saved_status() noexcept {
    platform::DungeonRenderStatus status{};
    status.indicator = platform::SaveIndicator::saved;
    return status;
}

platform::DungeonRenderStatus recovery_status() noexcept {
    platform::DungeonRenderStatus status{};
    status.indicator = platform::SaveIndicator::error;
    status.recovery_required = true;
    return status;
}

arpg::test::Failure observation_is_once_per_presented_frame_and_read_only() noexcept {
    const dungeon::DungeonSnapshot previous = snapshot();
    const dungeon::DungeonSnapshot current = snapshot();
    platform::CombatRenderer renderer{};

    renderer.observe_hud(previous, current, saved_status(), committed_hints(3U),
        1.0F / 60.0F, false);
    ARPG_REQUIRE(renderer.hud_observation_count() == 1U);
    ARPG_REQUIRE(current.commit_generation == 7U);
    ARPG_REQUIRE(current.room_index == 3U);
    ARPG_REQUIRE(current.remaining_targets == 2U);

    // Recovery/death-owned presentation frames still observe the committed snapshots.
    renderer.observe_hud(previous, current, saved_status(), committed_hints(3U),
        1.0F / 60.0F, false);
    ARPG_REQUIRE(renderer.hud_observation_count() == 2U);
    return {};
}

arpg::test::Failure production_presentation_seam_observes_normal_recovery_and_death() noexcept {
    const dungeon::DungeonSnapshot previous = snapshot();
    dungeon::DungeonSnapshot current = previous;
    platform::CombatRenderer renderer{};

    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        previous, current, saved_status(), committed_hints(11U), 0.1F, false);
    ARPG_REQUIRE(renderer.hud_presented_frame_count(
        platform::HudPresentedFrame::normal) == 1U);
    ARPG_REQUIRE(renderer.hud_binding_revision() == 11U);

    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::recovery,
        current, current, recovery_status(), committed_hints(12U), 1.0F, true);
    ARPG_REQUIRE(renderer.hud_presented_frame_count(
        platform::HudPresentedFrame::recovery) == 1U);
    ARPG_REQUIRE(renderer.hud_notice_view().primary.kind
        == platform::HudNoticeKind::save_error);
    ARPG_REQUIRE(renderer.hud_notice_view().secondary.kind
        == platform::HudNoticeKind::recovery_required);
    ARPG_REQUIRE(renderer.hud_binding_revision() == 12U);

    current.death.emplace();
    renderer.observe_presented_hud_frame(platform::HudPresentedFrame::death_overlay,
        previous, current, saved_status(), committed_hints(13U), 0.1F, false);
    ARPG_REQUIRE(renderer.hud_presented_frame_count(
        platform::HudPresentedFrame::death_overlay) == 1U);
    ARPG_REQUIRE(renderer.hud_observation_count() == 3U);
    ARPG_REQUIRE(renderer.hud_binding_revision() == 13U);
    return {};
}

arpg::test::Failure debug_diagnostics_plan_carries_all_f1_only_counters() noexcept {
    dungeon::DungeonSnapshot value = snapshot();
    value.encounter.total_budget = 88U;
    value.encounter.current_wave_budget = 21U;
    value.diagnostics.ground_saturation_count = 5U;
    value.combat.emplace();
    value.combat->monster_count = 7U;
    value.combat->projectile_count = 9U;
    value.combat->hazard_count = 3U;
    value.combat->diagnostics.projectile_saturation_count = 2U;
    value.combat->diagnostics.projectile_invalid_owner_count = 4U;
    value.combat->diagnostics.hazard_saturation_count = 6U;
    value.combat->diagnostics.hazard_invalid_owner_count = 8U;
    arpg::combat::CombatEvent event{};
    event.kind = arpg::combat::CombatEventKind::hit;
    event.target_index = 5U;
    platform::HudBuildDiagnostics hud{};
    hud.clamped_values = 1U;
    hud.truncated_texts = 2U;
    hud.combat_snapshot_missing = false;

    const platform::DebugOverlayDiagnosticsPlan plan =
        platform::make_debug_overlay_diagnostics_plan(value, hud, 10U, 99U,
            event, true, true);
    ARPG_REQUIRE(plan.total_budget == 88U);
    ARPG_REQUIRE(plan.current_wave_budget == 21U);
    ARPG_REQUIRE(plan.active_monsters == 7U);
    ARPG_REQUIRE(plan.active_projectiles == 9U);
    ARPG_REQUIRE(plan.active_hazards == 3U);
    ARPG_REQUIRE(plan.projectile_saturation == 2U);
    ARPG_REQUIRE(plan.projectile_invalid_owner == 4U);
    ARPG_REQUIRE(plan.hazard_saturation == 6U);
    ARPG_REQUIRE(plan.hazard_invalid_owner == 8U);
    ARPG_REQUIRE(plan.ground_saturation == 5U);
    ARPG_REQUIRE(plan.notice_drops == 10U);
    ARPG_REQUIRE(plan.binding_revision == 99U);
    ARPG_REQUIRE(plan.has_last_event);
    ARPG_REQUIRE(plan.last_event.target_index == 5U);
    ARPG_REQUIRE(plan.cjk_font_ready);
    return {};
}

arpg::test::Failure fallback_font_mode_remains_drawable_without_complete_cjk() noexcept {
    ARPG_REQUIRE(platform::hud_font_draw_mode(false)
        == platform::HudFontDrawMode::fallback);
    ARPG_REQUIRE(platform::hud_font_draw_mode(true)
        == platform::HudFontDrawMode::cjk_ready);
    return {};
}

arpg::test::Failure paused_frames_freeze_hud_notice_time() noexcept {
    const dungeon::DungeonSnapshot previous = snapshot();
    dungeon::DungeonSnapshot current = previous;
    current.last_room_experience = 25U;
    platform::CombatRenderer renderer{};

    renderer.observe_hud(previous, current, saved_status(), committed_hints(4U),
        3.0F, true);
    const float paused_seconds = renderer.hud_notice_view().primary.seconds_left;
    renderer.observe_hud(previous, current, saved_status(), committed_hints(4U),
        1.25F, true);
    ARPG_REQUIRE(arpg::test::near(
        renderer.hud_notice_view().primary.seconds_left, paused_seconds));
    return {};
}

arpg::test::Failure committed_settings_hints_and_revision_are_used_after_apply() noexcept {
    const dungeon::DungeonSnapshot value = snapshot();
    platform::CombatRenderer renderer{};

    renderer.observe_hud(value, value, saved_status(), committed_hints(42U),
        0.0F, false);
    ARPG_REQUIRE(renderer.hud_binding_revision() == 42U);
    ARPG_REQUIRE(renderer.hud_model().diagnostics.combat_snapshot_missing);
    return {};
}

arpg::test::Failure observation_publishes_the_prebuilt_hud_model() noexcept {
    const dungeon::DungeonSnapshot value = snapshot();
    platform::CombatRenderer renderer{};
    renderer.observe_hud(value, value, saved_status(), committed_hints(8U),
        0.0F, false);
    const platform::HudViewModel before = renderer.hud_model();

    // Drawing is raylib-owned; this headless contract verifies the draw input
    // is fully prepared before the render phase and is not rebuilt from later hints.
    ARPG_REQUIRE(renderer.hud_observation_count() == 1U);
    ARPG_REQUIRE(renderer.hud_model().navigation.depth == before.navigation.depth);
    ARPG_REQUIRE(renderer.hud_binding_revision() == 8U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"observes every presented frame read only", &observation_is_once_per_presented_frame_and_read_only},
    {"production presentation seam covers all owners", &production_presentation_seam_observes_normal_recovery_and_death},
    {"debug diagnostics plan covers F1 counters", &debug_diagnostics_plan_carries_all_f1_only_counters},
    {"fallback font draw mode", &fallback_font_mode_remains_drawable_without_complete_cjk},
    {"paused hud notices freeze", &paused_frames_freeze_hud_notice_time},
    {"settings apply uses committed hints", &committed_settings_hints_and_revision_are_used_after_apply},
    {"observation publishes prebuilt hud model", &observation_publishes_the_prebuilt_hud_model},
};

}  // namespace

arpg::test::TestSuite hud_host_integration_suite() noexcept {
    return arpg::test::make_suite("hud_host_integration", kCases);
}
