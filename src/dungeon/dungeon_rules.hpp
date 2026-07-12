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
};

struct DungeonRules final {
    std::array<std::uint32_t, 4> base_weights{{100, 100, 100, 100}};
    std::uint32_t bias_weight_increment{25};
    std::uint32_t hole_threshold{1000};
    std::uint32_t abyss_threshold{100};
    std::uint32_t rules_version{1};
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

}  // namespace arpg::dungeon
