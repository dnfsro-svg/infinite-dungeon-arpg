#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "persistence/save_store.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>

namespace {

namespace dungeon = arpg::dungeon;
namespace persistence = arpg::persistence;
namespace items = arpg::items;
namespace combat = arpg::combat;

items::ItemInstance normal_weapon(std::uint64_t id) noexcept {
    items::ItemInstance item{};
    item.id = id;
    item.base_id = 1U;
    item.rarity = items::ItemRarity::normal;
    item.item_level = 1U;
    item.required_level = 1U;
    return item;
}

bool same_ownership(const items::ItemOwnershipState& left,
    const items::ItemOwnershipState& right) noexcept {
    if (left.items.size() != right.items.size()
            || left.equipment.equipped_ids != right.equipment.equipped_ids
            || left.claimed_drop_bits != right.claimed_drop_bits
            || left.next_item_sequence != right.next_item_sequence) {
        return false;
    }
    for (std::size_t index = 0U; index < left.items.size(); ++index) {
        if (std::memcmp(&left.items[index], &right.items[index],
                sizeof(items::ItemInstance)) != 0) return false;
    }
    return true;
}

bool same_build(const combat::PlayerCombatBuild& left,
    const combat::PlayerCombatBuild& right) noexcept {
    const auto& a = left.values;
    const auto& b = right.values;
    return left.weapon_physical == right.weapon_physical
        && left.local_attack_speed_bp == right.local_attack_speed_bp
        && a.flat_damage == b.flat_damage
        && a.damage_increased == b.damage_increased
        && a.damage_reduction == b.damage_reduction
        && a.damage_reduction_cap_bonus == b.damage_reduction_cap_bonus
        && a.armor == b.armor && a.evasion == b.evasion
        && a.melee_damage == b.melee_damage
        && a.max_health == b.max_health
        && a.max_health_more == b.max_health_more
        && a.max_barrier == b.max_barrier
        && a.damage_taken == b.damage_taken
        && a.movement_speed == b.movement_speed
        && a.attack_speed == b.attack_speed
        && a.impulse_scale == b.impulse_scale
        && a.jump_speed == b.jump_speed
        && a.air_control == b.air_control && a.valid == b.valid;
}

struct TempDirectory final {
    std::filesystem::path path;

    TempDirectory() noexcept {
        std::error_code error;
        path = std::filesystem::temp_directory_path(error)
            / "arpg_dungeon_save_integration";
        path /= std::to_string(static_cast<unsigned long long>(
            std::hash<std::string>{}(path.string() + std::to_string(
                reinterpret_cast<std::uintptr_t>(this)))));
        std::filesystem::remove_all(path, error);
        std::filesystem::create_directories(path, error);
    }

