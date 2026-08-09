#pragma once

#include "ui_text_catalog.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace arpg::platform {

inline constexpr std::size_t kUiGlyphCapacity = 640U;
inline constexpr std::size_t kUiFontAtlasInstanceCount = 1U;
inline constexpr std::size_t kUiFontAtlasByteBudget =
    32U * 1024U * 1024U;
inline constexpr int kUiFontSourceBaseSize = 96;
inline constexpr int kUiFontMaximumDisplaySize = 39;
inline constexpr std::size_t kUiFontAtlasBytesPerPixel = 2U;

[[nodiscard]] constexpr std::size_t ui_font_atlas_bytes(
    int width, int height) noexcept {
    return width > 0 && height > 0
        ? static_cast<std::size_t>(width)
            * static_cast<std::size_t>(height)
            * kUiFontAtlasBytesPerPixel
        : 0U;
}

struct UiGlyphPlan final {
    std::array<const char*, 1U> candidate_paths{};
    std::size_t candidate_count{};
    std::array<int, kUiGlyphCapacity> codepoints{};
    std::size_t codepoint_count{};
    bool capacity_exceeded{};
};

class SharedUiFont final {
public:
    ~SharedUiFont() noexcept;
    [[nodiscard]] bool initialize() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] Font font() const noexcept;
    [[nodiscard]] std::size_t atlas_bytes() const noexcept;
    [[nodiscard]] const UiGlyphPlan& plan() const noexcept;

private:
    Font font_{};
    UiGlyphPlan plan_{};
    bool ready_{};
};

using UiGlyphPlanCatalogVisitor = void (*)(UiTextVisitor visitor,
    void* context) noexcept;

[[nodiscard]] UiGlyphPlan build_ui_glyph_plan(
    UiGlyphPlanCatalogVisitor visit_catalog) noexcept;
[[nodiscard]] UiGlyphPlan ui_glyph_plan() noexcept;
[[nodiscard]] bool ui_glyph_plan_has_codepoint(
    const UiGlyphPlan& plan, int codepoint) noexcept;
[[nodiscard]] bool ui_glyph_plan_covers_text(
    const UiGlyphPlan& plan, const char* utf8_text) noexcept;

}  // namespace arpg::platform
