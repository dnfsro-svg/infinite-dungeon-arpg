#pragma once

#include "combat_feedback.hpp"
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
    combat::CombatEvent last_event_{};
    bool has_last_event_{};
    TransitionVisualState transition_{};
};

}  // namespace arpg::platform
