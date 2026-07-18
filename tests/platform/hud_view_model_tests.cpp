#include "test_framework.hpp"

#include "hud_view_model.hpp"

#include <cstring>
#include <limits>

namespace {

namespace dungeon = arpg::dungeon;
namespace platform = arpg::platform;

platform::ControlHints default_hints() noexcept {
    platform::ControlHints hints{};
    static_cast<void>(std::snprintf(hints.primary.data(), hints.primary.size(),
        "W Move Up  S Move Down"));
    static_cast<void>(std::snprintf(hints.secondary.data(), hints.secondary.size(),
        "J Attack  K Jump"));
    return hints;
}

dungeon::DungeonSnapshot normal_snapshot() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.depth = 7U;
    snapshot.floor_room_index = 3U;
    snapshot.ecology = dungeon::DungeonElement::water;
    snapshot.biases = {{11U, 22U, 33U, 44U}};
    snapshot.remaining_targets = 5U;
    snapshot.progression = {4U, 40U, 3U, 2U};
    snapshot.combat.emplace();
    snapshot.combat->player.hp = 80;
    snapshot.combat->player.max_hp = 100;
    snapshot.combat->player.barrier = 25;
    snapshot.combat->player.max_barrier = 50;
    return snapshot;
}

arpg::test::Failure normal_combat_projects_snapshot_values() noexcept {
    const dungeon::DungeonSnapshot snapshot = normal_snapshot();
    platform::HudViewModel output{};
    platform::build_hud_view_model(
        output, snapshot, {}, default_hints());

    ARPG_REQUIRE(output.player.visible);
    ARPG_REQUIRE(output.player.hp == 80);
    ARPG_REQUIRE(output.player.max_hp == 100);
    ARPG_REQUIRE(arpg::test::near(output.player.hp_ratio, 0.8F));
    ARPG_REQUIRE(output.player.barrier == 25);
    ARPG_REQUIRE(output.player.max_barrier == 50);
    ARPG_REQUIRE(arpg::test::near(output.player.barrier_ratio, 0.5F));
    ARPG_REQUIRE(output.player.level == 4U);
    ARPG_REQUIRE(output.player.experience == 40U);
    ARPG_REQUIRE(output.player.required_experience == 100U);
    ARPG_REQUIRE(arpg::test::near(output.player.experience_ratio, 0.4F));
    ARPG_REQUIRE(output.player.unspent_passive_points == 2U);
    ARPG_REQUIRE(output.room.remaining_targets == 5U);
    ARPG_REQUIRE(output.navigation.depth == 7U);
    ARPG_REQUIRE(output.navigation.floor_room == 3U);
    ARPG_REQUIRE(output.navigation.ecology == dungeon::DungeonElement::water);
    ARPG_REQUIRE(output.navigation.biases == snapshot.biases);
    ARPG_REQUIRE(output.diagnostics.clamped_values == 0U);
    ARPG_REQUIRE(output.diagnostics.truncated_texts == 0U);
    ARPG_REQUIRE(!output.diagnostics.combat_snapshot_missing);
    return {};
}

arpg::test::Failure absent_combat_snapshot_is_reported() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, default_hints());

    ARPG_REQUIRE(!output.player.visible);
    ARPG_REQUIRE(output.diagnostics.combat_snapshot_missing);
    ARPG_REQUIRE(output.player.status_tag_count == 0U);
    return {};
}

arpg::test::Failure resource_ranges_clamp_without_mutating_snapshot() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.combat->player.hp = 101;
    snapshot.combat->player.barrier = 51;
    const dungeon::DungeonSnapshot before = snapshot;
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, default_hints());

    ARPG_REQUIRE(output.player.hp == 100);
    ARPG_REQUIRE(output.player.barrier == 50);
    ARPG_REQUIRE(arpg::test::near(output.player.hp_ratio, 1.0F));
    ARPG_REQUIRE(arpg::test::near(output.player.barrier_ratio, 1.0F));
    ARPG_REQUIRE(output.diagnostics.clamped_values == 2U);
    ARPG_REQUIRE(std::memcmp(&snapshot, &before, sizeof(snapshot)) == 0);
    return {};
}

arpg::test::Failure maximum_level_has_no_pending_threshold() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.progression = {100U, 0U, 99U, 99U};
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, default_hints());

    ARPG_REQUIRE(output.player.level == 100U);
    ARPG_REQUIRE(output.player.experience == 0U);
    ARPG_REQUIRE(output.player.required_experience == 0U);
    ARPG_REQUIRE(arpg::test::near(output.player.experience_ratio, 0.0F));
    return {};
}

arpg::test::Failure pending_experience_is_shown_in_secondary_text() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.pending_room_experience = 75U;
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, default_hints());

    ARPG_REQUIRE(std::strstr(output.room.secondary.bytes.data(), "75") != nullptr);
    return {};
}

arpg::test::Failure active_statuses_project_in_fixed_priority_order() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.combat->player.slow_bp = 1;
    snapshot.combat->player.slow_ticks = 1U;
    snapshot.combat->player.corrosion_damage_per_second = 1;
    snapshot.combat->player.corrosion_ticks = 1U;
    snapshot.combat->player.invulnerability_ticks = 1U;
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, default_hints());

    ARPG_REQUIRE(output.player.status_tag_count == 3U);
    ARPG_REQUIRE(output.player.status_tags[0] == platform::HudStatusTagKind::slow);
    ARPG_REQUIRE(output.player.status_tags[1] == platform::HudStatusTagKind::corrosion);
    ARPG_REQUIRE(output.player.status_tags[2] == platform::HudStatusTagKind::invulnerable);
    return {};
}

arpg::test::Failure navigation_preserves_all_uint32_biases() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.biases.fill((std::numeric_limits<std::uint32_t>::max)());
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, default_hints());

    ARPG_REQUIRE(output.navigation.biases == snapshot.biases);
    return {};
}

arpg::test::Failure objective_uses_chinese_target_text() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, default_hints());

    ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), u8"目标") != nullptr);
    ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), "5") != nullptr);
    return {};
}

arpg::test::Failure long_secondary_text_truncates_and_terminates() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.pending_room_experience = 1U;
    platform::ControlHints hints{};
    hints.primary.fill('P');
    hints.secondary.fill('S');
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, hints);

    ARPG_REQUIRE(output.room.secondary.truncated);
    ARPG_REQUIRE(output.room.secondary.bytes.back() == '\0');
    ARPG_REQUIRE(output.diagnostics.truncated_texts == 1U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"normal combat projection", &normal_combat_projects_snapshot_values},
    {"missing combat snapshot", &absent_combat_snapshot_is_reported},
    {"resource clamping is non-mutating", &resource_ranges_clamp_without_mutating_snapshot},
    {"level 100 threshold", &maximum_level_has_no_pending_threshold},
    {"pending experience text", &pending_experience_is_shown_in_secondary_text},
    {"status tag capacity", &active_statuses_project_in_fixed_priority_order},
    {"uint32 navigation biases", &navigation_preserves_all_uint32_biases},
    {"Chinese objective text", &objective_uses_chinese_target_text},
    {"text truncation and termination", &long_secondary_text_truncates_and_terminates},
};

}  // namespace

arpg::test::TestSuite hud_view_model_suite() noexcept {
    return arpg::test::make_suite("hud_view_model", kCases);
}
