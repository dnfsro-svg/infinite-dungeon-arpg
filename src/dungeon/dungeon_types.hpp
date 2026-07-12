#pragma once

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_checkpoint.hpp"
#include "dungeon/dungeon_rules.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace arpg::dungeon {

using ExitDirection = checkpoint::ExitDirection;
using EntrySide = checkpoint::EntrySide;
using DungeonElement = checkpoint::DungeonElement;
using TransitionKind = checkpoint::TransitionKind;
using DungeonRunState = checkpoint::DungeonRunState;

enum class RoomPhase : std::uint8_t {
    locked,
    combat,
    wave_delay,
    cleared,
    awaiting_exit,
    committing,
    transitioning,
    faulted,
};

enum class SaveDisposition : std::uint8_t {
    committed,
    not_committed,
    indeterminate,
};

enum class DungeonEventKind : std::uint8_t {
    room_entered,
    combat_started,
    room_cleared,
    exits_opened,
    transition_requested,
    transition_committed,
    save_failed,
    // Kept as a source-compatible name for pre-transaction consumers.
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
    std::uint64_t destination_room_index{};
    std::uint64_t destination_room_seed{};
    TransitionKind transition{TransitionKind::none};
    ExitDirection direction{ExitDirection::none};
};

struct DungeonDiagnostics final {
    std::uint32_t event_overflow_count{};
    std::uint32_t combat_relay_overflow_count{};
    std::uint32_t rejected_exit_count{};
    std::uint32_t save_failure_count{};
    DungeonFault fault{DungeonFault::none};
    bool room_index_overflow{};
};

struct DungeonEncounterDiagnostics final {
    std::uint8_t total_budget{};
    std::uint8_t current_wave_budget{};
    std::uint8_t current_wave_spawn_count{};
    bool plan_valid{};
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
    std::uint64_t root_seed{};
    std::uint64_t commit_generation{};
    std::uint64_t room_index{};
    std::uint64_t room_seed{};
    std::uint64_t depth{1};
    std::uint64_t floor_room_index{1};
    std::array<std::uint32_t, 4> biases{};
    RoomPhase phase{RoomPhase::locked};
    bool has_active_room{};
    std::array<bool, 4> exits_open{};
    std::uint8_t wave_index{};
    std::uint8_t wave_count{};
    std::uint16_t wave_delay_ticks{};
    std::uint8_t remaining_targets{};
    EntrySide entry_side{EntrySide::initial};
    ExitDirection last_exit{ExitDirection::none};
    TransitionKind last_transition{TransitionKind::none};
    DungeonElement ecology{DungeonElement::fire};
    bool has_hole{};
    bool is_abyss{};
    bool has_pending_transition{};
    std::optional<combat::CombatSnapshot> combat{};
    DungeonEncounterDiagnostics encounter{};
    DungeonDiagnostics diagnostics{};
};

struct PendingTransition final {
    TransitionKind kind{TransitionKind::none};
    ExitDirection direction{ExitDirection::none};
    std::uint64_t expected_generation{};
    DungeonRunState next_state{};
};

struct TransitionSaveResult final {
    SaveDisposition disposition{SaveDisposition::indeterminate};
    std::uint64_t generation{};
    DungeonRunState verified_state{};
};

}  // namespace arpg::dungeon
