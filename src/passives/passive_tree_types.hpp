#pragma once

#include "modifiers/modifier_types.hpp"
#include "progression/progression_types.hpp"

#include <array>
#include <cstdint>

namespace arpg::passives {

inline constexpr std::size_t kPassiveNodeCount = 64U;
inline constexpr std::size_t kPassiveNeighborCapacity = 12U;
inline constexpr std::size_t kPassiveModifierCapacity = 4U;

using PassiveNodeId = std::uint8_t;

struct PassiveTreeState final {
    std::uint64_t allocated_bits{1U};
};

enum class PassiveNodeType : std::uint8_t {
    start,
    small,
    notable,
    keystone,
    connector,
};

enum class PassiveTreeError : std::uint8_t {
    none,
    unknown_node,
    already_allocated,
    not_allocated,
    no_points,
    not_adjacent,
    disconnects_tree,
    invalid_state,
};

struct PassiveTreeResult final {
    PassiveTreeState state{};
    progression::ProgressionState progression{};
    PassiveTreeError error{PassiveTreeError::none};
    bool changed{};
};

struct PassiveNode final {
    PassiveNodeId id{};
    PassiveNodeType type{};
    const char* name{};
    std::int16_t x{};
    std::int16_t y{};
    std::array<PassiveNodeId, kPassiveNeighborCapacity> neighbors{};
    std::uint8_t neighbor_count{};
    std::array<modifiers::Modifier, kPassiveModifierCapacity> modifiers{};
    std::uint8_t modifier_count{};
};

}  // namespace arpg::passives
