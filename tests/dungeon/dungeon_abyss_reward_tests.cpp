#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "dungeon_test_support.hpp"

#include "dungeon/abyss_reward.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "items/item_generation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using arpg::abyss::AbyssDanger;
using arpg::dungeon::DungeonFault;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::GroundItem;
using arpg::dungeon::GroundItemSource;
using arpg::dungeon::PendingSaveKind;
using arpg::dungeon::RoomPhase;
using arpg::dungeon::SaveDisposition;

arpg::dungeon::DungeonRunState cleared_abyss_state(
    AbyssDanger wanted,
    std::uint64_t depth = 20U) noexcept {
    DungeonRunState state = arpg::dungeon::make_initial_run_state(
        0xA9B9555EEDULL, DungeonRules{}).state;
    for (std::uint64_t seed = 1U; seed != 0U; ++seed) {
        const auto selection = arpg::abyss::select_abyss_rule(seed, depth);
        if (!selection.has_value() || selection->danger != wanted) continue;
        state.current_room.seed = seed;
        state.current_room.depth = depth;
        state.current_room.is_abyss = true;
        state.current_room.has_hole = true;
        state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::cleared;
        state.abyss.danger = selection->danger;
        state.abyss.rule = selection->rule;
        state.abyss.rules_version = selection->rules_version;
        state.abyss.reward_total = arpg::abyss::reward_profile_for(
            selection->danger, 1U).item_count;
        return state;
    }
    return {};
}

arpg::items::ItemInstance normal_item(std::uint64_t id) noexcept {
    return arpg::items::generate_item({
        id ^ 0x91E10DA5ULL,
        arpg::items::ItemSlot::weapon,
        20U,
        id,
        arpg::items::ItemRarity::normal,
    }).value();
}

bool same_item(
    const arpg::items::ItemInstance& left,
    const arpg::items::ItemInstance& right) noexcept {
    if (left.id != right.id || left.base_id != right.base_id
            || left.rarity != right.rarity
            || left.item_level != right.item_level
            || left.required_level != right.required_level
            || left.affix_count != right.affix_count
            || left.reserved != right.reserved) return false;
    for (std::size_t index = 0U; index < left.affixes.size(); ++index) {
        if (left.affixes[index].affix_id != right.affixes[index].affix_id
                || left.affixes[index].tier != right.affixes[index].tier
                || left.affixes[index].variant
                    != right.affixes[index].variant) return false;
    }
    return true;
}

bool same_ground(const GroundItem& left, const GroundItem& right) noexcept {
    return left.active == right.active
        && left.drop_ordinal == right.drop_ordinal
        && left.source == right.source
        && left.abyss_reward_ordinal == right.abyss_reward_ordinal
        && left.position.x == right.position.x
        && left.position.y == right.position.y
        && left.position.z == right.position.z
        && same_item(left.item, right.item);
}

const GroundItem* abyss_ground(
    const DungeonSession& session,
    std::uint8_t reward_ordinal) noexcept {
    for (const GroundItem& ground : arpg::test::ground_items(session)) {
        if (ground.active && ground.source == GroundItemSource::abyss_chest
                && ground.abyss_reward_ordinal == reward_ordinal) {
            return &ground;
        }
    }
    return nullptr;
}

bool commit_pending(DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()) return false;
    session.resolve_pending_save({SaveDisposition::committed,
        pending->expected_generation, pending->next_state});
    return session.snapshot().phase != RoomPhase::faulted;
}

void fill_ground_pool(DungeonSession& session,
    std::uint16_t except = 0xFFFFU) noexcept {
    for (std::uint16_t index = 0U;
         index < arpg::dungeon::kGroundDropCapacity; ++index) {
        if (index == except) continue;
        arpg::test::install_ground_item(session, index,
            normal_item(0x100000U + index), {9.0F, 4.0F, 0.0F});
    }
}

