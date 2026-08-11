#include "ui_text_layout.hpp"

#include "ui_utf8.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>

namespace arpg::platform {
namespace {

constexpr char kEllipsis[] = u8"…";
constexpr std::size_t kEllipsisBytes = sizeof(kEllipsis) - 1U;

void clear_output(char* output, std::size_t capacity) noexcept {
    if (output != nullptr && capacity != 0U) output[0] = '\0';
}

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool valid_rectangle(Rectangle bounds) noexcept {
    return finite(bounds.x) && finite(bounds.y)
        && finite(bounds.width) && finite(bounds.height)
        && bounds.width > 0.0F && bounds.height > 0.0F
        && finite(bounds.x + bounds.width)
        && finite(bounds.y + bounds.height);
}

[[nodiscard]] bool valid_extent(UiTextExtent extent) noexcept {
    return finite(extent.width) && finite(extent.height)
        && extent.width > 0.0F && extent.height > 0.0F;
}

[[nodiscard]] bool fits(
    UiTextExtent extent, Rectangle bounds) noexcept {
    constexpr float kTolerance = 0.001F;
    return extent.width <= bounds.width + kTolerance
        && extent.height <= bounds.height + kTolerance;
}

[[nodiscard]] bool measure_at(UiTextMeasureFn measure, void* context,
    const char* text, float font_size, float spacing,
    UiTextExtent& extent) noexcept {
    if (measure == nullptr || text == nullptr
            || !finite(font_size) || !finite(spacing)
            || !(font_size > 0.0F) || spacing < 0.0F) {
        return false;
    }
    extent = measure(text, font_size, spacing, context);
    return valid_extent(extent);
}

[[nodiscard]] float scaled_spacing(float preferred_spacing,
    float font_size, float preferred_font_size) noexcept {
    return preferred_spacing * (font_size / preferred_font_size);
}

[[nodiscard]] bool choose_largest_fitting_size(const char* text,
    Rectangle bounds, float preferred_font_size, float minimum_font_size,
    float preferred_spacing, UiTextMeasureFn measure, void* context,
    float& font_size, float& spacing, UiTextExtent& extent) noexcept {
    spacing = preferred_spacing;
    font_size = preferred_font_size;
    if (!measure_at(measure, context, text, font_size, spacing, extent)) {
        return false;
    }
    if (fits(extent, bounds)) return true;

    float low = minimum_font_size;
    float high = preferred_font_size;
    float low_spacing = scaled_spacing(
        preferred_spacing, low, preferred_font_size);
    UiTextExtent low_extent{};
    if (!finite(low_spacing)
            || !measure_at(measure, context, text, low, low_spacing,
                low_extent)
            || !fits(low_extent, bounds)) {
        return false;
    }

    font_size = low;
    spacing = low_spacing;
    extent = low_extent;
    for (int iteration = 0; iteration < 24; ++iteration) {
        const float middle = low + (high - low) * 0.5F;
        const float middle_spacing = scaled_spacing(
            preferred_spacing, middle, preferred_font_size);
        UiTextExtent middle_extent{};
        if (!finite(middle) || !finite(middle_spacing)
                || !measure_at(measure, context, text, middle,
                    middle_spacing, middle_extent)) {
            return false;
        }
        if (fits(middle_extent, bounds)) {
            low = middle;
            font_size = middle;
            spacing = middle_spacing;
            extent = middle_extent;
        } else {
            high = middle;
        }
    }
    return true;
}

[[nodiscard]] std::size_t previous_codepoint_start(
    const char* text, std::size_t length) noexcept {
    if (text == nullptr || length == 0U) return 0U;
    std::size_t index = length - 1U;
    while (index > 0U
            && (static_cast<unsigned char>(text[index]) & 0xC0U) == 0x80U) {
        --index;
    }
    return index;
}

[[nodiscard]] bool ends_with_ellipsis(
    const char* text, std::size_t length) noexcept {
    return length >= kEllipsisBytes
        && std::memcmp(text + length - kEllipsisBytes,
            kEllipsis, kEllipsisBytes) == 0;
}

[[nodiscard]] bool shorten_with_ellipsis(
    char* output, std::size_t capacity) noexcept {
    if (output == nullptr || capacity <= kEllipsisBytes) return false;
    std::size_t length = std::strlen(output);
    if (ends_with_ellipsis(output, length)) length -= kEllipsisBytes;
    if (length == 0U) return false;
    length = previous_codepoint_start(output, length);
    while (length + kEllipsisBytes >= capacity) {
        if (length == 0U) return false;
        length = previous_codepoint_start(output, length);
    }
    std::memcpy(output + length, kEllipsis, sizeof(kEllipsis));
    return true;
}

[[nodiscard]] UiCenteredTextLayout fail(
    char* output, std::size_t output_capacity) noexcept {
    clear_output(output, output_capacity);
    return {};
}

}  // namespace

UiCenteredTextLayout fit_centered_ui_text(
    char* output,
    std::size_t output_capacity,
    const char* source,
    Rectangle interaction_bounds,
    float preferred_font_size,
    float minimum_font_size,
    float preferred_spacing,
    UiTextMeasureFn measure,
    void* measure_context) noexcept {
    if (output == nullptr || output_capacity == 0U
            || source == nullptr || source[0] == '\0'
            || !valid_rectangle(interaction_bounds)
            || !finite(preferred_font_size) || !finite(minimum_font_size)
            || !finite(preferred_spacing)
            || !(preferred_font_size > 0.0F)
            || !(minimum_font_size > 0.0F)
            || minimum_font_size > preferred_font_size
            || preferred_spacing < 0.0F || measure == nullptr) {
        return fail(output, output_capacity);
    }

    const UiUtf8CopyResult copied = ui_utf8_fit_with_ellipsis(
        output, output_capacity, source);
    if (!copied.input_was_valid || output[0] == '\0') {
        return fail(output, output_capacity);
    }

    bool truncated = copied.truncated;
    float font_size{};
    float spacing{};
    UiTextExtent extent{};
    if (!choose_largest_fitting_size(output, interaction_bounds,
            preferred_font_size, minimum_font_size, preferred_spacing,
            measure, measure_context, font_size, spacing, extent)) {
        const float minimum_spacing = scaled_spacing(
            preferred_spacing, minimum_font_size, preferred_font_size);
        if (!finite(minimum_spacing)) return fail(output, output_capacity);

        UiTextExtent minimum_extent{};
        if (!measure_at(measure, measure_context, output,
                minimum_font_size, minimum_spacing, minimum_extent)
                || minimum_extent.height > interaction_bounds.height) {
            return fail(output, output_capacity);
        }
        while (!fits(minimum_extent, interaction_bounds)) {
            if (!shorten_with_ellipsis(output, output_capacity)) {
                return fail(output, output_capacity);
            }
            truncated = true;
            if (!measure_at(measure, measure_context, output,
                    minimum_font_size, minimum_spacing, minimum_extent)) {
                return fail(output, output_capacity);
            }
        }
        if (!choose_largest_fitting_size(output, interaction_bounds,
                preferred_font_size, minimum_font_size, preferred_spacing,
                measure, measure_context, font_size, spacing, extent)) {
            return fail(output, output_capacity);
        }
    }

    const Vector2 position{
        interaction_bounds.x
            + (interaction_bounds.width - extent.width) * 0.5F,
        interaction_bounds.y
            + (interaction_bounds.height - extent.height) * 0.5F,
    };
    if (!finite(position.x) || !finite(position.y)) {
        return fail(output, output_capacity);
    }
    return {
        output,
        position,
        {extent.width, extent.height},
        font_size,
        spacing,
        truncated,
        true,
    };
}

UiLabelValueRowLayout ui_label_value_row_layout(
    Rectangle row_bounds, float gap) noexcept {
    if (!valid_rectangle(row_bounds) || !finite(gap) || gap < 0.0F
            || !(row_bounds.width > gap)) {
        return {};
    }
    const float column_width = (row_bounds.width - gap) * 0.5F;
    const float value_x = row_bounds.x + column_width + gap;
    if (!(column_width > 0.0F) || !finite(column_width)
            || !finite(value_x)) {
        return {};
    }
    return {
        {row_bounds.x, row_bounds.y, column_width, row_bounds.height},
        {value_x, row_bounds.y, column_width, row_bounds.height},
        gap,
        true,
    };
}

}  // namespace arpg::platform
