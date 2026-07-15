#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "dungeon_test_support.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace {

using arpg::dungeon::DungeonFault;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::PendingSaveKind;
using arpg::dungeon::RequestResult;
using arpg::dungeon::RoomPhase;
using arpg::dungeon::SaveDisposition;
using arpg::items::ItemInstance;
using arpg::items::ItemRarity;
using arpg::items::ItemSlot;

ItemInstance normal_item(std::uint64_t id,
    std::uint8_t base_id,
    std::uint8_t level = 95U) noexcept {
    ItemInstance item{};
    item.id = id;
    item.base_id = base_id;
    item.rarity = ItemRarity::normal;
    item.item_level = level;
    item.required_level = 1U;
    return item;
}

ItemInstance level_16_magic_weapon(std::uint64_t id) noexcept {
    ItemInstance item = normal_item(id, 1U, 16U);
    item.rarity = ItemRarity::magic;
    item.required_level = 16U;
    item.affixes[0] = {3U, 7U, 0xFFU};
    item.affix_count = 1U;
    return item;
}

DungeonRunState state_with_items(
    std::initializer_list<ItemInstance> items) {
    DungeonRunState state = arpg::dungeon::make_initial_run_state(
        0x81818181ULL, DungeonRules{}).state;
    state.item_ownership.items.assign(items.begin(), items.end());
    state.item_ownership.next_item_sequence = 9U;
    return state;
}

bool commit_pending(DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()) return false;
    session.resolve_pending_save({SaveDisposition::committed,
        pending->expected_generation, pending->next_state});
    return session.snapshot().phase != RoomPhase::faulted;
}

bool clear_to_awaiting_exit(DungeonSession& session) noexcept {
    arpg::test::EventSummary events{};
    if (!arpg::test::drive_until_cleared(session, events)) return false;
    if (session.snapshot().phase == RoomPhase::cleared) session.tick({});
    return session.snapshot().phase == RoomPhase::awaiting_exit;
}

