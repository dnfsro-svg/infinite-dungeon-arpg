#pragma once

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_checkpoint.hpp"

#include <cstdint>
#include <optional>

namespace arpg::dungeon {

[[nodiscard]] std::optional<combat::CombatLabConfig>
make_combat_lab_config(
    checkpoint::EntrySide entry,
    std::uint32_t rules_version) noexcept;

}  // namespace arpg::dungeon