arpg::test::Failure reward_profiles_levels_and_ordinals_are_exact() noexcept {
    constexpr std::array<AbyssDanger, 3> dangers{{
        AbyssDanger::low, AbyssDanger::medium, AbyssDanger::high}};
    constexpr std::array<std::uint8_t, 3> counts{{1U, 2U, 3U}};
    constexpr std::array<std::uint8_t, 3> levels{{21U, 23U, 25U}};
    for (std::size_t danger = 0U; danger < dangers.size(); ++danger) {
        for (std::uint8_t ordinal = 0U; ordinal < counts[danger]; ++ordinal) {
            const auto slot = arpg::dungeon::derive_abyss_reward_slot(
                0x12345678ULL, dangers[danger], 20U, ordinal);
            ARPG_REQUIRE(slot.has_value());
            ARPG_REQUIRE(slot->slot_index == ordinal);
            ARPG_REQUIRE(slot->item_level == levels[danger]);
            ARPG_REQUIRE(slot->item_id != 0U);
        }
        ARPG_REQUIRE(!arpg::dungeon::derive_abyss_reward_slot(
            0x12345678ULL, dangers[danger], 20U, counts[danger]).has_value());
        const auto capped = arpg::dungeon::derive_abyss_reward_slot(
            0x12345678ULL, dangers[danger], 100U, 0U);
        ARPG_REQUIRE(capped.has_value());
        ARPG_REQUIRE(capped->item_level == 100U);
    }
    ARPG_REQUIRE(!arpg::dungeon::derive_abyss_reward_slot(
        1U, AbyssDanger::low, 0U, 0U).has_value());

    DungeonSession maximum_depth{DungeonRules{}, cleared_abyss_state(
        AbyssDanger::high,
        (std::numeric_limits<std::uint64_t>::max)())};
    maximum_depth.tick({});
    ARPG_REQUIRE(commit_pending(maximum_depth));
    ARPG_REQUIRE(abyss_ground(maximum_depth, 0U)->item.item_level == 100U);
    return {};
}

arpg::test::Failure shifted_rarity_rolls_follow_danger_ordering() noexcept {
    std::array<std::uint32_t, 3> normal{};
    std::array<std::uint32_t, 3> rare{};
    for (std::uint64_t seed = 1U; seed <= 4096U; ++seed) {
        for (std::size_t danger = 0U; danger < 3U; ++danger) {
            const auto slot = arpg::dungeon::derive_abyss_reward_slot(
                seed, static_cast<AbyssDanger>(danger), 20U, 0U);
            ARPG_REQUIRE(slot.has_value());
            if (slot->rarity == arpg::items::ItemRarity::normal)
                ++normal[danger];
            if (slot->rarity == arpg::items::ItemRarity::rare)
                ++rare[danger];
        }
    }
    ARPG_REQUIRE(normal[0] > normal[1]);
    ARPG_REQUIRE(normal[1] > normal[2]);
    ARPG_REQUIRE(rare[0] < rare[1]);
    ARPG_REQUIRE(rare[1] < rare[2]);
    return {};
}

arpg::test::Failure ordinal_streams_are_stable_and_allow_repeated_slots() noexcept {
    bool found_repeat = false;
    for (std::uint64_t seed = 1U; seed <= 4096U; ++seed) {
        const auto first = arpg::dungeon::derive_abyss_reward_slot(
            seed, AbyssDanger::high, 50U, 0U);
        const auto second = arpg::dungeon::derive_abyss_reward_slot(
            seed, AbyssDanger::high, 50U, 1U);
        const auto third = arpg::dungeon::derive_abyss_reward_slot(
            seed, AbyssDanger::high, 50U, 2U);
        ARPG_REQUIRE(first.has_value() && second.has_value()
            && third.has_value());
        const auto repeat = arpg::dungeon::derive_abyss_reward_slot(
            seed, AbyssDanger::high, 50U, 1U);
        ARPG_REQUIRE(repeat->item_slot == second->item_slot);
        ARPG_REQUIRE(repeat->rarity == second->rarity);
        ARPG_REQUIRE(repeat->item_seed == second->item_seed);
        ARPG_REQUIRE(repeat->item_id == second->item_id);
        if (first->item_slot == second->item_slot
                || first->item_slot == third->item_slot
                || second->item_slot == third->item_slot) {
            found_repeat = true;
            break;
        }
    }
    ARPG_REQUIRE(found_repeat);
    return {};
}

