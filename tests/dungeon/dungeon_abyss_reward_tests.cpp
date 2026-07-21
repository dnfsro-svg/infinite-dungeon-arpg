#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "dungeon_test_support.hpp"

#include "dungeon/abyss_reward.hpp"
#include "combat/room_bounds.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_affix.hpp"
#include "dungeon/dungeon_session.hpp"
#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "items/item_generation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

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
        if (!arpg::abyss::is_abyss_roll(seed)
                || !selection.has_value() || selection->danger != wanted) continue;
        state.current_room.seed = seed;
        state.current_room.depth = depth;
        state.current_room.entry = arpg::dungeon::EntrySide::left;
        state.current_room.is_abyss = true;
        state.current_room.has_hole = true;
        state.last_transition = arpg::dungeon::TransitionKind::door;
        state.last_direction = arpg::dungeon::ExitDirection::right;
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

arpg::test::Failure cleared_abyss_requires_a_door_origin_on_load() noexcept {
    using arpg::dungeon::EntrySide;
    using arpg::dungeon::ExitDirection;
    using arpg::dungeon::TransitionKind;
    const DungeonRunState valid = cleared_abyss_state(AbyssDanger::low);
    ARPG_REQUIRE(arpg::abyss::is_abyss_roll(valid.current_room.seed));

    for (const auto transition : std::array<TransitionKind, 2U>{{
             TransitionKind::none, TransitionKind::descent}}) {
        DungeonRunState impossible = valid;
        impossible.current_room.entry = EntrySide::initial;
        impossible.last_transition = transition;
        impossible.last_direction = ExitDirection::none;
        DungeonSession session{DungeonRules{}, impossible};
        ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
        ARPG_REQUIRE(session.snapshot().diagnostics.fault
            == DungeonFault::invalid_abyss_state);
        ARPG_REQUIRE(!session.pending_save().has_value());
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
            || left.reserved != right.reserved
            || left.reinforcement != right.reinforcement
            || left.extension_reserved != right.extension_reserved) return false;
    for (std::size_t index = 0U; index < left.affixes.size(); ++index) {
        if (left.affixes[index].affix_id != right.affixes[index].affix_id
                || left.affixes[index].tier != right.affixes[index].tier
                || left.affixes[index].variant
                    != right.affixes[index].variant
                || left.affixes[index].value_roll_bp
                    != right.affixes[index].value_roll_bp) return false;
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

void set_player_position(DungeonSession& session,
    arpg::combat::Vec3 position) noexcept {
    arpg::test::DungeonSessionTestAccess::set_player_position(session, position);
}

void attempt_exit(DungeonSession& session,
    arpg::dungeon::ExitDirection direction) noexcept {
    arpg::test::DungeonSessionTestAccess::attempt_exit(session, direction);
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
    const auto left = arpg::dungeon::abyss_reward_position(0U);
    const auto center = arpg::dungeon::abyss_reward_position(1U);
    const auto right = arpg::dungeon::abyss_reward_position(2U);
    ARPG_REQUIRE(left.has_value() && center.has_value() && right.has_value());
    ARPG_REQUIRE(left->x == -0.75F && left->y == 0.0F && left->z == 0.0F);
    ARPG_REQUIRE(center->x == 0.0F && center->y == 0.0F
        && center->z == 0.0F);
    ARPG_REQUIRE(right->x == 0.75F && right->y == 0.0F
        && right->z == 0.0F);
    ARPG_REQUIRE(!arpg::dungeon::abyss_reward_position(3U).has_value());

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
    ARPG_REQUIRE(session.snapshot().has_active_room);
    ARPG_REQUIRE(session.snapshot().ground_item_count == 0U);

    session.tick({});

    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->kind
        == PendingSaveKind::abyss_reward_materialized);
    ARPG_REQUIRE(session.snapshot().exits_open[0]);
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
    set_player_position(session, abyss_ground(session, 0U)->position);
    session.tick({});
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->kind
        == PendingSaveKind::abyss_reward_claim);
    ARPG_REQUIRE(commit_pending(session));
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.generated_mask == 1U);

    arpg::test::clear_ground_item(session, 41U);
    session.tick({});
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->next_state.abyss.generated_mask == 3U);
    return {};
}

arpg::test::Failure not_committed_reward_receipt_fails_closed() noexcept {
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
    return {};
}

arpg::test::Failure indeterminate_reward_receipt_fails_closed() noexcept {
    const auto state = cleared_abyss_state(AbyssDanger::low);
    DungeonSession indeterminate{DungeonRules{}, state};
    indeterminate.tick({});
    indeterminate.resolve_pending_save({SaveDisposition::indeterminate, 0U, {}});
    ARPG_REQUIRE(indeterminate.snapshot().diagnostics.fault
        == DungeonFault::save_commit_indeterminate);
    ARPG_REQUIRE(indeterminate.snapshot().ground_item_count == 0U);
    return {};
}

