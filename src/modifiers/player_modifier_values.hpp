#pragma once

#include "modifiers/damage_types.hpp"
#include "modifiers/modifier_math.hpp"

#include <array>
#include <cstdint>

namespace arpg::modifiers {

struct PlayerModifierValues final {
    std::array<std::int64_t, 5> flat_damage{};
    std::array<std::int32_t, 5> damage_increased{{
        kFixedOne, kFixedOne, kFixedOne, kFixedOne, kFixedOne}};
    std::array<std::int32_t, 4> damage_reduction{};
    std::array<std::int32_t, 4> damage_reduction_cap_bonus{};
    std::int64_t armor{};
    std::int64_t evasion{};
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
[[nodiscard]] std::int32_t rating_to_basis_points(
    std::int64_t value) noexcept;

}  // namespace arpg::modifiers
