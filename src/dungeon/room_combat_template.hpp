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

[[nodiscard]] std::optional<combat::CombatEncounterConfig>
make_combat_encounter_config(
    checkpoint::EntrySide entry,
    std::uint32_t rules_version,
    const combat::EncounterWave& wave,
    bool reset_player_health,
    combat::PlayerCombatBuild player_build = {}) noexcept;

}  // namespace arpg::dungeon
