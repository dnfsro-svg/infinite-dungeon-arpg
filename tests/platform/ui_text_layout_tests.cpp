#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "ui_text_layout.hpp"
#include "ui_utf8.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {

namespace platform = arpg::platform;
namespace test = arpg::test;

[[nodiscard]] std::size_t codepoint_count(const char* text) noexcept {
    if (text == nullptr) return 0U;
    std::size_t count{};
    for (const auto* cursor = reinterpret_cast<const unsigned char*>(text);
         *cursor != 0U; ++cursor) {
        if ((*cursor & 0xC0U) != 0x80U) ++count;
    }
    return count;
}

[[nodiscard]] platform::UiTextExtent linear_measure(
    const char* text, float font_size, float spacing, void*) noexcept {
    const std::size_t count = codepoint_count(text);
    const float glyphs = static_cast<float>(count);
    const float gaps = count > 0U ? static_cast<float>(count - 1U) : 0.0F;
    return {
        glyphs * font_size * 0.5F + gaps * spacing,
        font_size,
    };
}

[[nodiscard]] platform::UiTextExtent invalid_measure(
    const char*, float, float, void*) noexcept {
    const float nan = (std::numeric_limits<float>::quiet_NaN)();
    return {nan, nan};
}

[[nodiscard]] Rectangle text_bounds(
    const platform::UiCenteredTextLayout& layout) noexcept {
    return {
        layout.position.x,
        layout.position.y,
        layout.size.x,
        layout.size.y,
    };
}

[[nodiscard]] bool inside(Rectangle inner, Rectangle outer) noexcept {
    constexpr float kTolerance = 0.01F;
    return inner.width > 0.0F && inner.height > 0.0F
        && inner.x + kTolerance >= outer.x
        && inner.y + kTolerance >= outer.y
        && inner.x + inner.width <= outer.x + outer.width + kTolerance
        && inner.y + inner.height <= outer.y + outer.height + kTolerance;
}

test::Failure centered_text_is_contained_at_three_resolutions() noexcept {
    constexpr std::array<std::array<int, 2>, 3U> kResolutions{{
        {{800, 450}},
        {{1280, 720}},
        {{1920, 1080}},
    }};
    for (const auto& resolution : kResolutions) {
        const float width = static_cast<float>(resolution[0]);
        const float height = static_cast<float>(resolution[1]);
        const float scale = width / 1280.0F;
        const Rectangle interaction{
            width * 0.25F,
            height * 0.35F,
            width * 0.50F,
            56.0F * scale,
        };
        std::array<char, 64U> output{};
        const auto layout = platform::fit_centered_ui_text(
            output.data(), output.size(), u8"装备详情", interaction,
            24.0F * scale, 12.0F * scale, 0.5F * scale,
            &linear_measure, nullptr);

        ARPG_REQUIRE(layout.drawable);
        ARPG_REQUIRE(!layout.truncated);
        ARPG_REQUIRE(layout.text == output.data());
        ARPG_REQUIRE(std::strcmp(output.data(), u8"装备详情") == 0);
        ARPG_REQUIRE(inside(text_bounds(layout), interaction));
        ARPG_REQUIRE(test::near(
            layout.position.x + layout.size.x * 0.5F,
            interaction.x + interaction.width * 0.5F, 0.01));
        ARPG_REQUIRE(test::near(
            layout.position.y + layout.size.y * 0.5F,
            interaction.y + interaction.height * 0.5F, 0.01));
        ARPG_REQUIRE(layout.font_size >= 12.0F * scale);
        ARPG_REQUIRE(layout.font_size <= 24.0F * scale);
        ARPG_REQUIRE(layout.spacing >= 0.0F);
        ARPG_REQUIRE(layout.spacing <= 0.5F * scale);
    }
    return {};
}