arpg::test::Failure cleared_abyss_starts_hidden_reward_transaction() noexcept {
    DungeonSession session{DungeonRules{}, cleared_abyss_state(AbyssDanger::low)};
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::cleared);
    ARPG_REQUIRE(!session.snapshot().has_active_room);
    ARPG_REQUIRE(session.snapshot().ground_item_count == 0U);

    session.tick({});

    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->kind
        == PendingSaveKind::abyss_reward_materialized);
    ARPG_REQUIRE(session.snapshot().ground_item_count == 0U);
    const auto& stable = arpg::test::stable_state(session);
    ARPG_REQUIRE(stable.abyss.generated_mask == 0U);
    ARPG_REQUIRE(stable.abyss.reward_revision == 0U);
    ARPG_REQUIRE(session.pending_save()->next_state.abyss.generated_mask == 1U);
    ARPG_REQUIRE(session.pending_save()->next_state.abyss.reward_revision == 1U);
    return {};
}

arpg::test::Failure partial_pool_uses_actual_free_ground_index() noexcept {
    DungeonSession session{DungeonRules{}, cleared_abyss_state(AbyssDanger::low)};
    fill_ground_pool(session, 17U);
    session.tick({});
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.snapshot().ground_item_count == 191U);
    ARPG_REQUIRE(commit_pending(session));
    const GroundItem* reward = abyss_ground(session, 0U);
    ARPG_REQUIRE(reward != nullptr);
    ARPG_REQUIRE(reward->drop_ordinal == 17U);
    ARPG_REQUIRE(reward->source == GroundItemSource::abyss_chest);
    ARPG_REQUIRE(reward->abyss_reward_ordinal == 0U);
    return {};
}

arpg::test::Failure full_pool_waits_without_advancing_then_continues() noexcept {
    DungeonSession session{
        DungeonRules{}, cleared_abyss_state(AbyssDanger::medium)};
    fill_ground_pool(session);
    session.tick({});
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.generated_mask == 0U);
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.reward_revision == 0U);

    arpg::test::clear_ground_item(session, 42U);
    session.tick({});
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(commit_pending(session));
    ARPG_REQUIRE(abyss_ground(session, 0U)->drop_ordinal == 42U);
    session.tick({});
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.generated_mask == 1U);

    arpg::test::clear_ground_item(session, 41U);
    session.tick({});
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->next_state.abyss.generated_mask == 3U);
    return {};
}

