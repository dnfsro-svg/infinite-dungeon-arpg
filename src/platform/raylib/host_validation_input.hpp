#pragma once

#include <cstdint>

namespace arpg::combat {
struct MovementInput;
}

namespace arpg::settings {
enum class SettingAction : std::uint8_t;
enum class StableKey : std::uint8_t;
struct SettingsData;
}

namespace arpg::platform {
struct PhysicalKeySnapshot;

namespace host_validation {

void inject_validation_pressed(PhysicalKeySnapshot&,
    settings::StableKey) noexcept;
void inject_validation_action(PhysicalKeySnapshot&,
    const settings::SettingsData&, settings::SettingAction,
    bool pressed) noexcept;
void inject_validation_movement(PhysicalKeySnapshot&,
    const settings::SettingsData&, combat::MovementInput) noexcept;

}  // namespace host_validation
}  // namespace arpg::platform
