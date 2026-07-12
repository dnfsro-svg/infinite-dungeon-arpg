#include "dungeon/room_generation.hpp"

#include "core/deterministic_rng.hpp"
#include "dungeon/room_combat_template.hpp"

#include <array>
#include <cstdint>
#include <limits>

namespace arpg::dungeon {
namespace {

constexpr std::uint64_t kInitialRoomDomain =
    0x524F4F4D5F494E49ULL;
constexpr std::uint64_t kDoorRoomDomain =
    0x524F4F4D5F4E4558ULL;
constexpr std::uint64_t kDescentRoomDomain =
    0x44455343454E5431ULL;
constexpr std::uint64_t kEcologyDomain =
    0x45434F4C4F475931ULL;
constexpr std::uint64_t kHoleDomain =
    0x484F4C455F563031ULL;
constexpr std::uint64_t kAbyssDomain =
    0x41425953535F3031ULL;

[[nodiscard]] std::uint64_t first(
    std::uint64_t seed,
    std::uint64_t domain) noexcept {
    auto stream = core::DeterministicRng::derive_stream(seed, domain);
    return stream.next_u64();
}

}  // namespace

EntrySide entry_side_for_exit(ExitDirection exit) noexcept {
    switch (exit) {
    case ExitDirection::up:
        return EntrySide::bottom;
    case ExitDirection::down:
        return EntrySide::top;
    case ExitDirection::left:
        return EntrySide::right;
    case ExitDirection::right:
        return EntrySide::left;
    case ExitDirection::none:
        return EntrySide::initial;
    }
    return EntrySide::initial;
}

std::uint64_t derive_initial_room_seed(
    std::uint64_t root_seed,
    std::uint64_t room_index) noexcept {
    return first(first(root_seed, kInitialRoomDomain), room_index);
}

std::uint64_t derive_door_room_seed(
    std::uint64_t current_seed,
    std::uint64_t next_serial,
    ExitDirection direction) noexcept {
    return first(
        first(first(current_seed, kDoorRoomDomain), next_serial),
        static_cast<std::uint64_t>(direction));
}

std::uint64_t derive_descent_room_seed(
    std::uint64_t current_seed,
    std::uint64_t next_serial) noexcept {
    return first(
        first(first(current_seed, kDescentRoomDomain), next_serial),
        static_cast<std::uint64_t>(ExitDirection::none));
}

RoomGenerationResult generate_room_descriptor(
    std::uint64_t seed,
    std::uint64_t global_index,
    std::uint64_t depth,
    std::uint64_t floor_room_index,
    checkpoint::EntrySide entry,
    const std::array<std::uint32_t, 4>& biases,
    const DungeonRules& rules) noexcept {
    std::array<std::uint64_t, 4> weights{};
    std::uint64_t total = 0U;
    const DungeonFault weight_fault = compute_ecology_weights(
        rules, biases, weights, total);
    if (weight_fault != DungeonFault::none) {
        return {weight_fault, {}, {}};
    }

    auto ecology_stream = core::DeterministicRng::derive_stream(
        seed, kEcologyDomain);
    const auto ecology_value = ecology_stream.next_bounded(total);
    auto hole_stream = core::DeterministicRng::derive_stream(
        seed, kHoleDomain);
    const auto hole_value = hole_stream.next_bounded(kProbabilityScale);
    auto abyss_stream = core::DeterministicRng::derive_stream(
        seed, kAbyssDomain);
    const auto abyss_value = abyss_stream.next_bounded(kProbabilityScale);
    if (!ecology_value.has_value()
            || !hole_value.has_value()
            || !abyss_value.has_value()) {
        return {DungeonFault::invalid_rules, {}, {}};
    }

    checkpoint::DungeonElement ecology = checkpoint::DungeonElement::fire;
    std::uint64_t remaining = ecology_value.value();
    for (std::size_t index = 0; index < weights.size(); ++index) {
        if (remaining < weights[index]) {
            ecology = static_cast<checkpoint::DungeonElement>(index);
            break;
        }
        remaining -= weights[index];
    }

    checkpoint::RoomDescriptor room;
    room.index = global_index;
    room.seed = seed;
    room.depth = depth;
    room.floor_room_index = floor_room_index;
    room.entry = entry;
    room.ecology = ecology;
    room.has_hole = hole_value.value() < rules.hole_threshold;
    room.is_abyss = abyss_value.value() < rules.abyss_threshold;

    RoomRandomSamples samples;
    samples.ecology = ecology_value.value();
    samples.hole = static_cast<std::uint32_t>(hole_value.value());
    samples.abyss = static_cast<std::uint32_t>(abyss_value.value());
    return {DungeonFault::none, room, samples};
}

std::uint64_t derive_next_room_seed(
    std::uint64_t current_seed,
    std::uint64_t next_index,
    ExitDirection exit) noexcept {
    return derive_door_room_seed(current_seed, next_index, exit);
}

RoomDescriptor make_initial_room(const DungeonSessionConfig& config) noexcept {
    RoomDescriptor room;
    room.index = config.initial_room_index;
    room.seed = derive_initial_room_seed(config.root_seed, room.index);
    room.entry = EntrySide::initial;
    room.combat = *make_combat_lab_config(
        checkpoint::EntrySide::initial, 1U);
    return room;
}

std::optional<RoomDescriptor> make_next_room(
    const RoomDescriptor& current,
    ExitDirection exit) noexcept {
    if (exit == ExitDirection::none
            || current.index == (std::numeric_limits<std::uint64_t>::max)()) {
        return std::nullopt;
    }

    RoomDescriptor next;
    next.index = current.index + 1U;
    next.seed = derive_door_room_seed(current.seed, next.index, exit);
    next.entry = entry_side_for_exit(exit);
    next.combat = *make_combat_lab_config(
        static_cast<checkpoint::EntrySide>(next.entry), 1U);
    return next;
}

}  // namespace arpg::dungeon
