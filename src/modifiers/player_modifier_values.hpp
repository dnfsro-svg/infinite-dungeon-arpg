#pragma once

#include "modifiers/damage_types.hpp"
#include "modifiers/modifier_math.hpp"

#include <array>

namespace arpg::modifiers {

struct PlayerModifierValues final {
    std::array<FixedValue, kDamageTypeCount> flat_damage{};
    std::array<FixedValue, kDamageTypeCount> damage_increased{{
        kFixedOne, kFixedOne, kFixedOne, kFixedOne, kFixedOne}};
    std::array<FixedValue, kElementCount> resistance{};
    FixedValue melee_damage{kFixedOne};
    FixedValue max_health{};
    FixedValue max_health_more{kFixedOne};
    FixedValue max_barrier{};
    FixedValue damage_taken{kFixedOne};
    FixedValue movement_speed{kFixedOne};
    FixedValue attack_speed{kFixedOne};
    FixedValue impulse_scale{kFixedOne};
    FixedValue jump_speed{kFixedOne};
    FixedValue air_control{kFixedOne};
    bool valid{true};
};

[[nodiscard]] PlayerModifierValues evaluate_player_modifiers(
    ModifierSpan modifiers) noexcept;

}  // namespace arpg::modifiers