test::Failure chinese_ellipsis_never_splits_utf8() noexcept {
    std::array<char, 96U> width_limited{};
    const Rectangle narrow{20.0F, 10.0F, 58.0F, 24.0F};
    const auto shortened = platform::fit_centered_ui_text(
        width_limited.data(), width_limited.size(),
        u8"超长装备属性说明文字", narrow,
        20.0F, 20.0F, 0.0F, &linear_measure, nullptr);
    ARPG_REQUIRE(shortened.drawable);
    ARPG_REQUIRE(shortened.truncated);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(width_limited.data()));
    ARPG_REQUIRE(std::strcmp(width_limited.data(), u8"超长装备…") == 0);
    ARPG_REQUIRE(inside(text_bounds(shortened), narrow));

    std::array<char, 13U> capacity_limited{};
    const Rectangle wide{0.0F, 0.0F, 400.0F, 40.0F};
    const auto capacity_fit = platform::fit_centered_ui_text(
        capacity_limited.data(), capacity_limited.size(),
        u8"超长装备属性说明文字", wide,
        20.0F, 12.0F, 0.0F, &linear_measure, nullptr);
    ARPG_REQUIRE(capacity_fit.drawable);
    ARPG_REQUIRE(capacity_fit.truncated);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(capacity_limited.data()));
    ARPG_REQUIRE(std::strcmp(capacity_limited.data(), u8"超长装…") == 0);

    std::array<char, 32U> in_place{};
    constexpr char kInPlaceText[] = u8"背包界面";
    std::memcpy(in_place.data(), kInPlaceText, sizeof(kInPlaceText));
    const auto in_place_fit = platform::fit_centered_ui_text(
        in_place.data(), in_place.size(), in_place.data(), wide,
        20.0F, 12.0F, 0.0F, &linear_measure, nullptr);
    ARPG_REQUIRE(in_place_fit.drawable);
    ARPG_REQUIRE(!in_place_fit.truncated);
    ARPG_REQUIRE(std::strcmp(in_place.data(), kInPlaceText) == 0);
    return {};
}

test::Failure label_and_value_regions_keep_the_exact_gap() noexcept {
    const Rectangle row{10.0F, 20.0F, 210.0F, 32.0F};
    const auto layout = platform::ui_label_value_row_layout(row, 14.0F);
    ARPG_REQUIRE(layout.valid);
    ARPG_REQUIRE(test::near(layout.gap, 14.0F));
    ARPG_REQUIRE(inside(layout.label_bounds, row));
    ARPG_REQUIRE(inside(layout.value_bounds, row));
    ARPG_REQUIRE(test::near(
        layout.value_bounds.x
            - (layout.label_bounds.x + layout.label_bounds.width),
        14.0F));
    ARPG_REQUIRE(layout.label_bounds.x + layout.label_bounds.width
        <= layout.value_bounds.x);
    ARPG_REQUIRE(test::near(
        layout.label_bounds.width, layout.value_bounds.width));
    return {};
}

test::Failure one_hundred_thousand_layouts_allocate_nothing() noexcept {
    constexpr Rectangle interaction{10.0F, 20.0F, 220.0F, 36.0F};
    std::array<char, 64U> output{};
    std::uint64_t drawable_count{};
    const std::uint64_t before = test::allocation_count();
    for (std::size_t iteration{}; iteration < 100000U; ++iteration) {
        const auto centered = platform::fit_centered_ui_text(
            output.data(), output.size(), u8"生命值", interaction,
            20.0F, 12.0F, 0.5F, &linear_measure, nullptr);
        const auto row = platform::ui_label_value_row_layout(
            interaction, 12.0F);
        drawable_count += centered.drawable && row.valid ? 1U : 0U;
    }
    const std::uint64_t after = test::allocation_count();
    std::printf("[ui-text-layout] iterations=100000 allocations=%llu\n",
        static_cast<unsigned long long>(after - before));
    ARPG_REQUIRE(drawable_count == 100000U);
    ARPG_REQUIRE(after == before);
    return {};
}