    ~TempDirectory() noexcept {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

struct FaultContext final {
    persistence::SaveFaultPoint point{};
    bool triggered{};
};

bool fail_once(persistence::SaveFaultPoint point, void* opaque) noexcept {
    auto* context = static_cast<FaultContext*>(opaque);
    if (point != context->point || context->triggered) {
        return false;
    }
    context->triggered = true;
    return true;
}

persistence::SaveStore make_store(const std::filesystem::path& directory,
    FaultContext* fault = nullptr) noexcept {
    persistence::SaveStoreConfig config{};
    config.directory = directory;
    if (fault != nullptr) {
        config.fault_hook = &fail_once;
        config.fault_context = fault;
    }
    return persistence::SaveStore(config);
}

dungeon::TransitionSaveResult to_session_result(
    const persistence::SaveCommitResult& saved) noexcept {
    using persistence::SaveCommitState;
    dungeon::SaveDisposition disposition =
        dungeon::SaveDisposition::indeterminate;
    if (saved.state == SaveCommitState::committed) {
        disposition = dungeon::SaveDisposition::committed;
    } else if (saved.state == SaveCommitState::not_committed) {
        disposition = dungeon::SaveDisposition::not_committed;
    }
    return {
        disposition,
        saved.verified_state.commit_generation,
        saved.verified_state,
    };
}

dungeon::DungeonRunState initial_state(std::uint64_t seed) noexcept {
    return dungeon::make_initial_run_state(seed, dungeon::DungeonRules{}).state;
}

dungeon::DungeonRunState initial_state_with_passive_points(
    std::uint64_t seed) noexcept {
    auto state = initial_state(seed);
    state.progression = {4U, 0U, 3U, 3U};
    return state;
}

bool same_descriptor(const dungeon::DungeonSnapshot& snapshot,
    const dungeon::DungeonRunState& state) noexcept {
    const auto& room = state.current_room;
    return snapshot.root_seed == state.root_seed
        && snapshot.room_index == room.index
        && snapshot.entry_side == room.entry
        && snapshot.last_transition == state.last_transition
        && snapshot.last_exit == state.last_direction
        && snapshot.commit_generation == state.commit_generation
        && snapshot.room_seed == room.seed
        && snapshot.depth == room.depth
        && snapshot.floor_room_index == room.floor_room_index
        && snapshot.biases == state.biases
        && snapshot.ecology == room.ecology
        && snapshot.has_hole == room.has_hole
        && snapshot.is_abyss == room.is_abyss;
}

bool clear_and_await(dungeon::DungeonSession& session) noexcept {
    arpg::test::EventSummary summary;
    if (!arpg::test::drive_until_cleared(session, summary)) {
        return false;
    }
    if (session.snapshot().phase == dungeon::RoomPhase::cleared) {
        session.tick({});
        arpg::test::drain_all_events(session, summary);
    }
    return session.snapshot().phase == dungeon::RoomPhase::awaiting_exit;
}

bool drive_door_pending(dungeon::DungeonSession& session,
    dungeon::ExitDirection direction) noexcept {
    if (!clear_and_await(session)) {
        return false;
    }
    for (int tick = 0; tick < 256; ++tick) {
        const auto snapshot = session.snapshot();
        if (!snapshot.combat.has_value()) {
            return false;
        }
        const auto position = snapshot.combat->player.position;
        arpg::combat::MovementInput alignment{};
        if (direction == dungeon::ExitDirection::left
                || direction == dungeon::ExitDirection::right) {
            alignment.y = position.y > 0.1F ? -1 : (position.y < -0.1F ? 1 : 0);
        } else {
            alignment.x = position.x > 0.1F ? -1 : (position.x < -0.1F ? 1 : 0);
        }
        if (alignment.x == 0 && alignment.y == 0) {
            break;
        }
        session.tick(alignment);
        arpg::test::EventSummary summary;
        arpg::test::drain_all_events(session, summary);
    }
    arpg::combat::MovementInput outward{};
    switch (direction) {
    case dungeon::ExitDirection::up:
        outward = {0, -1};
        break;
    case dungeon::ExitDirection::down:
        outward = {0, 1};
        break;
    case dungeon::ExitDirection::left:
        outward = {-1, 0};
        break;
    case dungeon::ExitDirection::right:
        outward = {1, 0};
        break;
    case dungeon::ExitDirection::none:
        return false;
    }
    for (int tick = 0; tick < 256; ++tick) {
        session.tick(outward);
        arpg::test::EventSummary summary;
        arpg::test::drain_all_events(session, summary);
        if (session.snapshot().phase == dungeon::RoomPhase::committing) {
            return session.pending_transition().has_value();
        }
    }
    return false;
}

bool commit_pending_passive(dungeon::DungeonSession& session,
    persistence::SaveStore& store) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()
            || pending->kind != dungeon::PendingSaveKind::passive_tree) {
        return false;
    }
    const auto saved = store.commit(pending->next_state);
    session.resolve_pending_save(to_session_result(saved));
    const auto snapshot = session.snapshot();
    return saved.state == persistence::SaveCommitState::committed
        && !snapshot.passive_save_pending
        && snapshot.passive_tree.allocated_bits
            == pending->next_state.passive_tree.allocated_bits
        && snapshot.progression.unspent_passive_points
            == pending->next_state.progression.unspent_passive_points;
}

bool allocate_committed(dungeon::DungeonSession& session,
    persistence::SaveStore& store, std::uint8_t node) noexcept {
    return session.request_passive_allocation(node)
        && commit_pending_passive(session, store);
}

arpg::test::Failure initial_generation_one_round_trips_descriptor() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto initial = initial_state(0x8101U);
    ARPG_REQUIRE(initial.commit_generation == 1U);
    ARPG_REQUIRE(store.commit(initial).state
        == persistence::SaveCommitState::committed);

    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    dungeon::DungeonSession session{dungeon::DungeonRules{}, loaded.checkpoint};
    ARPG_REQUIRE(same_descriptor(session.snapshot(), initial));
    return {};
}

