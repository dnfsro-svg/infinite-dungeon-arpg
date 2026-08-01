#pragma once

namespace arpg::combat::room_bounds {

inline constexpr float half_extent = 81.24038696F;
inline constexpr float min_x = -half_extent;
inline constexpr float max_x = half_extent;
inline constexpr float min_y = -half_extent;
inline constexpr float max_y = half_extent;
inline constexpr float width = max_x - min_x;
inline constexpr float depth = max_y - min_y;

static_assert(width == depth);

}  // namespace arpg::combat::room_bounds
