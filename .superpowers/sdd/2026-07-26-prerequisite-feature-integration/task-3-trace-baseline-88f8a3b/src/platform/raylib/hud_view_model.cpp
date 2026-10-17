#include "hud_view_model.hpp"

#include "dungeon_view_math.hpp"
#include "hud_notice_state.hpp"
#include "progression/progression_rules.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
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

HudPaletteId element_color(dungeon::DungeonElement ecology) noexcept {
    switch (ecology) {
    case dungeon::DungeonElement::fire: return HudPaletteId::fire;
    case dungeon::DungeonElement::water: return HudPaletteId::water;
    case dungeon::DungeonElement::lightning: return HudPaletteId::lightning;
    case dungeon::DungeonElement::chaos: return HudPaletteId::chaos;
    }
    return HudPaletteId::chaos;
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
        navigation.elements[index].color_id = element_color(kElements[index]);
        format_text(navigation.elements[index].label, diagnostics, u8"%s %u",
            ecology_name(kElements[index]),
            static_cast<unsigned>(navigation.biases[index]));
    }
}

[[nodiscard]] std::size_t bounded_hint_length(
    const std::array<char, 160U>& source) noexcept {
    std::size_t length{};
    while (length < source.size() && source[length] != '\0') ++length;
    return length;
}

void mark_truncated(HudText96& output,
    HudBuildDiagnostics& diagnostics) noexcept {
    if (output.truncated) return;
    output.truncated = true;
    ++diagnostics.truncated_texts;
}

void copy_movement_hint(HudText96& output,
    HudBuildDiagnostics& diagnostics,
    const std::array<char, 160U>& source) noexcept {
    const std::size_t length = bounded_hint_length(source);
    const std::size_t copied = std::min(length, output.bytes.size() - 1U);
    if (copied != 0U) {
        std::memcpy(output.bytes.data(), source.data(), copied);
    }
    output.bytes[copied] = '\0';
    if (copied != length || length == source.size()) {
        mark_truncated(output, diagnostics);
    }
}

[[nodiscard]] std::size_t text_length(const HudText96& text) noexcept {
    std::size_t length{};
    while (length < text.bytes.size() && text.bytes[length] != '\0') ++length;
    return length;
}

void split_control_hints(std::array<HudText96, 3U>& output,
    HudBuildDiagnostics& diagnostics,
    const std::array<char, 160U>& source) noexcept {
    constexpr std::size_t kPreferredLineBytes = 42U;
    const std::size_t source_length = bounded_hint_length(source);
    std::size_t cursor{};
    std::size_t line_index{};
    while (cursor < source_length) {
        while (cursor < source_length && source[cursor] == ' ') ++cursor;
        if (cursor == source_length) break;

        const std::size_t token_begin = cursor;
        while (cursor < source_length) {
            if (source[cursor] == ' ' && cursor + 1U < source_length
                && source[cursor + 1U] == ' ') {
                break;
            }
            ++cursor;
        }
        std::size_t token_end = cursor;
        while (token_end > token_begin && source[token_end - 1U] == ' ') {
            --token_end;
        }
        const std::size_t token_length = token_end - token_begin;
        if (token_length == 0U) continue;

        for (;;) {
            HudText96& line = output[line_index];
            const std::size_t line_length = text_length(line);
            const std::size_t separator = line_length == 0U ? 0U : 2U;
            const std::size_t required = separator + token_length;
            if (line_length != 0U
                && line_length + required > kPreferredLineBytes
                && line_index + 1U < output.size()) {
                ++line_index;
                continue;
            }
            if (line_length + required >= line.bytes.size()) {
                if (line_length != 0U && line_index + 1U < output.size()) {
                    ++line_index;
                    continue;
                }
                const std::size_t available = line.bytes.size() - 1U
                    - line_length - separator;
                std::size_t destination = line_length;
                if (separator != 0U) {
                    line.bytes[destination++] = ' ';
                    line.bytes[destination++] = ' ';
                }
                if (available != 0U) {
                    std::memcpy(line.bytes.data() + destination,
                        source.data() + token_begin, available);
                    destination += available;
                }
                line.bytes[destination] = '\0';
                mark_truncated(line, diagnostics);
                return;
            }

            std::size_t destination = line_length;
            if (separator != 0U) {
                line.bytes[destination++] = ' ';
                line.bytes[destination++] = ' ';
            }
            std::memcpy(line.bytes.data() + destination,
                source.data() + token_begin, token_length);
            destination += token_length;
            line.bytes[destination] = '\0';
            break;
        }

        while (cursor < source_length && source[cursor] == ' ') ++cursor;
    }

    if (source_length == source.size()) {
        mark_truncated(output[line_index], diagnostics);
    }
}

