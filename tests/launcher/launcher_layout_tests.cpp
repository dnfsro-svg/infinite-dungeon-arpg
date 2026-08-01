#include "launcher_layout.hpp"
#include "test_framework.hpp"

namespace {

using arpg::launcher::LauncherLayout;
using arpg::launcher::PixelSize;
using arpg::launcher::RectF;

constexpr RectF kEmptyRect{};

bool inside(RectF rectangle, float width, float height) noexcept {
    return rectangle.left >= 0.0F && rectangle.top >= 0.0F &&
        rectangle.right <= width && rectangle.bottom <= height &&
        rectangle.left <= rectangle.right && rectangle.top <= rectangle.bottom;
}

bool empty(RectF rectangle) noexcept {
    return rectangle.left == kEmptyRect.left && rectangle.top == kEmptyRect.top &&
        rectangle.right == kEmptyRect.right && rectangle.bottom == kEmptyRect.bottom;
}

bool same(RectF first, RectF second) noexcept {
    return first.left == second.left && first.top == second.top &&
        first.right == second.right && first.bottom == second.bottom;
}

bool same(PixelSize first, PixelSize second) noexcept {
    return first.width == second.width && first.height == second.height;
}

arpg::test::Failure design_size_keeps_every_rect_inside_client_area() noexcept {
    constexpr float width = 920.0F;
    constexpr float height = 560.0F;
    const LauncherLayout layout = arpg::launcher::make_launcher_layout(width, height);

    ARPG_REQUIRE(same(layout.title, RectF{56.0F, 42.0F, 864.0F, 98.0F}));
    ARPG_REQUIRE(same(layout.subtitle, RectF{58.0F, 102.0F, 862.0F, 132.0F}));
    ARPG_REQUIRE(same(layout.status, RectF{58.0F, 174.0F, 862.0F, 226.0F}));
    ARPG_REQUIRE(same(layout.start, RectF{58.0F, 260.0F, 862.0F, 342.0F}));
    ARPG_REQUIRE(same(layout.verify, RectF{58.0F, 370.0F, 308.0F, 426.0F}));
    ARPG_REQUIRE(same(layout.save, RectF{335.0F, 370.0F, 585.0F, 426.0F}));
    ARPG_REQUIRE(same(layout.exit, RectF{612.0F, 370.0F, 862.0F, 426.0F}));
    ARPG_REQUIRE(same(layout.path, RectF{58.0F, 474.0F, 862.0F, 522.0F}));
    ARPG_REQUIRE(inside(layout.title, width, height));
    ARPG_REQUIRE(inside(layout.subtitle, width, height));
    ARPG_REQUIRE(inside(layout.status, width, height));
    ARPG_REQUIRE(inside(layout.start, width, height));
    ARPG_REQUIRE(inside(layout.verify, width, height));
    ARPG_REQUIRE(inside(layout.save, width, height));
    ARPG_REQUIRE(inside(layout.exit, width, height));
    ARPG_REQUIRE(inside(layout.path, width, height));
    return {};
}

arpg::test::Failure buttons_never_overlap_each_other() noexcept {
    const LauncherLayout layout = arpg::launcher::make_launcher_layout(920.0F, 560.0F);

    ARPG_REQUIRE(!arpg::launcher::overlaps(layout.start, layout.verify));
    ARPG_REQUIRE(!arpg::launcher::overlaps(layout.start, layout.save));
    ARPG_REQUIRE(!arpg::launcher::overlaps(layout.start, layout.exit));
    ARPG_REQUIRE(!arpg::launcher::overlaps(layout.verify, layout.save));
    ARPG_REQUIRE(!arpg::launcher::overlaps(layout.verify, layout.exit));
    ARPG_REQUIRE(!arpg::launcher::overlaps(layout.save, layout.exit));
    return {};
}

arpg::test::Failure status_and_path_do_not_overlap_actions() noexcept {
    const LauncherLayout layout = arpg::launcher::make_launcher_layout(920.0F, 560.0F);
    const RectF actions[] = {layout.start, layout.verify, layout.save, layout.exit};

    for (const RectF action : actions) {
        ARPG_REQUIRE(!arpg::launcher::overlaps(layout.status, action));
        ARPG_REQUIRE(!arpg::launcher::overlaps(layout.path, action));
    }
    return {};
}

arpg::test::Failure undersized_client_area_returns_empty_layout() noexcept {
    const LauncherLayout layout = arpg::launcher::make_launcher_layout(799.0F, 449.0F);

    ARPG_REQUIRE(empty(layout.title));
    ARPG_REQUIRE(empty(layout.subtitle));
    ARPG_REQUIRE(empty(layout.status));
    ARPG_REQUIRE(empty(layout.start));
    ARPG_REQUIRE(empty(layout.verify));
    ARPG_REQUIRE(empty(layout.save));
    ARPG_REQUIRE(empty(layout.exit));
    ARPG_REQUIRE(empty(layout.path));
    return {};
}

arpg::test::Failure dpi_scaling_preserves_the_fixed_logical_size() noexcept {
    ARPG_REQUIRE(same(arpg::launcher::launcher_pixel_size(96U), PixelSize{920, 560}));
    ARPG_REQUIRE(same(arpg::launcher::launcher_pixel_size(120U), PixelSize{1150, 700}));
    ARPG_REQUIRE(same(arpg::launcher::launcher_pixel_size(144U), PixelSize{1380, 840}));
    ARPG_REQUIRE(same(arpg::launcher::launcher_pixel_size(192U), PixelSize{1840, 1120}));
    ARPG_REQUIRE(same(arpg::launcher::launcher_pixel_size(0U), PixelSize{0, 0}));
    return {};
}

}  // namespace

arpg::test::TestSuite launcher_layout_suite() noexcept {
    static const arpg::test::TestCase cases[] = {
        {"design size keeps every rect inside client area", design_size_keeps_every_rect_inside_client_area},
        {"buttons never overlap each other", buttons_never_overlap_each_other},
        {"status and path do not overlap actions", status_and_path_do_not_overlap_actions},
        {"undersized client area returns empty layout", undersized_client_area_returns_empty_layout},
        {"dpi scaling preserves the fixed logical size", dpi_scaling_preserves_the_fixed_logical_size},
    };
    return arpg::test::make_suite("launcher_layout", cases);
}
