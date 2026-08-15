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
    if (!input.gameplay_armed || !input.death_allows_gameplay) {
        return false;
    }
    return input.pause_screen == PauseScreen::reset_confirm
        && input.pause_reset_confirmed;
}

}  // namespace arpg::platform