void increment_saturating(std::uint64_t& value) noexcept {
    if (value != (std::numeric_limits<std::uint64_t>::max)()) {
        ++value;
    }
}

}  // namespace

void HudViewModelProjector::build(HudViewModel& output,
    const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& hints) noexcept {
    output = {};

    const ObjectiveKey objective_key{
        snapshot.is_abyss,
        snapshot.phase,
        snapshot.wave_index,
        snapshot.wave_count,
        snapshot.remaining_targets,
        snapshot.abyss_danger,
        snapshot.abyss_rule,
        snapshot.abyss_pending_rewards,
        snapshot.abyss_unpicked_rewards,
    };
    const bool objective_changed = !objective_ready_
        || objective_key_.is_abyss != objective_key.is_abyss
        || objective_key_.phase != objective_key.phase
        || objective_key_.wave_index != objective_key.wave_index
        || objective_key_.wave_count != objective_key.wave_count
        || objective_key_.remaining_targets != objective_key.remaining_targets
        || objective_key_.abyss_danger != objective_key.abyss_danger
        || objective_key_.abyss_rule != objective_key.abyss_rule
        || objective_key_.abyss_pending_rewards
            != objective_key.abyss_pending_rewards
        || objective_key_.abyss_unpicked_rewards
            != objective_key.abyss_unpicked_rewards;
    if (objective_changed) {
        cached_objective_ = {};
        HudBuildDiagnostics diagnostics{};
        build_room_objective(cached_objective_, diagnostics, snapshot);
        cached_objective_truncations_ = diagnostics.truncated_texts;
        objective_key_ = objective_key;
        objective_ready_ = true;
        increment_saturating(
            static_formatting_diagnostics_.objective_rebuilds);
    }
    output.room = cached_objective_;
    output.room.abyss = snapshot.is_abyss;
    output.room.remaining_targets = snapshot.remaining_targets;
    output.diagnostics.truncated_texts += cached_objective_truncations_;

    if (!control_hints_ready_
        || control_hints_revision_ != hints.revision) {
        cached_movement_hint_ = {};
        cached_control_hint_lines_ = {};
        HudBuildDiagnostics diagnostics{};
        copy_movement_hint(cached_movement_hint_, diagnostics, hints.primary);
        split_control_hints(cached_control_hint_lines_, diagnostics,
            hints.secondary);
        cached_control_hint_truncations_ = diagnostics.truncated_texts;
        control_hints_revision_ = hints.revision;
        control_hints_ready_ = true;
        increment_saturating(
            static_formatting_diagnostics_.control_hint_rebuilds);
    }
    output.diagnostics.truncated_texts += cached_control_hint_truncations_;
    output.room.movement = cached_movement_hint_;
    output.room.controls = cached_control_hint_lines_;
    if (!output.room.abyss) {
        format_text(output.room.secondary, output.diagnostics,
            u8"待结算经验 +%llu",
            static_cast<unsigned long long>(snapshot.pending_room_experience));
    }

    const NavigationKey navigation_key{
        snapshot.depth,
        snapshot.floor_room_index,
        snapshot.ecology,
        snapshot.biases,
    };
    const bool navigation_changed = !navigation_ready_
        || navigation_key_.depth != navigation_key.depth
        || navigation_key_.floor_room != navigation_key.floor_room
        || navigation_key_.ecology != navigation_key.ecology
        || navigation_key_.biases != navigation_key.biases;
    if (navigation_changed) {
        cached_navigation_ = {};
        HudBuildDiagnostics diagnostics{};
        build_navigation(cached_navigation_, diagnostics, snapshot);
        cached_navigation_truncations_ = diagnostics.truncated_texts;
        navigation_key_ = navigation_key;
        navigation_ready_ = true;
        increment_saturating(
            static_formatting_diagnostics_.navigation_rebuilds);
    }
    output.navigation = cached_navigation_;
    output.diagnostics.truncated_texts += cached_navigation_truncations_;

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

HudStaticFormattingDiagnostics
HudViewModelProjector::static_formatting_diagnostics() const noexcept {
    return static_formatting_diagnostics_;
}

void build_hud_view_model(HudViewModel& output,
    const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& hints) noexcept {
    HudViewModelProjector projector{};
    projector.build(output, snapshot, runtime_status, hints);
}

void attach_notice_view(HudViewModel& output,
    const HudNoticeView& notices) noexcept {
    output.context.primary = notices.primary.text;
    output.context.secondary = notices.secondary.text;
    output.context.primary_kind = notices.primary.kind;
    output.context.secondary_kind = notices.secondary.kind;
    output.context.primary_abyss = notices.primary.abyss;
    output.context.secondary_abyss = notices.secondary.abyss;
}

}  // namespace arpg::platform