arpg::test::Failure reward_receipts_fail_closed_without_publication() noexcept {
    const auto state = cleared_abyss_state(AbyssDanger::low);
    DungeonSession not_committed{DungeonRules{}, state};
    not_committed.tick({});
    const auto failed = *not_committed.pending_save();
    not_committed.resolve_pending_save({SaveDisposition::not_committed,
        failed.expected_generation, failed.next_state});
    ARPG_REQUIRE(not_committed.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(not_committed.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(not_committed.snapshot().ground_item_count == 0U);
    ARPG_REQUIRE(arpg::test::stable_state(not_committed).abyss.generated_mask == 0U);

    DungeonSession indeterminate{DungeonRules{}, state};
    indeterminate.tick({});
    indeterminate.resolve_pending_save({SaveDisposition::indeterminate, 0U, {}});
    ARPG_REQUIRE(indeterminate.snapshot().diagnostics.fault
        == DungeonFault::save_commit_indeterminate);
    ARPG_REQUIRE(indeterminate.snapshot().ground_item_count == 0U);

    DungeonSession mismatch{DungeonRules{}, state};
    mismatch.tick({});
    auto wrong = *mismatch.pending_save();
    ++wrong.next_state.abyss.reward_revision;
    mismatch.resolve_pending_save({SaveDisposition::committed,
        wrong.expected_generation, wrong.next_state});
    ARPG_REQUIRE(mismatch.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(mismatch.snapshot().ground_item_count == 0U);

    DungeonSession occupied{DungeonRules{}, state};
    occupied.tick({});
    const auto exact = *occupied.pending_save();
    arpg::test::install_ground_item(
        occupied, 0U, normal_item(0x999999U), {8.0F, 4.0F, 0.0F});
    occupied.resolve_pending_save({SaveDisposition::committed,
        exact.expected_generation, exact.next_state});
    ARPG_REQUIRE(occupied.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(occupied.snapshot().ground_item_count == 1U);
    ARPG_REQUIRE(arpg::test::ground_items(occupied)[0U].source
        == GroundItemSource::monster_drop);
    return {};
}

arpg::test::Failure exact_receipt_publishes_without_allocation() noexcept {
    DungeonSession session{DungeonRules{}, cleared_abyss_state(AbyssDanger::low)};
    session.tick({});
    const auto pending = *session.pending_save();
    const arpg::dungeon::PendingSaveResult receipt{
        SaveDisposition::committed,
        pending.expected_generation,
        pending.next_state,
    };
    const std::uint64_t before = arpg::test::allocation_count();
    {
        arpg::test::ScopedAllocationFailure fail{0U};
        session.resolve_pending_save(receipt);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    ARPG_REQUIRE(session.snapshot().ground_item_count == 1U);
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.generated_mask == 1U);
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.reward_revision == 1U);
    return {};
}

arpg::test::Failure reload_rebuilds_exact_items_without_save_or_combat() noexcept {
    DungeonSession original{
        DungeonRules{}, cleared_abyss_state(AbyssDanger::medium)};
    original.tick({});
    ARPG_REQUIRE(commit_pending(original));
    original.tick({});
    ARPG_REQUIRE(commit_pending(original));
    const GroundItem first = *abyss_ground(original, 0U);
    const GroundItem second = *abyss_ground(original, 1U);
    const DungeonRunState saved = arpg::test::stable_state(original);

    DungeonSession reloaded{DungeonRules{}, saved};
    ARPG_REQUIRE(reloaded.snapshot().phase == RoomPhase::cleared);
    ARPG_REQUIRE(!reloaded.snapshot().has_active_room);
    ARPG_REQUIRE(!reloaded.pending_save().has_value());
    ARPG_REQUIRE(reloaded.snapshot().ground_item_count == 2U);
    ARPG_REQUIRE(same_ground(first, *abyss_ground(reloaded, 0U)));
    ARPG_REQUIRE(same_ground(second, *abyss_ground(reloaded, 1U)));
    ARPG_REQUIRE(arpg::test::stable_state(reloaded).abyss.generated_mask == 3U);
    ARPG_REQUIRE(arpg::test::stable_state(reloaded).abyss.reward_revision == 2U);
    reloaded.tick({});
    ARPG_REQUIRE(!reloaded.pending_save().has_value());
    ARPG_REQUIRE(reloaded.snapshot().phase == RoomPhase::awaiting_exit);

    DungeonRunState claimed = saved;
    claimed.abyss.claimed_mask = 1U;
    DungeonSession claimed_reload{DungeonRules{}, claimed};
    ARPG_REQUIRE(claimed_reload.snapshot().ground_item_count == 1U);
    ARPG_REQUIRE(abyss_ground(claimed_reload, 0U) == nullptr);
    ARPG_REQUIRE(abyss_ground(claimed_reload, 1U) != nullptr);
    return {};
}

arpg::test::Failure retry_content_is_independent_of_pool_and_runtime_state() noexcept {
    DungeonRunState state = cleared_abyss_state(AbyssDanger::low, 35U);
    state.item_ownership.next_item_sequence = 9001U;
    DungeonSession delayed{DungeonRules{}, state};
    fill_ground_pool(delayed);
    delayed.tick({});
    ARPG_REQUIRE(!delayed.pending_save().has_value());
    arpg::test::clear_ground_item(delayed, 77U);
    delayed.tick({});
    ARPG_REQUIRE(commit_pending(delayed));

    state.commit_generation += 99U;
    state.item_ownership.next_item_sequence = 1U;
    DungeonSession immediate{DungeonRules{}, state};
    immediate.tick({});
    ARPG_REQUIRE(commit_pending(immediate));
    ARPG_REQUIRE(same_item(
        abyss_ground(delayed, 0U)->item,
        abyss_ground(immediate, 0U)->item));
    return {};
}

arpg::test::Failure collision_and_revision_overflow_fault_without_retry() noexcept {
    DungeonRunState collision = cleared_abyss_state(AbyssDanger::low);
    const auto plan = arpg::dungeon::derive_abyss_reward_slot(
        collision.current_room.seed, collision.abyss.danger,
        static_cast<std::uint8_t>(collision.current_room.depth), 0U);
    ARPG_REQUIRE(plan.has_value());
    collision.item_ownership.items.push_back(*arpg::items::generate_item({
        plan->item_seed, plan->item_slot, plan->item_level,
        plan->item_id, plan->rarity}));
    DungeonSession collided{DungeonRules{}, collision};
    collided.tick({});
    ARPG_REQUIRE(collided.snapshot().diagnostics.fault
        == DungeonFault::abyss_reward_collision);
    ARPG_REQUIRE(collided.snapshot().ground_item_count == 0U);

    DungeonRunState overflow = cleared_abyss_state(AbyssDanger::low);
    overflow.abyss.reward_revision =
        (std::numeric_limits<std::uint32_t>::max)();
    DungeonSession revision{DungeonRules{}, overflow};
    revision.tick({});
    ARPG_REQUIRE(revision.snapshot().diagnostics.fault
        == DungeonFault::abyss_reward_revision_overflow);
    ARPG_REQUIRE(revision.snapshot().ground_item_count == 0U);
    return {};
}

arpg::test::Failure abyss_pickup_is_reserved_for_task_ten() noexcept {
    DungeonSession session{};
    session.tick({});
    const auto player = session.snapshot().combat->player.position;
    const auto item = normal_item(0xABCDEFU);
    arpg::test::install_abyss_ground_item(
        session, 5U, 0U, item, player);
    ARPG_REQUIRE(session.request_pickup(5U)
        == arpg::dungeon::RequestResult::rejected);
    session.request_nearby_pickups(player);
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(arpg::test::ground_items(session)[5U].active);
    return {};
}

arpg::test::Failure allocation_failure_is_hidden_and_retryable() noexcept {
    DungeonRunState state = cleared_abyss_state(AbyssDanger::low);
    state.item_ownership.items.push_back(normal_item(0x777777U));
    DungeonSession session{DungeonRules{}, state};
    {
        arpg::test::ScopedAllocationFailure fail{0U};
        session.tick({});
    }
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::awaiting_exit);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault == DungeonFault::none);
    ARPG_REQUIRE(session.snapshot().ground_item_count == 0U);
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.generated_mask == 0U);
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.reward_revision == 0U);

    session.tick({});
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->kind
        == PendingSaveKind::abyss_reward_materialized);
    return {};
}

