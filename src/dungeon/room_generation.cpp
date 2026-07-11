#include "dungeon/room_generation.hpp"

#include "core/deterministic_rng.hpp"

#include <cstdint>
#include <limits>

namespace arpg::dungeon {
namespace {

constexpr std::uint64_t kInitialRoomDomain = 0x524F4F4D5F494E49ULL;
constexpr std::uint64_t kNextRoomDomain = 0x524F4F4D5F4E4558ULL;

combat::CombatLabConfig combat_template(EntrySide entry) noexcept {
    combat::CombatLabConfig config;
    config.respawn_defeated_dummies = false;

    switch (entry) {
    case EntrySide::initial:
        break;
    case EntrySide::left:
        config.player_spawn = {-6.50F, 0.0F, 0.0F};
        break;
    case EntrySide::right:
        config.player_spawn = {6.50F, 0.0F, 0.0F};
        config.dummy_spawns = {{
            {-2.30F, -0.35F, 0.0F},
            {-2.80F, 0.0F, 0.0F},
            {-3.30F, 0.35F, 0.0F},
        }};
        config.initial_facing = combat::Facing::left;
        break;
    case EntrySide::top:
        config.player_spawn = {0.0F, -2.75F, 0.0F};
        config.dummy_spawns = {{
            {2.30F, 2.30F, 0.0F},
            {2.80F, 2.30F, 0.0F},
            {3.30F, 2.30F, 0.0F},
        }};
        break;
    case EntrySide::bottom:
        config.player_spawn = {0.0F, 2.75F, 0.0F};
        config.dummy_spawns = {{
            {2.30F, -2.30F, 0.0F},
            {2.80F, -2.30F, 0.0F},
            {3.30F, -2.30F, 0.0F},
        }};
        break;
    }
    return config;
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
    auto domain = core::DeterministicRng::derive_stream(
        root_seed, kInitialRoomDomain);
    auto indexed = core::DeterministicRng::derive_stream(
        domain.next_u64(), room_index);
    return indexed.next_u64();
}

std::uint64_t derive_next_room_seed(
    std::uint64_t current_seed,
    std::uint64_t next_index,
    ExitDirection exit) noexcept {
    auto domain = core::DeterministicRng::derive_stream(
        current_seed, kNextRoomDomain);
    auto indexed = core::DeterministicRng::derive_stream(
        domain.next_u64(), next_index);
    auto directed = core::DeterministicRng::derive_stream(
        indexed.next_u64(), static_cast<std::uint64_t>(exit));
    return directed.next_u64();
}

RoomDescriptor make_initial_room(const DungeonSessionConfig& config) noexcept {
    RoomDescriptor room;
    room.index = config.initial_room_index;
    room.seed = derive_initial_room_seed(config.root_seed, room.index);
    room.entry = EntrySide::initial;
    room.combat = combat_template(room.entry);
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
    next.seed = derive_next_room_seed(current.seed, next.index, exit);
    next.entry = entry_side_for_exit(exit);
    next.combat = combat_template(next.entry);
    return next;
}

}  // namespace arpg::dungeon
