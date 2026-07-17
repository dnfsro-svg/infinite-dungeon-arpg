#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::settings {

enum class SettingAction : std::uint8_t {
    move_up,
    move_down,
    move_left,
    move_right,
    light_attack,
    jump,
    launcher,
    interact,
    inventory,
    passive_tree,
    count
};

enum class StableKey : std::uint8_t {
    a,
    b,
    c,
    d,
    e,
    f,
    g,
    h,
    i,
    j,
    k,
    l,
    m,
    n,
    o,
    p,
    q,
    r,
    s,
    t,
    u,
    v,
    w,
    x,
    y,
    z,
    digit_0,
    digit_1,
    digit_2,
    digit_3,
    digit_4,
    digit_5,
    digit_6,
    digit_7,
    digit_8,
    digit_9,
    arrow_up,
    arrow_down,
    arrow_left,
    arrow_right,
    space,
    left_shift,
    right_shift,
    left_control,
    right_control,
    count
};

enum class WindowMode : std::uint8_t {
    windowed,
    fullscreen
};

struct SettingsData final {
    std::uint8_t master_sfx_percent{100};
    WindowMode window_mode{WindowMode::windowed};
    bool vsync_enabled{true};
    std::array<StableKey, static_cast<std::size_t>(SettingAction::count)> bindings{};
    std::uint64_t revision{};
};

enum class SettingsValidationError : std::uint8_t {
    none,
    volume_range,
    volume_step,
    window_mode,
    key_range,
    duplicate_key
};

[[nodiscard]] SettingsData default_settings() noexcept;
[[nodiscard]] SettingsValidationError validate_settings(const SettingsData& settings) noexcept;
[[nodiscard]] StableKey binding_for(const SettingsData& settings, SettingAction action) noexcept;
[[nodiscard]] bool assign_or_swap(
    SettingsData& settings,
    SettingAction action,
    StableKey key) noexcept;
[[nodiscard]] const char* action_label(SettingAction action) noexcept;

}  // namespace arpg::settings