arpg::test::Failure mismatched_reward_receipt_fails_closed() noexcept {
    const auto state = cleared_abyss_state(AbyssDanger::low);
    DungeonSession mismatch{DungeonRules{}, state};
    mismatch.tick({});
    auto wrong = *mismatch.pending_save();
    ++wrong.next_state.abyss.reward_revision;
    mismatch.resolve_pending_save({SaveDisposition::committed,
        wrong.expected_generation, wrong.next_state});
    ARPG_REQUIRE(mismatch.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(mismatch.snapshot().ground_item_count == 0U);
    return {};
}

arpg::test::Failure occupied_reward_slot_fails_closed() noexcept {
    const auto state = cleared_abyss_state(AbyssDanger::low);
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
    ARPG_REQUIRE(reloaded.snapshot().has_active_room);
    ARPG_REQUIRE(!reloaded.pending_save().has_value());
    ARPG_REQUIRE(reloaded.snapshot().ground_item_count == 2U);
    ARPG_REQUIRE(same_ground(first, *abyss_ground(reloaded, 0U)));
    ARPG_REQUIRE(same_ground(second, *abyss_ground(reloaded, 1U)));
    ARPG_REQUIRE(arpg::test::stable_state(reloaded).abyss.generated_mask == 3U);
    ARPG_REQUIRE(arpg::test::stable_state(reloaded).abyss.reward_revision == 2U);
    set_player_position(reloaded, abyss_ground(reloaded, 0U)->position);
    reloaded.tick({});
    ARPG_REQUIRE(reloaded.pending_save().has_value());
    ARPG_REQUIRE(reloaded.pending_save()->kind
        == PendingSaveKind::abyss_reward_claim);
    ARPG_REQUIRE(reloaded.snapshot().phase == RoomPhase::committing);

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

arpg::test::Failure abyss_pickup_prepares_claim_without_consuming_sequence() noexcept {
    DungeonRunState filtered_state = cleared_abyss_state(AbyssDanger::low);
    constexpr std::uint64_t kNormalRewardSeedSearchLimit = 100000U;
    bool found_normal_reward = false;
    for (std::uint64_t seed = 1U;
         seed <= kNormalRewardSeedSearchLimit; ++seed) {
        const auto selection = arpg::abyss::select_abyss_rule(
            seed, filtered_state.current_room.depth);
        const auto reward = arpg::dungeon::derive_abyss_reward_slot(
            seed, AbyssDanger::low,
            static_cast<std::uint8_t>(filtered_state.current_room.depth), 0U);
        if (!arpg::abyss::is_abyss_roll(seed)
                || !selection.has_value()
                || selection->danger != AbyssDanger::low
                || !reward.has_value()
                || reward->rarity != arpg::items::ItemRarity::normal) {
            continue;
        }
        filtered_state.current_room.seed = seed;
        filtered_state.abyss.danger = selection->danger;
        filtered_state.abyss.rule = selection->rule;
        filtered_state.abyss.rules_version = selection->rules_version;
        found_normal_reward = true;
        break;
    }
    ARPG_REQUIRE(found_normal_reward);
    filtered_state.abyss.generated_mask = 1U;
    filtered_state.abyss.reward_revision = 1U;
    DungeonSession filtered{DungeonRules{}, filtered_state};
    const GroundItem* filtered_reward = abyss_ground(filtered, 0U);
    ARPG_REQUIRE(filtered_reward != nullptr);
    ARPG_REQUIRE(filtered_reward->item.rarity
        == arpg::items::ItemRarity::normal);
    const std::uint16_t reward_ground_ordinal =
        filtered_reward->drop_ordinal;
    const std::uint16_t ordinary_ground_ordinal =
        reward_ground_ordinal == 0U ? 1U : 0U;
    const auto reward_position = filtered_reward->position;
    arpg::test::install_ground_item(filtered, ordinary_ground_ordinal,
        normal_item(0xAB5511U), reward_position);
    set_player_position(filtered, reward_position);
    filtered.request_nearby_pickups(
        reward_position, {arpg::items::ItemRarity::rare});
    ARPG_REQUIRE(filtered.pending_save_view() != nullptr);
    ARPG_REQUIRE(filtered.pending_save_view()->kind
        == PendingSaveKind::abyss_reward_claim);
    ARPG_REQUIRE(filtered.pending_save_view()->pickup_ordinal
        == reward_ground_ordinal);
    ARPG_REQUIRE(filtered.snapshot().pending_pickup_ordinal
        == reward_ground_ordinal);
    ARPG_REQUIRE(arpg::test::ground_items(
        filtered)[ordinary_ground_ordinal].active);

    DungeonRunState state = cleared_abyss_state(AbyssDanger::medium);
    state.abyss.generated_mask = 3U;
    state.abyss.reward_revision = 2U;
    state.item_ownership.next_item_sequence = 9001U;
    DungeonSession session{DungeonRules{}, state};
    const GroundItem* reward = abyss_ground(session, 1U);
    ARPG_REQUIRE(reward != nullptr);
    const std::uint16_t ground_ordinal = reward->drop_ordinal;
    const std::uint64_t item_id = reward->item.id;
    set_player_position(session, reward->position);

    ARPG_REQUIRE(session.request_pickup(ground_ordinal)
        == arpg::dungeon::RequestResult::accepted);
    ARPG_REQUIRE(session.snapshot().exits_open[0]);
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == PendingSaveKind::abyss_reward_claim);
    ARPG_REQUIRE(pending->next_state.abyss.claimed_mask == 2U);
    ARPG_REQUIRE(pending->next_state.abyss.reward_revision == 3U);
    ARPG_REQUIRE(pending->next_state.item_ownership.next_item_sequence == 9001U);
    ARPG_REQUIRE(pending->next_state.item_ownership.items.back().id == item_id);
    ARPG_REQUIRE(arpg::test::ground_items(session)[ground_ordinal].active);

    ARPG_REQUIRE(commit_pending(session));
    ARPG_REQUIRE(!arpg::test::ground_items(session)[ground_ordinal].active);
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.claimed_mask == 2U);
    ARPG_REQUIRE(session.item_state().items.back().id == item_id);
    ARPG_REQUIRE(session.item_state().next_item_sequence == 9001U);
    return {};
}

