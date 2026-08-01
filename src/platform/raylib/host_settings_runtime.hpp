#pragma once

#include "window_settings.hpp"

#include <cstdint>

namespace arpg::settings {
class SettingsStore;
}

namespace arpg::platform {

struct HostSettingsNotice;
enum class PauseCommand : std::uint8_t;
enum class PauseScreen : std::uint8_t;
struct PauseMenuState;

struct HostSettingsRuntime final {
    HostSettingsNotice* notice{};
    PauseMenuState* pause_menu{};
    settings::SettingsData* live{};
    settings::SettingsData* input{};
    const settings::SettingsStore* store{};
    WindowSettingsBackend backend{};

    void consume_notice(PauseScreen previous_screen) noexcept;
    [[nodiscard]] bool settle(
        PauseCommand command,
        bool window_close_requested);
};

}  // namespace arpg::platform
