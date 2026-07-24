#pragma once

#include "combat/combat_types.hpp"

#include <algorithm>
#include <cmath>

namespace arpg::combat::fire_room_obstacle {

inline constexpr float half_width = 1.40F;
inline constexpr float half_height = 1.70F;
inline constexpr float clearance = 0.01F;

[[nodiscard]] inline bool contains(Vec3 position) noexcept {
    return position.x >= -half_width && position.x <= half_width
        && position.y >= -half_height && position.y <= half_height;
}

[[nodiscard]] inline float escape_progress(Vec3 position) noexcept {
    return (std::max)(std::fabs(position.x) / half_width,
        std::fabs(position.y) / half_height);
}

[[nodiscard]] inline bool blocks_player(
    Vec3 current, Vec3 candidate) noexcept {
    if (!contains(candidate)) return false;
    if (!contains(current)) return true;
    return escape_progress(candidate) <= escape_progress(current);
}

[[nodiscard]] inline Vec3 eject(Vec3 position) noexcept {
    if (!contains(position)) return position;
    const float horizontal_penetration = half_width - std::fabs(position.x);
    const float vertical_penetration = half_height - std::fabs(position.y);
    if (horizontal_penetration <= vertical_penetration) {
        position.x = position.x < 0.0F
            ? -half_width - clearance : half_width + clearance;
    } else {
        position.y = position.y < 0.0F
            ? -half_height - clearance : half_height + clearance;
    }
    return position;
}

[[nodiscard]] inline Vec3 route_monster(
    Vec3 previous, Vec3 candidate, Vec3 target) noexcept {
    if (!contains(candidate)) return candidate;
    if (contains(previous)) return eject(candidate);

    const float delta_x = candidate.x - previous.x;
    const float delta_y = candidate.y - previous.y;
    const float step = std::sqrt(delta_x * delta_x + delta_y * delta_y);
    if (step <= 1.0e-6F || step > 1.0F) return eject(candidate);

    Vec3 routed = previous;
    const float x_side = std::fabs(previous.x) / half_width;
    const float y_side = std::fabs(previous.y) / half_height;
    if (x_side >= y_side) {
        const float positive_lane = half_height + clearance;
        const float negative_lane = -positive_lane;
        const float positive_cost = std::fabs(previous.y - positive_lane)
            + std::fabs(target.y - positive_lane);
        const float negative_cost = std::fabs(previous.y - negative_lane)
            + std::fabs(target.y - negative_lane);
        routed.y += positive_cost <= negative_cost ? step : -step;
    } else {
        const float positive_lane = half_width + clearance;
        const float negative_lane = -positive_lane;
        const float positive_cost = std::fabs(previous.x - positive_lane)
            + std::fabs(target.x - positive_lane);
        const float negative_cost = std::fabs(previous.x - negative_lane)
            + std::fabs(target.x - negative_lane);
        routed.x += positive_cost <= negative_cost ? step : -step;
    }
    return routed;
}

}  // namespace arpg::combat::fire_room_obstacle