arpg::test::Failure abyss_claims_commit_in_any_order_and_fail_closed() noexcept {
    DungeonRunState state = cleared_abyss_state(AbyssDanger::high);
    state.abyss.generated_mask = 7U;
    state.abyss.reward_revision = 3U;
    state.item_ownership.next_item_sequence = 77U;
    DungeonSession ordered{DungeonRules{}, state};
    constexpr std::array<std::uint8_t, 3> kOrder{{2U, 0U, 1U}};
    std::uint8_t expected_claimed = 0U;
    for (const std::uint8_t reward_ordinal : kOrder) {
        const GroundItem* reward = abyss_ground(ordered, reward_ordinal);
        ARPG_REQUIRE(reward != nullptr);
        const std::uint16_t ground_ordinal = reward->drop_ordinal;
        set_player_position(ordered, reward->position);
        expected_claimed |= static_cast<std::uint8_t>(1U << reward_ordinal);
        ARPG_REQUIRE(ordered.request_pickup(ground_ordinal)
            == arpg::dungeon::RequestResult::accepted);
        ARPG_REQUIRE(ordered.pending_save()->next_state.abyss.claimed_mask
            == expected_claimed);
        ARPG_REQUIRE(arpg::test::ground_items(ordered)[ground_ordinal].active);
        ARPG_REQUIRE(commit_pending(ordered));
        ARPG_REQUIRE(!arpg::test::ground_items(ordered)[ground_ordinal].active);
    }
    ARPG_REQUIRE(arpg::test::stable_state(ordered).abyss.claimed_mask == 7U);
    ARPG_REQUIRE(arpg::test::stable_state(ordered).abyss.reward_revision == 6U);
    ARPG_REQUIRE(ordered.item_state().items.size() == 3U);
    ARPG_REQUIRE(ordered.item_state().next_item_sequence == 77U);

    const auto verify_failure = [&](SaveDisposition disposition,
                                    bool mismatch) noexcept {
        DungeonRunState one = cleared_abyss_state(AbyssDanger::low);
        one.abyss.generated_mask = 1U;
        one.abyss.reward_revision = 1U;
        DungeonSession failed{DungeonRules{}, one};
        const GroundItem* reward = abyss_ground(failed, 0U);
        if (reward == nullptr) return false;
        set_player_position(failed, reward->position);
        if (failed.request_pickup(reward->drop_ordinal)
                != arpg::dungeon::RequestResult::accepted) return false;
        const auto pending = *failed.pending_save();
        DungeonRunState verified = pending.next_state;
        if (mismatch) ++verified.abyss.reward_revision;
        failed.resolve_pending_save({disposition,
            pending.expected_generation, verified});
        return failed.snapshot().phase == RoomPhase::faulted
            && abyss_ground(failed, 0U) != nullptr
            && arpg::test::stable_state(failed).abyss.claimed_mask == 0U
            && failed.item_state().items.empty();
    };
    ARPG_REQUIRE(verify_failure(SaveDisposition::not_committed, false));
    ARPG_REQUIRE(verify_failure(SaveDisposition::indeterminate, false));
    ARPG_REQUIRE(verify_failure(SaveDisposition::committed, true));
    return {};
}

arpg::test::Failure abyss_claim_full_oom_overflow_and_exact_receipt_are_atomic() noexcept {
    DungeonRunState full = cleared_abyss_state(AbyssDanger::low);
    full.abyss.generated_mask = 1U;
    full.abyss.reward_revision = 1U;
    full.item_ownership.items.resize(65535U);
    for (std::size_t index = 0U; index < full.item_ownership.items.size(); ++index) {
        auto& item = full.item_ownership.items[index];
        item.id = static_cast<std::uint64_t>(index + 1U);
        item.base_id = 1U;
        item.rarity = arpg::items::ItemRarity::normal;
        item.item_level = 20U;
        item.required_level = 1U;
    }
    DungeonSession full_session{DungeonRules{}, full};
    const GroundItem* full_reward = abyss_ground(full_session, 0U);
    ARPG_REQUIRE(full_reward != nullptr);
    ARPG_REQUIRE(full_session.request_pickup(full_reward->drop_ordinal)
        == arpg::dungeon::RequestResult::rejected);
    ARPG_REQUIRE(full_session.snapshot().phase != RoomPhase::faulted);
    ARPG_REQUIRE(abyss_ground(full_session, 0U) != nullptr);
    ARPG_REQUIRE(!full_session.pending_save().has_value());

    DungeonRunState retryable = cleared_abyss_state(AbyssDanger::low);
    retryable.abyss.generated_mask = 1U;
    retryable.abyss.reward_revision = 1U;
    retryable.item_ownership.items.push_back(normal_item(0x777777U));
    DungeonSession oom{DungeonRules{}, retryable};
    const std::uint16_t oom_ordinal = abyss_ground(oom, 0U)->drop_ordinal;
    set_player_position(oom,
        arpg::test::ground_items(oom)[oom_ordinal].position);
    {
        arpg::test::ScopedAllocationFailure fail{0U};
        ARPG_REQUIRE(oom.request_pickup(oom_ordinal)
            == arpg::dungeon::RequestResult::rejected);
    }
    ARPG_REQUIRE(oom.snapshot().diagnostics.fault == DungeonFault::none);
    ARPG_REQUIRE(arpg::test::ground_items(oom)[oom_ordinal].active);
    ARPG_REQUIRE(!oom.pending_save().has_value());
    ARPG_REQUIRE(oom.request_pickup(oom_ordinal)
        == arpg::dungeon::RequestResult::accepted);
    const auto receipt = *oom.pending_save();
    const arpg::dungeon::PendingSaveResult exact{
        SaveDisposition::committed,
        receipt.expected_generation,
        receipt.next_state,
    };
    {
        arpg::test::ScopedAllocationFailure fail{0U};
        oom.resolve_pending_save(exact);
    }
    ARPG_REQUIRE(oom.snapshot().phase != RoomPhase::faulted);
    ARPG_REQUIRE(!arpg::test::ground_items(oom)[oom_ordinal].active);

    DungeonRunState overflow = cleared_abyss_state(AbyssDanger::low);
    overflow.abyss.generated_mask = 1U;
    overflow.abyss.reward_revision =
        (std::numeric_limits<std::uint32_t>::max)();
    DungeonSession revision{DungeonRules{}, overflow};
    const std::uint16_t revision_ordinal = abyss_ground(revision, 0U)->drop_ordinal;
    set_player_position(revision,
        arpg::test::ground_items(revision)[revision_ordinal].position);
    ARPG_REQUIRE(revision.request_pickup(revision_ordinal)
        == arpg::dungeon::RequestResult::faulted);
    ARPG_REQUIRE(revision.snapshot().diagnostics.fault
        == DungeonFault::abyss_reward_revision_overflow);
    ARPG_REQUIRE(arpg::test::ground_items(revision)[revision_ordinal].active);
    return {};
}

