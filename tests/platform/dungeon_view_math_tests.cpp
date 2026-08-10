#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat_renderer.hpp"
#include "dungeon_view_math.hpp"
#include "progression/progression_rules.hpp"
#include "ui_utf8.hpp"

#include <array>
#include <cstring>

namespace {

using arpg::dungeon::DungeonEventKind;
using arpg::dungeon::DungeonSnapshot;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::RoomPhase;
using arpg::platform::DoorTheme;
using arpg::platform::DoorVisualMode;
using arpg::platform::EnvironmentHazardVisualMode;
using arpg::platform::HoleVisualMode;
using arpg::platform::Rgba8;
using arpg::platform::SaveIndicator;

DungeonSnapshot active_snapshot(
    std::uint64_t room_index,
    std::uint64_t room_seed,
    std::uint64_t room_instance_generation = 1U) noexcept {
    DungeonSnapshot snapshot{};
    snapshot.has_active_room = true;
    snapshot.room_index = room_index;
    snapshot.room_seed = room_seed;
    snapshot.room_instance_generation = room_instance_generation;
    snapshot.combat.emplace();
    return snapshot;
}

arpg::test::Failure door_modes_follow_room_lifecycle() noexcept {
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::locked, true, false)
        == DoorVisualMode::closed);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::combat, true, false)
        == DoorVisualMode::closed);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::cleared, true, true)
        == DoorVisualMode::open);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::awaiting_exit, true, true) == DoorVisualMode::open);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::committing, true, false) == DoorVisualMode::closed);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::committing, true, true) == DoorVisualMode::open);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::faulted, true, false) == DoorVisualMode::closed);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(
        RoomPhase::transitioning, true, false) == DoorVisualMode::hidden);
    ARPG_REQUIRE(arpg::platform::door_visual_mode(RoomPhase::combat, false, false)
        == DoorVisualMode::hidden);
    return {};
}

bool rgba_equals(Rgba8 lhs, Rgba8 rhs) noexcept {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b
        && lhs.a == rhs.a;
}

