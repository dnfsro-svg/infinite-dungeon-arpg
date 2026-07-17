#pragma once

#include <array>
#include <cstddef>

namespace arpg::platform {

inline constexpr std::size_t kDeathOverlayFontCandidateCapacity = 4U;
inline constexpr std::size_t kDeathOverlayCodepointCapacity = 256U;

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
