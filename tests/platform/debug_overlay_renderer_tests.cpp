#include "test_framework.hpp"
#include "allocation_probe.hpp"

#include "combat_view_math.hpp"
#include "debug_overlay_renderer.hpp"
#include "ui_text_catalog.hpp"
#include "ui_text_contrast.hpp"
#include "ui_utf8.hpp"

#include <array>
#include <cstddef>
#include <cstring>

namespace {

namespace combat = arpg::combat;
namespace dungeon = arpg::dungeon;
namespace platform = arpg::platform;

[[nodiscard]] platform::DebugOverlayDiagnosticsPlan diagnostics_for(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    platform::HudBuildDiagnostics hud{};
    hud.clamped_values = 2U;
    hud.truncated_texts = 3U;
    combat::CombatEvent event{};
    event.kind = combat::CombatEventKind::hit;
    event.target_ordinal = 7U;
    return platform::make_debug_overlay_diagnostics_plan(
        snapshot, hud, 4U, 5U, event, true, true);
}

[[nodiscard]] bool contains(const char* text, const char* needle) noexcept {
    return text != nullptr && needle != nullptr
        && std::strstr(text, needle) != nullptr;
}

[[nodiscard]] bool view_contains(
    const platform::DebugOverlayView& view, const char* needle) noexcept {
    for (std::size_t index = 0U; index < view.line_count; ++index) {
        if (contains(view.lines[index].text.data(), needle)) return true;
    }
    return false;
}

[[nodiscard]] bool rectangle_inside(
    Rectangle inner, int width, int height) noexcept {
    return inner.x >= 0.0F && inner.y >= 0.0F
        && inner.width > 0.0F && inner.height > 0.0F
        && inner.x + inner.width <= static_cast<float>(width)
        && inner.y + inner.height <= static_cast<float>(height);
}

arpg::test::Failure combat_and_transition_views_keep_twelve_localized_utf8_lines()
    noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.root_seed = 0x1234U;
    snapshot.room_seed = 0x5678U;
    snapshot.commit_generation = 9U;
    snapshot.has_hole = true;
    snapshot.combat.emplace();
    snapshot.combat->player.state = combat::PlayerState::attack_active;
    snapshot.combat->player.active_attack = combat::AttackId::j2;
    snapshot.combat->player.attack_phase = combat::AttackPhase::active;
    const platform::DungeonRenderStatus status{};
    const platform::CombatFeedback feedback{};

    const platform::DebugOverlayView combat_view =
        platform::make_debug_overlay_view(snapshot, status, feedback, true,
            diagnostics_for(snapshot));
    ARPG_REQUIRE(combat_view.line_count == 12U);
    for (std::size_t index = 0U; index < combat_view.line_count; ++index) {
        ARPG_REQUIRE(combat_view.lines[index].text[0] != '\0');
        ARPG_REQUIRE(platform::ui_utf8_is_valid(
            combat_view.lines[index].text.data()));
    }
    ARPG_REQUIRE(view_contains(combat_view, u8"根种子"));
    ARPG_REQUIRE(view_contains(combat_view, u8"玩家状态 攻击生效"));
    ARPG_REQUIRE(view_contains(combat_view, u8"动作 J2 生效"));

    constexpr std::array<const char*, 22U> kOldPlayerLabels{{
        "Root", "Room", "Generation", "Saved", "Budget", "Dungeon",
        "Tick", "State", "Hit stop", "Effects", "Action", "Combo",
        "Input", "Last event", "None", "FX", "Dropped", "Shake",
        "Audio", "Projectile", "Hazard", "HUD",
    }};
    for (const char* label : kOldPlayerLabels) {
        ARPG_REQUIRE(!view_contains(combat_view, label));
    }

    snapshot.combat.reset();
    const platform::DebugOverlayView transition_view =
        platform::make_debug_overlay_view(snapshot, status, feedback, false,
            diagnostics_for(snapshot));
    ARPG_REQUIRE(transition_view.line_count == 12U);
    ARPG_REQUIRE(view_contains(transition_view, u8"转场期间无战斗快照"));
    ARPG_REQUIRE(view_contains(transition_view, u8"音频不可用"));
    for (std::size_t index = 0U; index < transition_view.line_count; ++index) {
        ARPG_REQUIRE(transition_view.lines[index].text[0] != '\0');
        ARPG_REQUIRE(platform::ui_utf8_is_valid(
            transition_view.lines[index].text.data()));
    }
    return {};
}

