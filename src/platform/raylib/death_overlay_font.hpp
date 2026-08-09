#pragma once

#include "shared_ui_font.hpp"

namespace arpg::platform {

using DeathOverlayFontPlan = UiGlyphPlan;

inline constexpr std::size_t kDeathOverlayFontCandidateCapacity = 1U;
inline constexpr std::size_t kDeathOverlayCodepointCapacity =
    kUiGlyphCapacity;
inline constexpr std::size_t kUiFontTotalAtlasByteBudget =
    kUiFontAtlasByteBudget * kUiFontAtlasInstanceCount;

[[nodiscard]] DeathOverlayFontPlan death_overlay_font_plan() noexcept;
[[nodiscard]] bool death_overlay_font_has_codepoint(
    const DeathOverlayFontPlan& plan, int codepoint) noexcept;
[[nodiscard]] bool death_overlay_font_covers_text(
    const DeathOverlayFontPlan& plan, const char* utf8_text) noexcept;

}  // namespace arpg::platform
