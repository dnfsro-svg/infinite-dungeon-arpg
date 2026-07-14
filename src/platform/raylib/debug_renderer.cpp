#include "combat_renderer.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/combat_collision.hpp"
#include "combat_view_math.hpp"
#include "dungeon_runtime.hpp"
#include "dungeon_view_math.hpp"
#include "render_layout.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace arpg::platform {
namespace {

const char* attack_name(combat::AttackId id) noexcept {
    switch (id) {
    case combat::AttackId::j1: return "J1";
    case combat::AttackId::j2: return "J2";
    case combat::AttackId::j3: return "J3";
    case combat::AttackId::launcher: return "L";
    case combat::AttackId::air_j: return "Air J";
    case combat::AttackId::none: return "None";
    }
    return "?";
}

const char* phase_name(combat::AttackPhase phase) noexcept {
    switch (phase) {
    case combat::AttackPhase::startup: return "Startup";
    case combat::AttackPhase::active: return "Active";
    case combat::AttackPhase::recovery: return "Recovery";
    case combat::AttackPhase::finished: return "Finished";
    }
    return "?";
}

const char* player_state_name(combat::PlayerState state) noexcept {
    switch (state) {
    case combat::PlayerState::idle: return "Idle";
    case combat::PlayerState::move: return "Move";
    case combat::PlayerState::attack_startup: return "Attack startup";
    case combat::PlayerState::attack_active: return "Attack active";
    case combat::PlayerState::attack_recovery: return "Attack recovery";
    case combat::PlayerState::jump_rise: return "Jump rise";
    case combat::PlayerState::jump_fall: return "Jump fall";
    case combat::PlayerState::landing: return "Landing";
    }
    return "?";
}

const char* event_name(combat::CombatEventKind kind) noexcept {
    switch (kind) {
    case combat::CombatEventKind::swing: return "Swing";
    case combat::CombatEventKind::hit: return "Hit";
    case combat::CombatEventKind::impact_summary: return "Impact";
    case combat::CombatEventKind::landing: return "Landing";
    case combat::CombatEventKind::break_started: return "Break";
    case combat::CombatEventKind::defeated: return "Defeated";
    case combat::CombatEventKind::respawned: return "Respawned";
    case combat::CombatEventKind::reset: return "Reset";
    case combat::CombatEventKind::player_hit: return "Player hit";
    case combat::CombatEventKind::player_hurt_started: return "Player hurt";
    case combat::CombatEventKind::player_health_reset: return "Player heal";
    }
    return "?";
}

const char* ecology_name(dungeon::DungeonElement element) noexcept {
    switch (element) {
    case dungeon::DungeonElement::fire: return "FIRE";
    case dungeon::DungeonElement::water: return "WATER";
    case dungeon::DungeonElement::lightning: return "LIGHTNING";
    case dungeon::DungeonElement::chaos: return "CHAOS";
    }
    return "UNKNOWN";
}

void draw_projected_aabb(const combat::Aabb& box, float width, float height,
    Color color, const char* label) noexcept {
    std::array<ScreenProjection, 8> corners{};
    for (std::size_t index = 0; index < corners.size(); ++index) {
        const combat::Vec3 corner{(index & 1U) != 0 ? box.maximum.x : box.minimum.x,
            (index & 2U) != 0 ? box.maximum.y : box.minimum.y,
            (index & 4U) != 0 ? box.maximum.z : box.minimum.z};
        corners[index] = project_combat_position(corner, width, height);
    }
    for (std::size_t index = 0; index < corners.size(); ++index) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const std::size_t bit = std::size_t{1} << axis;
            if ((index & bit) == 0) {
                const ScreenProjection& from = corners[index];
                const ScreenProjection& to = corners[index | bit];
                DrawLineEx({from.x, from.y}, {to.x, to.y}, 1.5F, color);
            }
        }
    }
    DrawText(label, static_cast<int>(corners[4].x + 4.0F),
        static_cast<int>(corners[4].y - 14.0F), 12, color);
}

}  // namespace