arpg::test::Failure structurally_invalid_abyss_ground_faults_in_place() noexcept {
    DungeonRunState state = cleared_abyss_state(AbyssDanger::low);
    state.abyss.generated_mask = 1U;
    state.abyss.reward_revision = 1U;

    DungeonSession invalid_source{DungeonRules{}, state};
    const GroundItem* source_reward = abyss_ground(invalid_source, 0U);
    ARPG_REQUIRE(source_reward != nullptr);
    const std::uint16_t source_ordinal = source_reward->drop_ordinal;
    auto& source_ground = const_cast<GroundItem&>(
        arpg::test::ground_items(invalid_source)[source_ordinal]);
    source_ground.source = static_cast<GroundItemSource>(0xFEU);
    ARPG_REQUIRE(invalid_source.request_pickup(source_ordinal)
        == arpg::dungeon::RequestResult::faulted);
    ARPG_REQUIRE(invalid_source.snapshot().diagnostics.fault
        == DungeonFault::invalid_item_state);
    ARPG_REQUIRE(arpg::test::ground_items(invalid_source)[source_ordinal].active);

    DungeonSession invalid_position{DungeonRules{}, state};
    const GroundItem* position_reward = abyss_ground(invalid_position, 0U);
    ARPG_REQUIRE(position_reward != nullptr);
    const std::uint16_t position_ordinal = position_reward->drop_ordinal;
    auto& position_ground = const_cast<GroundItem&>(
        arpg::test::ground_items(invalid_position)[position_ordinal]);
    position_ground.position.x += 0.25F;
    set_player_position(invalid_position, position_ground.position);
    ARPG_REQUIRE(invalid_position.request_pickup(position_ordinal)
        == arpg::dungeon::RequestResult::faulted);
    ARPG_REQUIRE(invalid_position.snapshot().diagnostics.fault
        == DungeonFault::invalid_abyss_state);
    ARPG_REQUIRE(arpg::test::ground_items(
        invalid_position)[position_ordinal].active);
    return {};
}

arpg::test::Failure abyss_claim_requires_navigation_world_and_pickup_range() noexcept {
    DungeonRunState state = cleared_abyss_state(AbyssDanger::low);
    state.current_room.entry = arpg::dungeon::EntrySide::left;
    state.abyss.generated_mask = 1U;
    state.abyss.reward_revision = 1U;
    DungeonSession session{DungeonRules{}, state};
    const GroundItem* reward = abyss_ground(session, 0U);
    ARPG_REQUIRE(reward != nullptr);
    const std::uint16_t ordinal = reward->drop_ordinal;
    const auto player = session.snapshot().combat->player.position;
    ARPG_REQUIRE(player.x < -9.0F);

    ARPG_REQUIRE(session.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::rejected);
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(arpg::test::ground_items(session)[ordinal].active);
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.claimed_mask == 0U);

    set_player_position(session, reward->position);
    ARPG_REQUIRE(session.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::accepted);
    ARPG_REQUIRE(session.pending_save()->kind
        == PendingSaveKind::abyss_reward_claim);
    return {};
}

arpg::test::Failure generated_unrebuilt_reward_still_counts_and_warns() noexcept {
    using arpg::dungeon::ExitDirection;
    DungeonRunState state = cleared_abyss_state(AbyssDanger::low);
    state.abyss.generated_mask = 1U;
    state.abyss.reward_revision = 1U;
    DungeonSession session{DungeonRules{}, state};
    const GroundItem* reward = abyss_ground(session, 0U);
    ARPG_REQUIRE(reward != nullptr);
    arpg::test::clear_ground_item(session, reward->drop_ordinal);
    fill_ground_pool(session);
    arpg::test::set_phase(session, RoomPhase::awaiting_exit);
    set_player_position(session, {arpg::combat::room_bounds::min_x, 0.0F, 0.0F});

    ARPG_REQUIRE(session.snapshot().abyss_pending_rewards == 0U);
    ARPG_REQUIRE(session.snapshot().abyss_unpicked_rewards == 1U);
    attempt_exit(session, ExitDirection::left);
    ARPG_REQUIRE(session.snapshot().abyss_exit_confirmation_armed);
    const auto warning = session.try_pop_event();
    ARPG_REQUIRE(warning.has_value());
    ARPG_REQUIRE(warning->kind
        == arpg::dungeon::DungeonEventKind::abyss_exit_warning);
    ARPG_REQUIRE(warning->abyss_pending_rewards == 0U);
    ARPG_REQUIRE(warning->abyss_unpicked_rewards == 1U);
    return {};
}

arpg::test::Failure reloaded_cleared_abyss_is_navigable_and_auto_claims() noexcept {
    DungeonRunState navigable = cleared_abyss_state(AbyssDanger::low);
    navigable.abyss.generated_mask = 1U;
    navigable.abyss.claimed_mask = 1U;
    navigable.abyss.reward_revision = 2U;
    DungeonSession movement{DungeonRules{}, navigable};
    ARPG_REQUIRE(movement.snapshot().phase == RoomPhase::cleared);
    ARPG_REQUIRE(movement.snapshot().initial_monster_count
        == movement.snapshot().encounter.initial_monster_count);
    ARPG_REQUIRE(movement.snapshot().has_active_room);
    ARPG_REQUIRE(movement.snapshot().combat.has_value());
    ARPG_REQUIRE(movement.snapshot().remaining_targets == 0U);
    const auto initial = movement.snapshot().combat->player.position;
    movement.tick({});
    movement.tick({1, 0});
    ARPG_REQUIRE(movement.snapshot().phase == RoomPhase::awaiting_exit);
    ARPG_REQUIRE(movement.snapshot().combat->player.position.x > initial.x);
    ARPG_REQUIRE(movement.snapshot().remaining_targets == 0U);

    DungeonRunState auto_state = cleared_abyss_state(AbyssDanger::low);
    auto_state.abyss.generated_mask = 1U;
    auto_state.abyss.reward_revision = 1U;
    DungeonSession automatic{DungeonRules{}, auto_state};
    set_player_position(automatic, abyss_ground(automatic, 0U)->position);
    automatic.tick({});
    automatic.tick({});
    ARPG_REQUIRE(automatic.pending_save().has_value());
    ARPG_REQUIRE(automatic.pending_save()->kind
        == PendingSaveKind::abyss_reward_claim);
    ARPG_REQUIRE(abyss_ground(automatic, 0U) != nullptr);
    return {};
}

