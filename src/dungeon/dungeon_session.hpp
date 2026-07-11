#pragma once

#include "core/bounded_queue.hpp"
#include "dungeon/dungeon_types.hpp"

#include "combat/combat_world.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::dungeon {

class DungeonSession final {
public:
    static constexpr std::size_t kDungeonEventCapacity = 32;
    static constexpr std::size_t kCombatRelayCapacity = 64;

    explicit DungeonSession(DungeonSessionConfig config = {}) noexcept;
    [[nodiscard]] bool queue_action(combat::Action action) noexcept;
    void tick(combat::MovementInput movement) noexcept;
    void reset_current_room() noexcept;
    [[nodiscard]] DungeonSnapshot snapshot() const noexcept;
    [[nodiscard]] std::optional<DungeonEvent> try_pop_event() noexcept;
    [[nodiscard]] std::optional<combat::CombatEvent>
    try_pop_combat_event() noexcept;

private:
    void construct_current_room() noexcept;
    void relay_combat_events() noexcept;
    void attempt_exit(ExitDirection direction) noexcept;
    bool emit(
        DungeonEventKind kind,
        ExitDirection direction = ExitDirection::none) noexcept;
    [[nodiscard]] std::uint8_t remaining_targets() const noexcept;

    DungeonSessionConfig config_{};
    RoomDescriptor current_room_{};
    std::optional<RoomDescriptor> pending_room_{};
    std::optional<combat::CombatWorld> combat_{};
    core::BoundedQueue<DungeonEvent, kDungeonEventCapacity> events_{};
    core::BoundedQueue<combat::CombatEvent, kCombatRelayCapacity>
        combat_events_{};
    RoomPhase phase_{RoomPhase::locked};
    ExitDirection last_exit_{ExitDirection::none};
    std::uint64_t session_tick_{};
    DungeonDiagnostics diagnostics_{};
    bool room_index_fault_emitted_{};
};

}  // namespace arpg::dungeon