void CombatRenderer::draw_debug_world_volumes(
    const combat::CombatSnapshot& snapshot, float width, float height) const noexcept {
    if (const combat::AttackDefinition* definition =
            combat::find_attack_definition(snapshot.player.active_attack)) {
        const combat::Aabb attack_box = combat::make_world_aabb(
            definition->local_hitbox, snapshot.player.position, snapshot.player.facing);
        const combat::Aabb assist_box = combat::make_attack_assist_volume(
            *definition, snapshot.player.position, snapshot.player.facing);
        draw_projected_aabb(assist_box, width, height, Color{90, 190, 255, 190}, "Assist");
        draw_projected_aabb(attack_box, width, height, Color{255, 88, 184, 230}, "Attack");
    }
    for (const combat::MonsterSnapshot& monster : snapshot.monsters) {
        if (monster_visible(monster)) {
            draw_projected_aabb(combat::make_dummy_hurtbox(monster.kind, monster.position),
                width, height, Color{94, 255, 173, 210}, "Hurt");
        }
    }
}

void CombatRenderer::draw_debug_overlay(
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    const CombatFeedback& feedback,
    bool audio_ready) const noexcept {
    const Color text{218, 226, 239, 255};
    const RenderLayout layout = render_layout(true);
    int y = debug_overlay_start_y(current.combat.has_value() ? 265 : 150);
    DrawText(TextFormat("Root %016llX Room %016llX Generation %llu",
        static_cast<unsigned long long>(current.root_seed),
        static_cast<unsigned long long>(current.room_seed),
        static_cast<unsigned long long>(current.commit_generation)),
        static_cast<int>(layout.hud_x), y, 16, text);
    y += layout.hud_line_step;
    DrawText(TextFormat("Saved ecology %s hole %s abyss %s slot %u error %u fault %u",
        ecology_name(current.ecology), current.has_hole ? "YES" : "NO",
        current.is_abyss ? "YES" : "NO", static_cast<unsigned>(runtime_status.active_slot),
        static_cast<unsigned>(runtime_status.error), static_cast<unsigned>(current.diagnostics.fault)),
        static_cast<int>(layout.hud_x), y, 16, text);
    y += layout.hud_line_step;
    DrawText(TextFormat("Dungeon event %u  relay %u  rejected %u  index fault %s",
        current.diagnostics.event_overflow_count,
        current.diagnostics.combat_relay_overflow_count,
        current.diagnostics.rejected_exit_count,
        current.diagnostics.room_index_overflow ? "YES" : "NO"),
        static_cast<int>(layout.hud_x), y, 16, text);
    if (current.combat.has_value()) {
        const combat::CombatSnapshot& state = *current.combat;
        y += layout.hud_line_step;
        DrawText(TextFormat("Tick %llu  State %s  Z %.2f  Hit stop %u",
            static_cast<unsigned long long>(state.tick), player_state_name(state.player.state),
            state.player.position.z, state.player.hit_stop_ticks),
            static_cast<int>(layout.hud_x), y, 16, text);
        y += layout.hud_line_step;
        DrawText(TextFormat("Effects owners/active %llu/%llu  overflow %u  commands %u",
            static_cast<unsigned long long>(state.diagnostics.effect_owner_count),
            static_cast<unsigned long long>(state.diagnostics.active_effect_count),
            state.diagnostics.effect_overflow_count,
            state.diagnostics.effect_command_overflow_count),
            static_cast<int>(layout.hud_x), y, 16, text);
        y += layout.hud_line_step;
        DrawText(TextFormat("Action %s  %s  Combo %u", attack_name(state.player.active_attack),
            phase_name(state.player.attack_phase), state.player.combo_stage),
            static_cast<int>(layout.hud_x), y, 16, text);
        y += layout.hud_line_step;
        DrawText(TextFormat("Input %llu  expired %u  overflow %u  event overflow %u",
            static_cast<unsigned long long>(state.diagnostics.input_size),
            state.diagnostics.input_expired_count, state.diagnostics.input_overflow_count,
            state.diagnostics.event_overflow_count), static_cast<int>(layout.hud_x), y, 16, text);
    } else {
        y += layout.hud_line_step;
        DrawText("Combat diagnostics unavailable during transition",
            static_cast<int>(layout.hud_x), y, 16, text);
    }
    y += layout.hud_line_step;
    DrawText(has_last_event_ ? TextFormat("Last event %s  target %u",
        event_name(last_event_.kind), last_event_.target_index) : "Last event None",
        static_cast<int>(layout.hud_x), y, 16, text);
    y += layout.hud_line_step;
    DrawText(TextFormat("FX %u  Dropped %u  Shake %.1f  Audio %s",
        static_cast<unsigned>(feedback.active_count()), feedback.dropped_count(),
        feedback.shake_amplitude(), audio_ready ? "Ready" : "Unavailable"),
        static_cast<int>(layout.hud_x), y, 16,
        audio_ready ? text : Color{255, 151, 117, 255});
}

}  // namespace arpg::platform
