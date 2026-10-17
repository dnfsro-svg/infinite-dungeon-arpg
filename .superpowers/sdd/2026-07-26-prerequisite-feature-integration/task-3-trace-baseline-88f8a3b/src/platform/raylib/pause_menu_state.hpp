#pragma once

#include "platform/settings/settings_types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::platform {

enum class PauseScreen : std::uint8_t {
    closed,
    root,
    settings,
    capture_binding,
    quit_confirm
};

enum class PauseCommand : std::uint8_t {
    none,
    preview,
    apply,
    rollback,
    resume,
    quit
};

struct PauseContext final {
    bool death_active{};
    bool recovery_active{};
    bool inventory_open{};
    bool passive_open{};
    bool pending_save{};
    bool window_focused{true};
};

struct PauseInput final {
    bool escape{};
    bool enter{};
    bool up{};
    bool down{};
    bool left{};
    bool right{};
    bool activate{};
    bool focus_lost{};
    std::optional<settings::StableKey> captured_key{};
};

struct PauseMenuState final {
    PauseScreen screen{PauseScreen::closed};
    std::size_t selected_row{};
    std::optional<settings::SettingAction> capture_action{};
    settings::SettingsData committed{};
    settings::SettingsData draft{};
    const char* message{};
};

[[nodiscard]] PauseCommand update_pause_menu(
    PauseMenuState& state,
    const PauseContext& context,
    const PauseInput& input) noexcept;

}  // namespace arpg::platform