arpg::test::Failure door_themes_match_directional_elements() noexcept {
    const DoorTheme up = arpg::platform::door_theme(ExitDirection::up);
    const DoorTheme down = arpg::platform::door_theme(ExitDirection::down);
    const DoorTheme left = arpg::platform::door_theme(ExitDirection::left);
    const DoorTheme right = arpg::platform::door_theme(ExitDirection::right);
    const DoorTheme unknown = arpg::platform::door_theme(ExitDirection::none);
    ARPG_REQUIRE(up.element == arpg::dungeon::DungeonElement::fire);
    ARPG_REQUIRE(down.element == arpg::dungeon::DungeonElement::water);
    ARPG_REQUIRE(left.element == arpg::dungeon::DungeonElement::lightning);
    ARPG_REQUIRE(right.element == arpg::dungeon::DungeonElement::chaos);
    ARPG_REQUIRE(std::strcmp(up.label, u8"火焰") == 0);
    ARPG_REQUIRE(std::strcmp(down.label, u8"水流") == 0);
    ARPG_REQUIRE(std::strcmp(left.label, u8"闪电") == 0);
    ARPG_REQUIRE(std::strcmp(right.label, u8"混沌") == 0);
    ARPG_REQUIRE(std::strcmp(unknown.label, u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(up.arrow, "\xE2\x86\x91") == 0);
    ARPG_REQUIRE(std::strcmp(down.arrow, "\xE2\x86\x93") == 0);
    ARPG_REQUIRE(std::strcmp(left.arrow, "\xE2\x86\x90") == 0);
    ARPG_REQUIRE(std::strcmp(right.arrow, "\xE2\x86\x92") == 0);
    ARPG_REQUIRE(rgba_equals(up.frame, {236U, 92U, 54U, 255U}));
    ARPG_REQUIRE(rgba_equals(down.frame, {64U, 156U, 236U, 255U}));
    ARPG_REQUIRE(rgba_equals(left.frame, {236U, 218U, 72U, 255U}));
    ARPG_REQUIRE(rgba_equals(right.frame, {154U, 76U, 210U, 255U}));

    return {};
}

arpg::test::Failure directional_door_decisions_use_element_sprites_and_modes() noexcept {
    struct Expected final {
        ExitDirection direction;
        arpg::dungeon::DungeonElement element;
        arpg::platform::MaterialSpriteId sprite;
    };
    constexpr std::array<Expected, 4> kExpected{{
        {ExitDirection::up, arpg::dungeon::DungeonElement::fire,
            arpg::platform::MaterialSpriteId::environment_door_fire},
        {ExitDirection::down, arpg::dungeon::DungeonElement::water,
            arpg::platform::MaterialSpriteId::environment_door_water},
        {ExitDirection::left, arpg::dungeon::DungeonElement::lightning,
            arpg::platform::MaterialSpriteId::environment_door_lightning},
        {ExitDirection::right, arpg::dungeon::DungeonElement::chaos,
            arpg::platform::MaterialSpriteId::environment_door_chaos},
    }};
    for (const Expected& expected : kExpected) {
        const DoorTheme theme = arpg::platform::door_theme(expected.direction);
        const auto closed = arpg::platform::door_render_decision(
            DoorVisualMode::closed, expected.direction);
        const auto open = arpg::platform::door_render_decision(
            DoorVisualMode::open, expected.direction);
        ARPG_REQUIRE(theme.element == expected.element);
        ARPG_REQUIRE(closed.sprite == expected.sprite);
        ARPG_REQUIRE(open.sprite == expected.sprite);
        ARPG_REQUIRE(std::strcmp(closed.label, theme.label) == 0);
        ARPG_REQUIRE(rgba_equals(closed.text, theme.frame));
        ARPG_REQUIRE(rgba_equals(closed.body_tint, {150U, 150U, 150U, 255U}));
        ARPG_REQUIRE(rgba_equals(open.body_tint, {255U, 255U, 255U, 255U}));
        ARPG_REQUIRE(closed.draw_lock_marker);
        ARPG_REQUIRE(!open.draw_lock_marker);
    }
    return {};
}

arpg::test::Failure door_arrow_geometry_points_outward_inside_scaled_box() noexcept {
    constexpr float kCenterX = 240.0F;
    constexpr float kCenterY = 160.0F;
    constexpr float kScale = 2.0F;
    const auto inside = [](Vector2 point) noexcept {
        return point.x >= kCenterX - 24.0F * kScale
            && point.x <= kCenterX + 24.0F * kScale
            && point.y >= kCenterY - 24.0F * kScale
            && point.y <= kCenterY + 24.0F * kScale;
    };
    for (const ExitDirection direction : {ExitDirection::up,
             ExitDirection::down, ExitDirection::left, ExitDirection::right}) {
        const auto arrow = arpg::platform::door_arrow_geometry(
            direction, kCenterX, kCenterY, kScale);
        ARPG_REQUIRE(arrow.thickness > 0.0F);
        ARPG_REQUIRE(inside(arrow.tail));
        ARPG_REQUIRE(inside(arrow.tip));
        ARPG_REQUIRE(inside(arrow.head_left));
        ARPG_REQUIRE(inside(arrow.head_right));
        switch (direction) {
        case ExitDirection::up:
            ARPG_REQUIRE(arrow.tip.y < arrow.tail.y);
            break;
        case ExitDirection::down:
            ARPG_REQUIRE(arrow.tip.y > arrow.tail.y);
            break;
        case ExitDirection::left:
            ARPG_REQUIRE(arrow.tip.x < arrow.tail.x);
            break;
        case ExitDirection::right:
            ARPG_REQUIRE(arrow.tip.x > arrow.tail.x);
            break;
        case ExitDirection::none:
            ARPG_REQUIRE(false);
            break;
        }
    }
    return {};
}

arpg::test::Failure abyss_door_markers_follow_only_directional_preview() noexcept {
    DungeonSnapshot snapshot = active_snapshot(3U, 4U);
    snapshot.abyss_rule = arpg::abyss::AbyssRuleId::life_sacrifice;
    snapshot.abyss_danger = arpg::abyss::AbyssDanger::high;
    for (const ExitDirection direction : {ExitDirection::up,
             ExitDirection::down, ExitDirection::left,
             ExitDirection::right, ExitDirection::none}) {
        ARPG_REQUIRE(!arpg::platform::abyss_door_marker(snapshot, direction));
    }

    for (const ExitDirection selected : {ExitDirection::up,
             ExitDirection::down, ExitDirection::left,
             ExitDirection::right}) {
        snapshot.abyss_doors.fill(false);
        snapshot.abyss_doors[static_cast<std::size_t>(selected)] = true;
        for (const ExitDirection direction : {ExitDirection::up,
                 ExitDirection::down, ExitDirection::left,
                 ExitDirection::right}) {
            ARPG_REQUIRE(arpg::platform::abyss_door_marker(snapshot, direction)
                == (direction == selected));
        }
        const auto closed = arpg::platform::door_render_decision(
            DoorVisualMode::closed, selected);
        const auto open = arpg::platform::door_render_decision(
            DoorVisualMode::open, selected);
        ARPG_REQUIRE(closed.draw_lock_marker);
        ARPG_REQUIRE(!open.draw_lock_marker);
    }
    return {};
}

arpg::test::Failure environment_hazards_use_snapshot_geometry_and_phase() noexcept {
    struct Expected final {
        arpg::combat::HazardKind kind;
        Rgba8 active_outline;
    };
    constexpr Expected cases[] = {
        {arpg::combat::HazardKind::thunderstorm,
            {132U, 211U, 255U, 235U}},
        {arpg::combat::HazardKind::hunting_flame,
            {255U, 91U, 48U, 235U}},
        {arpg::combat::HazardKind::chaos_expansion,
            {221U, 62U, 188U, 235U}},
    };
    float radius = 0.8F;
    for (const Expected& expected : cases) {
        arpg::combat::HazardSnapshot hazard{};
        hazard.active = true;
        hazard.source = arpg::combat::HazardSource::abyss_environment;
        hazard.kind = expected.kind;
        hazard.center = {2.25F, -3.5F, 0.0F};
        hazard.radius = radius;
        hazard.telegraph_ticks = 12U;
        auto visual = arpg::platform::environment_hazard_visual(hazard);
        ARPG_REQUIRE(visual.mode == EnvironmentHazardVisualMode::warning);
        ARPG_REQUIRE(arpg::test::near(visual.center.x, 2.25, 1.0e-6));
        ARPG_REQUIRE(arpg::test::near(visual.center.y, -3.5, 1.0e-6));
        ARPG_REQUIRE(arpg::test::near(visual.radius, radius, 1.0e-6));
        ARPG_REQUIRE(rgba_equals(visual.outline, {255U, 203U, 91U, 235U}));

        hazard.telegraph_ticks = 0U;
        hazard.active_ticks = 60U;
        visual = arpg::platform::environment_hazard_visual(hazard);
        ARPG_REQUIRE(visual.mode == EnvironmentHazardVisualMode::active);
        ARPG_REQUIRE(rgba_equals(visual.outline, expected.active_outline));
        ARPG_REQUIRE(arpg::test::near(visual.radius, radius, 1.0e-6));
        radius += 0.2F;
    }

    arpg::combat::HazardSnapshot monster_hazard{};
    monster_hazard.active = true;
    monster_hazard.source = arpg::combat::HazardSource::monster;
    monster_hazard.kind = arpg::combat::HazardKind::thunderstorm;
    ARPG_REQUIRE(arpg::platform::environment_hazard_visual(monster_hazard).mode
        == EnvironmentHazardVisualMode::hidden);
    return {};
}

arpg::test::Failure abyss_hud_uses_committed_snapshot_labels_and_counts() noexcept {
    DungeonSnapshot snapshot = active_snapshot(5U, 6U);
    auto hidden = arpg::platform::abyss_hud_values(snapshot);
    ARPG_REQUIRE(!hidden.visible);

    snapshot.is_abyss = true;
    snapshot.abyss_rule = arpg::abyss::AbyssRuleId::abyss_fury;
    snapshot.abyss_danger = arpg::abyss::AbyssDanger::high;
    snapshot.abyss_pending_rewards = 2U;
    snapshot.abyss_unpicked_rewards = 1U;
    const auto values = arpg::platform::abyss_hud_values(snapshot);
    ARPG_REQUIRE(values.visible);
    ARPG_REQUIRE(std::strcmp(values.danger_label, u8"高危") == 0);
    ARPG_REQUIRE(std::strcmp(values.rule_label, u8"深渊狂怒") == 0);
    ARPG_REQUIRE(std::strcmp(values.effect_label,
        u8"怪物：伤害与攻击速度×145%") == 0);
    ARPG_REQUIRE(values.pending_rewards == 2U);
    ARPG_REQUIRE(values.unpicked_rewards == 1U);
    ARPG_REQUIRE(!values.confirmation_visible);

    snapshot.abyss_danger = arpg::abyss::AbyssDanger::low;
    snapshot.abyss_rule = arpg::abyss::AbyssRuleId::thunderstorm;
    const auto low = arpg::platform::abyss_hud_values(snapshot);
    ARPG_REQUIRE(std::strcmp(low.danger_label, u8"低危") == 0);
    ARPG_REQUIRE(std::strcmp(low.rule_label, u8"雷暴") == 0);
    ARPG_REQUIRE(std::strcmp(low.effect_label,
        u8"每180帧：预警45帧/半径0.8，造成15%最大生命闪电伤害") == 0);
    snapshot.abyss_danger = arpg::abyss::AbyssDanger::medium;
    ARPG_REQUIRE(std::strcmp(
        arpg::platform::abyss_hud_values(snapshot).danger_label,
        u8"中危") == 0);
    return {};
}

arpg::test::Failure every_abyss_rule_has_a_chinese_name_and_effect() noexcept {
    struct Expected final {
        arpg::abyss::AbyssRuleId rule;
        const char* name;
        const char* effect;
    };
    constexpr Expected kExpected[] = {
        {arpg::abyss::AbyssRuleId::thunderstorm, u8"雷暴",
            u8"每180帧：预警45帧/半径0.8，造成15%最大生命闪电伤害"},
        {arpg::abyss::AbyssRuleId::hunting_flames, u8"猎杀之焰",
            u8"每240帧：预警45帧/半径1.0，灼烧180帧，每60帧造成10%最大生命伤害"},
        {arpg::abyss::AbyssRuleId::chaos_expansion, u8"混沌扩张",
            u8"每180帧扩张至半径1.0/2.3/3.6/4.9/6.2，每60帧造成8%最大生命混沌伤害"},
        {arpg::abyss::AbyssRuleId::swift_pursuit, u8"迅捷追猎",
            u8"怪物：移动速度+15%，冷却时间×85%"},
        {arpg::abyss::AbyssRuleId::abyss_bulwark, u8"深渊壁垒",
            u8"怪物：护甲×130%，护盾+30%最大生命"},
        {arpg::abyss::AbyssRuleId::abyss_fury, u8"深渊狂怒",
            u8"怪物：伤害与攻击速度×145%"},
        {arpg::abyss::AbyssRuleId::heavy_steps, u8"沉重脚步",
            u8"玩家：地面移动速度×85%"},
        {arpg::abyss::AbyssRuleId::exhausted_recovery, u8"疲惫恢复",
            u8"玩家：入场资源及后续恢复×70%"},
        {arpg::abyss::AbyssRuleId::life_sacrifice, u8"生命献祭",
            u8"玩家：最大生命×55%"},
    };
    DungeonSnapshot snapshot = active_snapshot(1U, 2U);
    snapshot.is_abyss = true;
    snapshot.abyss_danger = arpg::abyss::AbyssDanger::low;
    for (const Expected& expected : kExpected) {
        snapshot.abyss_rule = expected.rule;
        const auto values = arpg::platform::abyss_hud_values(snapshot);
        ARPG_REQUIRE(values.visible);
        ARPG_REQUIRE(std::strcmp(values.rule_label, expected.name) == 0);
        ARPG_REQUIRE(std::strcmp(values.effect_label, expected.effect) == 0);
    }
    snapshot.abyss_rule = arpg::abyss::AbyssRuleId::none;
    const auto none = arpg::platform::abyss_hud_values(snapshot);
    ARPG_REQUIRE(!none.visible);
    ARPG_REQUIRE(std::strcmp(none.rule_label, u8"无规则") == 0);
    ARPG_REQUIRE(std::strcmp(none.effect_label, "") == 0);
    return {};
}

arpg::test::Failure abyss_confirmation_prompt_preserves_transition_and_direction() noexcept {
    DungeonSnapshot snapshot = active_snapshot(7U, 8U);
    snapshot.is_abyss = true;
    snapshot.abyss_rule = arpg::abyss::AbyssRuleId::hunting_flames;
    snapshot.abyss_danger = arpg::abyss::AbyssDanger::medium;
    snapshot.abyss_pending_rewards = 1U;
    snapshot.abyss_unpicked_rewards = 2U;
    snapshot.abyss_exit_confirmation_armed = true;
    snapshot.abyss_exit_confirmation_transition =
        arpg::dungeon::TransitionKind::door;
    snapshot.abyss_exit_confirmation_direction = ExitDirection::left;
    auto values = arpg::platform::abyss_hud_values(snapshot);
    ARPG_REQUIRE(values.confirmation_visible);
    ARPG_REQUIRE(values.confirmation_transition
        == arpg::dungeon::TransitionKind::door);
    ARPG_REQUIRE(values.confirmation_direction == ExitDirection::left);
    ARPG_REQUIRE(std::strcmp(values.confirmation_label,
        u8"离开后再次触碰同一出口以放弃全部剩余奖励") == 0);

    snapshot.abyss_exit_confirmation_transition =
        arpg::dungeon::TransitionKind::descent;
    snapshot.abyss_exit_confirmation_direction = ExitDirection::none;
    values = arpg::platform::abyss_hud_values(snapshot);
    ARPG_REQUIRE(std::strcmp(values.confirmation_label,
        u8"再次交互，放弃剩余奖励并下潜") == 0);

    snapshot.abyss_exit_confirmation_armed = false;
    ARPG_REQUIRE(!arpg::platform::abyss_hud_values(snapshot)
        .confirmation_visible);
    return {};
}

arpg::test::Failure ecosystem_tints_are_stable() noexcept {
    const Rgba8 fire = arpg::platform::ecosystem_tint(
        arpg::dungeon::DungeonElement::fire);
    const Rgba8 water = arpg::platform::ecosystem_tint(
        arpg::dungeon::DungeonElement::water);
    const Rgba8 lightning = arpg::platform::ecosystem_tint(
        arpg::dungeon::DungeonElement::lightning);
    const Rgba8 chaos = arpg::platform::ecosystem_tint(
        arpg::dungeon::DungeonElement::chaos);
    ARPG_REQUIRE(rgba_equals(fire, {88U, 43U, 34U, 255U}));
    ARPG_REQUIRE(rgba_equals(water, {35U, 70U, 104U, 255U}));
    ARPG_REQUIRE(rgba_equals(lightning, {96U, 90U, 34U, 255U}));
    ARPG_REQUIRE(rgba_equals(chaos, {73U, 43U, 99U, 255U}));
    return {};
}

arpg::test::Failure abyss_pulse_is_bounded_and_periodic() noexcept {
    const float negative = arpg::platform::abyss_pulse_alpha(-0.25F);
    const float at_start = arpg::platform::abyss_pulse_alpha(0.0F);
    const float at_cycle = arpg::platform::abyss_pulse_alpha(1.0F);
    const float huge = arpg::platform::abyss_pulse_alpha(100000000.0F);
    ARPG_REQUIRE(negative >= 0.0F && negative <= 1.0F);
    ARPG_REQUIRE(at_start >= 0.0F && at_start <= 1.0F);
    ARPG_REQUIRE(at_cycle >= 0.0F && at_cycle <= 1.0F);
    ARPG_REQUIRE(huge >= 0.0F && huge <= 1.0F);
    ARPG_REQUIRE(arpg::test::near(at_start, at_cycle, 1.0e-6));
    return {};
}

arpg::test::Failure hole_modes_follow_room_phase() noexcept {
    DungeonSnapshot snapshot = active_snapshot(1U, 2U);
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::hidden);
    snapshot.has_hole = true;
    snapshot.phase = RoomPhase::locked;
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::sealed);
    snapshot.phase = RoomPhase::combat;
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::sealed);
    snapshot.exits_unlocked = true;
    snapshot.phase = RoomPhase::cleared;
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::ready);
    snapshot.phase = RoomPhase::awaiting_exit;
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::ready);
    snapshot.phase = RoomPhase::committing;
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::busy);
    snapshot.phase = RoomPhase::faulted;
    ARPG_REQUIRE(arpg::platform::hole_visual_mode(snapshot)
        == HoleVisualMode::faulted);
    return {};
}

