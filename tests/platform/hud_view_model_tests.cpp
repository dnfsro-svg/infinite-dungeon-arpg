#include "test_framework.hpp"

#include "hud_view_model.hpp"
#include "hud_notice_state.hpp"
#include "hud_palette.hpp"
#include "dungeon/room_affix.hpp"

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

arpg::test::Failure large_room_progression_uses_explicit_chinese_fields() noexcept {
    std::uint64_t horde_seed{};
    for (std::uint64_t candidate = 1U; candidate != 10000U; ++candidate) {
        if (dungeon::roll_room_density(candidate, true).affix
                == dungeon::RoomDensityAffix::horde) {
            horde_seed = candidate;
            break;
        }
    }
    ARPG_REQUIRE(horde_seed != 0U);

    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.room_seed = horde_seed;
    snapshot.is_abyss = true;
    snapshot.phase = dungeon::RoomPhase::combat;
    snapshot.initial_monster_count = 1125U;
    snapshot.defeated_monster_count = 281U;
    snapshot.remaining_targets = 844U;
    snapshot.exits_unlocked = false;
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, default_hints());

    ARPG_REQUIRE(std::strcmp(output.room.density_text.bytes.data(),
        u8"怪群规模：兽潮") == 0);
    ARPG_REQUIRE(std::strcmp(output.room.progress_text.bytes.data(),
        u8"消灭 281/282（总计 1125）") == 0);
    ARPG_REQUIRE(std::strcmp(output.room.remaining_text.bytes.data(),
        u8"剩余 844") == 0);
    ARPG_REQUIRE(std::strcmp(output.room.exit_text.bytes.data(),
        u8"出口尚未开放") == 0);
    ARPG_REQUIRE(output.room.density_affix
        == dungeon::RoomDensityAffix::horde);
    ARPG_REQUIRE(output.room.required_kills == 282U);
    ARPG_REQUIRE(output.diagnostics.truncated_texts == 0U);
    return {};
}

arpg::test::Failure exit_copy_distinguishes_threshold_full_clear_and_abyss_warning() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.phase = dungeon::RoomPhase::combat;
    snapshot.initial_monster_count = 1125U;
    snapshot.defeated_monster_count = 282U;
    snapshot.remaining_targets = 843U;
    snapshot.exits_unlocked = true;
    platform::HudViewModel output{};

    platform::build_hud_view_model(output, snapshot, {}, default_hints());
    ARPG_REQUIRE(std::strcmp(output.room.exit_text.bytes.data(),
        u8"出口已开放，战斗仍可继续") == 0);

    snapshot.is_abyss = true;
    platform::build_hud_view_model(output, snapshot, {}, default_hints());
    ARPG_REQUIRE(std::strcmp(output.room.exit_text.bytes.data(),
        u8"出口已开放，离开将放弃剩余奖励") == 0);

    snapshot.phase = dungeon::RoomPhase::awaiting_exit;
    snapshot.defeated_monster_count = 1125U;
    snapshot.remaining_targets = 0U;
    platform::build_hud_view_model(output, snapshot, {}, default_hints());
    ARPG_REQUIRE(std::strcmp(output.room.exit_text.bytes.data(),
        u8"出口已开放") == 0);
    return {};
}

arpg::test::Failure objective_preserves_the_complete_movement_hint() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.pending_room_experience = 75U;
    platform::ControlHints hints{};
    static_cast<void>(std::snprintf(hints.primary.data(), hints.primary.size(),
        "W Move Up  S Move Down  A Move Left  D Move Right"));
    static_cast<void>(std::snprintf(hints.secondary.data(), hints.secondary.size(),
        "J Light Attack  K Jump  L Launcher  E Interact  I Inventory  "
        "P Passive Tree  F1 Debug  F12 Screenshot  Esc Pause"));
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, hints);

    ARPG_REQUIRE(std::strcmp(output.room.secondary.bytes.data(),
        u8"待结算经验 +75") == 0);
    ARPG_REQUIRE(std::strcmp(output.room.movement.bytes.data(),
        "W Move Up  S Move Down  A Move Left  D Move Right") == 0);
    ARPG_REQUIRE(std::strcmp(output.room.controls[0].bytes.data(),
        "J Light Attack  K Jump  L Launcher") == 0);
    ARPG_REQUIRE(std::strcmp(output.room.controls[1].bytes.data(),
        "E Interact  I Inventory  P Passive Tree") == 0);
    ARPG_REQUIRE(std::strcmp(output.room.controls[2].bytes.data(),
        "F1 Debug  F12 Screenshot  Esc Pause") == 0);
    ARPG_REQUIRE(!output.room.secondary.truncated);
    ARPG_REQUIRE(!output.room.movement.truncated);
    for (const platform::HudText96& line : output.room.controls) {
        ARPG_REQUIRE(!line.truncated);
        ARPG_REQUIRE(line.bytes.back() == '\0');
    }
    ARPG_REQUIRE(output.diagnostics.truncated_texts == 0U);
    return {};
}

