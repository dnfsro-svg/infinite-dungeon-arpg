#include "test_framework.hpp"

#include "passives/passive_tree_rules.hpp"

#include <array>
#include <cstddef>
#include <cstring>

namespace {

using namespace arpg::passives;
using arpg::progression::ProgressionState;

arpg::test::Failure allocation_requires_adjacency_and_refund_keeps_connectivity() noexcept {
    ProgressionState progress{10U, 0U, 9U, 9U};
    PassiveTreeState state{};
    ARPG_REQUIRE(!allocate_node(state, progress, 21U).changed);
    ARPG_REQUIRE(allocate_node(state, progress, 8U).changed);
    ARPG_REQUIRE(allocate_node(state, progress, 9U).changed);
    ARPG_REQUIRE(allocate_node(state, progress, 10U).changed);
    ARPG_REQUIRE(!refund_node(state, progress, 8U).changed);
    ARPG_REQUIRE(refund_node(state, progress, 10U).changed);
    ARPG_REQUIRE(progress.unspent_passive_points == 7U);
    return {};
}

arpg::test::Failure duplicate_no_points_and_unknown_nodes_are_rejected() noexcept {
    ProgressionState progress{1U, 0U, 1U, 1U};
    PassiveTreeState state{};
    ARPG_REQUIRE(allocate_node(state, progress, 8U).changed);
    ARPG_REQUIRE(allocate_node(state, progress, 8U).error == PassiveTreeError::already_allocated);
    ARPG_REQUIRE(allocate_node(state, progress, 9U).error == PassiveTreeError::no_points);
    ARPG_REQUIRE(allocate_node(state, progress, 64U).error == PassiveTreeError::unknown_node);
    return {};
}

arpg::test::Failure invalid_state_rejects_disconnected_bits() noexcept {
    ProgressionState progress{10U, 0U, 9U, 0U};
    PassiveTreeState state{};
    state.allocated_bits = (std::uint64_t{1U} << 0U) | (std::uint64_t{1U} << 10U);
    ARPG_REQUIRE(!valid_passive_tree_state(state, progress));
    ARPG_REQUIRE(allocate_node(state, progress, 9U).error == PassiveTreeError::invalid_state);
    return {};
}

arpg::test::Failure point_conservation_is_checked() noexcept {
    ProgressionState progress{10U, 0U, 2U, 0U};
    PassiveTreeState state{};
    ARPG_REQUIRE(!valid_passive_tree_state(state, progress));
    progress.unspent_passive_points = 2U;
    ARPG_REQUIRE(valid_passive_tree_state(state, progress));
    ARPG_REQUIRE(allocate_node(state, progress, 8U).changed);
    ARPG_REQUIRE(progress.unspent_passive_points == 1U);
    ARPG_REQUIRE(valid_passive_tree_state(state, progress));
    return {};
}

arpg::test::Failure projection_is_deterministic_for_same_bitmap() noexcept {
    PassiveTreeState state{};
    ProgressionState progress{10U, 0U, 3U, 3U};
    ARPG_REQUIRE(allocate_node(state, progress, 8U).changed);
    ARPG_REQUIRE(allocate_node(state, progress, 9U).changed);
    ARPG_REQUIRE(allocate_node(state, progress, 10U).changed);
    const auto first = evaluate_passive_tree(state);
    const auto second = evaluate_passive_tree(state);
    ARPG_REQUIRE(first.valid && second.valid);
    ARPG_REQUIRE(first.flat_damage == second.flat_damage);
    ARPG_REQUIRE(first.damage_increased == second.damage_increased);
    ARPG_REQUIRE(first.melee_damage == second.melee_damage);
    ARPG_REQUIRE(first.flat_damage[arpg::modifiers::damage_index(
        arpg::modifiers::DamageType::fire)] == 3 * arpg::modifiers::kFixedOne);
    ARPG_REQUIRE(first.melee_damage == arpg::modifiers::kFixedOne);

    const auto unallocated = evaluate_passive_tree(PassiveTreeState{});
    ARPG_REQUIRE(unallocated.max_health_more == arpg::modifiers::kFixedOne);
    PassiveTreeState stormstep{};
    for (PassiveNodeId id = 36U; id <= 49U; ++id)
        stormstep.allocated_bits |= std::uint64_t{1U} << id;
    const auto storm_values = evaluate_passive_tree(stormstep);
    ARPG_REQUIRE(storm_values.max_health_more == 8000);
    PassiveTreeState vitality{};
    vitality.allocated_bits |= std::uint64_t{1U} << 1U;
    const auto vitality_values = evaluate_passive_tree(vitality);
    ARPG_REQUIRE(vitality_values.max_health == 20 * arpg::modifiers::kFixedOne);
    ARPG_REQUIRE(vitality_values.max_health_more == arpg::modifiers::kFixedOne);
    return {};
}

arpg::test::Failure raw_modifiers_append_after_existing_prefix() noexcept {
    PassiveTreeState state{};
    state.allocated_bits |= std::uint64_t{1U} << 1U;
    std::array<arpg::modifiers::Modifier, 4> output{};
    output[0] = {42U, arpg::modifiers::StatId::armor,
        arpg::modifiers::ModifierOperation::flat, 123U};
    const arpg::modifiers::Modifier prefix = output[0];
    std::size_t count = 1U;
    ARPG_REQUIRE(append_passive_modifiers(
        state, output.data(), output.size(), count));
    ARPG_REQUIRE(count == 2U);
    ARPG_REQUIRE(std::memcmp(&output[0], &prefix, sizeof(prefix)) == 0);
    ARPG_REQUIRE(output[1].id == 1003U);
    ARPG_REQUIRE(output[1].stat == arpg::modifiers::StatId::max_health);
    ARPG_REQUIRE(output[1].value == 20 * arpg::modifiers::kFixedOne);
    const auto values = arpg::modifiers::evaluate_player_modifiers(
        {output.data() + 1U, count - 1U});
    const auto compatibility = evaluate_passive_tree(state);
    ARPG_REQUIRE(values.valid && compatibility.valid);
    ARPG_REQUIRE(values.max_health == compatibility.max_health);
    return {};
}

arpg::test::Failure append_failure_is_transactional() noexcept {
    PassiveTreeState state{};
    state.allocated_bits |= std::uint64_t{1U} << 1U;
    std::array<arpg::modifiers::Modifier, 2> output{{
        {51U, arpg::modifiers::StatId::armor,
            arpg::modifiers::ModifierOperation::flat, 1U},
        {52U, arpg::modifiers::StatId::evasion,
            arpg::modifiers::ModifierOperation::flat, 2U},
    }};
    const auto before = output;
    std::size_t count = output.size();
    ARPG_REQUIRE(!append_passive_modifiers(
        state, output.data(), output.size(), count));
    ARPG_REQUIRE(count == output.size());
    ARPG_REQUIRE(std::memcmp(output.data(), before.data(),
        sizeof(output)) == 0);

    count = 1U;
    ARPG_REQUIRE(!append_passive_modifiers(state, nullptr, 2U, count));
    ARPG_REQUIRE(count == 1U);
    PassiveTreeState invalid{};
    invalid.allocated_bits = std::uint64_t{1U} << 1U;
    ARPG_REQUIRE(!append_passive_modifiers(
        invalid, output.data(), output.size(), count));
    ARPG_REQUIRE(count == 1U);
    ARPG_REQUIRE(std::memcmp(output.data(), before.data(),
        sizeof(output)) == 0);
    return {};
}

arpg::test::Failure null_zero_capacity_accepts_empty_tree_output() noexcept {
    PassiveTreeState state{};
    std::size_t count = 0U;
    ARPG_REQUIRE(append_passive_modifiers(state, nullptr, 0U, count));
    ARPG_REQUIRE(count == 0U);
    count = 1U;
    ARPG_REQUIRE(!append_passive_modifiers(state, nullptr, 0U, count));
    ARPG_REQUIRE(count == 1U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"allocation requires adjacency and connected refund", &allocation_requires_adjacency_and_refund_keeps_connectivity},
    {"duplicate no-points and unknown nodes are rejected", &duplicate_no_points_and_unknown_nodes_are_rejected},
    {"invalid state rejects disconnected bits", &invalid_state_rejects_disconnected_bits},
    {"point conservation is checked", &point_conservation_is_checked},
    {"projection is deterministic", &projection_is_deterministic_for_same_bitmap},
    {"raw modifier prefix append", &raw_modifiers_append_after_existing_prefix},
    {"transactional append failure", &append_failure_is_transactional},
    {"null zero-capacity empty append",
        &null_zero_capacity_accepts_empty_tree_output},
};

}  // namespace

arpg::test::TestSuite passive_tree_rules_suite() noexcept {
    return arpg::test::make_suite("passive_tree_rules", kCases);
}
