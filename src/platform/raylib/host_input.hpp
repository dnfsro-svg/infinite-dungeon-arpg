#pragma once

#include "combat/combat_types.hpp"
#include "platform/settings/settings_types.hpp"
#include "raylib_input.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace arpg::dungeon {
class DungeonSession;
}

namespace arpg::platform {

struct PhysicalKeySnapshot final {
    std::array<bool,
        static_cast<std::size_t>(settings::StableKey::count)> down{};
    std::array<bool,
        static_cast<std::size_t>(settings::StableKey::count)> pressed{};
    bool escape{};
    bool enter{};
    bool f1{};
    bool f12{};
    bool v{};
    bool mouse_left{};
    bool focus_lost{};
    Vector2 mouse_position{};
};

struct HostFrameInput final {
    FrameKeyState keys{};
    combat::MovementInput movement{};
    std::array<bool, 3> combat_actions{};
    Vector2 mouse_position{};
};

struct PhysicalKeySource final {
    void* context{};
    bool (*pressed)(void*, int) noexcept{};
    bool (*down)(void*, int) noexcept{};
    bool (*mouse_left_pressed)(void*) noexcept{};
    Vector2 (*mouse_position)(void*) noexcept{};
    bool (*focus_lost)(void*) noexcept{};
};

[[nodiscard]] PhysicalKeySnapshot sample_physical_keys() noexcept;
[[nodiscard]] PhysicalKeySnapshot sample_physical_keys(
    const PhysicalKeySource& source) noexcept;
[[nodiscard]] HostFrameInput map_host_frame_input(
    const settings::SettingsData& settings,
    const PhysicalKeySnapshot& snapshot) noexcept;
void submit_frame_actions(
    dungeon::DungeonSession& session,
    const HostFrameInput& input) noexcept;

}  // namespace arpg::platform