arpg::test::Failure non_terminated_hint_buffers_are_bounded_and_terminated() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.pending_room_experience = 1U;
    platform::ControlHints hints{};
    hints.primary.fill('P');
    hints.secondary.fill('S');
    hints.revision = (std::numeric_limits<std::uint64_t>::max)();
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, hints);

    ARPG_REQUIRE(std::strstr(output.room.movement.bytes.data(),
        "PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP") != nullptr);
    ARPG_REQUIRE(std::strstr(output.room.controls[0].bytes.data(), "SSSS")
        != nullptr);
    ARPG_REQUIRE(output.room.movement.truncated);
    ARPG_REQUIRE(output.room.controls[0].truncated);
    ARPG_REQUIRE(output.room.movement.bytes.back() == '\0');
    for (const platform::HudText96& line : output.room.controls) {
        ARPG_REQUIRE(line.bytes.back() == '\0');
    }
    ARPG_REQUIRE(output.diagnostics.truncated_texts == 2U);
    return {};
}

arpg::test::Failure truncated_secondary_text_remains_nul_terminated() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.pending_room_experience =
        (std::numeric_limits<std::uint64_t>::max)();
    platform::ControlHints hints{};
    static_cast<void>(std::snprintf(hints.primary.data(), hints.primary.size(),
        "W Move Up  S Move Down  A Move Left  D Move Right"));
    hints.secondary.fill('S');
    hints.secondary.back() = '\0';
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, hints);

    ARPG_REQUIRE(!output.room.secondary.truncated);
    ARPG_REQUIRE(output.room.secondary.bytes.back() == '\0');
    ARPG_REQUIRE(output.room.controls[0].truncated);
    ARPG_REQUIRE(output.room.controls[0].bytes.back() == '\0');
    ARPG_REQUIRE(output.diagnostics.truncated_texts == 1U);
    return {};
}

arpg::test::Failure objective_describes_every_player_visible_room_state() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.wave_index = 1U;
    snapshot.wave_count = 3U;
    platform::HudViewModel output{};

    snapshot.phase = dungeon::RoomPhase::combat;
    platform::build_hud_view_model(output, snapshot, {}, default_hints());
    ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), u8"第 2/3 波") != nullptr);
    ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), u8"剩余 5") != nullptr);

    snapshot.phase = dungeon::RoomPhase::wave_delay;
    platform::build_hud_view_model(output, snapshot, {}, default_hints());
    ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), u8"下一波") != nullptr);

    snapshot.phase = dungeon::RoomPhase::cleared;
    platform::build_hud_view_model(output, snapshot, {}, default_hints());
    ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), u8"出口已开放") != nullptr);

    snapshot.phase = dungeon::RoomPhase::committing;
    platform::build_hud_view_model(output, snapshot, {}, default_hints());
    ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), u8"正在保存") != nullptr);

    snapshot.is_abyss = true;
    snapshot.abyss_danger = arpg::abyss::AbyssDanger::high;
    snapshot.abyss_rule = arpg::abyss::AbyssRuleId::abyss_fury;
    snapshot.abyss_pending_rewards = 3U;
    snapshot.abyss_unpicked_rewards = 2U;
    platform::build_hud_view_model(output, snapshot, {}, default_hints());
    ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), u8"深渊") != nullptr);
    ARPG_REQUIRE(std::strstr(output.room.objective.bytes.data(), "ABYSS HIGH") != nullptr);
    ARPG_REQUIRE(std::strstr(output.room.secondary.bytes.data(),
        u8"待领奖励 3") != nullptr);
    ARPG_REQUIRE(std::strstr(output.room.secondary.bytes.data(),
        u8"未领取 2") != nullptr);
    ARPG_REQUIRE(std::strstr(output.room.movement.bytes.data(), "W Move Up")
        != nullptr);
    ARPG_REQUIRE(std::strstr(output.room.controls[0].bytes.data(), "J Attack")
        != nullptr);
    return {};
}

