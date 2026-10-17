#pragma once

#include "platform/settings/settings_types.hpp"

#include <cstdint>

namespace arpg::platform {

struct WindowSettingsBackend final {
    void* context{};
    bool (*set_volume)(void*, std::uint8_t){};
    bool (*set_window_mode)(void*, settings::WindowMode){};
    bool (*set_vsync)(void*, bool){};
    settings::WindowMode (*window_mode)(void*){};
    bool (*vsync)(void*){};
};

enum class LiveSettingsResult : std::uint8_t {
    applied,
    invalid,
    backend_failed,
    readback_failed
};

[[nodiscard]] LiveSettingsResult apply_live_settings(
    const settings::SettingsData& current,
    const settings::SettingsData& desired,
    WindowSettingsBackend backend) noexcept;
[[nodiscard]] LiveSettingsResult rollback_live_settings(
    const settings::SettingsData& previewed,
    const settings::SettingsData& committed,
    WindowSettingsBackend backend) noexcept;
[[nodiscard]] WindowSettingsBackend raylib_window_settings_backend() noexcept;

[[nodiscard]] float master_volume_fraction(std::uint8_t percent) noexcept;
[[nodiscard]] unsigned int initial_window_flags(
    const settings::SettingsData& committed) noexcept;

}  // namespace arpg::platform
