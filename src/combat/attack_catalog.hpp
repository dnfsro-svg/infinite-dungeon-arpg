#pragma once

#include "combat/combat_types.hpp"

#include <cstddef>
#include <cstdint>

namespace arpg::combat {

inline constexpr std::size_t kAttackCount = 5;

[[nodiscard]] const AttackDefinition* find_attack_definition(
    AttackId id) noexcept;
[[nodiscard]] AttackPhase attack_phase_at(
    const AttackDefinition& definition,
    std::uint32_t elapsed_ticks) noexcept;
[[nodiscard]] bool validate_attack_catalog() noexcept;

}  // namespace arpg::combat
