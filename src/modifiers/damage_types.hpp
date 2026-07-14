#pragma once

#include <cstddef>
#include <cstdint>

namespace arpg::modifiers {

enum class DamageType : std::uint8_t {
    physical,
    fire,
    water,
    lightning,
    chaos,
    count,
};

inline constexpr std::size_t kDamageTypeCount =
    static_cast<std::size_t>(DamageType::count);
inline constexpr std::size_t kElementCount = 4U;

[[nodiscard]] constexpr std::size_t damage_index(DamageType type) noexcept {
    return static_cast<std::size_t>(type);
}

[[nodiscard]] constexpr bool is_elemental(DamageType type) noexcept {
    return type != DamageType::physical && type != DamageType::count;
}

[[nodiscard]] constexpr std::size_t element_index(DamageType type) noexcept {
    return static_cast<std::size_t>(type) - 1U;
}

}  // namespace arpg::modifiers

