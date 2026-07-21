#pragma once

namespace arpg::combat::room_bounds {

inline constexpr float min_x = -24.0F;
inline constexpr float max_x = 24.0F;
inline constexpr float min_y = -11.0F;
inline constexpr float max_y = 11.0F;
inline constexpr float width = max_x - min_x;
inline constexpr float depth = max_y - min_y;

}  // namespace arpg::combat::room_bounds