arpg::test::Failure player_hole_range_uses_xy_radius() noexcept {
    using arpg::combat::Vec3;
    const Vec3 center{4.0F, -2.0F, 5.0F};
    ARPG_REQUIRE(arpg::platform::player_in_hole_range(center, center, 2.0F));
    ARPG_REQUIRE(arpg::platform::player_in_hole_range(
        Vec3{6.0F, -2.0F, -100.0F}, center, 2.0F));
    ARPG_REQUIRE(!arpg::platform::player_in_hole_range(
        Vec3{6.01F, -2.0F, 0.0F}, center, 2.0F));
    ARPG_REQUIRE(!arpg::platform::player_in_hole_range(
        center, center, -1.0F));
    return {};
}

arpg::test::Failure indicators_and_phase_labels_are_stable() noexcept {
    ARPG_REQUIRE(std::strcmp(arpg::platform::save_indicator_label(
        SaveIndicator::none), "") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::save_indicator_label(
        SaveIndicator::saving), u8"保存中") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::save_indicator_label(
        SaveIndicator::saved), u8"已保存") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::save_indicator_label(
        SaveIndicator::recovered), u8"已恢复") == 0);
    ARPG_REQUIRE(std::strcmp(arpg::platform::save_indicator_label(
        SaveIndicator::error), u8"存档错误") == 0);

    struct PhaseExpected final { RoomPhase phase; const char* label; };
    constexpr PhaseExpected kPhases[] = {
        {RoomPhase::locked, u8"锁定"},
        {RoomPhase::combat, u8"战斗中"},
        {RoomPhase::wave_delay, u8"等待下一波"},
        {RoomPhase::cleared, u8"已清理"},
        {RoomPhase::awaiting_exit, u8"等待离开"},
        {RoomPhase::committing, u8"保存中"},
        {RoomPhase::transitioning, u8"切换房间"},
        {RoomPhase::death_pending, u8"处理撤退中"},
        {RoomPhase::faulted, u8"异常"},
    };
    for (const PhaseExpected& expected : kPhases) {
        ARPG_REQUIRE(std::strcmp(arpg::platform::room_phase_label(
            expected.phase), expected.label) == 0);
    }

    struct DirectionExpected final {
        ExitDirection direction;
        const char* label;
    };
    constexpr DirectionExpected kDirections[] = {
        {ExitDirection::up, u8"上方"},
        {ExitDirection::down, u8"下方"},
        {ExitDirection::left, u8"左方"},
        {ExitDirection::right, u8"右方"},
        {ExitDirection::none, u8"无方向"},
    };
    for (const DirectionExpected& expected : kDirections) {
        ARPG_REQUIRE(std::strcmp(arpg::platform::exit_direction_label(
            expected.direction), expected.label) == 0);
    }
    return {};
}

