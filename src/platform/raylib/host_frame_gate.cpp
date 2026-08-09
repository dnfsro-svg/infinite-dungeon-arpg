#include "raylib_host.hpp"

#include "pause_menu_state.hpp"

namespace arpg::platform {

HostFrameGateResult gate_host_frame(
    core::FixedStepRunner& fixed_step,
    bool& pause_latched,
    bool paused,
    double frame_seconds) noexcept {
    if (paused) {
        if (!pause_latched) fixed_step.clear_accumulator();
        pause_latched = true;
        return {};
    }
    pause_latched = false;
    return {true, fixed_step.advance(frame_seconds)};
}

bool host_requests_room_reset(
    const HostRoomResetGateInput& input) noexcept {
    if (!input.reset_pressed || !input.gameplay_armed
        || !input.death_allows_gameplay) {
        return false;
    }
    if (input.pause_screen == PauseScreen::root) return true;
    return input.pause_screen == PauseScreen::closed
        && input.forward_actions && input.inventory_allows_room_reset;
}

}  // namespace arpg::platform
