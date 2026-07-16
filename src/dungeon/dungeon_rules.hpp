#pragma once

#include "dungeon/dungeon_checkpoint.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace arpg::dungeon {

inline constexpr std::uint32_t kProbabilityScale = 10000U;

enum class DungeonFault : std::uint8_t {
    none,
    invalid_rules,
    invalid_direction,
    commit_generation_overflow,
    room_index_overflow,
    depth_overflow,
    floor_room_overflow,
    bias_overflow,
    weight_overflow,
    event_overflow,
    combat_relay_overflow,
    save_commit_indeterminate,
    save_receipt_mismatch,
    item_id_collision,
    item_sequence_overflow,
    invalid_item_state,
    invalid_abyss_state,
    abyss_generation_failed,
    abyss_reward_collision,
    abyss_reward_revision_overflow,
};

// Gray-box encounter tuning is intentionally centralized here so later
// balancing changes do not alter the director's algorithm or RNG contract.
struct EncounterDirectorConfig final {
    std::uint8_t base_budget{8};
    std::uint8_t depth_step{5};
    std::uint8_t budget_per_step{1};
    std::uint8_t max_budget{24};
    std::uint8_t two_wave_threshold{12};
    std::uint8_t matching_ecology_weight{4};
    std::uint8_t off_ecology_weight{1};
    std::uint8_t normal_high_priority_limit{1};
    std::uint8_t high_budget_priority_limit{2};
    std::uint8_t ranged_limit{4};
    std::uint8_t support_limit{2};
    std::uint8_t ground_hazard_limit{3};
};

struct DungeonRules final {
    std::array<std::uint32_t, 4> base_weights{{100, 100, 100, 100}};
    std::uint32_t bias_weight_increment{25};
    std::uint32_t hole_threshold{1000};
    std::uint32_t abyss_threshold{100};
    std::uint32_t rules_version{1};
    EncounterDirectorConfig encounter{};
};

[[nodiscard]] std::optional<checkpoint::DungeonElement> element_for_exit(
    checkpoint::ExitDirection direction) noexcept;
[[nodiscard]] DungeonFault validate_rules(
    const DungeonRules& rules) noexcept;
[[nodiscard]] DungeonFault compute_ecology_weights(
    const DungeonRules& rules,
    const std::array<std::uint32_t, 4>& biases,
    std::array<std::uint64_t, 4>& weights,
    std::uint64_t& total) noexcept;
[[nodiscard]] DungeonFault validate_encounter_director_config(
    const EncounterDirectorConfig& config) noexcept;

}  // namespace arpg::dungeon
