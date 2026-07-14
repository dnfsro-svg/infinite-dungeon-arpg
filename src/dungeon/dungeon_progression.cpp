#include "dungeon/dungeon_progression.hpp"

#include <cstdint>
#include <limits>

namespace arpg::dungeon {
namespace {

[[nodiscard]] RunStateBuildResult unchanged(
    DungeonFault fault,
    const checkpoint::DungeonRunState& state) noexcept {
    RunStateBuildResult result;
    result.fault = fault;
    result.state = state;
    return result;
}

[[nodiscard]] bool checked_increment(
    std::uint64_t value,
    std::uint64_t& next) noexcept {
    if (value == (std::numeric_limits<std::uint64_t>::max)()) {
        return false;
    }
    next = value + 1U;
    return true;
}

[[nodiscard]] bool checked_increment(
    std::uint32_t value,
    std::uint32_t& next) noexcept {
    if (value == (std::numeric_limits<std::uint32_t>::max)()) {
        return false;
    }
    next = value + 1U;
    return true;
}

}  // namespace

RunStateBuildResult make_initial_run_state(
    std::uint64_t root_seed,
    const DungeonRules& rules) noexcept {
    const std::array<std::uint32_t, 4> biases{};
    const auto generated = generate_room_descriptor(
        derive_initial_room_seed(root_seed, 0U),
        0U,
        1U,
        1U,
        checkpoint::EntrySide::initial,
        biases,
        rules);
    if (generated.fault != DungeonFault::none) {
        return {generated.fault, {}, {}};
    }

    checkpoint::DungeonRunState state;
    state.root_seed = root_seed;
    state.commit_generation = 1U;
    state.biases = biases;
    state.current_room = generated.room;
    state.last_transition = checkpoint::TransitionKind::none;
    state.last_direction = checkpoint::ExitDirection::none;
    return {DungeonFault::none, state, generated.samples};
}

RunStateBuildResult make_door_transition(
    const checkpoint::DungeonRunState& current,
    checkpoint::ExitDirection direction,
    const DungeonRules& rules) noexcept {
    const auto element = element_for_exit(direction);
    if (!element.has_value()) {
        return unchanged(DungeonFault::invalid_direction, current);
    }

    checkpoint::DungeonRunState next = current;
    if (!checked_increment(
            current.commit_generation, next.commit_generation)) {
        return unchanged(DungeonFault::commit_generation_overflow, current);
    }
    if (!checked_increment(
            current.current_room.index, next.current_room.index)) {
        return unchanged(DungeonFault::room_index_overflow, current);
    }
    if (!checked_increment(
            current.current_room.floor_room_index,
            next.current_room.floor_room_index)) {
        return unchanged(DungeonFault::floor_room_overflow, current);
    }
    const auto element_index = static_cast<std::size_t>(element.value());
    if (!checked_increment(
            current.biases[element_index], next.biases[element_index])) {
        return unchanged(DungeonFault::bias_overflow, current);
    }

    next.current_room.seed = derive_door_room_seed(
        current.current_room.seed,
        next.current_room.index,
        direction);
    next.current_room.entry = entry_side_for_exit(direction);
    const auto generated = generate_room_descriptor(
        next.current_room.seed,
        next.current_room.index,
        next.current_room.depth,
        next.current_room.floor_room_index,
        next.current_room.entry,
        next.biases,
        rules);
    if (generated.fault != DungeonFault::none) {
        return unchanged(generated.fault, current);
    }

    next.current_room = generated.room;
    next.last_transition = checkpoint::TransitionKind::door;
    next.last_direction = direction;
    return {DungeonFault::none, next, generated.samples};
}

RunStateBuildResult make_descent_transition(
    const checkpoint::DungeonRunState& current,
    const DungeonRules& rules) noexcept {
    checkpoint::DungeonRunState next = current;
    if (!checked_increment(
            current.commit_generation, next.commit_generation)) {
        return unchanged(DungeonFault::commit_generation_overflow, current);
    }
    if (!checked_increment(
            current.current_room.index, next.current_room.index)) {
        return unchanged(DungeonFault::room_index_overflow, current);
    }
    if (!checked_increment(
            current.current_room.depth, next.current_room.depth)) {
        return unchanged(DungeonFault::depth_overflow, current);
    }

    next.biases = {};
    next.current_room.seed = derive_descent_room_seed(
        current.current_room.seed,
        next.current_room.index);
    next.current_room.floor_room_index = 1U;
    next.current_room.entry = checkpoint::EntrySide::initial;
    const auto generated = generate_room_descriptor(
        next.current_room.seed,
        next.current_room.index,
        next.current_room.depth,
        next.current_room.floor_room_index,
        next.current_room.entry,
        next.biases,
        rules);
    if (generated.fault != DungeonFault::none) {
        return unchanged(generated.fault, current);
    }

    next.current_room = generated.room;
    next.last_transition = checkpoint::TransitionKind::descent;
    next.last_direction = checkpoint::ExitDirection::none;
    return {DungeonFault::none, next, generated.samples};
}

bool same_run_state(
    const checkpoint::DungeonRunState& lhs,
    const checkpoint::DungeonRunState& rhs) noexcept {
    return lhs.root_seed == rhs.root_seed
        && lhs.commit_generation == rhs.commit_generation
        && lhs.biases == rhs.biases
        && lhs.current_room.index == rhs.current_room.index
        && lhs.current_room.seed == rhs.current_room.seed
        && lhs.current_room.depth == rhs.current_room.depth
        && lhs.current_room.floor_room_index == rhs.current_room.floor_room_index
        && lhs.current_room.entry == rhs.current_room.entry
        && lhs.current_room.ecology == rhs.current_room.ecology
        && lhs.current_room.has_hole == rhs.current_room.has_hole
        && lhs.current_room.is_abyss == rhs.current_room.is_abyss
        && lhs.progression.level == rhs.progression.level
        && lhs.progression.experience == rhs.progression.experience
        && lhs.progression.earned_passive_points
            == rhs.progression.earned_passive_points
        && lhs.progression.unspent_passive_points
            == rhs.progression.unspent_passive_points
        && lhs.passive_tree.allocated_bits == rhs.passive_tree.allocated_bits
        && lhs.last_transition == rhs.last_transition
        && lhs.last_direction == rhs.last_direction;
}

}  // namespace arpg::dungeon
