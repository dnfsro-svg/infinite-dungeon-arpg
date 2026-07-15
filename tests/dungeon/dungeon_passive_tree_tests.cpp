#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "modifiers/damage_types.hpp"

#include <cstdint>

namespace {

using arpg::dungeon::DungeonFault;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::PendingSaveKind;
using arpg::dungeon::RoomPhase;
using arpg::dungeon::SaveDisposition;

DungeonSession session_with_passive_points(std::uint8_t points) noexcept {
    auto built = arpg::dungeon::make_initial_run_state(0x51515151ULL,
        DungeonRules{});
    built.state.progression = {
        static_cast<std::uint8_t>(points + 1U), 0U, points, points};
    return DungeonSession(DungeonRules{}, built.state);
}

bool clear_to_awaiting_exit(DungeonSession& session) noexcept {
    arpg::test::EventSummary events{};
    if (!arpg::test::drive_until_cleared(session, events)) return false;
    if (session.snapshot().phase == RoomPhase::cleared) session.tick({});
    return session.snapshot().phase == RoomPhase::awaiting_exit;
}

bool commit_passive_save(DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value() || pending->kind != PendingSaveKind::passive_tree) {
        return false;
    }
    session.resolve_pending_save({SaveDisposition::committed,
        pending->next_state.commit_generation, pending->next_state});
    return true;
}

arpg::test::Failure passive_mutation_is_only_available_after_clear() noexcept {
    DungeonSession session = session_with_passive_points(4U);
    ARPG_REQUIRE(!session.request_passive_allocation(8U));
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    ARPG_REQUIRE(session.request_passive_allocation(8U));
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == PendingSaveKind::passive_tree);
    ARPG_REQUIRE(!session.pending_transition().has_value());
    ARPG_REQUIRE(!session.request_descent(true));
    return {};
}

arpg::test::Failure not_committed_passive_mutation_keeps_old_state() noexcept {
    DungeonSession session = session_with_passive_points(4U);
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    ARPG_REQUIRE(session.request_passive_allocation(8U));
    const auto pending = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::not_committed,
        pending.next_state.commit_generation, pending.next_state});
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.passive_tree.allocated_bits == 1ULL);
    ARPG_REQUIRE(snapshot.phase == RoomPhase::awaiting_exit);
    ARPG_REQUIRE(!snapshot.passive_save_pending);
    ARPG_REQUIRE(snapshot.diagnostics.save_failure_count == 1U);
    return {};
}

arpg::test::Failure committed_passive_mutation_adopts_new_tree_and_points() noexcept {
    DungeonSession session = session_with_passive_points(4U);
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    const auto before = session.snapshot();
    ARPG_REQUIRE(session.request_passive_allocation(8U));
    ARPG_REQUIRE(commit_passive_save(session));
    const auto after = session.snapshot();
    ARPG_REQUIRE(after.passive_tree.allocated_bits == ((1ULL << 0U) | (1ULL << 8U)));
    ARPG_REQUIRE(after.progression.unspent_passive_points
        == before.progression.unspent_passive_points - 1U);
    ARPG_REQUIRE(after.phase == RoomPhase::awaiting_exit);
    return {};
}

arpg::test::Failure indeterminate_passive_mutation_faults() noexcept {
    DungeonSession session = session_with_passive_points(4U);
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    ARPG_REQUIRE(session.request_passive_allocation(8U));
    session.resolve_pending_save({SaveDisposition::indeterminate, 0U, {}});
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::faulted);
    ARPG_REQUIRE(snapshot.diagnostics.fault
        == DungeonFault::save_commit_indeterminate);
    return {};
}

arpg::test::Failure committed_passive_generation_mismatch_faults() noexcept {
    DungeonSession session = session_with_passive_points(4U);
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    ARPG_REQUIRE(session.request_passive_allocation(8U));
    const auto pending = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::committed,
        pending.next_state.commit_generation + 1U, pending.next_state});
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::faulted);
    ARPG_REQUIRE(snapshot.diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    return {};
}

arpg::test::Failure committed_passive_bitset_mismatch_faults() noexcept {
    DungeonSession session = session_with_passive_points(4U);
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    ARPG_REQUIRE(session.request_passive_allocation(8U));
    const auto pending = *session.pending_save();
    auto wrong = pending.next_state;
    wrong.passive_tree.allocated_bits = 1ULL;
    session.resolve_pending_save({SaveDisposition::committed,
        pending.next_state.commit_generation, wrong});
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == RoomPhase::faulted);
    ARPG_REQUIRE(snapshot.diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    return {};
}

arpg::test::Failure disconnecting_refund_is_rejected_without_save() noexcept {
    DungeonSession session = session_with_passive_points(4U);
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    ARPG_REQUIRE(session.request_passive_allocation(8U));
    ARPG_REQUIRE(commit_passive_save(session));
    ARPG_REQUIRE(session.request_passive_allocation(9U));
    ARPG_REQUIRE(commit_passive_save(session));
    ARPG_REQUIRE(!session.request_passive_refund(8U));
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.passive_tree.allocated_bits
        == ((1ULL << 0U) | (1ULL << 8U) | (1ULL << 9U)));
    ARPG_REQUIRE(!snapshot.passive_save_pending);
    ARPG_REQUIRE(snapshot.passive_tree_error
        == arpg::passives::PassiveTreeError::disconnects_tree);
    return {};
}

