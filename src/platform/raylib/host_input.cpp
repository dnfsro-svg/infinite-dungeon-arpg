#include "host_input.hpp"

#include "dungeon/dungeon_session.hpp"
#include "stable_key_raylib.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {
namespace {

constexpr std::size_t kStableKeyCount =
    static_cast<std::size_t>(settings::StableKey::count);

[[nodiscard]] constexpr std::size_t stable_index(
    settings::StableKey key) noexcept {
    return static_cast<std::size_t>(key);
}

[[nodiscard]] bool default_pressed(void*, int key) noexcept {
    return platform_key_pressed(key);
}

[[nodiscard]] bool default_down(void*, int key) noexcept {
    return platform_key_down(key);
}

[[nodiscard]] bool default_mouse_left_pressed(void*) noexcept {
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

[[nodiscard]] Vector2 default_mouse_position(void*) noexcept {
    return GetMousePosition();
}

[[nodiscard]] bool default_focus_lost(void*) noexcept {
    return !IsWindowFocused();
}

[[nodiscard]] bool pressed(
    const PhysicalKeySnapshot& snapshot,
    settings::StableKey key) noexcept {
    const std::size_t index = stable_index(key);
    return index < snapshot.pressed.size() && snapshot.pressed[index];
}

[[nodiscard]] bool down(
    const PhysicalKeySnapshot& snapshot,
    settings::StableKey key) noexcept {
    const std::size_t index = stable_index(key);
    return index < snapshot.down.size() && snapshot.down[index];
}

[[nodiscard]] bool action_pressed(
    const settings::SettingsData& settings_data,
    const PhysicalKeySnapshot& snapshot,
    settings::SettingAction action) noexcept {
    return pressed(snapshot, settings::binding_for(settings_data, action));
}

[[nodiscard]] bool binding_owns(
    const settings::SettingsData& settings_data,
    settings::StableKey key) noexcept {
    for (settings::StableKey binding : settings_data.bindings) {
        if (binding == key) return true;
    }
    return false;
}

}  // namespace

PhysicalKeySnapshot sample_physical_keys() noexcept {
    const PhysicalKeySource source{
        nullptr,
        &default_pressed,
        &default_down,
        &default_mouse_left_pressed,
        &default_mouse_position,
        &default_focus_lost,
    };
    return sample_physical_keys(source);
}

PhysicalKeySnapshot sample_physical_keys(
    const PhysicalKeySource& source) noexcept {
    PhysicalKeySnapshot snapshot{};
    if (source.pressed == nullptr || source.down == nullptr) return snapshot;

    for (std::size_t index = 0U; index < kStableKeyCount; ++index) {
        const auto key = static_cast<settings::StableKey>(index);
        const int code = raylib_key(key);
        snapshot.pressed[index] = source.pressed(source.context, code);
        snapshot.down[index] = source.down(source.context, code);
    }
    snapshot.escape = source.pressed(source.context, KEY_ESCAPE);
    snapshot.enter = source.pressed(source.context, KEY_ENTER);
    snapshot.f1 = source.pressed(source.context, KEY_F1);
    snapshot.f12 = source.pressed(source.context, KEY_F12);
    snapshot.v = snapshot.pressed[stable_index(settings::StableKey::v)];
    snapshot.mouse_left = source.mouse_left_pressed != nullptr
        && source.mouse_left_pressed(source.context);
    if (snapshot.mouse_left && source.mouse_position != nullptr) {
        snapshot.mouse_position = source.mouse_position(source.context);
    }
    snapshot.focus_lost = source.focus_lost != nullptr
        && source.focus_lost(source.context);
    return snapshot;
}

HostFrameInput map_host_frame_input(
    const settings::SettingsData& settings_data,
    const PhysicalKeySnapshot& snapshot) noexcept {
    HostFrameInput input{};
    const int left = down(snapshot, settings::binding_for(
        settings_data, settings::SettingAction::move_left)) ? 1 : 0;
    const int right = down(snapshot, settings::binding_for(
        settings_data, settings::SettingAction::move_right)) ? 1 : 0;
    const int up = down(snapshot, settings::binding_for(
        settings_data, settings::SettingAction::move_up)) ? 1 : 0;
    const int down_value = down(snapshot, settings::binding_for(
        settings_data, settings::SettingAction::move_down)) ? 1 : 0;
    input.movement = {
        static_cast<std::int8_t>(right - left),
        static_cast<std::int8_t>(down_value - up),
    };
    input.keys.movement = input.movement.x != 0 || input.movement.y != 0;

    constexpr std::array<settings::SettingAction, 3> combat_actions{{
        settings::SettingAction::light_attack,
        settings::SettingAction::jump,
        settings::SettingAction::launcher,
    }};
    for (std::size_t index = 0U; index < combat_actions.size(); ++index) {
        input.combat_actions[index] = action_pressed(
            settings_data, snapshot, combat_actions[index]);
        input.keys.attack = input.keys.attack || input.combat_actions[index];
    }

    input.keys.e = action_pressed(
        settings_data, snapshot, settings::SettingAction::interact);
    input.keys.inventory = action_pressed(
        settings_data, snapshot, settings::SettingAction::inventory);
    input.keys.passives = action_pressed(
        settings_data, snapshot, settings::SettingAction::passive_tree);
    input.keys.reset = pressed(snapshot, settings::StableKey::r)
        && !binding_owns(settings_data, settings::StableKey::r);
    input.keys.recovery = pressed(snapshot, settings::StableKey::n);
    input.keys.escape = snapshot.escape;
    input.keys.enter = snapshot.enter;
    input.keys.f1 = snapshot.f1;
    input.keys.f12 = snapshot.f12;
    input.keys.v = snapshot.v;
    input.keys.mouse_gameplay = snapshot.mouse_left;
    input.keys.focus_lost = snapshot.focus_lost;
    input.mouse_position = snapshot.mouse_position;
    return input;
}

void submit_frame_actions(
    dungeon::DungeonSession& session,
    const HostFrameInput& input) noexcept {
    constexpr std::array<combat::Action, 3> actions{{
        combat::Action::light,
        combat::Action::jump,
        combat::Action::launcher,
    }};
    for (std::size_t index = 0U; index < actions.size(); ++index) {
        if (input.combat_actions[index]) {
            static_cast<void>(session.queue_action(actions[index]));
        }
    }
}

}  // namespace arpg::platform