arpg::test::Failure cleared_abyss_reset_is_rejected_without_state_drift() noexcept {
    constexpr std::array<std::uint8_t, 3U> kGenerated{{0U, 1U, 1U}};
    constexpr std::array<std::uint8_t, 3U> kClaimed{{0U, 0U, 1U}};
    for (std::size_t index = 0U; index < kGenerated.size(); ++index) {
        DungeonRunState state = cleared_abyss_state(AbyssDanger::low);
        state.abyss.generated_mask = kGenerated[index];
        state.abyss.claimed_mask = kClaimed[index];
        state.abyss.reward_revision = kGenerated[index] + kClaimed[index];
        DungeonSession session{DungeonRules{}, state};
        const auto before_snapshot = session.snapshot();
        const auto before_state = arpg::test::stable_state(session);
        const auto before_ground = arpg::test::ground_items(session);

        ARPG_REQUIRE(session.reset_current_room()
            == arpg::dungeon::RequestResult::rejected);
        const auto after = session.snapshot();
        const auto& after_state = arpg::test::stable_state(session);
        ARPG_REQUIRE(after.phase == before_snapshot.phase);
        ARPG_REQUIRE(after.diagnostics.fault == DungeonFault::none);
        ARPG_REQUIRE(after_state.commit_generation
            == before_state.commit_generation);
        ARPG_REQUIRE(after_state.abyss.generated_mask
            == before_state.abyss.generated_mask);
        ARPG_REQUIRE(after_state.abyss.claimed_mask
            == before_state.abyss.claimed_mask);
        ARPG_REQUIRE(after_state.abyss.abandoned_mask
            == before_state.abyss.abandoned_mask);
        ARPG_REQUIRE(after_state.abyss.reward_revision
            == before_state.abyss.reward_revision);
        for (std::size_t ground = 0U; ground < before_ground.size(); ++ground) {
            ARPG_REQUIRE(same_ground(
                arpg::test::ground_items(session)[ground], before_ground[ground]));
        }

        if (index == 0U) {
            session.tick({});
            ARPG_REQUIRE(session.pending_save().has_value());
            ARPG_REQUIRE(session.pending_save()->kind
                == PendingSaveKind::abyss_reward_materialized);
        } else if (index == 1U) {
            const GroundItem* reward = abyss_ground(session, 0U);
            ARPG_REQUIRE(reward != nullptr);
            set_player_position(session, reward->position);
            ARPG_REQUIRE(session.request_pickup(reward->drop_ordinal)
                == arpg::dungeon::RequestResult::accepted);
        } else {
            arpg::test::set_phase(session, RoomPhase::awaiting_exit);
            set_player_position(session, {arpg::combat::room_bounds::min_x, 0.0F, 0.0F});
            attempt_exit(session, arpg::dungeon::ExitDirection::left);
            ARPG_REQUIRE(session.pending_save().has_value());
            ARPG_REQUIRE(session.pending_save()->kind
                == PendingSaveKind::transition);
        }
    }
    return {};
}

arpg::test::Failure abyss_door_confirmation_warns_and_abandons_atomically() noexcept {
    using arpg::dungeon::ExitDirection;
    using arpg::dungeon::TransitionKind;
    DungeonRunState state = cleared_abyss_state(AbyssDanger::high);
    state.abyss.generated_mask = 7U;
    state.abyss.reward_revision = 3U;
    DungeonSession session{DungeonRules{}, state};
    arpg::test::set_phase(session, RoomPhase::awaiting_exit);
    set_player_position(session, {arpg::combat::room_bounds::min_x, 0.0F, 0.0F});

    attempt_exit(session, ExitDirection::left);
    const auto armed = session.snapshot();
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(armed.abyss_pending_rewards == 0U);
    ARPG_REQUIRE(armed.abyss_unpicked_rewards == 3U);
    ARPG_REQUIRE(armed.abyss_exit_confirmation_armed);
    ARPG_REQUIRE(armed.abyss_exit_confirmation_transition
        == TransitionKind::door);
    ARPG_REQUIRE(armed.abyss_exit_confirmation_direction
        == ExitDirection::left);
    const auto warning = session.try_pop_event();
    ARPG_REQUIRE(warning.has_value());
    ARPG_REQUIRE(warning->kind
        == arpg::dungeon::DungeonEventKind::abyss_exit_warning);
    ARPG_REQUIRE(warning->abyss_pending_rewards == 0U);
    ARPG_REQUIRE(warning->abyss_unpicked_rewards == 3U);

    for (int held = 0; held < 3; ++held) {
        session.tick({-1, 0});
        ARPG_REQUIRE(!session.pending_save().has_value());
        ARPG_REQUIRE(session.snapshot().abyss_exit_confirmation_armed);
    }
    session.tick({});
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(session.snapshot().abyss_exit_confirmation_armed);
    session.tick({-1, 0});
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->kind == PendingSaveKind::abyss_abandon);
    const auto pending = *session.pending_save();
    ARPG_REQUIRE(pending.transition == TransitionKind::door);
    ARPG_REQUIRE(pending.direction == ExitDirection::left);
    ARPG_REQUIRE(pending.next_state.last_abyss_resolution.valid);
    ARPG_REQUIRE(pending.next_state.last_abyss_resolution.room_seed
        == state.current_room.seed);
    ARPG_REQUIRE(pending.next_state.last_abyss_resolution.total == 3U);
    ARPG_REQUIRE(pending.next_state.last_abyss_resolution.generated == 3U);
    ARPG_REQUIRE(pending.next_state.last_abyss_resolution.claimed == 0U);
    ARPG_REQUIRE(pending.next_state.last_abyss_resolution.abandoned == 0U);
    ARPG_REQUIRE(pending.next_state.abyss.lifecycle
        != arpg::abyss::AbyssLifecycle::cleared);
    ARPG_REQUIRE(abyss_ground(session, 0U) != nullptr);

    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation, pending.next_state});
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::transitioning);
    ARPG_REQUIRE(session.snapshot().ground_item_count == 0U);
    ARPG_REQUIRE(!session.snapshot().abyss_exit_confirmation_armed);
    ARPG_REQUIRE(arpg::test::stable_state(session)
        .last_abyss_resolution.abandoned == 0U);
    return {};
}

