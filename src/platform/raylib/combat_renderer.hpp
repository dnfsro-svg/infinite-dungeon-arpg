#pragma once

#include "combat_feedback.hpp"
#include "death_overlay_renderer.hpp"
#include "debug_overlay_renderer.hpp"
#include "dungeon_view_math.hpp"
#include "hud_notice_state.hpp"
#include "hud_renderer.hpp"

#include <cstdint>
#include <array>

namespace arpg::platform {

struct DungeonRenderStatus;
struct ControlHints;
enum class HudPresentedFrame : std::uint8_t { normal, recovery, death_overlay };

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
    void observe_hud(
        const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& runtime_status,
        const ControlHints& control_hints,
        float frame_seconds,
        bool paused) noexcept;
    void observe_presented_hud_frame(
        HudPresentedFrame,
        const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& runtime_status,
        const ControlHints& control_hints,
        float frame_seconds,
        bool paused) noexcept;
    [[nodiscard]] const HudViewModel& hud_model() const noexcept;
    [[nodiscard]] HudNoticeView hud_notice_view() const noexcept;
    [[nodiscard]] std::uint64_t hud_binding_revision() const noexcept;
    [[nodiscard]] std::uint64_t hud_observation_count() const noexcept;
    [[nodiscard]] std::uint64_t hud_presented_frame_count(
        HudPresentedFrame) const noexcept;
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
    void draw_hud() const noexcept;
    void draw_abyss_hud(
        const dungeon::DungeonSnapshot& current,
        float x,
        int& y,
        int line_step) const noexcept;
    void draw_debug_world_volumes(
        const combat::CombatSnapshot& snapshot,
        float width,
        float height) const noexcept;
    combat::CombatEvent last_event_{};
    bool has_last_event_{};
    TransitionVisualState transition_{};
    DeathOverlayRenderer death_overlay_{};
    HudRenderer hud_renderer_{};
    DebugOverlayRenderer debug_overlay_{};
    HudNoticeState hud_notices_{};
    HudViewModel hud_model_{};
    HudLayout hud_layout_{};
    std::uint64_t hud_binding_revision_{};
    std::uint64_t hud_observation_count_{};
    std::array<std::uint64_t, 3> hud_presented_frame_counts_{};
};

}  // namespace arpg::platform