arpg::test::Failure visible_hole_modes_have_explicit_chinese_labels() noexcept {
    struct Expected final {
        HoleVisualMode mode;
        const char* label;
    };
    constexpr Expected kExpected[] = {
        {HoleVisualMode::sealed, u8"封闭"},
        {HoleVisualMode::ready, u8"可进入"},
        {HoleVisualMode::busy, u8"保存中"},
        {HoleVisualMode::faulted, u8"异常"},
    };
    ARPG_REQUIRE(std::strcmp(arpg::platform::hole_visual_label(
        HoleVisualMode::hidden), "") == 0);
    for (const Expected& expected : kExpected) {
        ARPG_REQUIRE(std::strcmp(arpg::platform::hole_visual_label(
            expected.mode), expected.label) == 0);
    }
    return {};
}

arpg::test::Failure recovery_requires_fault_and_n_press() noexcept {
    ARPG_REQUIRE(!arpg::platform::recovery_requested(false, false));
    ARPG_REQUIRE(!arpg::platform::recovery_requested(false, true));
    ARPG_REQUIRE(!arpg::platform::recovery_requested(true, false));
    ARPG_REQUIRE(arpg::platform::recovery_requested(true, true));
    return {};
}

arpg::test::Failure same_room_snapshots_can_interpolate() noexcept {
    const DungeonSnapshot previous = active_snapshot(7U, 0x1234U, 9U);
    const DungeonSnapshot current = active_snapshot(7U, 0x1234U, 9U);
    ARPG_REQUIRE(arpg::platform::can_interpolate_room(previous, current));

    const DungeonSnapshot reconstructed = active_snapshot(7U, 0x1234U, 10U);
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        previous, reconstructed));

    DungeonSnapshot death_visible = current;
    death_visible.death.emplace();
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        current, death_visible));
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        death_visible, current));

    DungeonSnapshot death_pending = current;
    death_pending.phase = RoomPhase::death_pending;
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        current, death_pending));
    return {};
}

