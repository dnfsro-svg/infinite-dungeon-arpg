#include "combat_renderer.hpp"

#include "debug_overlay_renderer.hpp"
#include "dungeon_runtime.hpp"

#include <raylib.h>

#include <algorithm>

namespace arpg::platform {

bool CombatRenderer::initialize_resources() noexcept {
    const bool death_font_ready = death_overlay_.initialize();
    const bool hud_font_ready = hud_renderer_.initialize();
    if (!death_font_ready || !hud_font_ready) {
        TraceLog(LOG_WARNING,
            "HUD overlays are using a fallback font; formal CJK validation will fail");
    }
    return true;
}

void CombatRenderer::shutdown_resources() noexcept {
    hud_renderer_.shutdown();
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

void CombatRenderer::observe_hud(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& control_hints,
    float frame_seconds,
    bool paused) noexcept {
    hud_notices_.observe(previous, current, runtime_status, control_hints,
        runtime_status.recovery_required);
    hud_notices_.update(frame_seconds, paused);
    HudViewModel model{};
    build_hud_view_model(model, current, runtime_status, control_hints);
    attach_notice_view(model, hud_notices_.view());
    hud_model_ = model;
    hud_layout_ = IsWindowReady()
        ? make_hud_layout(GetScreenWidth(), GetScreenHeight(), false)
        : HudLayout{};
    hud_binding_revision_ = control_hints.revision;
    ++hud_observation_count_;
}

void CombatRenderer::observe_presented_hud_frame(
    HudPresentedFrame frame,
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    const ControlHints& control_hints,
    float frame_seconds,
    bool paused) noexcept {
    observe_hud(previous, current, runtime_status, control_hints, frame_seconds, paused);
    ++hud_presented_frame_counts_[static_cast<std::size_t>(frame)];
}

const HudViewModel& CombatRenderer::hud_model() const noexcept {
    return hud_model_;
}

HudNoticeView CombatRenderer::hud_notice_view() const noexcept {
    return hud_notices_.view();
}

std::uint64_t CombatRenderer::hud_binding_revision() const noexcept {
    return hud_binding_revision_;
}

std::uint64_t CombatRenderer::hud_observation_count() const noexcept {
    return hud_observation_count_;
}

std::uint64_t CombatRenderer::hud_presented_frame_count(
    HudPresentedFrame frame) const noexcept {
    return hud_presented_frame_counts_[static_cast<std::size_t>(frame)];
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

    draw_hud();
    if (draw_debug) {
        const DebugOverlayDiagnosticsPlan diagnostics =
            make_debug_overlay_diagnostics_plan(current, hud_model_.diagnostics,
                hud_notices_.dropped_count(), hud_binding_revision_, last_event_,
                has_last_event_, hud_renderer_.font_ready());
        debug_overlay_.draw(current, runtime_status, feedback, audio_ready, diagnostics);
    }

    const float overlay_alpha = transition_overlay_alpha(transition_.seconds_left);
    if (overlay_alpha > 0.0F) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
            Fade(BLACK, overlay_alpha));
    }
    death_overlay_.draw(current);
}

}  // namespace arpg::platform
