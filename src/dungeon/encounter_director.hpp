#pragma once

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_rules.hpp"

#include <array>
#include <cstdint>

namespace arpg::dungeon {

struct RoomEncounterPlan final {
    std::array<combat::EncounterWave, combat::kEncounterWaveCapacity> waves{};
    std::uint8_t wave_count{};
    std::uint8_t total_budget{};
};

struct EncounterPlanResult final {
    DungeonFault fault{DungeonFault::none};
    RoomEncounterPlan plan{};
};

[[nodiscard]] std::uint8_t encounter_budget(
    std::uint64_t depth,
    const EncounterDirectorConfig& config) noexcept;

[[nodiscard]] bool encounter_plan_legal(
    const RoomEncounterPlan& plan,
    const EncounterDirectorConfig& config) noexcept;

[[nodiscard]] EncounterPlanResult build_encounter_plan(
    std::uint64_t room_seed,
    std::uint64_t depth,
    checkpoint::DungeonElement ecology,
    const EncounterDirectorConfig& config) noexcept;

}  // namespace arpg::dungeon