bool same_build(const arpg::combat::PlayerCombatBuild& left,
    const arpg::combat::PlayerCombatBuild& right) noexcept {
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

struct AtomicStateProbe final {
    RoomPhase phase{RoomPhase::faulted};
    std::uint64_t generation{};
    std::array<std::uint64_t, 8> item_ids{};
    std::size_t item_count{};
    arpg::items::EquipmentState equipment{};
    std::uint64_t next_item_sequence{};
    arpg::combat::PlayerCombatBuild build{};
    int hp{};
    int max_hp{};
    int barrier{};
    int max_barrier{};
    bool pending{};
};

AtomicStateProbe capture_atomic_state(
    const DungeonSession& session) noexcept {
    AtomicStateProbe result{};
    const auto snapshot = session.snapshot();
    result.phase = snapshot.phase;
    result.generation = snapshot.commit_generation;
    result.pending = snapshot.pending_save_kind.has_value();
    const auto& ownership = session.item_state();
    result.item_count = ownership.items.size();
    for (std::size_t index = 0U;
         index < ownership.items.size() && index < result.item_ids.size();
         ++index) {
        result.item_ids[index] = ownership.items[index].id;
    }
    result.equipment = ownership.equipment;
    result.next_item_sequence = ownership.next_item_sequence;
    result.build = arpg::test::player_build(session);
    if (snapshot.combat.has_value()) {
        result.hp = snapshot.combat->player.hp;
        result.max_hp = snapshot.combat->player.max_hp;
        result.barrier = snapshot.combat->player.barrier;
        result.max_barrier = snapshot.combat->player.max_barrier;
    }
    return result;
}

bool same_atomic_state(const AtomicStateProbe& left,
    const AtomicStateProbe& right) noexcept {
    return left.phase == right.phase
        && left.generation == right.generation
        && left.item_ids == right.item_ids
        && left.item_count == right.item_count
        && left.equipment.equipped_ids == right.equipment.equipped_ids
        && left.next_item_sequence == right.next_item_sequence
        && same_build(left.build, right.build)
        && left.hp == right.hp && left.max_hp == right.max_hp
        && left.barrier == right.barrier
        && left.max_barrier == right.max_barrier
        && left.pending == right.pending;
}

arpg::test::Failure item_snapshot_is_lightweight_and_reports_pending_kind() noexcept {
    DungeonRunState state = state_with_items({
        normal_item(11U, 2U), normal_item(12U, 3U)});
    state.item_ownership.equipment.equipped_ids[2] = 12U;
    DungeonSession session{DungeonRules{}, state};
    const auto before = session.snapshot();
    ARPG_REQUIRE(before.inventory_count == 1U);
    ARPG_REQUIRE(before.equipped_ids[2] == 12U);
    ARPG_REQUIRE(before.ground_item_count == 0U);
    for (const auto& ground : before.ground_items) {
        ARPG_REQUIRE(ground.item_id == 0U);
    }
    ARPG_REQUIRE(!before.pending_save_kind.has_value());

    ARPG_REQUIRE(session.request_equip(11U) == RequestResult::accepted);
    const auto pending = session.snapshot();
    ARPG_REQUIRE(pending.inventory_count == 1U);
    ARPG_REQUIRE(pending.equipped_ids[1] == 0U);
    ARPG_REQUIRE(pending.pending_save_kind == PendingSaveKind::equipment);
    return {};
}

arpg::test::Failure equip_rejects_missing_level_and_no_change_requests() noexcept {
    const ItemInstance helmet = normal_item(21U, 2U);
    const ItemInstance high_level = level_16_magic_weapon(22U);
    ARPG_REQUIRE(arpg::items::validate_item(high_level));
    DungeonRunState state = state_with_items({helmet, high_level});
    state.item_ownership.equipment.equipped_ids[1] = helmet.id;
    DungeonSession session{DungeonRules{}, state};

    ARPG_REQUIRE(session.request_equip(999U) == RequestResult::rejected);
    ARPG_REQUIRE(session.request_equip(high_level.id) == RequestResult::rejected);
    ARPG_REQUIRE(session.request_equip(helmet.id) == RequestResult::rejected);
    ARPG_REQUIRE(session.request_unequip(ItemSlot::weapon)
        == RequestResult::rejected);
    ARPG_REQUIRE(session.request_unequip(ItemSlot::count)
        == RequestResult::rejected);
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(session.item_state().equipment.equipped_ids[1] == helmet.id);
    return {};
}

arpg::test::Failure equip_publishes_only_after_commit_and_preserves_health() noexcept {
    DungeonSession session{DungeonRules{},
        state_with_items({normal_item(31U, 2U), normal_item(32U, 3U)})};
    session.tick({});
    const int base_max_hp = session.snapshot().combat->player.max_hp;
    arpg::test::damage_current_player(session, 30);
    const auto damaged = session.snapshot();
    ARPG_REQUIRE(damaged.phase == RoomPhase::combat);
    ARPG_REQUIRE(damaged.combat->player.max_hp == base_max_hp);
    ARPG_REQUIRE(damaged.combat->player.hp < damaged.combat->player.max_hp);
    const int damaged_hp = damaged.combat->player.hp;

    ARPG_REQUIRE(session.request_equip(31U) == RequestResult::accepted);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::committing);
    ARPG_REQUIRE(session.item_state().equipment.equipped_ids[1] == 0U);
    ARPG_REQUIRE(session.snapshot().combat->player.max_hp == base_max_hp);
    const auto first = *session.pending_save();
    session.resolve_pending_save({SaveDisposition::not_committed,
        first.expected_generation, first.next_state});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::combat);
    ARPG_REQUIRE(session.item_state().equipment.equipped_ids[1] == 0U);
    ARPG_REQUIRE(session.snapshot().combat->player.max_hp == base_max_hp);

    ARPG_REQUIRE(session.request_equip(31U) == RequestResult::accepted);
    ARPG_REQUIRE(commit_pending(session));
    const auto equipped = session.snapshot();
    ARPG_REQUIRE(equipped.phase == RoomPhase::combat);
    ARPG_REQUIRE(session.item_state().equipment.equipped_ids[1] == 31U);
    ARPG_REQUIRE(equipped.combat->player.max_hp == base_max_hp + 38);
    ARPG_REQUIRE(equipped.combat->player.hp == damaged_hp);

    ARPG_REQUIRE(session.request_equip(32U) == RequestResult::accepted);
    ARPG_REQUIRE(commit_pending(session));
    const auto barrier_equipped = session.snapshot();
    ARPG_REQUIRE(barrier_equipped.combat->player.max_barrier == 46);
    ARPG_REQUIRE(barrier_equipped.combat->player.barrier == 0);
    ARPG_REQUIRE(barrier_equipped.combat->player.hp == damaged_hp);

    ARPG_REQUIRE(session.request_unequip(ItemSlot::helmet)
        == RequestResult::accepted);
    ARPG_REQUIRE(commit_pending(session));
    const auto unequipped = session.snapshot();
    ARPG_REQUIRE(unequipped.combat->player.max_hp == base_max_hp);
    ARPG_REQUIRE(unequipped.combat->player.hp == damaged_hp);
    return {};
}

