#include "combat_renderer.hpp"

#include "dungeon_runtime.hpp"

#include <raylib.h>

#include <algorithm>

namespace arpg::platform {

bool CombatRenderer::initialize_resources() noexcept {
    return death_overlay_.initialize();
}

void CombatRenderer::shutdown_resources() noexcept {
    death_overlay_.shutdown();
}

DoorRenderDecision door_render_decision(
    DoorVisualMode mode,
    dungeon::ExitDirection direction) noexcept {
    const DoorTheme theme = door_theme(direction);
    return {
        theme.label,
        theme.arrow,
        theme.frame,
        theme.frame,
        {177U, 31U, 46U, 235U},
        mode == DoorVisualMode::closed,
    };
}

void CombatRenderer::consume_event(const combat::CombatEvent& event) noexcept {
    last_event_ = event;
    has_last_event_ = true;
}

void CombatRenderer::consume_dungeon_event(
    const dungeon::DungeonEvent& event) noexcept {
    transition_ = transition_after_dungeon_event(transition_, event.kind);
}

void CombatRenderer::clear_combat_transients() noexcept {
    last_event_ = combat::CombatEvent{};
    has_last_event_ = false;
}

void CombatRenderer::update(float frame_seconds) noexcept {
    transition_ = advance_transition(transition_, frame_seconds);
}

void CombatRenderer::draw(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    float interpolation_alpha,
    bool draw_debug,
    const CombatFeedback& feedback,
    bool audio_ready) noexcept {
    transition_ = transition_after_room_phase(transition_, current.phase);

    const CameraOffset camera_offset = feedback.camera_offset();
    Camera2D world_camera{};
    world_camera.offset = {camera_offset.x, camera_offset.y};
    world_camera.zoom = 1.0F;
    BeginMode2D(world_camera);
    draw_room(current);
    draw_actors(previous, current, std::clamp(interpolation_alpha, 0.0F, 1.0F),
        draw_debug, feedback);
    EndMode2D();

    draw_hud(current, runtime_status, draw_debug);
    if (draw_debug) {
        draw_debug_overlay(current, runtime_status, feedback, audio_ready);
    }
    DrawText(draw_debug ? "F1 DEBUG ON" : "F1 DEBUG OFF",
        GetScreenWidth() - 150, 20, 16,
        draw_debug ? Color{255, 126, 197, 255} : Color{142, 153, 170, 255});

    const float overlay_alpha = transition_overlay_alpha(transition_.seconds_left);
    if (overlay_alpha > 0.0F) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
            Fade(BLACK, overlay_alpha));
    }
    death_overlay_.draw(current);
}

}  // namespace arpg::platform
