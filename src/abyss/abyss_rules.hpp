#pragma once

#include "abyss/abyss_types.hpp"

#include <cstdint>
#include <optional>

namespace arpg::abyss {

[[nodiscard]] bool is_abyss_roll(std::uint64_t room_seed) noexcept;
[[nodiscard]] std::optional<AbyssSelection> select_abyss_rule(
    std::uint64_t room_seed,
    std::uint64_t depth) noexcept;
[[nodiscard]] std::optional<AbyssDanger> danger_for_rule(
    AbyssRuleId rule) noexcept;
[[nodiscard]] std::uint8_t abyss_encounter_budget(
    std::uint8_t normal_budget) noexcept;
[[nodiscard]] std::uint8_t minimum_abyss_affixes(
    std::uint64_t depth) noexcept;
[[nodiscard]] std::optional<int> map_resource_ratio(
    int current,
    int old_max,
    int new_max,
    bool alive) noexcept;
[[nodiscard]] std::optional<int> percent_of_actual_max_hp(
    int actual_max_hp,
    std::uint16_t basis_points) noexcept;
[[nodiscard]] AbyssCombatConfig combat_config_for(
    AbyssRuleId rule) noexcept;

}  // namespace arpg::abyss