arpg::test::Failure committed_door_transition_restarts_in_next_room() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto initial = initial_state(0x8102U);
    ARPG_REQUIRE(store.commit(initial).state
        == persistence::SaveCommitState::committed);

    dungeon::DungeonSession session{dungeon::DungeonRules{}, initial};
    ARPG_REQUIRE(drive_door_pending(session, dungeon::ExitDirection::right));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    const auto saved = store.commit(pending->next_state);
    ARPG_REQUIRE(saved.state == persistence::SaveCommitState::committed);
    session.resolve_pending_transition(to_session_result(saved));
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::transitioning);
    ARPG_REQUIRE(!session.snapshot().combat.has_value());
    ARPG_REQUIRE(same_descriptor(session.snapshot(), saved.verified_state));

    auto restarted_store = make_store(directory.path);
    const auto restarted = restarted_store.load();
    ARPG_REQUIRE(restarted.state == persistence::SaveLoadState::ready);
    dungeon::DungeonSession restarted_session{
        dungeon::DungeonRules{}, restarted.checkpoint};
    ARPG_REQUIRE(same_descriptor(restarted_session.snapshot(),
        pending->next_state));
    return {};
}

arpg::test::Failure pre_publish_failure_keeps_old_room_in_memory_and_on_disk() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto initial = initial_state(0x8103U);
    ARPG_REQUIRE(store.commit(initial).state
        == persistence::SaveCommitState::committed);

    dungeon::DungeonSession session{dungeon::DungeonRules{}, initial};
    ARPG_REQUIRE(drive_door_pending(session, dungeon::ExitDirection::up));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    FaultContext fault{persistence::SaveFaultPoint::before_publish, false};
    auto faulty_store = make_store(directory.path, &fault);
    const auto saved = faulty_store.commit(pending->next_state);
    ARPG_REQUIRE(saved.state == persistence::SaveCommitState::not_committed);
    session.resolve_pending_transition(to_session_result(saved));
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.phase == dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(snapshot.root_seed == initial.root_seed);
    ARPG_REQUIRE(snapshot.commit_generation == initial.commit_generation);
    ARPG_REQUIRE(snapshot.room_index == initial.current_room.index);
    ARPG_REQUIRE(snapshot.room_seed == initial.current_room.seed);
    ARPG_REQUIRE(snapshot.depth == initial.current_room.depth);
    ARPG_REQUIRE(snapshot.floor_room_index
        == initial.current_room.floor_room_index);
    ARPG_REQUIRE(snapshot.biases == initial.biases);
    ARPG_REQUIRE(snapshot.ecology == initial.current_room.ecology);
    ARPG_REQUIRE(snapshot.has_hole == initial.current_room.has_hole);
    ARPG_REQUIRE(snapshot.is_abyss == initial.current_room.is_abyss);

    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(dungeon::same_run_state(loaded.checkpoint, initial));
    return {};
}

arpg::test::Failure lost_post_publish_receipt_faults_session_but_restart_uses_new_room() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto initial = initial_state(0x8104U);
    ARPG_REQUIRE(store.commit(initial).state
        == persistence::SaveCommitState::committed);

    dungeon::DungeonSession session{dungeon::DungeonRules{}, initial};
    ARPG_REQUIRE(drive_door_pending(session, dungeon::ExitDirection::down));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    FaultContext fault{persistence::SaveFaultPoint::after_publish, false};
    auto faulty_store = make_store(directory.path, &fault);
    const auto saved = faulty_store.commit(pending->next_state);
    ARPG_REQUIRE(saved.state == persistence::SaveCommitState::indeterminate);
    session.resolve_pending_transition(to_session_result(saved));
    ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == dungeon::DungeonFault::save_commit_indeterminate);

    auto restarted_store = make_store(directory.path);
    const auto restarted = restarted_store.load();
    ARPG_REQUIRE(restarted.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(dungeon::same_run_state(restarted.checkpoint,
        pending->next_state));
    return {};
}

