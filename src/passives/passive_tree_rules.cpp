#include "passives/passive_tree_rules.hpp"

#include "modifiers/modifier_math.hpp"

#include <array>

namespace arpg::passives {
namespace {

bool allocated(PassiveTreeState state, PassiveNodeId node) noexcept {
    return (state.allocated_bits & (std::uint64_t{1U} << node)) != 0U;
}

std::size_t count_bits(std::uint64_t value) noexcept {
    std::size_t count = 0U;
    while (value != 0U) { value &= value - 1U; ++count; }
    return count;
}

bool connected(PassiveTreeState state) noexcept {
    const auto& nodes = passive_nodes();
    std::array<bool, kPassiveNodeCount> visited{};
    std::array<PassiveNodeId, kPassiveNodeCount> queue{};
    std::size_t head = 0U;
    std::size_t tail = 0U;
    queue[tail++] = 0U;
    visited[0] = true;
    while (head < tail) {
        const auto node = queue[head++];
        for (std::size_t index = 0; index < nodes[node].neighbor_count; ++index) {
            const auto neighbor = nodes[node].neighbors[index];
            if (allocated(state, neighbor) && !visited[neighbor]) {
                visited[neighbor] = true;
                queue[tail++] = neighbor;
            }
        }
    }
    for (PassiveNodeId node = 0U; node < kPassiveNodeCount; ++node)
        if (allocated(state, node) && !visited[node]) return false;
    return true;
}

PassiveTreeResult result_snapshot(const PassiveTreeState& state,
    const progression::ProgressionState& progression,
    PassiveTreeError error, bool changed) noexcept {
    return {state, progression, error, changed};
}

bool adjacent_to_allocated(const PassiveTreeState& state,
    const PassiveNode& node) noexcept {
    for (std::size_t index = 0; index < node.neighbor_count; ++index)
        if (allocated(state, node.neighbors[index])) return true;
    return false;
}

}  // namespace

bool valid_passive_tree_state(const PassiveTreeState& state,
    const progression::ProgressionState& progression) noexcept {
    if ((state.allocated_bits & 1U) == 0U) return false;
    if (!catalog_is_valid() || !connected(state)) return false;
    const std::size_t spent = count_bits(state.allocated_bits & ~std::uint64_t{1U});
    return spent + progression.unspent_passive_points
        == progression.earned_passive_points;
}

PassiveTreeResult allocate_node(PassiveTreeState& state,
    progression::ProgressionState& progression, PassiveNodeId node) noexcept {
    if (node >= kPassiveNodeCount)
        return result_snapshot(state, progression, PassiveTreeError::unknown_node, false);
    if (!valid_passive_tree_state(state, progression))
        return result_snapshot(state, progression, PassiveTreeError::invalid_state, false);
    const auto& definition = passive_nodes()[node];
    if (allocated(state, node))
        return result_snapshot(state, progression, PassiveTreeError::already_allocated, false);
    if (progression.unspent_passive_points == 0U)
        return result_snapshot(state, progression, PassiveTreeError::no_points, false);
    if (!adjacent_to_allocated(state, definition))
        return result_snapshot(state, progression, PassiveTreeError::not_adjacent, false);
    state.allocated_bits |= std::uint64_t{1U} << node;
    --progression.unspent_passive_points;
    return result_snapshot(state, progression, PassiveTreeError::none, true);
}

PassiveTreeResult refund_node(PassiveTreeState& state,
    progression::ProgressionState& progression, PassiveNodeId node) noexcept {
    if (node >= kPassiveNodeCount)
        return result_snapshot(state, progression, PassiveTreeError::unknown_node, false);
    if (!valid_passive_tree_state(state, progression))
        return result_snapshot(state, progression, PassiveTreeError::invalid_state, false);
    if (node == 0U || !allocated(state, node))
        return result_snapshot(state, progression, PassiveTreeError::not_allocated, false);
    state.allocated_bits &= ~(std::uint64_t{1U} << node);
    if (!connected(state)) {
        state.allocated_bits |= std::uint64_t{1U} << node;
        return result_snapshot(state, progression, PassiveTreeError::disconnects_tree, false);
    }
    ++progression.unspent_passive_points;
    return result_snapshot(state, progression, PassiveTreeError::none, true);
}

modifiers::PlayerModifierValues evaluate_passive_tree(
    const PassiveTreeState& state) noexcept {
    if ((state.allocated_bits & 1U) == 0U || !connected(state)) {
        modifiers::PlayerModifierValues invalid{};
        invalid.valid = false;
        return invalid;
    }
    std::array<modifiers::Modifier, 128U> modifiers{};
    std::size_t count = 0U;
    for (const auto& node : passive_nodes()) {
        if (!allocated(state, node.id)) continue;
        for (std::size_t index = 0; index < node.modifier_count; ++index) {
            if (count >= modifiers.size()) {
                modifiers::PlayerModifierValues invalid{};
                invalid.valid = false;
                return invalid;
            }
            modifiers[count++] = node.modifiers[index];
        }
    }
    const auto result = modifiers::evaluate_player_modifiers(
        modifiers::ModifierSpan{modifiers.data(), count});
    return result;
}

}  // namespace arpg::passives
