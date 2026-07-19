#pragma once

namespace arpg::platform {

struct EnvironmentFrameAvailability final {
    bool floor{};
    bool door{};
    bool hole{};
    bool door_required{};
    bool hole_required{};
};

[[nodiscard]] constexpr bool should_draw_material_environment(
    EnvironmentFrameAvailability availability) noexcept {
    return availability.floor
        && (!availability.door_required || availability.door)
        && (!availability.hole_required || availability.hole);
}

}  // namespace arpg::platform