arpg::test::Failure committed_descent_restarts_without_rerolling_hole_or_abyss() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    auto initial = initial_state(0x8105U);
    initial.current_room.has_hole = true;
    initial.current_room.is_abyss = true;
    ARPG_REQUIRE(store.commit(initial).state
        == persistence::SaveCommitState::committed);

    dungeon::DungeonSession session{dungeon::DungeonRules{}, initial};
    ARPG_REQUIRE(clear_and_await(session));
    ARPG_REQUIRE(session.request_descent(true));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == dungeon::TransitionKind::descent);
    const auto saved = store.commit(pending->next_state);
    ARPG_REQUIRE(saved.state == persistence::SaveCommitState::committed);
    session.resolve_pending_transition(to_session_result(saved));

    auto restarted_store = make_store(directory.path);
    const auto restarted = restarted_store.load();
    ARPG_REQUIRE(restarted.state == persistence::SaveLoadState::ready);
    const auto& room = restarted.checkpoint.current_room;
    const std::array<std::uint32_t, 4> zero_biases{{0U, 0U, 0U, 0U}};
    ARPG_REQUIRE(room.depth == initial.current_room.depth + 1U);
    ARPG_REQUIRE(room.floor_room_index == 1U);
    ARPG_REQUIRE(restarted.checkpoint.biases == zero_biases);
    ARPG_REQUIRE(dungeon::same_run_state(restarted.checkpoint,
        pending->next_state));
    dungeon::DungeonSession restarted_session{
        dungeon::DungeonRules{}, restarted.checkpoint};
    const auto restarted_snapshot = restarted_session.snapshot();
    ARPG_REQUIRE(restarted_snapshot.commit_generation
        == pending->next_state.commit_generation);
    ARPG_REQUIRE(restarted_snapshot.room_seed
        == pending->next_state.current_room.seed);
    ARPG_REQUIRE(restarted_snapshot.depth
        == pending->next_state.current_room.depth);
    ARPG_REQUIRE(restarted_snapshot.floor_room_index
        == pending->next_state.current_room.floor_room_index);
    ARPG_REQUIRE(restarted_snapshot.biases == pending->next_state.biases);
    ARPG_REQUIRE(restarted_snapshot.ecology
        == pending->next_state.current_room.ecology);
    ARPG_REQUIRE(restarted_snapshot.has_hole
        == pending->next_state.current_room.has_hole);
    ARPG_REQUIRE(restarted_snapshot.is_abyss
        == pending->next_state.current_room.is_abyss);
    return {};
}