arpg::test::Failure cross_room_and_missing_room_reject_interpolation() noexcept {
    const DungeonSnapshot room = active_snapshot(7U, 0x1234U);
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        room, active_snapshot(8U, 0x1234U)));
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        room, active_snapshot(7U, 0x5678U)));

    DungeonSnapshot missing_combat = room;
    missing_combat.combat.reset();
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        room, missing_combat));
    ARPG_REQUIRE(!arpg::platform::can_interpolate_room(
        DungeonSnapshot{}, room));
    return {};
}

arpg::test::Failure cleanup_routing_and_labels_are_stable() noexcept {
    ARPG_REQUIRE(arpg::platform::dungeon_event_clears_transients(
        DungeonEventKind::room_destroyed));
    ARPG_REQUIRE(arpg::platform::dungeon_event_clears_transients(
        DungeonEventKind::room_reset));
    ARPG_REQUIRE(!arpg::platform::dungeon_event_clears_transients(
        DungeonEventKind::room_entered));
    ARPG_REQUIRE(std::strcmp(
        arpg::platform::room_phase_label(RoomPhase::awaiting_exit),
        u8"等待离开") == 0);
    ARPG_REQUIRE(std::strcmp(
        arpg::platform::exit_direction_label(ExitDirection::left),
        u8"左方") == 0);
    ARPG_REQUIRE(std::strcmp(
        arpg::platform::exit_direction_label(ExitDirection::none),
        u8"无方向") == 0);
    return {};
}