arpg::test::Failure ownership_scratch_oom_rejects_atomically() noexcept {
    DungeonSession session{DungeonRules{}, state_with_items({
        normal_item(131U, 2U), normal_item(132U, 3U)})};
    const AtomicStateProbe before = capture_atomic_state(session);
    RequestResult result{};
    {
        arpg::test::ScopedAllocationFailure fail{0U};
        result = session.request_equip(131U);
    }
    ARPG_REQUIRE(result == RequestResult::rejected);
    ARPG_REQUIRE(same_atomic_state(before, capture_atomic_state(session)));
    return {};
}

arpg::test::Failure stable_vector_copy_oom_rejects_atomically() noexcept {
    DungeonSession session{DungeonRules{}, state_with_items({
        normal_item(141U, 2U), normal_item(142U, 3U)})};
    const AtomicStateProbe before = capture_atomic_state(session);
    RequestResult result{};
    {
        // Ownership validation allocates first; the stable item-vector copy
        // is the second allocation and must be caught by request_equip.
        arpg::test::ScopedAllocationFailure fail{1U};
        result = session.request_equip(141U);
    }
    ARPG_REQUIRE(result == RequestResult::rejected);
    ARPG_REQUIRE(same_atomic_state(before, capture_atomic_state(session)));
    return {};
}

arpg::test::Failure recipe_post_copy_scratch_oom_rejects_atomically() noexcept {
    DungeonSession session{DungeonRules{}, state_with_items({
        normal_item(151U, 2U), normal_item(152U, 2U),
        normal_item(153U, 2U), normal_item(154U, 3U)})};
    const AtomicStateProbe before = capture_atomic_state(session);
    RequestResult result{};
    {
        // After validation and the single stable-state vector copy, deleting
        // three items leaves capacity for the one product, so append cannot
        // grow. This third allocation is prepare_item_save's validation
        // scratch and exercises rollback after the local recipe mutation.
        arpg::test::ScopedAllocationFailure fail{2U};
        result = session.request_recipe({{151U, 152U, 153U}});
    }
    ARPG_REQUIRE(result == RequestResult::rejected);
    ARPG_REQUIRE(same_atomic_state(before, capture_atomic_state(session)));
    return {};
}

arpg::test::Failure build_projection_scratch_oom_rejects_atomically() noexcept {
    DungeonSession session{DungeonRules{}, state_with_items({
        normal_item(161U, 2U), normal_item(162U, 3U)})};
    const AtomicStateProbe before = capture_atomic_state(session);
    RequestResult result{};
    {
        // Stable validation, vector copy, and candidate validation succeed;
        // detailed equipment projection then reports allocation_failure.
        arpg::test::ScopedAllocationFailure fail{3U};
        result = session.request_equip(161U);
    }
    ARPG_REQUIRE(result == RequestResult::rejected);
    ARPG_REQUIRE(same_atomic_state(before, capture_atomic_state(session)));
    return {};
}

