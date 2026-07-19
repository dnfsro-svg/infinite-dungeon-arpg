#include "platform/settings/settings_types.hpp"

#include <array>
#include <cstddef>

namespace arpg::settings {
namespace {

constexpr std::size_t action_count = static_cast<std::size_t>(SettingAction::count);

constexpr std::array<StableKey, action_count> default_bindings{
    StableKey::w,
    StableKey::s,
    StableKey::a,
    StableKey::d,
    StableKey::j,
    StableKey::k,
    StableKey::l,
    StableKey::e,
    StableKey::i,
    StableKey::p};

constexpr std::array<const char*, action_count> action_labels{
    "Move Up",
    "Move Down",
    "Move Left",
    "Move Right",
    "Light Attack",
    "Jump",
    "Launcher",
    "Interact",
    "Inventory",
    "Passive Tree"};

constexpr std::array<const char*, 3> loot_filter_labels{
    "Show All",
    "Magic or Better",
    "Rare Only"};

[[nodiscard]] constexpr std::size_t action_index(SettingAction action) noexcept {
    return static_cast<std::size_t>(action);
}

[[nodiscard]] constexpr bool valid_key(StableKey key) noexcept {
    return static_cast<std::size_t>(key) < static_cast<std::size_t>(StableKey::count);
}

[[nodiscard]] constexpr bool valid_loot_filter_mode(LootFilterMode mode) noexcept {
    return static_cast<std::size_t>(mode) < loot_filter_labels.size();
}

}  // namespace

SettingsData default_settings() noexcept {
    SettingsData settings{};
    settings.bindings = default_bindings;
    return settings;
}

SettingsValidationError validate_settings(const SettingsData& settings) noexcept {
    if (settings.master_sfx_percent > 100U) {
        return SettingsValidationError::volume_range;
    }
    if (settings.master_sfx_percent % 5U != 0U) {
        return SettingsValidationError::volume_step;
    }
    if (settings.window_mode != WindowMode::windowed &&
        settings.window_mode != WindowMode::fullscreen) {
        return SettingsValidationError::window_mode;
    }
    if (!valid_loot_filter_mode(settings.loot_filter_mode)) {
        return SettingsValidationError::loot_filter_mode;
    }

    for (std::size_t index = 0; index < settings.bindings.size(); ++index) {
        if (!valid_key(settings.bindings[index])) {
            return SettingsValidationError::key_range;
        }
        if (settings.bindings[index] == StableKey::v) {
            return SettingsValidationError::reserved_key;
        }
        for (std::size_t other = index + 1U; other < settings.bindings.size(); ++other) {
            if (settings.bindings[index] == settings.bindings[other]) {
                return SettingsValidationError::duplicate_key;
            }
        }
    }
    return SettingsValidationError::none;
}

StableKey binding_for(const SettingsData& settings, SettingAction action) noexcept {
    const std::size_t index = action_index(action);
    return index < settings.bindings.size() ? settings.bindings[index] : StableKey::count;
}

bool assign_or_swap(SettingsData& settings, SettingAction action, StableKey key) noexcept {
    const std::size_t target = action_index(action);
    if (target >= settings.bindings.size() || !valid_key(key) || key == StableKey::v) {
        return false;
    }

    const StableKey old_key = settings.bindings[target];
    for (std::size_t owner = 0; owner < settings.bindings.size(); ++owner) {
        if (owner != target && settings.bindings[owner] == key) {
            settings.bindings[owner] = old_key;
            break;
        }
    }
    settings.bindings[target] = key;
    return true;
}

const char* action_label(SettingAction action) noexcept {
    const std::size_t index = action_index(action);
    return index < action_labels.size() ? action_labels[index] : "Unknown";
}

const char* loot_filter_label(LootFilterMode mode) noexcept {
    const std::size_t index = static_cast<std::size_t>(mode);
    return index < loot_filter_labels.size() ? loot_filter_labels[index] : "Unknown";
}

}  // namespace arpg::settings