arpg::test::Failure navigation_formats_extremes_and_four_shared_element_visuals() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.depth = (std::numeric_limits<std::uint64_t>::max)();
    snapshot.floor_room_index = (std::numeric_limits<std::uint64_t>::max)();
    snapshot.biases.fill((std::numeric_limits<std::uint32_t>::max)());
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, default_hints());

    ARPG_REQUIRE(std::strstr(output.navigation.primary.bytes.data(), u8"深度") != nullptr);
    ARPG_REQUIRE(std::strstr(output.navigation.primary.bytes.data(), "18446744073709551615") != nullptr);
    ARPG_REQUIRE(std::strstr(output.navigation.primary.bytes.data(), u8"层房间") != nullptr);
    ARPG_REQUIRE(output.navigation.element_count == 4U);
    ARPG_REQUIRE(std::strstr(output.navigation.elements[0].label.bytes.data(), u8"火") != nullptr);
    ARPG_REQUIRE(std::strstr(output.navigation.elements[1].label.bytes.data(), u8"水") != nullptr);
    ARPG_REQUIRE(std::strstr(output.navigation.elements[2].label.bytes.data(), u8"电") != nullptr);
    ARPG_REQUIRE(std::strstr(output.navigation.elements[3].label.bytes.data(), u8"混沌") != nullptr);
    const platform::HudPalette palette = platform::hud_palette();
    ARPG_REQUIRE(platform::hud_palette_color(output.navigation.elements[0].color_id).r
        == palette.fire.r);
    ARPG_REQUIRE(platform::hud_palette_color(output.navigation.elements[1].color_id).b
        == palette.water.b);
    ARPG_REQUIRE(platform::hud_palette_color(output.navigation.elements[2].color_id).g
        == palette.lightning.g);
    ARPG_REQUIRE(platform::hud_palette_color(output.navigation.elements[3].color_id).r
        == palette.chaos.r);
    return {};
}

arpg::test::Failure context_attaches_notice_priority_without_changing_abyss_values() noexcept {
    dungeon::DungeonSnapshot snapshot = normal_snapshot();
    snapshot.is_abyss = true;
    snapshot.abyss_rule = arpg::abyss::AbyssRuleId::abyss_fury;
    snapshot.abyss_exit_confirmation_armed = true;
    snapshot.abyss_exit_confirmation_transition = dungeon::TransitionKind::descent;
    const auto before = arpg::platform::abyss_hud_values(snapshot);
    platform::HudNoticeView notice{};
    notice.primary.kind = platform::HudNoticeKind::abyss_abandon;
    static_cast<void>(std::snprintf(notice.primary.text.bytes.data(),
        notice.primary.text.bytes.size(), "E %s", before.confirmation_label));
    notice.secondary.kind = platform::HudNoticeKind::hole_interact;
    static_cast<void>(std::snprintf(notice.secondary.text.bytes.data(),
        notice.secondary.text.bytes.size(), "E to descend"));
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, snapshot, {}, default_hints());
    platform::attach_notice_view(output, notice);

    ARPG_REQUIRE(output.context.primary_kind == platform::HudNoticeKind::abyss_abandon);
    ARPG_REQUIRE(output.context.secondary_kind == platform::HudNoticeKind::hole_interact);
    ARPG_REQUIRE(std::strstr(output.context.primary.bytes.data(), "Press E again") != nullptr);
    const auto after = arpg::platform::abyss_hud_values(snapshot);
    ARPG_REQUIRE(after.confirmation_visible == before.confirmation_visible);
    ARPG_REQUIRE(after.confirmation_label == before.confirmation_label);
    return {};
}

arpg::test::Failure rebound_context_hints_use_committed_e_i_and_p_labels() noexcept {
    dungeon::DungeonSnapshot previous{};
    dungeon::DungeonSnapshot current = normal_snapshot();
    current.has_active_room = true;
    current.phase = dungeon::RoomPhase::awaiting_exit;
    current.has_hole = true;
    current.exits_unlocked = true;
    platform::ControlHints hints{};
    static_cast<void>(std::snprintf(hints.secondary.data(), hints.secondary.size(),
        "E Interact  I Inventory  P Passive Tree"));
    platform::HudNoticeState notices{};
    notices.observe(previous, current, {}, hints, false);
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, current, {}, hints);
    platform::attach_notice_view(output, notices.view());

    ARPG_REQUIRE(output.context.primary_kind == platform::HudNoticeKind::hole_interact);
    ARPG_REQUIRE(std::strstr(output.context.primary.bytes.data(), "E ") != nullptr);

    current = normal_snapshot();
    current.inventory_count = 1U;
    current.progression.level = 0U;
    current.progression.unspent_passive_points = 0U;
    platform::HudNoticeState inventory_notices{};
    inventory_notices.observe(previous, current, {}, hints, false);
    platform::build_hud_view_model(output, current, {}, hints);
    platform::attach_notice_view(output, inventory_notices.view());
    ARPG_REQUIRE(output.context.primary_kind == platform::HudNoticeKind::inventory);
    ARPG_REQUIRE(std::strstr(output.context.primary.bytes.data(), "I ") != nullptr);

    current = normal_snapshot();
    current.has_active_room = true;
    current.phase = dungeon::RoomPhase::awaiting_exit;
    current.progression.level = 0U;
    current.progression.unspent_passive_points = 1U;
    platform::HudNoticeState passive_notices{};
    passive_notices.observe(previous, current, {}, hints, false);
    platform::build_hud_view_model(output, current, {}, hints);
    platform::attach_notice_view(output, passive_notices.view());
    ARPG_REQUIRE(output.context.secondary_kind == platform::HudNoticeKind::passive_tree);
    ARPG_REQUIRE(std::strstr(output.context.secondary.bytes.data(), "P ") != nullptr);
    return {};
}

