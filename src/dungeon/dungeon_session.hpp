#pragma once

#include "core/bounded_queue.hpp"
#include "dungeon/encounter_director.hpp"
#include "dungeon/dungeon_types.hpp"
#include "progression/progression_rules.hpp"

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
    [[nodiscard]] bool request_passive_allocation(
        passives::PassiveNodeId node) noexcept;
    [[nodiscard]] bool request_passive_refund(
        passives::PassiveNodeId node) noexcept;
    [[nodiscard]] std::optional<PendingSave> pending_save() const noexcept;
    void resolve_pending_save(const PendingSaveResult& result) noexcept;
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
    void settle_room_experience() noexcept;
    void attempt_exit(ExitDirection direction) noexcept;
    [[nodiscard]] bool prepare_transition(
        TransitionKind kind,
        ExitDirection direction) noexcept;
    [[nodiscard]] bool prepare_passive_mutation(
        passives::PassiveNodeId node, bool refund) noexcept;
    void commit_pending_save(const PendingSaveResult& result) noexcept;
    [[nodiscard]] DungeonSnapshot build_dungeon_snapshot() const noexcept;
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
    std::optional<PendingSave> pending_save_{};
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
    progression::ProgressionRules progression_rules_{
        progression::default_progression_rules()};
    progression::ProgressionState room_progression_{};
    std::uint64_t pending_room_experience_{};
    std::uint64_t last_room_experience_{};
    std::uint8_t last_levels_gained_{};
    passives::PassiveTreeError last_passive_tree_error_{
        passives::PassiveTreeError::none};
};

}  // namespace arpg::dungeon
