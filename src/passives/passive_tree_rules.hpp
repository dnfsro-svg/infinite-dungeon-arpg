#pragma once

#include "modifiers/player_modifier_values.hpp"
#include "passives/passive_tree_catalog.hpp"

#include <cstddef>

namespace arpg::passives {

[[nodiscard]] bool valid_passive_tree_state(
    const PassiveTreeState& state,
    const progression::ProgressionState& progression) noexcept;

[[nodiscard]] PassiveTreeResult allocate_node(
    PassiveTreeState& state,
    progression::ProgressionState& progression,
    PassiveNodeId node) noexcept;

[[nodiscard]] PassiveTreeResult refund_node(
    PassiveTreeState& state,
    progression::ProgressionState& progression,
    PassiveNodeId node) noexcept;

[[nodiscard]] modifiers::PlayerModifierValues evaluate_passive_tree(
    const PassiveTreeState& state) noexcept;

[[nodiscard]] bool append_passive_modifiers(
    const PassiveTreeState& tree,
    modifiers::Modifier* output,
    std::size_t capacity,
    std::size_t& count) noexcept;

}  // namespace arpg::passives