arpg::test::Failure committed_receipt_publishes_without_allocation() noexcept {
    DungeonSession session{DungeonRules{},
        state_with_items({normal_item(165U, 2U)})};
    session.tick({});
    arpg::test::damage_current_player(session, 30);
    const auto before = session.snapshot();
    ARPG_REQUIRE(before.phase == RoomPhase::combat);
    ARPG_REQUIRE(before.combat.has_value());
    const int hp_before = before.combat->player.hp;
    const int barrier_before = before.combat->player.barrier;
    const int max_hp_before = before.combat->player.max_hp;
    const int max_barrier_before = before.combat->player.max_barrier;

    ARPG_REQUIRE(session.request_equip(165U) == RequestResult::accepted);
    const auto pending = *session.pending_save();
    const arpg::dungeon::PendingSaveResult receipt{
        SaveDisposition::committed,
        pending.expected_generation,
        pending.next_state,
    };
    const std::uint64_t allocations_before =
        arpg::test::allocation_count();
    {
        arpg::test::ScopedAllocationFailure fail{0U};
        session.resolve_pending_save(receipt);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    const auto after = session.snapshot();
    ARPG_REQUIRE(after.phase == RoomPhase::combat);
    ARPG_REQUIRE(!after.pending_save_kind.has_value());
    ARPG_REQUIRE(session.item_state().equipment.equipped_ids[1] == 165U);
    ARPG_REQUIRE(after.commit_generation == pending.expected_generation);
    ARPG_REQUIRE(after.combat.has_value());
    ARPG_REQUIRE(after.combat->player.max_hp == max_hp_before + 38);
    ARPG_REQUIRE(after.combat->player.hp == hp_before);
    ARPG_REQUIRE(after.combat->player.max_barrier == max_barrier_before);
    ARPG_REQUIRE(after.combat->player.barrier == barrier_before);
    ARPG_REQUIRE(arpg::test::player_build(session).values.max_health
        == 38 * arpg::modifiers::kFixedOne);
    return {};
}

arpg::test::Failure commit_generation_overflow_faults_without_publish() noexcept {
    DungeonRunState state = state_with_items({normal_item(171U, 2U)});
    state.commit_generation = (std::numeric_limits<std::uint64_t>::max)();
    DungeonSession session{DungeonRules{}, state};
    const AtomicStateProbe before = capture_atomic_state(session);
    ARPG_REQUIRE(session.request_equip(171U) == RequestResult::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::commit_generation_overflow);
    ARPG_REQUIRE(session.item_state().equipment.equipped_ids[1] == 0U);
    ARPG_REQUIRE(session.item_state().next_item_sequence
        == before.next_item_sequence);
    ARPG_REQUIRE(session.snapshot().commit_generation == before.generation);
    const AtomicStateProbe after = capture_atomic_state(session);
    ARPG_REQUIRE(after.item_ids == before.item_ids);
    ARPG_REQUIRE(after.equipment.equipped_ids
        == before.equipment.equipped_ids);
    ARPG_REQUIRE(same_build(after.build, before.build));
    ARPG_REQUIRE(after.hp == before.hp && after.barrier == before.barrier);
    return {};
}

arpg::test::Failure item_transactions_restore_locked_and_awaiting_phases() noexcept {
    DungeonSession locked{DungeonRules{},
        state_with_items({normal_item(181U, 2U)})};
    ARPG_REQUIRE(locked.snapshot().phase == RoomPhase::locked);
    ARPG_REQUIRE(locked.request_equip(181U) == RequestResult::accepted);
    const auto locked_pending = *locked.pending_save();
    locked.resolve_pending_save({SaveDisposition::not_committed,
        locked_pending.expected_generation, locked_pending.next_state});
    ARPG_REQUIRE(locked.snapshot().phase == RoomPhase::locked);
    ARPG_REQUIRE(locked.item_state().equipment.equipped_ids[1] == 0U);

    DungeonSession awaiting{DungeonRules{},
        state_with_items({normal_item(182U, 3U)})};
    ARPG_REQUIRE(clear_to_awaiting_exit(awaiting));
    ARPG_REQUIRE(awaiting.request_equip(182U) == RequestResult::accepted);
    ARPG_REQUIRE(commit_pending(awaiting));
    ARPG_REQUIRE(awaiting.snapshot().phase == RoomPhase::awaiting_exit);
    ARPG_REQUIRE(awaiting.item_state().equipment.equipped_ids[2] == 182U);
    return {};
}

arpg::test::Failure invalid_loaded_equipment_ownership_faults() noexcept {
    DungeonRunState state = state_with_items({normal_item(191U, 2U)});
    state.progression = {2U, 0U, 1U, 0U};
    state.passive_tree.allocated_bits =
        (std::uint64_t{1U} << 0U) | (std::uint64_t{1U} << 2U);
    state.item_ownership.items[0].reserved[0] = 1U;
    state.item_ownership.equipment.equipped_ids[1] = 191U;
    DungeonSession session{DungeonRules{}, state};
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::invalid_item_state);
    return {};
}

