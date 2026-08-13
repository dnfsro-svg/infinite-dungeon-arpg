#pragma once

#include "combat/monster_affix_runtime.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace arpg::combat {

enum class BasisPointRounding : std::uint8_t {
    floor,
    ceil,
};

inline int scale_basis_points(
    int value,
    std::uint32_t basis_points,
    BasisPointRounding rounding = BasisPointRounding::floor) noexcept {
    if (value <= 0 || basis_points == 0U) return 0;
    const std::uint64_t product = static_cast<std::uint64_t>(value)
        * static_cast<std::uint64_t>(basis_points);
    const std::uint64_t scaled = rounding == BasisPointRounding::ceil
        ? (product + 9999U) / 10000U
        : product / 10000U;
    return static_cast<int>(std::min<std::uint64_t>(
        scaled,
        static_cast<std::uint64_t>((std::numeric_limits<int>::max)())));
}

inline float scale_basis_points(float value, std::uint32_t basis_points) noexcept {
    return value * static_cast<float>(basis_points) / 10000.0F;
}

inline std::uint32_t compose_damage_basis_points(
    std::uint32_t left,
    std::uint32_t right) noexcept {
    const std::uint64_t product = static_cast<std::uint64_t>(left)
        * static_cast<std::uint64_t>(right);
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        product / 10000U,
        (std::numeric_limits<std::uint32_t>::max)()));
}

inline std::uint16_t scale_ticks_ratio(
    std::uint16_t base,
    std::uint32_t numerator,
    std::uint32_t denominator = 10000U) noexcept {
    if (base == 0U) return 0U;
    if (numerator == 0U || denominator == 0U) return 1U;
    const std::uint64_t product = static_cast<std::uint64_t>(base)
        * static_cast<std::uint64_t>(numerator);
    const std::uint64_t scaled = (product + denominator - 1U) / denominator;
    return static_cast<std::uint16_t>(std::clamp<std::uint64_t>(
        scaled,
        1U,
        (std::numeric_limits<std::uint16_t>::max)()));
}

inline DamagePacket scale_monster_affix_damage(
    DamagePacket packet,
    const MonsterAffixProfile& profile) noexcept {
    for (int& amount : packet.amount) {
        amount = scaled_monster_damage(amount, profile);
    }
    return packet;
}

inline DamagePacket scale_monster_outgoing_damage(
    DamagePacket packet,
    std::uint32_t basis_points) noexcept {
    for (int& amount : packet.amount) {
        amount = scale_basis_points(amount, basis_points);
    }
    return packet;
}

}  // namespace arpg::combat
