#include "host_validation_input.hpp"

#include "combat/combat_types.hpp"
#include "host_input.hpp"
#include "platform/settings/settings_types.hpp"

#include <cstddef>

namespace arpg::platform::host_validation {

void inject_validation_pressed(PhysicalKeySnapshot& snapshot,
    settings::StableKey key) noexcept {
    const std::size_t index = static_cast<std::size_t>(key);
    if (index < snapshot.pressed.size()) {
        snapshot.pressed[index] = true;
        snapshot.down[index] = true;
    }
}

void inject_validation_action(PhysicalKeySnapshot& snapshot,
    const settings::SettingsData& settings_data,
    settings::SettingAction action, bool pressed) noexcept {
    const settings::StableKey key = settings::binding_for(settings_data, action);
    const std::size_t index = static_cast<std::size_t>(key);
    if (index >= snapshot.down.size()) return;
    snapshot.down[index] = true;
    if (pressed) snapshot.pressed[index] = true;
}

void inject_validation_movement(PhysicalKeySnapshot& snapshot,
    const settings::SettingsData& settings_data,
    combat::MovementInput movement) noexcept {
    if (movement.x < 0) {
        inject_validation_action(snapshot, settings_data,
            settings::SettingAction::move_left, false);
    } else if (movement.x > 0) {
        inject_validation_action(snapshot, settings_data,
            settings::SettingAction::move_right, false);
    }
    if (movement.y < 0) {
        inject_validation_action(snapshot, settings_data,
            settings::SettingAction::move_up, false);
    } else if (movement.y > 0) {
        inject_validation_action(snapshot, settings_data,
            settings::SettingAction::move_down, false);
    }
}

}  // namespace arpg::platform::host_validation
