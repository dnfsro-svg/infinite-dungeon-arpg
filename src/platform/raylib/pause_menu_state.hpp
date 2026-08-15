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
    reset_confirm,
    quit_confirm,
    quit_saving,
};

enum class PauseSettingsPage : std::uint8_t {
    home,
    audio,
    display,
    bindings,
};

enum class PauseCommand : std::uint8_t {
    none,
    preview,
    apply,
    rollback,
    resume,
    reset_room,
    quit
};

enum class PauseRoomResetContext : std::uint8_t {
    normal,
    active_abyss,
    cleared_abyss,
};

enum class PauseRoomResetOrigin : std::uint8_t {
    root,
    closed,
};

struct PauseContext final {
    bool death_active{};
    bool recovery_active{};
    bool inventory_open{};
    bool passive_open{};
    bool pending_save{};
    bool window_focused{true};
    PauseRoomResetContext reset_room_context{PauseRoomResetContext::normal};
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
    bool reset{};
};

struct PauseMenuState final {
    PauseScreen screen{PauseScreen::closed};
    PauseSettingsPage settings_page{PauseSettingsPage::home};
    std::size_t selected_row{};
    std::optional<settings::SettingAction> capture_action{};
    settings::SettingsData committed{};
    settings::SettingsData draft{};
    const char* message{};
    PauseRoomResetContext reset_room_context{PauseRoomResetContext::normal};
    PauseRoomResetOrigin reset_room_origin{PauseRoomResetOrigin::root};
};

[[nodiscard]] PauseCommand update_pause_menu(
    PauseMenuState& state,
    const PauseContext& context,
    const PauseInput& input) noexcept;

void begin_pause_quit_saving(PauseMenuState& state) noexcept;
void cancel_pause_quit_saving(PauseMenuState& state) noexcept;
void resolve_pause_room_reset(PauseMenuState& state, bool accepted) noexcept;

}  // namespace arpg::platform
