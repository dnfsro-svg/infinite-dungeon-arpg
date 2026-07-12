#pragma once

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_checkpoint.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace arpg::dungeon {

using ExitDirection = checkpoint::ExitDirection;
using EntrySide = checkpoint::EntrySide;

enum class RoomPhase : std::uint8_t {
    locked,
    combat,
    cleared,
    awaiting_exit,
    transitioning,
};

enum class DungeonEventKind : std::uint8_t {
    room_entered,
    combat_started,
    room_cleared,
    exits_opened,
    exit_committed,
    room_destroyed,
    room_reset,
    faulted,
};

struct DungeonEvent final {
    DungeonEventKind kind{};
    std::uint64_t session_tick{};
    std::uint64_t room_index{};
    std::uint64_t room_seed{};
    ExitDirection direction{ExitDirection::none};
};

struct DungeonDiagnostics final {
    std::uint32_t event_overflow_count{};
    std::uint32_t combat_relay_overflow_count{};
    std::uint32_t rejected_exit_count{};
    bool room_index_overflow{};
};

struct DungeonSessionConfig final {
    std::uint64_t root_seed{0x6D30305F5241594CULL};
    std::uint64_t initial_room_index{};
};

struct RoomDescriptor final {
    std::uint64_t index{};
    std::uint64_t seed{};
    EntrySide entry{EntrySide::initial};
    combat::CombatLabConfig combat{};
};

struct DungeonSnapshot final {
    std::uint64_t session_tick{};
    std::uint64_t room_index{};
    std::uint64_t room_seed{};
    RoomPhase phase{RoomPhase::locked};
    bool has_active_room{};
    std::array<bool, 4> exits_open{};
    std::uint8_t remaining_targets{};
    EntrySide entry_side{EntrySide::initial};
    ExitDirection last_exit{ExitDirection::none};
    std::optional<combat::CombatSnapshot> combat{};
    DungeonDiagnostics diagnostics{};
};

}  // namespace arpg::dungeon
