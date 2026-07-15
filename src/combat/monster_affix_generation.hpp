#pragma once

#include "combat/monster_affix_types.hpp"
#include "combat/combat_types.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace arpg::combat {

[[nodiscard]] std::array<std::uint16_t, 4> affix_count_weights(
    std::uint64_t depth) noexcept;
[[nodiscard]] std::array<std::uint16_t, 3> affix_tier_weights(
    std::uint64_t depth) noexcept;
[[nodiscard]] std::optional<MonsterAffixSet> generate_monster_affixes(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index,
    const MonsterDefinition& monster) noexcept;
[[nodiscard]] std::uint16_t monster_affix_danger_score(
    const MonsterAffixSet& set) noexcept;

}  // namespace arpg::combat
