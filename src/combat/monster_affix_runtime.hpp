#pragma once

#include "combat/combat_types.hpp"

#include <cstdint>

namespace arpg::combat {

struct MonsterAffixProfile final {
    int max_hp{};
    int armor_rating{};
    int max_shield{};
    std::uint16_t shield_recharge_delay_ticks{};
    std::int32_t damage_bp{10000};
    std::int32_t attack_timing_bp{10000};
    std::int32_t move_bp{10000};
    std::int32_t cooldown_bp{10000};
    std::int32_t horizontal_impulse_bp{10000};
};

[[nodiscard]] MonsterAffixProfile evaluate_monster_affixes(
    const MonsterDefinition& monster,
    const MonsterAffixSet& affixes) noexcept;

}  // namespace arpg::combat
