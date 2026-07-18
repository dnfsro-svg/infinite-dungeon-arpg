#include "test_framework.hpp"

#include "combat_renderer.hpp"
#include "control_hints.hpp"
#include "dungeon_runtime.hpp"

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
    {"paused hud notices freeze", &paused_frames_freeze_hud_notice_time},
    {"settings apply uses committed hints", &committed_settings_hints_and_revision_are_used_after_apply},
    {"observation publishes prebuilt hud model", &observation_publishes_the_prebuilt_hud_model},
};

}  // namespace

arpg::test::TestSuite hud_host_integration_suite() noexcept {
    return arpg::test::make_suite("hud_host_integration", kCases);
}