arpg::test::Failure abyss_door_abandon_counts_only_ungenerated_rewards() noexcept {
    using arpg::dungeon::ExitDirection;
    using arpg::dungeon::TransitionKind;
    DungeonRunState state = cleared_abyss_state(AbyssDanger::high);
    state.abyss.generated_mask = 1U;
    state.abyss.reward_revision = 1U;
    DungeonSession session{DungeonRules{}, state};
    arpg::test::set_phase(session, RoomPhase::awaiting_exit);
    const GroundItem* generated = abyss_ground(session, 0U);
    ARPG_REQUIRE(generated != nullptr);
    const std::uint16_t generated_ground_ordinal = generated->drop_ordinal;
    fill_ground_pool(session, generated_ground_ordinal);
    set_player_position(session, {arpg::combat::room_bounds::min_x, 0.0F, 0.0F});

    session.tick({-1, 0});
    const auto armed = session.snapshot();
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(armed.abyss_pending_rewards == 2U);
    ARPG_REQUIRE(armed.abyss_unpicked_rewards == 1U);
    ARPG_REQUIRE(armed.abyss_exit_confirmation_armed);
    const auto warning = session.try_pop_event();
    ARPG_REQUIRE(warning.has_value());
    ARPG_REQUIRE(warning->kind
        == arpg::dungeon::DungeonEventKind::abyss_exit_warning);
    ARPG_REQUIRE(warning->abyss_pending_rewards == 2U);
    ARPG_REQUIRE(warning->abyss_unpicked_rewards == 1U);

    session.tick({});
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(session.snapshot().abyss_exit_confirmation_armed);
    ARPG_REQUIRE(abyss_ground(session, 0U) != nullptr);

    session.tick({-1, 0});
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->kind == PendingSaveKind::abyss_abandon);
    const auto pending = *session.pending_save();
    ARPG_REQUIRE(pending.transition == TransitionKind::door);
    ARPG_REQUIRE(pending.direction == ExitDirection::left);
    ARPG_REQUIRE(pending.next_state.last_abyss_resolution.valid);
    ARPG_REQUIRE(pending.next_state.last_abyss_resolution.total == 3U);
    ARPG_REQUIRE(pending.next_state.last_abyss_resolution.generated == 1U);
    ARPG_REQUIRE(pending.next_state.last_abyss_resolution.claimed == 0U);
    ARPG_REQUIRE(pending.next_state.last_abyss_resolution.abandoned == 2U);
    ARPG_REQUIRE(abyss_ground(session, 0U) != nullptr);

    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation, pending.next_state});
    const auto departed = session.snapshot();
    const auto expected_density = arpg::dungeon::roll_room_density(
        departed.room_seed, departed.is_abyss);
    ARPG_REQUIRE(departed.phase == RoomPhase::transitioning);
    ARPG_REQUIRE(departed.room_seed == pending.next_state.current_room.seed);
    ARPG_REQUIRE(departed.room_seed != state.current_room.seed);
    const auto previous_density = arpg::dungeon::roll_room_density(
        state.current_room.seed, state.current_room.is_abyss);
    ARPG_REQUIRE(previous_density.affix != expected_density.affix
        || previous_density.base_count != expected_density.base_count
        || previous_density.monster_count != expected_density.monster_count);
    ARPG_REQUIRE(departed.density_affix == expected_density.affix);
    ARPG_REQUIRE(departed.base_monster_count == expected_density.base_count);
    ARPG_REQUIRE(departed.initial_monster_count
        == expected_density.monster_count);
    ARPG_REQUIRE(departed.ground_item_count == 0U);
    const auto& resolution = arpg::test::stable_state(session)
                                 .last_abyss_resolution;
    ARPG_REQUIRE(resolution.generated == 1U);
    ARPG_REQUIRE(resolution.claimed == 0U);
    ARPG_REQUIRE(resolution.abandoned == 2U);
    return {};
}