arpg::test::Failure committed_reward_waits_for_reload_pool_space() noexcept {
    DungeonRunState state = cleared_abyss_state(AbyssDanger::low);
    state.abyss.generated_mask = 1U;
    state.abyss.reward_revision = 1U;
    DungeonSession session{DungeonRules{}, state};
    const GroundItem* initial = abyss_ground(session, 0U);
    ARPG_REQUIRE(initial != nullptr);
    arpg::test::clear_ground_item(session, initial->drop_ordinal);
    fill_ground_pool(session);
    session.tick({});
    ARPG_REQUIRE(abyss_ground(session, 0U) == nullptr);
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.generated_mask == 1U);
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.reward_revision == 1U);
    ARPG_REQUIRE(!session.pending_save().has_value());

    arpg::test::clear_ground_item(session, 88U);
    session.tick({});
    ARPG_REQUIRE(abyss_ground(session, 0U) != nullptr);
    ARPG_REQUIRE(abyss_ground(session, 0U)->drop_ordinal == 88U);
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.reward_revision == 1U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"reward profiles levels and ordinals", &reward_profiles_levels_and_ordinals_are_exact},
    {"shifted rarity danger ordering", &shifted_rarity_rolls_follow_danger_ordering},
    {"stable independent ordinal streams", &ordinal_streams_are_stable_and_allow_repeated_slots},
    {"cleared starts hidden transaction", &cleared_abyss_starts_hidden_reward_transaction},
    {"partial pool uses free index", &partial_pool_uses_actual_free_ground_index},
    {"full pool waits and continues", &full_pool_waits_without_advancing_then_continues},
    {"reward receipts fail closed", &reward_receipts_fail_closed_without_publication},
    {"exact receipt no allocation", &exact_receipt_publishes_without_allocation},
    {"reload rebuilds exact items", &reload_rebuilds_exact_items_without_save_or_combat},
    {"retry content stable", &retry_content_is_independent_of_pool_and_runtime_state},
    {"collision and revision overflow", &collision_and_revision_overflow_fault_without_retry},
    {"abyss pickup reserved", &abyss_pickup_is_reserved_for_task_ten},
    {"allocation failure retryable", &allocation_failure_is_hidden_and_retryable},
    {"reload pool space wait", &committed_reward_waits_for_reload_pool_space},
};

}  // namespace

arpg::test::TestSuite dungeon_abyss_reward_suite() noexcept {
    return arpg::test::make_suite("dungeon_abyss_reward", kCases);
}
