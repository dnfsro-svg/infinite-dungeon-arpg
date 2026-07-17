#pragma once

#include "combat_feedback.hpp"
#include "death_overlay_renderer.hpp"
#include "dungeon_view_math.hpp"

namespace arpg::platform {

struct DungeonRenderStatus;

struct DoorRenderDecision final {
    const char* label{};
    const char* arrow{};
    Rgba8 frame{};
    Rgba8 text{};
    Rgba8 locked_interior{};
    bool draw_locked_interior{};
};

[[nodiscard]] DoorRenderDecision door_render_decision(
    DoorVisualMode mode,
    dungeon::ExitDirection direction) noexcept;

class CombatRenderer final {
public:
    [[nodiscard]] bool initialize_resources() noexcept;
    void shutdown_resources() noexcept;
    void consume_event(const combat::CombatEvent& event) noexcept;
    void consume_dungeon_event(const dungeon::DungeonEvent& event) noexcept;
    void clear_combat_transients() noexcept;
    void update(float frame_seconds) noexcept;
    void draw(
        const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& runtime_status,
        float interpolation_alpha,
        bool draw_debug,
        const CombatFeedback& feedback,
        bool audio_ready) noexcept;

private:
    void draw_room(const dungeon::DungeonSnapshot& current) const noexcept;
    void draw_actors(
        const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        float interpolation_alpha,
        bool draw_debug,
        const CombatFeedback& feedback) const noexcept;
    void draw_hud(
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& runtime_status,
        bool draw_debug) const noexcept;
    void draw_abyss_hud(
        const dungeon::DungeonSnapshot& current,
        float x,
        int& y,
        int line_step) const noexcept;
    void draw_debug_world_volumes(
        const combat::CombatSnapshot& snapshot,
        float width,
        float height) const noexcept;
    void draw_debug_overlay(
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& runtime_status,
        const CombatFeedback& feedback,
        bool audio_ready) const noexcept;

    combat::CombatEvent last_event_{};
    bool has_last_event_{};
    TransitionVisualState transition_{};
    DeathOverlayRenderer death_overlay_{};
};

}  // namespace arpg::platform
