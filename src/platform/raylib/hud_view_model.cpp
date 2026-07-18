#include "hud_view_model.hpp"

#include "dungeon_view_math.hpp"
#include "hud_notice_state.hpp"
#include "progression/progression_rules.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace arpg::platform {
namespace {

template <typename... Args>
void format_text(HudText96& output,
    HudBuildDiagnostics& diagnostics,
    const char* format,
    Args... args) noexcept {
    const int written = std::snprintf(
        output.bytes.data(), output.bytes.size(), format, args...);
    output.bytes.back() = '\0';
    output.truncated = written < 0
        || static_cast<std::size_t>(written) >= output.bytes.size();
    if (output.truncated) {
        ++diagnostics.truncated_texts;
    }
}

int clamped_resource(int value,
    int maximum,
    HudBuildDiagnostics& diagnostics) noexcept {
    const int bounded_maximum = std::max(0, maximum);
    const int bounded_value = std::clamp(value, 0, bounded_maximum);
    if (bounded_value != value) {
        ++diagnostics.clamped_values;
    }
    return bounded_value;
}

float resource_ratio(int value, int maximum) noexcept {
    if (maximum <= 0) {
        return 0.0F;
    }
    return std::clamp(static_cast<float>(value) / static_cast<float>(maximum),
        0.0F, 1.0F);
}

void append_status_tag(PlayerHudModel& player, HudStatusTagKind tag) noexcept {
    if (player.status_tag_count >= player.status_tags.size()) {
        return;
    }
    player.status_tags[player.status_tag_count++] = tag;
}

const char* ecology_name(dungeon::DungeonElement ecology) noexcept {
    switch (ecology) {
    case dungeon::DungeonElement::fire: return u8"火";
    case dungeon::DungeonElement::water: return u8"水";
    case dungeon::DungeonElement::lightning: return u8"电";
    case dungeon::DungeonElement::chaos: return u8"混沌";
    }
    return u8"未知";
}

NavigationHudModel::Element::Color element_color(
    dungeon::DungeonElement ecology) noexcept {
    switch (ecology) {
    case dungeon::DungeonElement::fire: return {227U, 91U, 62U, 255U};
    case dungeon::DungeonElement::water: return {64U, 169U, 222U, 255U};
    case dungeon::DungeonElement::lightning: return {240U, 211U, 73U, 255U};
    case dungeon::DungeonElement::chaos: return {166U, 91U, 205U, 255U};
    }
    return {166U, 91U, 205U, 255U};
}

void build_room_objective(RoomHudModel& room,
    HudBuildDiagnostics& diagnostics,
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    const AbyssHudValues abyss = abyss_hud_values(snapshot);
    if (abyss.visible) {
        format_text(room.objective, diagnostics, u8"深渊 %s · %s",
            abyss.danger_label, abyss.rule_label);
        format_text(room.secondary, diagnostics, u8"待领奖励 %u · 未领取 %u",
            static_cast<unsigned>(abyss.pending_rewards),
            static_cast<unsigned>(abyss.unpicked_rewards));
        return;
    }

    switch (snapshot.phase) {
    case dungeon::RoomPhase::combat:
        format_text(room.objective, diagnostics, u8"第 %u/%u 波 · 剩余 %u",
            snapshot.wave_count == 0U ? 0U
                : static_cast<unsigned>(snapshot.wave_index) + 1U,
            static_cast<unsigned>(snapshot.wave_count),
            static_cast<unsigned>(snapshot.remaining_targets));
        break;
    case dungeon::RoomPhase::wave_delay:
        format_text(room.objective, diagnostics, u8"下一波即将开始 · 剩余 %u",
            static_cast<unsigned>(snapshot.remaining_targets));
        break;
    case dungeon::RoomPhase::cleared:
    case dungeon::RoomPhase::awaiting_exit:
        format_text(room.objective, diagnostics, u8"出口已开放");
        break;
    case dungeon::RoomPhase::committing:
        format_text(room.objective, diagnostics, u8"正在保存房间");
        break;
    case dungeon::RoomPhase::death_pending:
        format_text(room.objective, diagnostics, u8"正在处理撤退");
        break;
    case dungeon::RoomPhase::faulted:
        format_text(room.objective, diagnostics, u8"房间状态异常");
        break;
    default:
        format_text(room.objective, diagnostics, u8"目标：剩余 %u 个敌人",
            static_cast<unsigned>(snapshot.remaining_targets));
        break;
    }
}

void build_navigation(NavigationHudModel& navigation,
    HudBuildDiagnostics& diagnostics,
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    navigation.depth = snapshot.depth;
    navigation.floor_room = snapshot.floor_room_index;
    navigation.ecology = snapshot.ecology;
    navigation.biases = snapshot.biases;
    format_text(navigation.primary, diagnostics, u8"深度 %llu · 层房间 %llu",
        static_cast<unsigned long long>(navigation.depth),
        static_cast<unsigned long long>(navigation.floor_room));
    format_text(navigation.ecology_label, diagnostics, u8"生态：%s",
        ecology_name(navigation.ecology));
    constexpr std::array<dungeon::DungeonElement, 4> kElements{{
        dungeon::DungeonElement::fire, dungeon::DungeonElement::water,
        dungeon::DungeonElement::lightning, dungeon::DungeonElement::chaos,
    }};
    navigation.element_count = static_cast<std::uint8_t>(kElements.size());
    for (std::size_t index{}; index < kElements.size(); ++index) {
        navigation.elements[index].color = element_color(kElements[index]);
        format_text(navigation.elements[index].label, diagnostics, u8"%s %u",
            ecology_name(kElements[index]),
            static_cast<unsigned>(navigation.biases[index]));
    }
}

}  // namespace

