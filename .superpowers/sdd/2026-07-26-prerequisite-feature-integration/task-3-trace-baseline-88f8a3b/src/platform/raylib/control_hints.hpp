#pragma once

#include "platform/settings/settings_types.hpp"

#include <array>
#include <cstdint>

namespace arpg::platform {

struct ControlHints final {
    std::array<char, 160> primary{};
    std::array<char, 160> secondary{};
    std::uint64_t revision{};
};

void refresh_control_hints(
    ControlHints& hints,
    const settings::SettingsData& settings) noexcept;

}  // namespace arpg::platform