arpg::test::Failure fade_alpha_clamps_to_transition_window() noexcept {
    using arpg::platform::TransitionVisualState;

    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::transition_overlay_alpha(-1.0F), 0.0, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::transition_overlay_alpha(0.0F), 0.0, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::transition_overlay_alpha(0.06F), 0.5, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::transition_overlay_alpha(0.12F), 1.0, 1.0e-6));
    ARPG_REQUIRE(arpg::test::near(
        arpg::platform::transition_overlay_alpha(1.0F), 1.0, 1.0e-6));

    const TransitionVisualState destroyed =
        arpg::platform::transition_after_dungeon_event(
            {}, DungeonEventKind::room_destroyed);
    ARPG_REQUIRE(arpg::test::near(
        destroyed.seconds_left, 0.12, 1.0e-6));

    const TransitionVisualState first_transition =
        arpg::platform::transition_after_room_phase(
            {}, RoomPhase::transitioning);
    ARPG_REQUIRE(arpg::test::near(
        first_transition.seconds_left, 0.12, 1.0e-6));
    ARPG_REQUIRE(first_transition.transition_phase_seen);

    const TransitionVisualState advanced =
        arpg::platform::advance_transition(
            first_transition, 0.05F);
    ARPG_REQUIRE(arpg::test::near(
        advanced.seconds_left, 0.07, 1.0e-6));
    const TransitionVisualState repeated_transition =
        arpg::platform::transition_after_room_phase(
            advanced, RoomPhase::transitioning);
    ARPG_REQUIRE(arpg::test::near(
        repeated_transition.seconds_left, 0.07, 1.0e-6));

    const TransitionVisualState left_transition =
        arpg::platform::transition_after_room_phase(
            repeated_transition, RoomPhase::combat);
    ARPG_REQUIRE(!left_transition.transition_phase_seen);
    const TransitionVisualState reentered_transition =
        arpg::platform::transition_after_room_phase(
            left_transition, RoomPhase::transitioning);
    ARPG_REQUIRE(arpg::test::near(
        reentered_transition.seconds_left, 0.12, 1.0e-6));

    const TransitionVisualState reset =
        arpg::platform::transition_after_dungeon_event(
            reentered_transition, DungeonEventKind::room_reset);
    ARPG_REQUIRE(arpg::test::near(
        reset.seconds_left, 0.0, 1.0e-6));
    ARPG_REQUIRE(!reset.transition_phase_seen);
    return {};
}

