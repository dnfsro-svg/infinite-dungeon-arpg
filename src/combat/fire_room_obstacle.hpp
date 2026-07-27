#pragma once

#include "combat/combat_types.hpp"

#include <algorithm>
#include <cmath>

namespace arpg::combat::fire_room_obstacle {

inline constexpr float half_width = 1.40F;
inline constexpr float half_height = 1.70F;
inline constexpr float clearance = 0.01F;

[[nodiscard]] inline bool contains(
    const Aabb bounds, const Vec3 position) noexcept {
    return position.x >= bounds.minimum.x && position.x <= bounds.maximum.x
        && position.y >= bounds.minimum.y && position.y <= bounds.maximum.y;
}

[[nodiscard]] inline float escape_progress(
    const Aabb bounds, const Vec3 position) noexcept {
    const float center_x = (bounds.minimum.x + bounds.maximum.x) * 0.5F;
    const float center_y = (bounds.minimum.y + bounds.maximum.y) * 0.5F;
    const float extent_x = (bounds.maximum.x - bounds.minimum.x) * 0.5F;
    const float extent_y = (bounds.maximum.y - bounds.minimum.y) * 0.5F;
    return (std::max)(std::fabs(position.x - center_x) / extent_x,
        std::fabs(position.y - center_y) / extent_y);
}

[[nodiscard]] inline bool blocks_player(
    const Aabb bounds, const Vec3 current, const Vec3 candidate) noexcept {
    if (!contains(bounds, candidate)) return false;
    if (!contains(bounds, current)) return true;
    return escape_progress(bounds, candidate) <= escape_progress(bounds, current);
}

[[nodiscard]] inline Vec3 eject(
    const Aabb bounds, Vec3 position) noexcept {
    if (!contains(bounds, position)) return position;
    const float left = position.x - bounds.minimum.x;
    const float right = bounds.maximum.x - position.x;
    const float bottom = position.y - bounds.minimum.y;
    const float top = bounds.maximum.y - position.y;
    const float minimum = (std::min)((std::min)(left, right),
        (std::min)(bottom, top));
    if (minimum == left) position.x = bounds.minimum.x - clearance;
    else if (minimum == right) position.x = bounds.maximum.x + clearance;
    else if (minimum == bottom) position.y = bounds.minimum.y - clearance;
    else position.y = bounds.maximum.y + clearance;
    return position;
}

[[nodiscard]] inline bool contains(Vec3 position) noexcept {
    return contains({{-half_width, -half_height, 0.0F},
        {half_width, half_height, 2.0F}}, position);
}

[[nodiscard]] inline float escape_progress(Vec3 position) noexcept {
    return (std::max)(std::fabs(position.x) / half_width,
        std::fabs(position.y) / half_height);
}

[[nodiscard]] inline bool blocks_player(
    Vec3 current, Vec3 candidate) noexcept {
    return blocks_player({{-half_width, -half_height, 0.0F},
        {half_width, half_height, 2.0F}}, current, candidate);
}

[[nodiscard]] inline Vec3 eject(Vec3 position) noexcept {
    return eject({{-half_width, -half_height, 0.0F},
        {half_width, half_height, 2.0F}}, position);
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
