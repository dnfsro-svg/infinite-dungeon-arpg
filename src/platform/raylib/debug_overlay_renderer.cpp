#include "debug_overlay_renderer.hpp"

#include "combat/attack_catalog.hpp"
#include "combat_view_math.hpp"
#include "render_layout.hpp"

#include <raylib.h>

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

}  // namespace

DebugOverlayDiagnosticsPlan make_debug_overlay_diagnostics_plan(
    const dungeon::DungeonSnapshot& current,
    const HudBuildDiagnostics& hud_diagnostics,
    std::uint32_t notice_drops,
    std::uint64_t binding_revision,
    const combat::CombatEvent& last_event,
    bool has_last_event,
    bool cjk_font_ready) noexcept {
    DebugOverlayDiagnosticsPlan plan{};
    plan.total_budget = current.encounter.total_budget;
    plan.current_wave_budget = current.encounter.current_wave_budget;
    plan.ground_saturation = current.diagnostics.ground_saturation_count;
    if (current.combat.has_value()) {
        const combat::CombatSnapshot& combat = *current.combat;
        plan.active_monsters = combat.monster_count;
        plan.active_projectiles = combat.projectile_count;
        plan.active_hazards = combat.hazard_count;
        plan.projectile_saturation = combat.diagnostics.projectile_saturation_count;
        plan.projectile_invalid_owner = combat.diagnostics.projectile_invalid_owner_count;
        plan.hazard_saturation = combat.diagnostics.hazard_saturation_count;
        plan.hazard_invalid_owner = combat.diagnostics.hazard_invalid_owner_count;
    }
    plan.hud = hud_diagnostics;
    plan.notice_drops = notice_drops;
    plan.binding_revision = binding_revision;
    plan.last_event = last_event;
    plan.has_last_event = has_last_event;
    plan.cjk_font_ready = cjk_font_ready;
    return plan;
}

void DebugOverlayRenderer::draw(const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status, const CombatFeedback& feedback,
    bool audio_ready, const DebugOverlayDiagnosticsPlan& diagnostics) const noexcept {
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
    DrawText(TextFormat("Budget total/current %u/%u  M/P/H %llu/%llu/%llu",
        static_cast<unsigned>(diagnostics.total_budget),
        static_cast<unsigned>(diagnostics.current_wave_budget),
        static_cast<unsigned long long>(diagnostics.active_monsters),
        static_cast<unsigned long long>(diagnostics.active_projectiles),
        static_cast<unsigned long long>(diagnostics.active_hazards)),
        static_cast<int>(layout.hud_x), y, 16, text);
    y += layout.hud_line_step;
    DrawText(TextFormat("Dungeon event %u relay %u rejected %u ground saturation %u",
        current.diagnostics.event_overflow_count,
        current.diagnostics.combat_relay_overflow_count,
        current.diagnostics.rejected_exit_count,
        diagnostics.ground_saturation),
        static_cast<int>(layout.hud_x), y, 16, text);
    if (current.combat.has_value()) {
        const combat::CombatSnapshot& state = *current.combat;
        y += layout.hud_line_step;
        DrawText(TextFormat("Tick %llu State %s Z %.2f Hit stop %u",
            static_cast<unsigned long long>(state.tick), player_state_name(state.player.state),
            state.player.position.z, state.player.hit_stop_ticks),
            static_cast<int>(layout.hud_x), y, 16, text);
        y += layout.hud_line_step;
        DrawText(TextFormat("Effects owners/active %llu/%llu overflow %u commands %u",
            static_cast<unsigned long long>(state.diagnostics.effect_owner_count),
            static_cast<unsigned long long>(state.diagnostics.active_effect_count),
            state.diagnostics.effect_overflow_count, state.diagnostics.effect_command_overflow_count),
            static_cast<int>(layout.hud_x), y, 16, text);
        y += layout.hud_line_step;
        DrawText(TextFormat("Action %s %s Combo %u", attack_name(state.player.active_attack),
            phase_name(state.player.attack_phase), state.player.combo_stage),
            static_cast<int>(layout.hud_x), y, 16, text);
        y += layout.hud_line_step;
        DrawText(TextFormat("Input %llu expired %u overflow %u event overflow %u",
            static_cast<unsigned long long>(state.diagnostics.input_size),
            state.diagnostics.input_expired_count, state.diagnostics.input_overflow_count,
            state.diagnostics.event_overflow_count), static_cast<int>(layout.hud_x), y, 16, text);
    } else {
        y += layout.hud_line_step;
        DrawText("Combat diagnostics unavailable during transition",
            static_cast<int>(layout.hud_x), y, 16, text);
    }
    y += layout.hud_line_step;
    DrawText(diagnostics.has_last_event
            ? TextFormat("Last event %s target %u",
                event_name(diagnostics.last_event.kind), diagnostics.last_event.target_index)
            : "Last event None",
        static_cast<int>(layout.hud_x), y, 16, text);
    y += layout.hud_line_step;
    DrawText(TextFormat("FX %u Dropped %u Shake %.1f Audio %s",
        static_cast<unsigned>(feedback.active_count()), feedback.dropped_count(),
        feedback.shake_amplitude(), audio_ready ? "Ready" : "Unavailable"),
        static_cast<int>(layout.hud_x), y, 16,
        audio_ready ? text : Color{255, 151, 117, 255});
    y += layout.hud_line_step;
    DrawText(TextFormat("Projectile saturation/invalid %u/%u Hazard saturation/invalid %u/%u",
        diagnostics.projectile_saturation, diagnostics.projectile_invalid_owner,
        diagnostics.hazard_saturation, diagnostics.hazard_invalid_owner),
        static_cast<int>(layout.hud_x), y, 16, text);
    y += layout.hud_line_step;
    DrawText(TextFormat("HUD clamp %u trunc %u missing %s notices dropped %u bindings %llu CJK %s",
        diagnostics.hud.clamped_values, diagnostics.hud.truncated_texts,
        diagnostics.hud.combat_snapshot_missing ? "YES" : "NO", diagnostics.notice_drops,
        static_cast<unsigned long long>(diagnostics.binding_revision),
        diagnostics.cjk_font_ready ? "READY" : "FALLBACK"),
        static_cast<int>(layout.hud_x), y, 16, text);
}

}  // namespace arpg::platform
