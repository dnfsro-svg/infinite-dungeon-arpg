#pragma once

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_types.hpp"

#include <cstdint>

namespace arpg::dungeon {
class DungeonSession;
}

namespace arpg::platform {
struct RaylibHostConfig;

namespace host_validation {

struct Stage10ValidationState final {
    bool entered_abyss{};
    bool reset_requested{};
    bool descent_warning_seen{};
    std::uint32_t chaos_presented_frames{};
};

struct Stage11ValidationState final {
    bool entered_abyss{};
    bool saw_depth_two{};
    bool continue_requested{};
    std::uint32_t target_presented_frames{};
};

[[nodiscard]] combat::MovementInput stage10_validation_input(
    dungeon::DungeonSession&, const dungeon::DungeonSnapshot&,
    const RaylibHostConfig&, Stage10ValidationState&) noexcept;
[[nodiscard]] combat::MovementInput stage11_validation_input(
    dungeon::DungeonSession&, const dungeon::DungeonSnapshot&,
    const RaylibHostConfig&, Stage11ValidationState&) noexcept;
[[nodiscard]] bool stage10_validation_reached(
    const dungeon::DungeonSnapshot&, const RaylibHostConfig&,
    const Stage10ValidationState&) noexcept;
[[nodiscard]] bool stage11_validation_reached(
    const dungeon::DungeonSnapshot&, const RaylibHostConfig&,
    const Stage11ValidationState&) noexcept;

}  // namespace host_validation
}  // namespace arpg::platform