arpg::test::Failure abyss_confirmation_context_preserves_descent_and_door_meaning_after_rebind() noexcept {
    dungeon::DungeonSnapshot previous = normal_snapshot();
    previous.is_abyss = true;
    previous.abyss_rule = arpg::abyss::AbyssRuleId::abyss_fury;
    platform::ControlHints hints{};
    static_cast<void>(std::snprintf(hints.secondary.data(), hints.secondary.size(),
        "F Interact"));

    dungeon::DungeonSnapshot descent = previous;
    descent.abyss_exit_confirmation_armed = true;
    descent.abyss_exit_confirmation_transition = dungeon::TransitionKind::descent;
    platform::HudNoticeState descent_notices{};
    descent_notices.observe(previous, descent, {}, hints, false);
    platform::HudViewModel output{};
    platform::build_hud_view_model(output, descent, {}, hints);
    platform::attach_notice_view(output, descent_notices.view());
    ARPG_REQUIRE(output.context.primary_kind == platform::HudNoticeKind::abyss_abandon);
    ARPG_REQUIRE(std::strstr(output.context.primary.bytes.data(), "F") != nullptr);
    ARPG_REQUIRE(std::strstr(output.context.primary.bytes.data(),
        u8"放弃剩余奖励") != nullptr);
    ARPG_REQUIRE(std::strstr(output.context.primary.bytes.data(), u8"下降") != nullptr);

    dungeon::DungeonSnapshot door = previous;
    door.abyss_exit_confirmation_armed = true;
    door.abyss_exit_confirmation_transition = dungeon::TransitionKind::door;
    door.abyss_exit_confirmation_direction = dungeon::ExitDirection::left;
    platform::HudNoticeState door_notices{};
    door_notices.observe(previous, door, {}, hints, false);
    platform::build_hud_view_model(output, door, {}, hints);
    platform::attach_notice_view(output, door_notices.view());
    ARPG_REQUIRE(output.context.primary_kind == platform::HudNoticeKind::abyss_abandon);
    ARPG_REQUIRE(std::strstr(output.context.primary.bytes.data(), "F") == nullptr);
    ARPG_REQUIRE(std::strstr(output.context.primary.bytes.data(), u8"离开后") != nullptr);
    ARPG_REQUIRE(std::strstr(output.context.primary.bytes.data(), u8"同一出口") != nullptr);
    ARPG_REQUIRE(std::strstr(output.context.primary.bytes.data(), u8"全部剩余奖励") != nullptr);
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
    {"large room explicit progression text",
        &large_room_progression_uses_explicit_chinese_fields},
    {"large room exit copy",
        &exit_copy_distinguishes_threshold_full_clear_and_abyss_warning},
    {"complete movement objective hint",
        &objective_preserves_the_complete_movement_hint},
    {"non-terminated hint buffers", &non_terminated_hint_buffers_are_bounded_and_terminated},
    {"truncated secondary text", &truncated_secondary_text_remains_nul_terminated},
    {"room objective states", &objective_describes_every_player_visible_room_state},
    {"navigation extremes and elements", &navigation_formats_extremes_and_four_shared_element_visuals},
    {"abyss context attachment", &context_attaches_notice_priority_without_changing_abyss_values},
    {"rebound context hints", &rebound_context_hints_use_committed_e_i_and_p_labels},
    {"abyss confirmation context semantics", &abyss_confirmation_context_preserves_descent_and_door_meaning_after_rebind},
};

}  // namespace

arpg::test::TestSuite hud_view_model_suite() noexcept {
    return arpg::test::make_suite("hud_view_model", kCases);
}
