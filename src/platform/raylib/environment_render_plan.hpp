#pragma once

namespace arpg::platform {

struct EnvironmentFrameAvailability final {
    bool background{};
    bool door{};
    bool hole{};
    bool props{};
    bool door_required{};
    bool hole_required{};
};

[[nodiscard]] constexpr bool should_draw_material_environment(
    EnvironmentFrameAvailability availability) noexcept {
    return availability.background
        && availability.props
        && (!availability.door_required || availability.door)
        && (!availability.hole_required || availability.hole);
}

}  // namespace arpg::platform