arpg::test::Failure abyss_confirmation_invalidates_on_key_range_revision_and_capacity() noexcept {
    using arpg::dungeon::ExitDirection;
    DungeonRunState state = cleared_abyss_state(AbyssDanger::high);
    state.abyss.generated_mask = 7U;
    state.abyss.reward_revision = 3U;

    auto direction = std::make_unique<DungeonSession>(DungeonRules{}, state);
    arpg::test::set_phase(*direction, RoomPhase::awaiting_exit);
    attempt_exit(*direction, ExitDirection::left);
    ARPG_REQUIRE(direction->snapshot().abyss_exit_confirmation_armed);
    attempt_exit(*direction, ExitDirection::right);
    ARPG_REQUIRE(!direction->snapshot().abyss_exit_confirmation_armed);
    ARPG_REQUIRE(!direction->pending_save().has_value());

    auto range = std::make_unique<DungeonSession>(DungeonRules{}, state);
    arpg::test::set_phase(*range, RoomPhase::awaiting_exit);
    set_player_position(*range, {arpg::combat::room_bounds::min_x, 0.0F, 0.0F});
    attempt_exit(*range, ExitDirection::left);
    ARPG_REQUIRE(range->snapshot().abyss_exit_confirmation_armed);
    set_player_position(*range, {8.0F, 3.0F, 0.0F});
    range->tick({});
    ARPG_REQUIRE(!range->snapshot().abyss_exit_confirmation_armed);

    auto revision = std::make_unique<DungeonSession>(DungeonRules{}, state);
    arpg::test::set_phase(*revision, RoomPhase::awaiting_exit);
    set_player_position(*revision, {arpg::combat::room_bounds::min_x, 0.0F, 0.0F});
    attempt_exit(*revision, ExitDirection::left);
    const std::uint16_t claim = abyss_ground(*revision, 0U)->drop_ordinal;
    set_player_position(*revision,
        arpg::test::ground_items(*revision)[claim].position);
    ARPG_REQUIRE(revision->request_pickup(claim)
        == arpg::dungeon::RequestResult::accepted);
    ARPG_REQUIRE(!revision->snapshot().abyss_exit_confirmation_armed);

    auto full_events = std::make_unique<DungeonSession>(DungeonRules{}, state);
    arpg::test::set_phase(*full_events, RoomPhase::awaiting_exit);
    ARPG_REQUIRE(arpg::test::DungeonSessionTestAccess::fill_dungeon_events(
        *full_events, DungeonSession::kDungeonEventCapacity)
        == DungeonSession::kDungeonEventCapacity);
    attempt_exit(*full_events, ExitDirection::left);
    ARPG_REQUIRE(full_events->snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(full_events->snapshot().diagnostics.fault
        == DungeonFault::event_overflow);
    ARPG_REQUIRE(!full_events->snapshot().abyss_exit_confirmation_armed);
    ARPG_REQUIRE(!full_events->pending_save().has_value());
    return {};
}

arpg::test::Failure abyss_hole_confirmation_leaves_range_and_descends_on_second_e() noexcept {
    using arpg::dungeon::ExitDirection;
    using arpg::dungeon::TransitionKind;
    DungeonRunState state = cleared_abyss_state(AbyssDanger::high);
    state.abyss.generated_mask = 7U;
    state.abyss.reward_revision = 3U;

    auto range = std::make_unique<DungeonSession>(DungeonRules{}, state);
    arpg::test::set_phase(*range, RoomPhase::awaiting_exit);
    set_player_position(*range, {0.0F, 3.5F, 0.0F});
    ARPG_REQUIRE(!range->request_descent(true));
    ARPG_REQUIRE(range->snapshot().abyss_exit_confirmation_armed);
    ARPG_REQUIRE(range->snapshot().abyss_exit_confirmation_transition
        == TransitionKind::descent);
    ARPG_REQUIRE(range->snapshot().abyss_exit_confirmation_direction
        == ExitDirection::none);
    set_player_position(*range, {8.0F, 3.5F, 0.0F});
    range->tick({});
    ARPG_REQUIRE(!range->snapshot().abyss_exit_confirmation_armed);

    auto descent = std::make_unique<DungeonSession>(DungeonRules{}, state);
    arpg::test::set_phase(*descent, RoomPhase::awaiting_exit);
    set_player_position(*descent, {0.0F, 3.5F, 0.0F});
    ARPG_REQUIRE(!descent->request_descent(true));
    ARPG_REQUIRE(descent->request_descent(true));
    ARPG_REQUIRE(descent->pending_save()->kind == PendingSaveKind::abyss_abandon);
    ARPG_REQUIRE(descent->pending_save()->transition == TransitionKind::descent);
    ARPG_REQUIRE(descent->pending_save()->direction == ExitDirection::none);
    ARPG_REQUIRE(descent->pending_save()->next_state
        .last_abyss_resolution.generated == 3U);
    ARPG_REQUIRE(descent->pending_save()->next_state
        .last_abyss_resolution.abandoned == 0U);

    DungeonRunState claimed = state;
    claimed.abyss.claimed_mask = 7U;
    auto one_press = std::make_unique<DungeonSession>(DungeonRules{}, claimed);
    arpg::test::set_phase(*one_press, RoomPhase::awaiting_exit);
    ARPG_REQUIRE(one_press->request_descent(true));
    ARPG_REQUIRE(one_press->pending_save()->kind == PendingSaveKind::transition);
    ARPG_REQUIRE(one_press->pending_save()->next_state
        .last_abyss_resolution.generated == 3U);
    ARPG_REQUIRE(one_press->pending_save()->next_state
        .last_abyss_resolution.claimed == 3U);
    ARPG_REQUIRE(one_press->pending_save()->next_state
        .last_abyss_resolution.abandoned == 0U);
    return {};
}

arpg::test::Failure abyss_abandon_failure_stays_and_next_resolution_overwrites() noexcept {
    using arpg::dungeon::ExitDirection;
    DungeonRunState state = cleared_abyss_state(AbyssDanger::medium);
    state.abyss.generated_mask = 3U;
    state.abyss.reward_revision = 2U;
    state.last_abyss_resolution.valid = true;
    state.last_abyss_resolution.room_seed = 0xBADU;
    state.last_abyss_resolution.total = 3U;
    state.last_abyss_resolution.generated = 3U;
    state.last_abyss_resolution.claimed = 3U;

    DungeonSession failed{DungeonRules{}, state};
    arpg::test::set_phase(failed, RoomPhase::awaiting_exit);
    set_player_position(failed, {arpg::combat::room_bounds::max_x, 0.0F, 0.0F});
    attempt_exit(failed, ExitDirection::right);
    failed.tick({});
    attempt_exit(failed, ExitDirection::right);
    const auto pending = *failed.pending_save();
    failed.resolve_pending_save({SaveDisposition::not_committed,
        pending.expected_generation, pending.next_state});
    ARPG_REQUIRE(failed.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(failed.snapshot().room_seed == state.current_room.seed);
    ARPG_REQUIRE(abyss_ground(failed, 0U) != nullptr);
    ARPG_REQUIRE(arpg::test::stable_state(failed).abyss.generated_mask == 3U);
    ARPG_REQUIRE(arpg::test::stable_state(failed)
        .last_abyss_resolution.room_seed == 0xBADU);

    DungeonRunState complete = state;
    complete.abyss.generated_mask = 3U;
    complete.abyss.claimed_mask = 3U;
    complete.abyss.reward_revision = 4U;
    DungeonSession overwrite{DungeonRules{}, complete};
    arpg::test::set_phase(overwrite, RoomPhase::awaiting_exit);
    attempt_exit(overwrite, ExitDirection::up);
    ARPG_REQUIRE(overwrite.pending_save().has_value());
    ARPG_REQUIRE(overwrite.pending_save()->kind == PendingSaveKind::transition);
    const auto& summary = overwrite.pending_save()->next_state
        .last_abyss_resolution;
    ARPG_REQUIRE(summary.valid);
    ARPG_REQUIRE(summary.room_seed == complete.current_room.seed);
    ARPG_REQUIRE(summary.rule == complete.abyss.rule);
    ARPG_REQUIRE(summary.total == 2U);
    ARPG_REQUIRE(summary.generated == 2U);
    ARPG_REQUIRE(summary.claimed == 2U);
    ARPG_REQUIRE(summary.abandoned == 0U);
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
    set_player_position(session, abyss_ground(session, 0U)->position);
    session.tick({});
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->kind
        == PendingSaveKind::abyss_reward_claim);
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.reward_revision == 1U);
    return {};
}

