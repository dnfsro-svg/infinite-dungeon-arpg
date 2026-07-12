#pragma once

#include "core/bounded_queue.hpp"
#include "dungeon/encounter_director.hpp"
#include "dungeon/dungeon_types.hpp"

#include "combat/combat_world.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::test {
struct DungeonSessionTestAccess;
}

namespace arpg::dungeon {

class DungeonSession final {
public:
    static constexpr std::size_t kDungeonEventCapacity = 32;
    static constexpr std::size_t kCombatRelayCapacity = 64;

    explicit DungeonSession(DungeonSessionConfig config = {}) noexcept;
    explicit DungeonSession(
        DungeonRules rules,
        DungeonRunState stable_state) noexcept;
    [[nodiscard]] bool queue_action(combat::Action action) noexcept;
    void tick(combat::MovementInput movement) noexcept;
    [[nodiscard]] bool request_descent(bool player_in_range) noexcept;
    [[nodiscard]] std::optional<PendingTransition>
    pending_transition() const noexcept;
    void resolve_pending_transition(
        const TransitionSaveResult& result) noexcept;
    void reset_current_room() noexcept;
    [[nodiscard]] DungeonSnapshot snapshot() const noexcept;
    [[nodiscard]] std::optional<DungeonEvent> try_pop_event() noexcept;
    [[nodiscard]] std::optional<combat::CombatEvent>
    try_pop_combat_event() noexcept;

private:
    friend struct ::arpg::test::DungeonSessionTestAccess;
    void construct_current_room() noexcept;
    void start_next_wave() noexcept;
    void relay_combat_events() noexcept;
    void attempt_exit(ExitDirection direction) noexcept;
    void enter_fault(DungeonFault fault) noexcept;
    void emit_committed(
        const DungeonRunState& previous,
        const DungeonRunState& current) noexcept;
    bool emit(
        DungeonEventKind kind,
        const DungeonRunState* subject = nullptr,
        const DungeonRunState* destination = nullptr,
        TransitionKind transition = TransitionKind::none,
        ExitDirection direction = ExitDirection::none) noexcept;
    [[nodiscard]] std::uint8_t remaining_targets() const noexcept;

    DungeonRules rules_{};
    DungeonRunState stable_state_{};
    std::optional<PendingTransition> pending_{};
    std::optional<combat::CombatWorld> combat_{};
    RoomEncounterPlan encounter_plan_{};
    std::uint8_t wave_index_{};
    std::uint16_t wave_delay_ticks_{};
    core::BoundedQueue<DungeonEvent, kDungeonEventCapacity> events_{};
    core::BoundedQueue<combat::CombatEvent, kCombatRelayCapacity>
        combat_events_{};
    RoomPhase phase_{RoomPhase::locked};
    ExitDirection last_exit_{ExitDirection::none};
    std::uint64_t session_tick_{};
    DungeonDiagnostics diagnostics_{};
};

}  // namespace arpg::dungeon
