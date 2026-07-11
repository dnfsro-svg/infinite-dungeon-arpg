#include "dungeon/room_navigation.hpp"

#include <cmath>

namespace arpg::dungeon {

std::optional<ExitDirection> requested_exit(
    combat::Vec3 position,
    combat::MovementInput movement) noexcept {
    constexpr float kLeftBoundary = -8.0F;
    constexpr float kRightBoundary = 8.0F;
    constexpr float kTopBoundary = -3.5F;
    constexpr float kBottomBoundary = 3.5F;
    constexpr float kSideDoorHalfWidth = 0.90F;
    constexpr float kVerticalDoorHalfWidth = 1.50F;

    if (position.x == kLeftBoundary
            && std::fabs(position.y) <= kSideDoorHalfWidth
            && movement.x < 0) {
        return ExitDirection::left;
    }
    if (position.x == kRightBoundary
            && std::fabs(position.y) <= kSideDoorHalfWidth
            && movement.x > 0) {
        return ExitDirection::right;
    }
    if (position.y == kTopBoundary
            && std::fabs(position.x) <= kVerticalDoorHalfWidth
            && movement.y < 0) {
        return ExitDirection::up;
    }
    if (position.y == kBottomBoundary
            && std::fabs(position.x) <= kVerticalDoorHalfWidth
            && movement.y > 0) {
        return ExitDirection::down;
    }
    return std::nullopt;
}

}  // namespace arpg::dungeon