arpg::test::Failure equipment_indeterminate_save_faults() noexcept {
    DungeonSession session{DungeonRules{},
        state_with_items({normal_item(41U, 2U)})};
    ARPG_REQUIRE(session.request_equip(41U) == RequestResult::accepted);
    session.resolve_pending_save({SaveDisposition::indeterminate, 0U, {}});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::save_commit_indeterminate);
    ARPG_REQUIRE(session.item_state().equipment.equipped_ids[1] == 0U);
    return {};
}

arpg::test::Failure recipe_is_atomic_ordered_and_retry_deterministic() noexcept {
    DungeonRunState state = state_with_items({normal_item(51U, 2U),
        normal_item(52U, 2U), normal_item(53U, 2U), normal_item(54U, 3U)});
    DungeonSession session{DungeonRules{}, state};
    const std::array<std::uint64_t, 3> ids{{51U, 52U, 53U}};
    ARPG_REQUIRE(session.request_recipe(ids) == RequestResult::accepted);
    const auto first = *session.pending_save();
    ARPG_REQUIRE(first.kind == PendingSaveKind::recipe);
    ARPG_REQUIRE(session.item_state().items.size() == 4U);
    ARPG_REQUIRE(first.next_state.item_ownership.items.size() == 2U);
    ARPG_REQUIRE(first.next_state.item_ownership.items[0].id == 54U);
    const std::uint64_t product_id =
        first.next_state.item_ownership.items[1].id;
    ARPG_REQUIRE(product_id != 0U);
    ARPG_REQUIRE(first.next_state.item_ownership.next_item_sequence == 10U);

    session.resolve_pending_save({SaveDisposition::not_committed,
        first.expected_generation, first.next_state});
    ARPG_REQUIRE(session.item_state().items.size() == 4U);
    ARPG_REQUIRE(session.item_state().next_item_sequence == 9U);
    ARPG_REQUIRE(session.request_recipe(ids) == RequestResult::accepted);
    const auto second = *session.pending_save();
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        first.next_state, second.next_state));
    ARPG_REQUIRE(commit_pending(session));
    ARPG_REQUIRE(session.item_state().items.size() == 2U);
    ARPG_REQUIRE(session.item_state().items[0].id == 54U);
    ARPG_REQUIRE(session.item_state().items[1].id == product_id);
    ARPG_REQUIRE(session.item_state().next_item_sequence == 10U);
    return {};
}