arpg::test::Failure responsive_layout_switches_at_1280_with_six_lines_per_column()
    noexcept {
    const platform::DebugOverlayLayout narrow =
        platform::debug_overlay_layout(800, 450);
    ARPG_REQUIRE(narrow.column_count == 1U);
    for (std::size_t index = 1U; index < narrow.line_bounds.size(); ++index) {
        ARPG_REQUIRE(narrow.line_bounds[index].x
            == narrow.line_bounds[0].x);
        ARPG_REQUIRE(narrow.line_bounds[index].y
            > narrow.line_bounds[index - 1U].y);
    }

    for (const auto viewport : std::array<std::array<int, 2>, 2U>{{
            {{1280, 720}}, {{1920, 1080}},
        }}) {
        const platform::DebugOverlayLayout wide =
            platform::debug_overlay_layout(viewport[0], viewport[1]);
        ARPG_REQUIRE(wide.column_count == 2U);
        for (std::size_t index = 1U; index < 6U; ++index) {
            ARPG_REQUIRE(wide.line_bounds[index].x
                == wide.line_bounds[0].x);
            ARPG_REQUIRE(wide.line_bounds[index].y
                > wide.line_bounds[index - 1U].y);
            ARPG_REQUIRE(wide.line_bounds[index + 6U].x
                == wide.line_bounds[6U].x);
            ARPG_REQUIRE(wide.line_bounds[index + 6U].y
                > wide.line_bounds[index + 5U].y);
        }
        ARPG_REQUIRE(wide.line_bounds[6U].x
            > wide.line_bounds[0].x);
        ARPG_REQUIRE(wide.line_bounds[6U].y
            == wide.line_bounds[0].y);
    }
    return {};
}

arpg::test::Failure layout_rectangles_and_contrast_are_safe_on_supported_viewports()
    noexcept {
    constexpr std::array<std::array<int, 2>, 3U> kViewports{{
        {{800, 450}}, {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto viewport : kViewports) {
        const platform::DebugOverlayLayout layout =
            platform::debug_overlay_layout(viewport[0], viewport[1]);
        ARPG_REQUIRE(layout.backing_color.r == 0x0BU);
        ARPG_REQUIRE(layout.backing_color.g == 0x10U);
        ARPG_REQUIRE(layout.backing_color.b == 0x19U);
        ARPG_REQUIRE(layout.backing_color.a == 255U);
        for (std::size_t column = 0U; column < layout.column_count; ++column) {
            ARPG_REQUIRE(rectangle_inside(layout.column_backings[column],
                viewport[0], viewport[1]));
        }
        for (const Rectangle line : layout.line_bounds) {
            ARPG_REQUIRE(rectangle_inside(line, viewport[0], viewport[1]));
        }
    }

    dungeon::DungeonSnapshot snapshot{};
    platform::DungeonRenderStatus status{};
    platform::CombatFeedback feedback{};
    status.indicator = platform::SaveIndicator::error;
    const platform::DebugOverlayView view =
        platform::make_debug_overlay_view(snapshot, status, feedback, false,
            diagnostics_for(snapshot));
    const Color backing{0x0B, 0x10, 0x19, 255};
    bool saw_text{};
    bool saw_warning{};
    bool saw_error{};
    for (const platform::DebugOverlayLine& line : view.lines) {
        ARPG_REQUIRE(line.color.a == 255U);
        ARPG_REQUIRE(platform::ui_luma_contrast_ratio(
            line.color, backing) >= 4.5F);
        saw_text = saw_text || (line.color.r == 218U
            && line.color.g == 226U && line.color.b == 239U);
        saw_warning = saw_warning || (line.color.r == 255U
            && line.color.g == 210U && line.color.b == 118U);
        saw_error = saw_error || (line.color.r == 255U
            && line.color.g == 151U && line.color.b == 117U);
    }
    ARPG_REQUIRE(saw_text);
    ARPG_REQUIRE(saw_warning);
    ARPG_REQUIRE(saw_error);
    ARPG_REQUIRE(view_contains(view, u8"错误"));
    ARPG_REQUIRE(view_contains(view, u8"不可用"));
    return {};
}

arpg::test::Failure ten_thousand_view_builds_allocate_nothing() noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.combat.emplace();
    snapshot.combat->tick = 44U;
    const platform::DungeonRenderStatus status{};
    const platform::CombatFeedback feedback{};
    const platform::DebugOverlayDiagnosticsPlan diagnostics =
        diagnostics_for(snapshot);
    std::uint64_t checksum{};
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t index = 0U; index < 10000U; ++index) {
        const platform::DebugOverlayView view =
            platform::make_debug_overlay_view(snapshot, status, feedback,
                true, diagnostics);
        checksum += view.line_count
            + static_cast<unsigned char>(view.lines[0].text[0]);
    }
    const std::uint64_t after = arpg::test::allocation_count();
    ARPG_REQUIRE(checksum != 0U);
    ARPG_REQUIRE(after == before);
    return {};
}

