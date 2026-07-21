#pragma once

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_rules.hpp"

#include <array>
#include <cstdint>

namespace arpg::dungeon {

struct RoomEncounterPlan final {
    std::array<combat::EncounterWave, combat::kEncounterWaveCapacity> waves{};
    std::uint8_t wave_count{};
    std::uint8_t initial_monster_count{};
    std::uint8_t total_budget{};
};

struct EncounterBuildRequest final {
    std::uint64_t room_seed{};
    std::uint64_t depth{1U};
    checkpoint::DungeonElement ecology{checkpoint::DungeonElement::fire};
    checkpoint::EntrySide entry{checkpoint::EntrySide::initial};
    bool has_hole{};
    std::uint8_t target_monster_count{};
};

struct EncounterPlanResult final {
    DungeonFault fault{DungeonFault::none};
    RoomEncounterPlan plan{};
};

[[nodiscard]] bool encounter_plan_legal(
    const RoomEncounterPlan& plan,
    const EncounterBuildRequest& request,
    const EncounterDirectorConfig& config) noexcept;

[[nodiscard]] EncounterPlanResult build_encounter_plan(
    const EncounterBuildRequest& request,
    const EncounterDirectorConfig& config) noexcept;

[[nodiscard]] EncounterPlanResult build_abyss_encounter_plan(
    const EncounterBuildRequest& request,
    const EncounterDirectorConfig& config) noexcept;

}  // namespace arpg::dungeon
