#pragma once

#include "platform/settings/settings_types.hpp"

#include <array>
#include <cstdint>

namespace arpg::platform {

struct ControlHints final {
    std::array<settings::StableKey,
        static_cast<std::size_t>(settings::SettingAction::count)> bindings{};
    std::uint64_t revision{};
    bool initialized{};
};

[[nodiscard]] settings::StableKey control_hint_binding(
    const ControlHints& hints,
    settings::SettingAction action) noexcept;
[[nodiscard]] const char* control_hint_key_label(
    const ControlHints& hints,
    settings::SettingAction action) noexcept;

void refresh_control_hints(
    ControlHints& hints,
    const settings::SettingsData& settings) noexcept;

}  // namespace arpg::platform