arpg::test::Failure status_and_world_debug_labels_are_exhaustively_chinese()
    noexcept {
    dungeon::DungeonSnapshot snapshot{};
    snapshot.combat.emplace();
    const platform::DungeonRenderStatus status{};
    const platform::CombatFeedback feedback{};

    constexpr std::array<combat::AttackPhase, 4U> kAttackPhases{{
        combat::AttackPhase::startup, combat::AttackPhase::active,
        combat::AttackPhase::recovery, combat::AttackPhase::finished,
    }};
    constexpr std::array<const char*, 4U> kAttackLabels{{
        u8"起手", u8"生效", u8"后摇", u8"结束",
    }};
    snapshot.combat->player.active_attack = combat::AttackId::j1;
    for (std::size_t index = 0U; index < kAttackPhases.size(); ++index) {
        snapshot.combat->player.attack_phase = kAttackPhases[index];
        const platform::DebugOverlayView view =
            platform::make_debug_overlay_view(snapshot, status, feedback,
                true, diagnostics_for(snapshot));
        ARPG_REQUIRE(view_contains(view, kAttackLabels[index]));
    }

    constexpr std::array<combat::PlayerState, 8U> kPlayerStates{{
        combat::PlayerState::idle, combat::PlayerState::move,
        combat::PlayerState::attack_startup,
        combat::PlayerState::attack_active,
        combat::PlayerState::attack_recovery,
        combat::PlayerState::jump_rise, combat::PlayerState::jump_fall,
        combat::PlayerState::landing,
    }};
    constexpr std::array<const char*, 8U> kPlayerLabels{{
        u8"待机", u8"移动", u8"攻击起手", u8"攻击生效",
        u8"攻击后摇", u8"上升", u8"下落", u8"落地",
    }};
    for (std::size_t index = 0U; index < kPlayerStates.size(); ++index) {
        snapshot.combat->player.state = kPlayerStates[index];
        const platform::DebugOverlayView view =
            platform::make_debug_overlay_view(snapshot, status, feedback,
                true, diagnostics_for(snapshot));
        ARPG_REQUIRE(view_contains(view, kPlayerLabels[index]));
    }

    ARPG_REQUIRE(std::strcmp(platform::debug_world_volume_label(
        platform::DebugWorldVolumeKind::assist), u8"辅助判定") == 0);
    ARPG_REQUIRE(std::strcmp(platform::debug_world_volume_label(
        platform::DebugWorldVolumeKind::attack), u8"攻击判定") == 0);
    ARPG_REQUIRE(std::strcmp(platform::debug_world_volume_label(
        platform::DebugWorldVolumeKind::hurt), u8"受击判定") == 0);
    for (std::size_t index = 0U;
            index < static_cast<std::size_t>(combat::MonsterId::count);
            ++index) {
        const combat::MonsterId id = static_cast<combat::MonsterId>(index);
        const platform::MonsterVisual visual = platform::monster_visual(
            id, combat::MonsterAiPhase::idle,
            dungeon::DungeonElement::fire);
        ARPG_REQUIRE(std::strcmp(
            visual.role_label, platform::ui_monster_name(id)) == 0);
    }
    constexpr std::array<combat::MonsterAiPhase, 7U> kMonsterPhases{{
        combat::MonsterAiPhase::idle, combat::MonsterAiPhase::move,
        combat::MonsterAiPhase::telegraph, combat::MonsterAiPhase::active,
        combat::MonsterAiPhase::recovery, combat::MonsterAiPhase::cooldown,
        combat::MonsterAiPhase::defeated,
    }};
    constexpr std::array<const char*, 7U> kMonsterPhaseLabels{{
        u8"待机", u8"移动", u8"预警", u8"生效", u8"恢复",
        u8"冷却", u8"已击败",
    }};
    for (std::size_t index = 0U; index < kMonsterPhases.size(); ++index) {
        ARPG_REQUIRE(std::strcmp(platform::monster_phase_label(
            kMonsterPhases[index]), kMonsterPhaseLabels[index]) == 0);
    }
    constexpr std::array<const char*, 3U> kTierLabels{{
        u8"词缀Ⅰ", u8"词缀Ⅱ", u8"词缀Ⅲ",
    }};
    for (std::size_t index = 0U; index < kTierLabels.size(); ++index) {
        combat::MonsterAffixInstance affix{};
        affix.tier = static_cast<combat::MonsterAffixTier>(index);
        ARPG_REQUIRE(std::strcmp(
            platform::monster_affix_badge(affix).tier_text,
            kTierLabels[index]) == 0);
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"combat and transition fixed localized lines",
        &combat_and_transition_views_keep_twelve_localized_utf8_lines},
    {"responsive one and two column layout",
        &responsive_layout_switches_at_1280_with_six_lines_per_column},
    {"supported viewport bounds and contrast",
        &layout_rectangles_and_contrast_are_safe_on_supported_viewports},
    {"fixed-buffer view allocation pressure",
        &ten_thousand_view_builds_allocate_nothing},
    {"Chinese status and world debug labels",
        &status_and_world_debug_labels_are_exhaustively_chinese},
};

}  // namespace

arpg::test::TestSuite debug_overlay_renderer_suite() noexcept {
    return arpg::test::make_suite("debug_overlay_renderer", kCases);
}
