#include "launcher_layout.hpp"

namespace arpg::launcher {
namespace {

constexpr float kLogicalWidth = 920.0F;
constexpr float kLogicalHeight = 560.0F;

constexpr LauncherLayout kDesignLayout{
    {56.0F, 42.0F, 864.0F, 98.0F},
    {58.0F, 102.0F, 862.0F, 132.0F},
    {58.0F, 174.0F, 862.0F, 226.0F},
    {58.0F, 260.0F, 862.0F, 342.0F},
    {58.0F, 370.0F, 308.0F, 426.0F},
    {335.0F, 370.0F, 585.0F, 426.0F},
    {612.0F, 370.0F, 862.0F, 426.0F},
    {58.0F, 474.0F, 862.0F, 522.0F},
};

RectF offset(RectF rectangle, float horizontal, float vertical) noexcept {
    return {
        rectangle.left + horizontal,
        rectangle.top + vertical,
        rectangle.right + horizontal,
        rectangle.bottom + vertical,
    };
}

}  // namespace

LauncherLayout make_launcher_layout(float width, float height) noexcept {
    if (width < 800.0F || height < 450.0F) {
        return {};
    }

    const float horizontal = (width - kLogicalWidth) / 2.0F;
    const float vertical = (height - kLogicalHeight) / 2.0F;
    return {
        offset(kDesignLayout.title, horizontal, vertical),
        offset(kDesignLayout.subtitle, horizontal, vertical),
        offset(kDesignLayout.status, horizontal, vertical),
        offset(kDesignLayout.start, horizontal, vertical),
        offset(kDesignLayout.verify, horizontal, vertical),
        offset(kDesignLayout.save, horizontal, vertical),
        offset(kDesignLayout.exit, horizontal, vertical),
        offset(kDesignLayout.path, horizontal, vertical),
    };
}

bool overlaps(RectF first, RectF second) noexcept {
    return first.left < second.right && first.right > second.left &&
        first.top < second.bottom && first.bottom > second.top;
}

PixelSize launcher_pixel_size(unsigned int dpi) noexcept {
    if (dpi == 0U) {
        return {};
    }

    return {
        static_cast<int>((920U * dpi + 95U) / 96U),
        static_cast<int>((560U * dpi + 95U) / 96U),
    };
}

}  // namespace arpg::launcher