arpg::test::Failure next_room_uses_committed_passive_player_build() noexcept {
    DungeonSession session = session_with_passive_points(5U);
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    for (const std::uint8_t node : {
            std::uint8_t{2U}, std::uint8_t{8U}, std::uint8_t{9U},
            std::uint8_t{10U}, std::uint8_t{13U}}) {
        ARPG_REQUIRE(session.request_passive_allocation(node));
        ARPG_REQUIRE(commit_passive_save(session));
    }
    const auto committed = session.snapshot();
    ARPG_REQUIRE(committed.passive_tree.allocated_bits == ((1ULL << 0U)
        | (1ULL << 2U) | (1ULL << 8U) | (1ULL << 9U)
        | (1ULL << 10U) | (1ULL << 13U)));

    const auto pending = session.pending_save();
    ARPG_REQUIRE(!pending.has_value());
    // A direct descent request needs a hole; replace the stable room only via
    // the test hook so the transition path itself remains production code.
    arpg::test::set_current_room_hole(session, true);
    ARPG_REQUIRE(session.request_descent(true));
    const auto transition = session.pending_save();
    ARPG_REQUIRE(transition.has_value());
    ARPG_REQUIRE(transition->kind == PendingSaveKind::transition);
    session.resolve_pending_save({SaveDisposition::committed,
        transition->next_state.commit_generation, transition->next_state});
    session.tick({});
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.combat.has_value());
    ARPG_REQUIRE(snapshot.combat->player.max_barrier == 10);
    ARPG_REQUIRE(snapshot.combat->player.damage_reduction[0] == 700);
    const auto& build = arpg::test::player_build(session);
    ARPG_REQUIRE(build.values.flat_damage[
        arpg::modifiers::damage_index(arpg::modifiers::DamageType::fire)]
        == 30000);
    return {};
}

arpg::test::Failure current_room_keeps_old_build_until_transition() noexcept {
    DungeonSession session = session_with_passive_points(1U);
    ARPG_REQUIRE(clear_to_awaiting_exit(session));
    const auto before = session.snapshot();
    const auto* const combat_before = arpg::test::combat_world_address(session);
    ARPG_REQUIRE(before.combat.has_value());
    ARPG_REQUIRE(combat_before != nullptr);
    ARPG_REQUIRE(before.combat->tick != 0U);
    ARPG_REQUIRE(before.combat->player.max_barrier == 0);
    ARPG_REQUIRE(before.combat->player.barrier == 0);
    ARPG_REQUIRE(before.combat->player.damage_reduction[0] == 0);

    ARPG_REQUIRE(session.request_passive_allocation(2U));
    ARPG_REQUIRE(commit_passive_save(session));
    const auto after_commit = session.snapshot();
    ARPG_REQUIRE(after_commit.combat.has_value());
    ARPG_REQUIRE(arpg::test::combat_world_address(session) == combat_before);
    ARPG_REQUIRE(after_commit.combat->tick == before.combat->tick);
    ARPG_REQUIRE(after_commit.combat->player.max_barrier
        == before.combat->player.max_barrier);
    ARPG_REQUIRE(after_commit.combat->player.barrier
        == before.combat->player.barrier);
    ARPG_REQUIRE(after_commit.combat->player.damage_reduction
        == before.combat->player.damage_reduction);

    arpg::test::set_current_room_hole(session, true);
    ARPG_REQUIRE(session.request_descent(true));
    const auto transition = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::committed,
        transition.next_state.commit_generation, transition.next_state});
    session.tick({});
    const auto next_room = session.snapshot();
    ARPG_REQUIRE(next_room.combat.has_value());
    ARPG_REQUIRE(next_room.combat->player.max_barrier == 10);
    ARPG_REQUIRE(next_room.combat->player.barrier == 10);
    ARPG_REQUIRE(next_room.combat->player.damage_reduction[0] == 0);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"passive mutation only after clear", &passive_mutation_is_only_available_after_clear},
    {"not committed passive mutation keeps old state", &not_committed_passive_mutation_keeps_old_state},
    {"committed passive mutation adopts tree", &committed_passive_mutation_adopts_new_tree_and_points},
    {"indeterminate passive mutation faults", &indeterminate_passive_mutation_faults},
    {"committed passive generation mismatch faults", &committed_passive_generation_mismatch_faults},
    {"committed passive bitset mismatch faults", &committed_passive_bitset_mismatch_faults},
    {"disconnecting passive refund is rejected", &disconnecting_refund_is_rejected_without_save},
    {"next room uses committed passive player build", &next_room_uses_committed_passive_player_build},
    {"current room keeps old build until transition", &current_room_keeps_old_build_until_transition},
};

}  // namespace

arpg::test::TestSuite dungeon_passive_tree_suite() noexcept {
    return arpg::test::make_suite("dungeon_passive_tree", kCases);
}
