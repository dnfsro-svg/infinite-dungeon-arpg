#pragma once

namespace arpg::launcher {

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