test::Failure invalid_or_impossible_inputs_fail_closed() noexcept {
    constexpr Rectangle good{0.0F, 0.0F, 120.0F, 30.0F};
    std::array<char, 32U> output{};
    const auto require_cleared = [&](const Rectangle bounds,
                                     float preferred,
                                     float minimum,
                                     float spacing,
                                     platform::UiTextMeasureFn measure)
        noexcept -> test::Failure {
        constexpr char kSentinel[] = "sentinel";
        std::memcpy(output.data(), kSentinel, sizeof(kSentinel));
        const auto layout = platform::fit_centered_ui_text(
            output.data(), output.size(), u8"装备", bounds,
            preferred, minimum, spacing, measure, nullptr);
        ARPG_REQUIRE(!layout.drawable);
        ARPG_REQUIRE(layout.text == nullptr);
        ARPG_REQUIRE(output[0] == '\0');
        return {};
    };

    const auto no_destination = platform::fit_centered_ui_text(
        nullptr, output.size(), u8"装备", good,
        20.0F, 12.0F, 0.5F, &linear_measure, nullptr);
    ARPG_REQUIRE(!no_destination.drawable);
    constexpr char kSentinel[] = "sentinel";
    std::memcpy(output.data(), kSentinel, sizeof(kSentinel));
    const auto zero_capacity = platform::fit_centered_ui_text(
        output.data(), 0U, u8"装备", good,
        20.0F, 12.0F, 0.5F, &linear_measure, nullptr);
    ARPG_REQUIRE(!zero_capacity.drawable);
    ARPG_REQUIRE(std::strcmp(output.data(), "sentinel") == 0);

    const float nan = (std::numeric_limits<float>::quiet_NaN)();
    const float infinity = (std::numeric_limits<float>::infinity)();
    ARPG_REQUIRE(require_cleared(
        {nan, 0.0F, 120.0F, 30.0F}, 20.0F, 12.0F, 0.5F,
        &linear_measure).expression == nullptr);
    ARPG_REQUIRE(require_cleared(
        {0.0F, 0.0F, 120.0F, infinity}, 20.0F, 12.0F, 0.5F,
        &linear_measure).expression == nullptr);
    ARPG_REQUIRE(require_cleared(
        good, nan, 12.0F, 0.5F, &linear_measure).expression == nullptr);
    ARPG_REQUIRE(require_cleared(
        good, 20.0F, 12.0F, infinity,
        &linear_measure).expression == nullptr);
    ARPG_REQUIRE(require_cleared(
        good, 20.0F, 12.0F, 0.5F,
        &invalid_measure).expression == nullptr);
    ARPG_REQUIRE(require_cleared(
        {0.0F, 0.0F, 1.0F, 1.0F}, 20.0F, 12.0F, 0.5F,
        &linear_measure).expression == nullptr);
    ARPG_REQUIRE(require_cleared(
        good, 20.0F, 12.0F, 0.5F, nullptr).expression == nullptr);

    ARPG_REQUIRE(!platform::ui_label_value_row_layout(
        {nan, 0.0F, 120.0F, 30.0F}, 8.0F).valid);
    ARPG_REQUIRE(!platform::ui_label_value_row_layout(
        good, infinity).valid);
    ARPG_REQUIRE(!platform::ui_label_value_row_layout(
        good, good.width).valid);
    return {};
}

constexpr test::TestCase kCases[] = {
    {"three resolution centering and containment",
        &centered_text_is_contained_at_three_resolutions},
    {"Chinese ellipsis stays valid UTF-8",
        &chinese_ellipsis_never_splits_utf8},
    {"label and value regions keep fixed gap",
        &label_and_value_regions_keep_the_exact_gap},
    {"100000 layouts allocate nothing",
        &one_hundred_thousand_layouts_allocate_nothing},
    {"invalid and impossible inputs fail closed",
        &invalid_or_impossible_inputs_fail_closed},
};

}  // namespace

arpg::test::TestSuite ui_text_layout_suite() noexcept {
    return arpg::test::make_suite("ui_text_layout", kCases);
}
