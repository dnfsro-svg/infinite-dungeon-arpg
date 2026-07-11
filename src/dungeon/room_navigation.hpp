#pragma once

#include "dungeon/dungeon_types.hpp"

#include <optional>

namespace arpg::dungeon {

[[nodiscard]] std::optional<ExitDirection> requested_exit(
    combat::Vec3 position,
    combat::MovementInput movement) noexcept;

}  // namespace arpg::dungeon
