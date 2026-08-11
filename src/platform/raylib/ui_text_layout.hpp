#pragma once

#include <raylib.h>

#include <cstddef>

namespace arpg::platform {

struct UiTextExtent final {
    float width{};
    float height{};
};

using UiTextMeasureFn = UiTextExtent (*)(
    const char* text,
    float font_size,
    float spacing,
    void* context) noexcept;

struct UiCenteredTextLayout final {
    const char* text{};
    Vector2 position{};
    Vector2 size{};
    float font_size{};
    float spacing{};
    bool truncated{};
    bool drawable{};
};

struct UiLabelValueRowLayout final {
    Rectangle label_bounds{};
    Rectangle value_bounds{};
    float gap{};
    bool valid{};
};

// Copies source into the caller-owned output buffer, shrinks to the supplied
// minimum, and then elides whole UTF-8 code points when required. The returned
// text pointer aliases output and is non-null only for a drawable result.
// source may equal output exactly; other overlapping ranges fail closed.
[[nodiscard]] UiCenteredTextLayout fit_centered_ui_text(
    char* output,
    std::size_t output_capacity,
    const char* source,
    Rectangle interaction_bounds,
    float preferred_font_size,
    float minimum_font_size,
    float preferred_spacing,
    UiTextMeasureFn measure,
    void* measure_context) noexcept;

// Splits a complete row into equal left-label and right-value regions with an
// exact caller-supplied gap. Invalid geometry returns valid=false.
[[nodiscard]] UiLabelValueRowLayout ui_label_value_row_layout(
    Rectangle row_bounds,
    float gap) noexcept;

}  // namespace arpg::platform