arpg::test::Failure recipe_rejects_invalid_or_equipped_materials() noexcept {
    DungeonRunState state = state_with_items({normal_item(61U, 2U),
        normal_item(62U, 2U), normal_item(63U, 2U), normal_item(64U, 3U)});
    state.item_ownership.equipment.equipped_ids[1] = 61U;
    DungeonSession session{DungeonRules{}, state};
    ARPG_REQUIRE(session.request_recipe({{61U, 62U, 63U}})
        == RequestResult::rejected);
    ARPG_REQUIRE(session.request_recipe({{62U, 62U, 63U}})
        == RequestResult::rejected);
    ARPG_REQUIRE(session.request_recipe({{62U, 63U, 64U}})
        == RequestResult::rejected);
    ARPG_REQUIRE(session.request_recipe({{62U, 63U, 999U}})
        == RequestResult::rejected);
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(session.item_state().items.size() == 4U);
    return {};
}

arpg::test::Failure equipment_receipt_compares_complete_item_state() noexcept {
    DungeonSession session{DungeonRules{},
        state_with_items({normal_item(71U, 2U)})};
    ARPG_REQUIRE(session.request_equip(71U) == RequestResult::accepted);
    const auto pending = *session.pending_save();
    DungeonRunState wrong = pending.next_state;
    wrong.item_ownership.items[0].item_level = 94U;
    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation, wrong});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    return {};
}

arpg::test::Failure recipe_product_collision_faults_without_mutation() noexcept {
    const ItemInstance a = normal_item(81U, 2U);
    const ItemInstance b = normal_item(82U, 2U);
    const ItemInstance c = normal_item(83U, 2U);
    const auto product = arpg::items::generate_recipe_item(
        0x81818181ULL, 9U, a, b, c);
    ARPG_REQUIRE(product.has_value());
    ItemInstance collision = normal_item(product->id, 3U);
    DungeonSession session{DungeonRules{},
        state_with_items({a, b, c, collision})};
    ARPG_REQUIRE(session.request_recipe({{a.id, b.id, c.id}})
        == RequestResult::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::item_id_collision);
    ARPG_REQUIRE(session.item_state().items.size() == 4U);
    ARPG_REQUIRE(session.item_state().next_item_sequence == 9U);
    return {};
}

arpg::test::Failure recipe_sequence_overflow_has_explicit_fault() noexcept {
    DungeonRunState state = state_with_items({normal_item(91U, 2U),
        normal_item(92U, 2U), normal_item(93U, 2U)});
    state.item_ownership.next_item_sequence =
        (std::numeric_limits<std::uint64_t>::max)();
    DungeonSession session{DungeonRules{}, state};
    ARPG_REQUIRE(session.request_recipe({{91U, 92U, 93U}})
        == RequestResult::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::item_sequence_overflow);
    ARPG_REQUIRE(session.item_state().items.size() == 3U);
    return {};
}

arpg::test::Failure invalid_loaded_progression_tree_is_faulted() noexcept {
    DungeonRunState state = state_with_items({normal_item(101U, 2U)});
    state.progression.earned_passive_points = 1U;
    state.progression.unspent_passive_points = 0U;
    DungeonSession session{DungeonRules{}, state};
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::invalid_item_state);
    return {};
}

arpg::test::Failure room_build_combines_passives_and_equipment() noexcept {
    DungeonRunState passive_state = state_with_items({});
    passive_state.progression = {2U, 0U, 1U, 0U};
    passive_state.passive_tree.allocated_bits =
        (std::uint64_t{1U} << 0U) | (std::uint64_t{1U} << 2U);
    DungeonSession passive_only{DungeonRules{}, passive_state};
    const auto passive_snapshot = passive_only.snapshot();
    ARPG_REQUIRE(passive_snapshot.combat.has_value());
    ARPG_REQUIRE(passive_snapshot.combat->player.max_barrier == 10);

    DungeonRunState combined_state = passive_state;
    combined_state.item_ownership.items.push_back(normal_item(111U, 2U));
    combined_state.item_ownership.equipment.equipped_ids[1] = 111U;
    DungeonSession combined{DungeonRules{}, combined_state};
    const auto combined_snapshot = combined.snapshot();
    ARPG_REQUIRE(combined_snapshot.combat.has_value());
    ARPG_REQUIRE(combined_snapshot.combat->player.max_barrier
        == passive_snapshot.combat->player.max_barrier);
    ARPG_REQUIRE(combined_snapshot.combat->player.max_hp
        == passive_snapshot.combat->player.max_hp + 38);
    return {};
}