void build_hud_view_model(HudViewModel& output,
    const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& hints) noexcept {
    output = {};

    output.room.abyss = snapshot.is_abyss;
    output.room.remaining_targets = snapshot.remaining_targets;
    build_room_objective(output.room, output.diagnostics, snapshot);
    if (!output.room.abyss) {
        format_text(output.room.secondary, output.diagnostics,
            u8"待结算经验 +%llu | %.32s | %.32s",
            static_cast<unsigned long long>(snapshot.pending_room_experience),
            hints.primary.data(), hints.secondary.data());
    }
    build_navigation(output.navigation, output.diagnostics, snapshot);

    static_cast<void>(runtime_status);
    if (!snapshot.combat.has_value()) {
        output.diagnostics.combat_snapshot_missing = true;
        return;
    }

    const combat::PlayerSnapshot& player = snapshot.combat->player;
    output.player.visible = true;
    output.player.max_hp = std::max(0, player.max_hp);
    output.player.hp = clamped_resource(
        player.hp, output.player.max_hp, output.diagnostics);
    output.player.max_barrier = std::max(0, player.max_barrier);
    output.player.barrier = clamped_resource(
        player.barrier, output.player.max_barrier, output.diagnostics);
    output.player.hp_ratio = resource_ratio(
        output.player.hp, output.player.max_hp);
    output.player.barrier_ratio = resource_ratio(
        output.player.barrier, output.player.max_barrier);

    output.player.level = snapshot.progression.level;
    output.player.experience = snapshot.progression.experience;
    output.player.unspent_passive_points =
        snapshot.progression.unspent_passive_points;
    static const progression::ProgressionRules kRules =
        progression::default_progression_rules();
    if (output.player.level > 0U
        && output.player.level < progression::kMaximumLevel) {
        output.player.required_experience = kRules.experience_to_next[
            static_cast<std::size_t>(output.player.level - 1U)];
        output.player.experience_ratio = resource_ratio(
            static_cast<int>(std::min<std::uint64_t>(output.player.experience,
                static_cast<std::uint64_t>((std::numeric_limits<int>::max)()))),
            static_cast<int>(output.player.required_experience));
    }

    if (player.slow_bp != 0 && player.slow_ticks != 0U) {
        append_status_tag(output.player, HudStatusTagKind::slow);
    }
    if (player.corrosion_damage_per_second != 0
        && player.corrosion_ticks != 0U) {
        append_status_tag(output.player, HudStatusTagKind::corrosion);
    }
    if (player.invulnerability_ticks != 0U) {
        append_status_tag(output.player, HudStatusTagKind::invulnerable);
    }
}

void attach_notice_view(HudViewModel& output,
    const HudNoticeView& notices) noexcept {
    output.context.primary = notices.primary.text;
    output.context.secondary = notices.secondary.text;
    output.context.primary_kind = notices.primary.kind;
    output.context.secondary_kind = notices.secondary.kind;
}

}  // namespace arpg::platform
