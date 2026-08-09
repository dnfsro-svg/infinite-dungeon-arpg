#include "control_hints.hpp"

#include "stable_key_raylib.hpp"

namespace arpg::platform {

settings::StableKey control_hint_binding(
    const ControlHints& hints,
    settings::SettingAction action) noexcept {
    if (!hints.initialized) return settings::StableKey::count;
    const std::size_t index = static_cast<std::size_t>(action);
    return index < hints.bindings.size()
        ? hints.bindings[index] : settings::StableKey::count;
}

const char* control_hint_key_label(
    const ControlHints& hints,
    settings::SettingAction action) noexcept {
    return stable_key_label(control_hint_binding(hints, action));
}

void refresh_control_hints(
    ControlHints& hints,
    const settings::SettingsData& settings) noexcept {
    if (hints.initialized && hints.revision == settings.revision) {
        return;
    }
    hints.bindings = settings.bindings;
    hints.revision = settings.revision;
    hints.initialized = true;
}

}  // namespace arpg::platform