arpg::test::Failure progression_hud_values_follow_snapshot() noexcept {
    arpg::dungeon::DungeonSnapshot snapshot{};
    snapshot.progression = {37U, 42U, 36U, 36U};
    snapshot.pending_room_experience = 25U;
    const auto rules = arpg::progression::default_progression_rules();
    const auto values = arpg::platform::progression_hud_values(snapshot, rules);
    ARPG_REQUIRE(values.level == 37U);
    ARPG_REQUIRE(values.experience == 42U);
    ARPG_REQUIRE(values.required_experience == 100U);
    ARPG_REQUIRE(values.unspent_passive_points == 36U);
    ARPG_REQUIRE(values.pending_room_experience == 25U);
    ARPG_REQUIRE(!values.maximum_level);

    snapshot.progression = {100U, 0U, 99U, 99U};
    const auto maximum = arpg::platform::progression_hud_values(snapshot, rules);
    ARPG_REQUIRE(maximum.maximum_level);
    ARPG_REQUIRE(maximum.required_experience == 0U);
    return {};
}

arpg::test::Failure hole_prompt_follows_current_interact_binding() noexcept {
    using arpg::platform::HoleInteractionAction;
    const auto descend = arpg::platform::hole_interaction_prompt(
        "Q", HoleInteractionAction::descend);
    const auto next_floor = arpg::platform::hole_interaction_prompt(
        "Q", HoleInteractionAction::next_floor);
    const auto longest_prompt =
        arpg::platform::hole_interaction_prompt(
            u8"右 Shift", HoleInteractionAction::next_floor);
    ARPG_REQUIRE(std::strcmp(descend.data(), u8"Q：下潜") == 0);
    ARPG_REQUIRE(std::strcmp(next_floor.data(), u8"Q：进入下一层") == 0);
    ARPG_REQUIRE(std::strcmp(longest_prompt.data(),
        u8"右 Shift：进入下一层") == 0);
    ARPG_REQUIRE(arpg::platform::ui_utf8_is_valid(descend.data()));
    ARPG_REQUIRE(arpg::platform::ui_utf8_is_valid(next_floor.data()));
    ARPG_REQUIRE(arpg::platform::ui_utf8_is_valid(longest_prompt.data()));
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    for (std::size_t iteration = 0U; iteration < 10000U; ++iteration) {
        const auto prompt = arpg::platform::hole_interaction_prompt(
            "Q", iteration % 2U == 0U ? HoleInteractionAction::descend
                                       : HoleInteractionAction::next_floor);
        ARPG_REQUIRE(prompt.back() == '\0');
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"door lifecycle modes", &door_modes_follow_room_lifecycle},
    {"same room interpolation", &same_room_snapshots_can_interpolate},
    {"cross and missing room rejection", &cross_room_and_missing_room_reject_interpolation},
    {"cleanup routing and labels", &cleanup_routing_and_labels_are_stable},
    {"transition fade alpha", &fade_alpha_clamps_to_transition_window},
    {"directional door themes", &door_themes_match_directional_elements},
    {"directional door sprites and modes",
        &directional_door_decisions_use_element_sprites_and_modes},
    {"directional door arrow geometry",
        &door_arrow_geometry_points_outward_inside_scaled_box},
    {"abyss door marker preview", &abyss_door_markers_follow_only_directional_preview},
    {"environment hazard snapshot visuals", &environment_hazards_use_snapshot_geometry_and_phase},
    {"abyss HUD snapshot values", &abyss_hud_uses_committed_snapshot_labels_and_counts},
    {"all abyss rule names and effects",
        &every_abyss_rule_has_a_chinese_name_and_effect},
    {"abyss confirmation prompt values", &abyss_confirmation_prompt_preserves_transition_and_direction},
    {"ecosystem tints", &ecosystem_tints_are_stable},
    {"abyss pulse bounds", &abyss_pulse_is_bounded_and_periodic},
    {"hole visual modes", &hole_modes_follow_room_phase},
    {"hole visual Chinese labels",
        &visible_hole_modes_have_explicit_chinese_labels},
    {"XY hole range", &player_hole_range_uses_xy_radius},
    {"save indicators and labels", &indicators_and_phase_labels_are_stable},
    {"recovery request gate", &recovery_requires_fault_and_n_press},
    {"progression HUD values", &progression_hud_values_follow_snapshot},
    {"hole prompt follows interact binding",
        &hole_prompt_follows_current_interact_binding},
};

}  // namespace

arpg::test::TestSuite dungeon_view_math_suite() noexcept {
    return arpg::test::make_suite("dungeon_view_math", kCases);
}
