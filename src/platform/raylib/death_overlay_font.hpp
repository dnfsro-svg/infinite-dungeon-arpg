#pragma once

#include <array>
#include <cstddef>

namespace arpg::platform {

inline constexpr std::size_t kDeathOverlayFontCandidateCapacity = 4U;
inline constexpr std::size_t kDeathOverlayCodepointCapacity = 384U;
inline constexpr int kUiFontSourceBaseSize = 64;
inline constexpr int kUiFontMaximumDisplaySize = 26;
inline constexpr std::size_t kUiFontAtlasBytesPerPixel = 2U;
inline constexpr std::size_t kUiFontAtlasByteBudget = 8U * 1024U * 1024U;
inline constexpr std::size_t kUiFontAtlasInstanceCount = 3U;
inline constexpr std::size_t kUiFontTotalAtlasByteBudget =
    kUiFontAtlasByteBudget * kUiFontAtlasInstanceCount;

[[nodiscard]] constexpr std::size_t ui_font_atlas_bytes(
    int width, int height) noexcept {
    return width > 0 && height > 0
        ? static_cast<std::size_t>(width)
            * static_cast<std::size_t>(height)
            * kUiFontAtlasBytesPerPixel
        : 0U;
}

struct DeathOverlayFontPlan final {
    std::array<const char*, kDeathOverlayFontCandidateCapacity>
        candidate_paths{};
    std::size_t candidate_count{};
    std::array<int, kDeathOverlayCodepointCapacity> codepoints{};
    std::size_t codepoint_count{};
};

[[nodiscard]] DeathOverlayFontPlan death_overlay_font_plan() noexcept;

[[nodiscard]] bool death_overlay_font_has_codepoint(
    const DeathOverlayFontPlan& plan,
    int codepoint) noexcept;

[[nodiscard]] bool death_overlay_font_covers_text(
    const DeathOverlayFontPlan& plan,
    const char* utf8_text) noexcept;

}  // namespace arpg::platform