arpg::test::Failure tampered_cached_position_rejects_exact_receipt() noexcept {
    DungeonSession session{DungeonRules{}, cleared_abyss_state(AbyssDanger::low)};
    session.tick({});
    const auto pending = *session.pending_save();
    arpg::test::offset_pending_abyss_reward_position(
        session, {0.25F, -0.5F, 1.0F});

    session.resolve_pending_save({SaveDisposition::committed,
        pending.expected_generation, pending.next_state});

    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(session.snapshot().ground_item_count == 0U);
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.generated_mask == 0U);
    ARPG_REQUIRE(arpg::test::stable_state(session).abyss.reward_revision == 0U);
    return {};
}

arpg::test::Failure recipe_product_colliding_with_abyss_ground_is_atomic() noexcept {
    DungeonRunState state = arpg::dungeon::make_initial_run_state(
        0x51515151ULL, DungeonRules{}).state;
    const auto a = normal_item(0x5101U);
    auto b = normal_item(0x5102U);
    auto c = normal_item(0x5103U);
    b.base_id = a.base_id;
    c.base_id = a.base_id;
    state.item_ownership.items = {a, b, c};
    state.item_ownership.next_item_sequence = 9U;
    const auto product = arpg::items::generate_recipe_item(
        state.root_seed, state.item_ownership.next_item_sequence, a, b, c);
    ARPG_REQUIRE(product.has_value());

    DungeonSession session{DungeonRules{}, state};
    arpg::test::install_abyss_ground_item(
        session, 17U, 0U, normal_item(product->id), {8.0F, 4.0F, 0.0F});

    ARPG_REQUIRE(session.request_recipe({{a.id, b.id, c.id}})
        == arpg::dungeon::RequestResult::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == DungeonFault::item_id_collision);
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(session.item_state().items.size() == 3U);
    ARPG_REQUIRE(session.item_state().items[0].id == a.id);
    ARPG_REQUIRE(session.item_state().items[1].id == b.id);
    ARPG_REQUIRE(session.item_state().items[2].id == c.id);
    ARPG_REQUIRE(session.item_state().next_item_sequence == 9U);
    ARPG_REQUIRE(arpg::test::ground_items(session)[17U].active);
    ARPG_REQUIRE(arpg::test::ground_items(session)[17U].item.id == product->id);
    ARPG_REQUIRE(arpg::test::ground_items(session)[17U].source
        == GroundItemSource::abyss_chest);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"cleared abyss requires door origin",
        &cleared_abyss_requires_a_door_origin_on_load},
    {"reward profiles levels and ordinals", &reward_profiles_levels_and_ordinals_are_exact},
    {"shifted rarity danger ordering", &shifted_rarity_rolls_follow_danger_ordering},
    {"stable independent ordinal streams", &ordinal_streams_are_stable_and_allow_repeated_slots},
    {"cleared starts hidden transaction", &cleared_abyss_starts_hidden_reward_transaction},
    {"partial pool uses free index", &partial_pool_uses_actual_free_ground_index},
    {"full pool waits and continues", &full_pool_waits_without_advancing_then_continues},
    {"not committed reward receipt fails closed",
        &not_committed_reward_receipt_fails_closed},
    {"indeterminate reward receipt fails closed",
        &indeterminate_reward_receipt_fails_closed},
    {"mismatched reward receipt fails closed",
        &mismatched_reward_receipt_fails_closed},
    {"occupied reward slot fails closed", &occupied_reward_slot_fails_closed},
    {"exact receipt no allocation", &exact_receipt_publishes_without_allocation},
    {"reload rebuilds exact items", &reload_rebuilds_exact_items_without_save_or_combat},
    {"retry content stable", &retry_content_is_independent_of_pool_and_runtime_state},
    {"collision and revision overflow", &collision_and_revision_overflow_fault_without_retry},
    {"abyss pickup prepares claim",
        &abyss_pickup_prepares_claim_without_consuming_sequence},
    {"abyss claims any order fail closed",
        &abyss_claims_commit_in_any_order_and_fail_closed},
    {"abyss claim full oom overflow atomic",
        &abyss_claim_full_oom_overflow_and_exact_receipt_are_atomic},
    {"invalid abyss ground faults in place",
        &structurally_invalid_abyss_ground_faults_in_place},
    {"abyss claim requires pickup range",
        &abyss_claim_requires_navigation_world_and_pickup_range},
    {"unrebuilt generated reward still warns",
        &generated_unrebuilt_reward_still_counts_and_warns},
    {"reloaded cleared abyss navigable",
        &reloaded_cleared_abyss_is_navigable_and_auto_claims},
    {"cleared abyss reset rejected atomically",
        &cleared_abyss_reset_is_rejected_without_state_drift},
    {"abyss door confirmation abandon",
        &abyss_door_confirmation_warns_and_abandons_atomically},
    {"abyss door abandon excludes generated unclaimed",
        &abyss_door_abandon_counts_only_ungenerated_rewards},
    {"abyss confirmation invalidation",
        &abyss_confirmation_invalidates_on_key_range_revision_and_capacity},
    {"abyss hole confirmation descent",
        &abyss_hole_confirmation_leaves_range_and_descends_on_second_e},
    {"abyss abandon failure and overwrite",
        &abyss_abandon_failure_stays_and_next_resolution_overwrites},
    {"allocation failure retryable", &allocation_failure_is_hidden_and_retryable},
    {"reload pool space wait", &committed_reward_waits_for_reload_pool_space},
    {"tampered cached position", &tampered_cached_position_rejects_exact_receipt},
    {"recipe collides with abyss ground", &recipe_product_colliding_with_abyss_ground_is_atomic},
};

}  // namespace

arpg::test::TestSuite dungeon_abyss_reward_suite() noexcept {
    return arpg::test::make_suite("dungeon_abyss_reward", kCases);
}