arpg::test::Failure same_run_state_compares_all_item_ownership_fields() noexcept {
    DungeonRunState original = state_with_items({
        normal_item(121U, 2U), normal_item(122U, 3U)});
    original.item_ownership.equipment.equipped_ids[1] = 121U;
    DungeonRunState changed = original;
    ARPG_REQUIRE(arpg::dungeon::same_run_state(original, changed));

    changed.item_ownership.items[0].affixes[5].variant = 1U;
    ARPG_REQUIRE(!arpg::dungeon::same_run_state(original, changed));
    changed = original;
    std::swap(changed.item_ownership.items[0],
        changed.item_ownership.items[1]);
    ARPG_REQUIRE(!arpg::dungeon::same_run_state(original, changed));
    changed = original;
    changed.item_ownership.equipment.equipped_ids[1] = 0U;
    ARPG_REQUIRE(!arpg::dungeon::same_run_state(original, changed));
    changed = original;
    changed.item_ownership.claimed_drop_bits[2] = 1U;
    ARPG_REQUIRE(!arpg::dungeon::same_run_state(original, changed));
    changed = original;
    ++changed.item_ownership.next_item_sequence;
    ARPG_REQUIRE(!arpg::dungeon::same_run_state(original, changed));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"item snapshot lightweight pending fields",
        &item_snapshot_is_lightweight_and_reports_pending_kind},
    {"equip rejects invalid requests",
        &equip_rejects_missing_level_and_no_change_requests},
    {"equip publishes on commit and preserves health",
        &equip_publishes_only_after_commit_and_preserves_health},
    {"equipment indeterminate faults", &equipment_indeterminate_save_faults},
    {"recipe atomic order and retry",
        &recipe_is_atomic_ordered_and_retry_deterministic},
    {"recipe rejects invalid materials",
        &recipe_rejects_invalid_or_equipped_materials},
    {"equipment receipt compares complete item state",
        &equipment_receipt_compares_complete_item_state},
    {"recipe product collision faults",
        &recipe_product_collision_faults_without_mutation},
    {"recipe sequence overflow faults",
        &recipe_sequence_overflow_has_explicit_fault},
    {"invalid loaded progression tree faults",
        &invalid_loaded_progression_tree_is_faulted},
    {"room build combines passives and equipment",
        &room_build_combines_passives_and_equipment},
    {"same run state compares item ownership",
        &same_run_state_compares_all_item_ownership_fields},
    {"ownership scratch oom is atomic",
        &ownership_scratch_oom_rejects_atomically},
    {"stable vector copy oom is atomic",
        &stable_vector_copy_oom_rejects_atomically},
    {"recipe post-copy scratch oom is atomic",
        &recipe_post_copy_scratch_oom_rejects_atomically},
    {"build projection scratch oom is atomic",
        &build_projection_scratch_oom_rejects_atomically},
    {"commit generation overflow faults",
        &commit_generation_overflow_faults_without_publish},
    {"item transaction phase restoration",
        &item_transactions_restore_locked_and_awaiting_phases},
    {"invalid loaded equipment ownership faults",
        &invalid_loaded_equipment_ownership_faults},
    {"committed receipt publishes without allocation",
        &committed_receipt_publishes_without_allocation},
};

}  // namespace

arpg::test::TestSuite dungeon_item_transaction_suite() noexcept {
    return arpg::test::make_suite("dungeon_item_transaction", kCases);
}
