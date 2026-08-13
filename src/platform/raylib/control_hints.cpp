#include "control_hints.hpp"

#include "stable_key_raylib.hpp"

#include <cstdio>

namespace arpg::platform {
namespace {

[[nodiscard]] const char* key_for(
    const settings::SettingsData& settings,
    settings::SettingAction action) noexcept {
    return stable_key_label(settings::binding_for(settings, action));
}

}  // namespace

void refresh_control_hints(
    ControlHints& hints,
    const settings::SettingsData& settings) noexcept {
    if ((hints.primary[0] != '\0' || hints.secondary[0] != '\0')
            && hints.revision == settings.revision) {
        return;
    }

    static_cast<void>(std::snprintf(hints.primary.data(), hints.primary.size(),
        "%s Move Up  %s Move Down  %s Move Left  %s Move Right",
        key_for(settings, settings::SettingAction::move_up),
        key_for(settings, settings::SettingAction::move_down),
        key_for(settings, settings::SettingAction::move_left),
        key_for(settings, settings::SettingAction::move_right)));
    static_cast<void>(std::snprintf(hints.secondary.data(), hints.secondary.size(),
        "%s Light Attack  %s Jump  %s Launcher  %s Interact  %s Inventory  "
        "%s Passive Tree (After Full Clear)  F1 Debug  F12 Screenshot  Esc Pause",
        key_for(settings, settings::SettingAction::light_attack),
        key_for(settings, settings::SettingAction::jump),
        key_for(settings, settings::SettingAction::launcher),
        key_for(settings, settings::SettingAction::interact),
        key_for(settings, settings::SettingAction::inventory),
        key_for(settings, settings::SettingAction::passive_tree)));
    hints.primary.back() = '\0';
    hints.secondary.back() = '\0';
    hints.revision = settings.revision;
}

}  // namespace arpg::platform
