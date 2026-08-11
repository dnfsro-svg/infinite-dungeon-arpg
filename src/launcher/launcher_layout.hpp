#pragma once

#include <cstdint>

namespace arpg::launcher {

struct LauncherColor {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha;
};

struct LauncherStatusStyle {
    LauncherColor backing;
    LauncherColor foreground;
};

[[nodiscard]] LauncherStatusStyle launcher_status_style(bool ready) noexcept;

struct RectF {
    float left;
    float top;
    float right;
    float bottom;
};

struct LauncherLayout {
    RectF title;
    RectF subtitle;
    RectF status;
    RectF start;
    RectF verify;
    RectF save;
    RectF exit;
    RectF path;
};

[[nodiscard]] LauncherLayout make_launcher_layout(float width, float height) noexcept;
[[nodiscard]] bool overlaps(RectF first, RectF second) noexcept;

struct PixelSize {
    int width;
    int height;
};

[[nodiscard]] PixelSize launcher_pixel_size(unsigned int dpi) noexcept;

}  // namespace arpg::launcher
