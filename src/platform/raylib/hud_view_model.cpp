#include "hud_view_model.hpp"

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

}  // namespace

void build_hud_view_model(HudViewModel& output,
    const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& hints) noexcept {
    output = {};

    output.room.abyss = snapshot.is_abyss;
    output.room.remaining_targets = snapshot.remaining_targets;
    format_text(output.room.objective, output.diagnostics,
        u8"目标：剩余 %u 个敌人",
        static_cast<unsigned>(snapshot.remaining_targets));
    format_text(output.room.secondary, output.diagnostics,
        u8"待结算经验 +%llu | %.32s | %.32s",
        static_cast<unsigned long long>(snapshot.pending_room_experience),
        hints.primary.data(), hints.secondary.data());

    output.navigation.depth = snapshot.depth;
    output.navigation.floor_room = snapshot.floor_room_index;
    output.navigation.ecology = snapshot.ecology;
    output.navigation.biases = snapshot.biases;

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

}  // namespace arpg::platform