arpg::test::Failure reset_reopen_and_repeated_load_keep_persisted_room_fields() noexcept {
    TempDirectory directory;
    auto store = make_store(directory.path);
    const auto initial = initial_state(0x8106U);
    ARPG_REQUIRE(store.commit(initial).state
        == persistence::SaveCommitState::committed);

    dungeon::DungeonSession session{dungeon::DungeonRules{}, initial};
    ARPG_REQUIRE(drive_door_pending(session, dungeon::ExitDirection::left));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    const auto saved = store.commit(pending->next_state);
    ARPG_REQUIRE(saved.state == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(saved.verified_state.commit_generation != 1U);
    session.resolve_pending_transition(to_session_result(saved));
    session.tick({});
    arpg::test::EventSummary summary;
    arpg::test::drain_all_events(session, summary);
    ARPG_REQUIRE(same_descriptor(session.snapshot(), saved.verified_state));

    const auto before_reset = session.snapshot();
    session.reset_current_room();
    const auto after_reset = session.snapshot();
    ARPG_REQUIRE(after_reset.commit_generation == before_reset.commit_generation);
    ARPG_REQUIRE(after_reset.room_seed == before_reset.room_seed);
    ARPG_REQUIRE(after_reset.ecology == before_reset.ecology);
    ARPG_REQUIRE(after_reset.has_hole == before_reset.has_hole);
    ARPG_REQUIRE(after_reset.is_abyss == before_reset.is_abyss);

    auto reopened_store = make_store(directory.path);
    const auto first_load = reopened_store.load();
    const auto second_load = reopened_store.load();
    ARPG_REQUIRE(first_load.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(second_load.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(dungeon::same_run_state(first_load.checkpoint,
        saved.verified_state));
    ARPG_REQUIRE(dungeon::same_run_state(second_load.checkpoint,
        saved.verified_state));
    dungeon::DungeonSession reopened{dungeon::DungeonRules{}, first_load.checkpoint};
    dungeon::DungeonSession repeated{dungeon::DungeonRules{}, second_load.checkpoint};
    ARPG_REQUIRE(same_descriptor(after_reset, saved.verified_state));
    ARPG_REQUIRE(same_descriptor(reopened.snapshot(), saved.verified_state));
    ARPG_REQUIRE(same_descriptor(repeated.snapshot(), saved.verified_state));
    return {};
}

arpg::test::Failure passive_fault_recovery_never_persists_partial_points_or_bits() noexcept {
    constexpr std::uint64_t kTwoNodes = (1ULL << 0U) | (1ULL << 8U)
        | (1ULL << 9U);
    constexpr std::uint64_t kThreeNodes = kTwoNodes | (1ULL << 10U);

    {
        TempDirectory directory;
        auto store = make_store(directory.path);
        const auto initial = initial_state_with_passive_points(0x8110U);
        ARPG_REQUIRE(store.commit(initial).state
            == persistence::SaveCommitState::committed);
        dungeon::DungeonSession session{dungeon::DungeonRules{}, initial};
        ARPG_REQUIRE(clear_and_await(session));
        ARPG_REQUIRE(allocate_committed(session, store, 8U));
        ARPG_REQUIRE(allocate_committed(session, store, 9U));
        const auto before = session.snapshot();
        ARPG_REQUIRE(before.passive_tree.allocated_bits == kTwoNodes);
        ARPG_REQUIRE(session.request_passive_allocation(10U));
        const auto pending = *session.pending_save();
        FaultContext fault{persistence::SaveFaultPoint::before_publish, false};
        auto faulty = make_store(directory.path, &fault);
        const auto saved = faulty.commit(pending.next_state);
        ARPG_REQUIRE(saved.state == persistence::SaveCommitState::not_committed);
        session.resolve_pending_save(to_session_result(saved));
        const auto after = session.snapshot();
        ARPG_REQUIRE(after.passive_tree.allocated_bits == before.passive_tree.allocated_bits);
        ARPG_REQUIRE(after.progression.unspent_passive_points
            == before.progression.unspent_passive_points);
        auto restarted_store = make_store(directory.path);
        const auto restarted = restarted_store.load();
        ARPG_REQUIRE(restarted.state == persistence::SaveLoadState::ready);
        ARPG_REQUIRE(restarted.checkpoint.passive_tree.allocated_bits == kTwoNodes);
        ARPG_REQUIRE(restarted.checkpoint.progression.unspent_passive_points
            == before.progression.unspent_passive_points);
    }

    {
        TempDirectory directory;
        auto store = make_store(directory.path);
        const auto initial = initial_state_with_passive_points(0x8111U);
        ARPG_REQUIRE(store.commit(initial).state
            == persistence::SaveCommitState::committed);
        dungeon::DungeonSession session{dungeon::DungeonRules{}, initial};
        ARPG_REQUIRE(clear_and_await(session));
        ARPG_REQUIRE(allocate_committed(session, store, 8U));
        ARPG_REQUIRE(allocate_committed(session, store, 9U));
        ARPG_REQUIRE(session.request_passive_allocation(10U));
        const auto pending = *session.pending_save();
        FaultContext fault{persistence::SaveFaultPoint::after_publish, false};
        auto faulty = make_store(directory.path, &fault);
        const auto saved = faulty.commit(pending.next_state);
        ARPG_REQUIRE(saved.state == persistence::SaveCommitState::indeterminate);
        session.resolve_pending_save(to_session_result(saved));
        ARPG_REQUIRE(session.snapshot().phase == dungeon::RoomPhase::faulted);
        auto restarted_store = make_store(directory.path);
        const auto restarted = restarted_store.load();
        ARPG_REQUIRE(restarted.state == persistence::SaveLoadState::ready);
        ARPG_REQUIRE(restarted.checkpoint.passive_tree.allocated_bits == kThreeNodes);
        ARPG_REQUIRE(restarted.checkpoint.progression.unspent_passive_points
            == pending.next_state.progression.unspent_passive_points);
        ARPG_REQUIRE(dungeon::same_run_state(restarted.checkpoint,
            pending.next_state));
    }
    return {};
}

arpg::test::Failure equipment_dispositions_restart_with_exact_disk_winner() noexcept {
    constexpr std::array<persistence::SaveCommitState, 3> kDispositions{{
        persistence::SaveCommitState::committed,
        persistence::SaveCommitState::not_committed,
        persistence::SaveCommitState::indeterminate,
    }};
    for (const auto disposition : kDispositions) {
        TempDirectory directory;
        auto initial = initial_state(0x8120U);
        initial.item_ownership.items.push_back(normal_weapon(501U));
        initial.item_ownership.claimed_drop_bits[1] = 0x400U;
        initial.item_ownership.next_item_sequence = 77U;
        auto healthy = make_store(directory.path);
        ARPG_REQUIRE(healthy.commit(initial).state
            == persistence::SaveCommitState::committed);

        dungeon::DungeonSession session{{}, initial};
        const auto before_build = arpg::test::player_build(session);
        ARPG_REQUIRE(session.request_equip(501U)
            == dungeon::RequestResult::accepted);
        const dungeon::PendingSave* const pending = session.pending_save_view();
        ARPG_REQUIRE(pending != nullptr);
        const dungeon::DungeonRunState expected = pending->next_state;
        dungeon::DungeonSession expected_session{{}, expected};
        const auto expected_build = arpg::test::player_build(expected_session);

        persistence::SaveCommitResult saved{};
        if (disposition == persistence::SaveCommitState::committed) {
            saved = healthy.commit(expected);
        } else {
            FaultContext fault{
                disposition == persistence::SaveCommitState::not_committed
                    ? persistence::SaveFaultPoint::before_publish
                    : persistence::SaveFaultPoint::after_publish,
                false,
            };
            auto faulty = make_store(directory.path, &fault);
            saved = faulty.commit(expected);
        }
        ARPG_REQUIRE(saved.state == disposition);
        session.resolve_pending_save(to_session_result(saved));
        ARPG_REQUIRE(session.snapshot().phase
            == (disposition == persistence::SaveCommitState::indeterminate
                ? dungeon::RoomPhase::faulted : dungeon::RoomPhase::locked));
        ARPG_REQUIRE(same_ownership(session.item_state(),
            disposition == persistence::SaveCommitState::committed
                ? expected.item_ownership : initial.item_ownership));
        ARPG_REQUIRE(same_build(arpg::test::player_build(session),
            disposition == persistence::SaveCommitState::committed
                ? expected_build : before_build));

        auto restarted_store = make_store(directory.path);
        const auto loaded = restarted_store.load();
        ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
        const auto& disk_winner =
            disposition == persistence::SaveCommitState::not_committed
                ? initial : expected;
        ARPG_REQUIRE(loaded.checkpoint.commit_generation
            == disk_winner.commit_generation);
        ARPG_REQUIRE(same_ownership(
            loaded.checkpoint.item_ownership, disk_winner.item_ownership));
        dungeon::DungeonSession restarted{{}, loaded.checkpoint};
        ARPG_REQUIRE(same_ownership(
            restarted.item_state(), disk_winner.item_ownership));
        ARPG_REQUIRE(same_build(arpg::test::player_build(restarted),
            disposition == persistence::SaveCommitState::not_committed
                ? before_build : expected_build));
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"initial generation one round trips descriptor", &initial_generation_one_round_trips_descriptor},
    {"committed door transition restarts in next room", &committed_door_transition_restarts_in_next_room},
    {"pre publish failure keeps old room in memory and on disk", &pre_publish_failure_keeps_old_room_in_memory_and_on_disk},
    {"lost post publish receipt faults session but restart uses new room", &lost_post_publish_receipt_faults_session_but_restart_uses_new_room},
    {"committed descent restarts without rerolling hole or abyss", &committed_descent_restarts_without_rerolling_hole_or_abyss},
    {"reset reopen and repeated load keep persisted room fields", &reset_reopen_and_repeated_load_keep_persisted_room_fields},
    {"passive fault recovery never persists partial points or bits", &passive_fault_recovery_never_persists_partial_points_or_bits},
    {"equipment dispositions restart with exact disk winner", &equipment_dispositions_restart_with_exact_disk_winner},
};

}  // namespace

arpg::test::TestSuite dungeon_save_integration_suite() noexcept {
    return arpg::test::make_suite("dungeon_save_integration", kCases);
}
