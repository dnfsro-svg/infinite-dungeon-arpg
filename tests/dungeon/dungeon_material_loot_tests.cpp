#include "test_framework.hpp"

#include "dungeon/material_loot.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_progress_checkpoint.hpp"
#include "dungeon_test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace {

using arpg::items::MaterialId;

using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;

DungeonRunState material_state(
    std::uint64_t depth,
    std::uint16_t ordinal,
    std::uint16_t score,
    bool require_coupon = false) noexcept {
    DungeonRunState state = arpg::dungeon::make_initial_run_state(
        0x1634U, DungeonRules{}).state;
    state.current_room.depth = depth;
    for (std::uint64_t seed = 1U; seed < 1000000U; ++seed) {
        const bool common = arpg::dungeon::roll_material_drop(
            seed, ordinal, depth, score).has_value();
        const bool coupon = arpg::dungeon::roll_coupon_drop(
            seed, ordinal, depth, score, false).has_value();
        if (common && (!require_coupon || coupon)) {
            state.current_room.seed = seed;
            return state;
        }
    }
    state.current_room.seed = 0U;
    return state;
}

bool relay_material(DungeonSession& session,
    std::uint16_t ordinal,
    std::uint16_t score,
    arpg::combat::Vec3 position) noexcept {
    const bool relayed = arpg::test::relay_defeated(
        session,
        static_cast<std::uint8_t>(ordinal / 96U),
        static_cast<std::uint8_t>(ordinal % 96U),
        position, true, arpg::combat::MonsterId::fire_bomber,
        ordinal, score);
    static_cast<void>(session.try_pop_combat_event());
    return relayed;
}

bool resolve_committed(DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()) return false;
    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation,
        pending->next_state,
        pending->kind,
    });
    return session.snapshot().phase != arpg::dungeon::RoomPhase::faulted;
}

arpg::test::Failure material_drop_chance_uses_frozen_basis_points() noexcept {
    ARPG_REQUIRE(arpg::dungeon::material_drop_chance_bp(0U) == 800U);
    ARPG_REQUIRE(arpg::dungeon::material_drop_chance_bp(1U) == 900U);
    ARPG_REQUIRE(arpg::dungeon::material_drop_chance_bp(17U) == 2500U);
    ARPG_REQUIRE(arpg::dungeon::material_drop_chance_bp(26U) == 3400U);
    ARPG_REQUIRE(arpg::dungeon::material_drop_chance_bp(27U) == 3500U);
    ARPG_REQUIRE(arpg::dungeon::material_drop_chance_bp(99U) == 3500U);
    return {};
}

arpg::test::Failure coupon_thresholds_are_exact() noexcept {
    ARPG_REQUIRE(arpg::dungeon::coupon_eligible(
        MaterialId::coupon_6, 16U, 6U));
    ARPG_REQUIRE(!arpg::dungeon::coupon_eligible(
        MaterialId::coupon_6, 15U, 6U));
    ARPG_REQUIRE(!arpg::dungeon::coupon_eligible(
        MaterialId::coupon_6, 16U, 5U));
    ARPG_REQUIRE(arpg::dungeon::coupon_eligible(
        MaterialId::coupon_9, 30U, 10U));
    ARPG_REQUIRE(arpg::dungeon::coupon_eligible(
        MaterialId::coupon_12, 60U, 14U));
    ARPG_REQUIRE(arpg::dungeon::coupon_eligible(
        MaterialId::coupon_15, 90U, 18U));
    ARPG_REQUIRE(!arpg::dungeon::coupon_eligible(
        MaterialId::reinforcement_stone, 999U, 999U));
    return {};
}

arpg::test::Failure normal_material_rolls_respect_depth_unlocks() noexcept {
    constexpr std::array<std::uint64_t, 4U> kDepths{{1U, 16U, 30U, 45U}};
    constexpr std::array<std::size_t, 4U> kExpectedUnlocked{{4U, 6U, 8U, 10U}};
    for (std::size_t band = 0U; band < kDepths.size(); ++band) {
        std::array<bool, arpg::items::kMaterialCount> seen{};
        for (std::uint64_t seed = 1U; seed <= 50000U; ++seed) {
            const auto rolled = arpg::dungeon::roll_material_drop(
                seed, 7U, kDepths[band], 99U);
            if (!rolled.has_value()) continue;
            ARPG_REQUIRE(!arpg::items::material_is_coupon(*rolled));
            seen[arpg::items::material_index(*rolled)] = true;
        }
        std::size_t seen_count = 0U;
        for (const bool value : seen) seen_count += value ? 1U : 0U;
        ARPG_REQUIRE(seen_count == kExpectedUnlocked[band]);
    }
    return {};
}

arpg::test::Failure common_material_and_coupon_use_independent_rolls() noexcept {
    bool found_both = false;
    for (std::uint64_t seed = 1U; seed <= 100000U; ++seed) {
        const auto common = arpg::dungeon::roll_material_drop(
            seed, 23U, 90U, 18U);
        const auto coupon = arpg::dungeon::roll_coupon_drop(
            seed, 23U, 90U, 18U, false);
        if (common.has_value() && coupon.has_value()) {
            ARPG_REQUIRE(!arpg::items::material_is_coupon(*common));
            ARPG_REQUIRE(arpg::items::material_is_coupon(*coupon));
            found_both = true;
            break;
        }
    }
    ARPG_REQUIRE(found_both);
    return {};
}

arpg::test::Failure abyss_material_rewards_use_fixed_counts_and_pool() noexcept {
    using arpg::abyss::AbyssDanger;
    ARPG_REQUIRE(arpg::dungeon::abyss_material_reward_count(
        AbyssDanger::low) == 1U);
    ARPG_REQUIRE(arpg::dungeon::abyss_material_reward_count(
        AbyssDanger::medium) == 2U);
    ARPG_REQUIRE(arpg::dungeon::abyss_material_reward_count(
        AbyssDanger::high) == 3U);

    std::array<bool, arpg::items::kMaterialCount> seen{};
    for (std::uint64_t seed = 1U; seed <= 50000U; ++seed) {
        const auto rolled = arpg::dungeon::roll_abyss_material(
            seed, 45U, 2U);
        ARPG_REQUIRE(rolled.has_value());
        ARPG_REQUIRE(!arpg::items::material_is_coupon(*rolled));
        seen[arpg::items::material_index(*rolled)] = true;
    }
    std::size_t seen_count = 0U;
    for (const bool value : seen) seen_count += value ? 1U : 0U;
    ARPG_REQUIRE(seen_count == 10U);
    return {};
}

arpg::test::Failure monster_materials_use_independent_fixed_ordinals() noexcept {
    constexpr std::uint16_t kSpawnOrdinal = 23U;
    DungeonRunState state = material_state(90U, kSpawnOrdinal, 18U, true);
    ARPG_REQUIRE(state.current_room.seed != 0U);
    DungeonSession session{DungeonRules{}, state};
    ARPG_REQUIRE(relay_material(
        session, kSpawnOrdinal, 18U, {4.0F, 5.0F, 0.0F}));
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.ground_material_count == 2U);
    ARPG_REQUIRE(snapshot.ground_materials[0U].ordinal
        == kSpawnOrdinal * 2U);
    ARPG_REQUIRE(snapshot.ground_materials[0U].source
        == arpg::dungeon::GroundMaterialSource::monster_common);
    ARPG_REQUIRE(snapshot.ground_materials[1U].ordinal
        == kSpawnOrdinal * 2U + 1U);
    ARPG_REQUIRE(snapshot.ground_materials[1U].source
        == arpg::dungeon::GroundMaterialSource::monster_coupon);
    ARPG_REQUIRE(snapshot.ground_item_count <= 1U);
    return {};
}

arpg::test::Failure material_pickup_is_atomic_and_claim_survives_reload()
    noexcept {
    constexpr std::uint16_t kSpawnOrdinal = 7U;
    constexpr std::uint16_t kMaterialOrdinal = kSpawnOrdinal * 2U;
    DungeonRunState state = material_state(45U, kSpawnOrdinal, 27U);
    ARPG_REQUIRE(state.current_room.seed != 0U);
    DungeonSession rollback{DungeonRules{}, state};
    const auto player = rollback.snapshot().combat->player.position;
    ARPG_REQUIRE(relay_material(rollback, kSpawnOrdinal, 27U, player));
    ARPG_REQUIRE(rollback.request_material_pickup(kMaterialOrdinal)
        == arpg::dungeon::RequestResult::accepted);
    const auto failed = *rollback.pending_save();
    ARPG_REQUIRE(failed.kind
        == arpg::dungeon::PendingSaveKind::material_pickup);
    ARPG_REQUIRE(failed.pickup_ordinal == kMaterialOrdinal);
    ARPG_REQUIRE(rollback.snapshot().pending_material_pickup_ordinal
        == kMaterialOrdinal);
    ARPG_REQUIRE((rollback.item_state().materials
        == std::array<std::uint64_t, arpg::items::kMaterialCount>{}));
    rollback.resolve_pending_save({
        arpg::dungeon::SaveDisposition::not_committed,
        failed.expected_generation,
        failed.next_state,
        failed.kind,
    });
    ARPG_REQUIRE(rollback.snapshot().ground_material_count >= 1U);
    ARPG_REQUIRE((rollback.item_state().materials
        == std::array<std::uint64_t, arpg::items::kMaterialCount>{}));

    ARPG_REQUIRE(rollback.request_material_pickup(kMaterialOrdinal)
        == arpg::dungeon::RequestResult::accepted);
    const auto committed = *rollback.pending_save();
    const MaterialId picked = rollback.snapshot().ground_materials[0U].material;
    ARPG_REQUIRE(resolve_committed(rollback));
    ARPG_REQUIRE(rollback.item_state().materials[
        arpg::items::material_index(picked)] == 1U);
    ARPG_REQUIRE((rollback.item_state().material_claimed_drop_bits[0U]
        & (std::uint64_t{1U} << kMaterialOrdinal)) != 0U);
    ARPG_REQUIRE(rollback.snapshot().material_pickup_receipt.valid);
    ARPG_REQUIRE(rollback.snapshot().material_pickup_receipt.commit_generation
        == committed.expected_generation);
    ARPG_REQUIRE(rollback.snapshot().material_pickup_receipt.counts[
        arpg::items::material_index(picked)] == 1U);

    DungeonSession reloaded{DungeonRules{}, committed.next_state};
    ARPG_REQUIRE(relay_material(reloaded, kSpawnOrdinal, 27U, player));
    ARPG_REQUIRE(reloaded.snapshot().ground_material_count == 0U);
    return {};
}

arpg::test::Failure expanded_room_material_pickup_uses_v9_claim_authority()
    noexcept {
    constexpr std::uint16_t kSpawnOrdinal = 200U;
    constexpr std::uint16_t kMaterialOrdinal = kSpawnOrdinal * 2U;
    DungeonRunState state = material_state(45U, kSpawnOrdinal, 27U);
    ARPG_REQUIRE(state.current_room.seed != 0U);
    DungeonSession session{DungeonRules{}, state};
    const auto player = session.snapshot().combat->player.position;
    ARPG_REQUIRE(relay_material(
        session, kSpawnOrdinal, 27U, player));
    ARPG_REQUIRE(session.request_material_pickup(kMaterialOrdinal)
        == arpg::dungeon::RequestResult::accepted);
    const arpg::dungeon::PendingSave* const pending =
        session.pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    for (const std::uint64_t word : pending->next_state.item_ownership
             .material_claimed_drop_bits) {
        ARPG_REQUIRE(word == 0U);
    }

    std::unique_ptr<arpg::checkpoint::SaveCheckpointSlot> saved{
        new (std::nothrow) arpg::checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(saved != nullptr);
    ARPG_REQUIRE(session.capture_save_checkpoint(
        *saved, 81U, &pending->next_state));
    ARPG_REQUIRE((saved->room_progress.secondary_claim_bits[
            kMaterialOrdinal / 64U]
        & (std::uint64_t{1U} << (kMaterialOrdinal % 64U))) != 0U);

    DungeonSession reloaded{DungeonRules{}, saved->state};
    ARPG_REQUIRE(reloaded.restore_room_progress_checkpoint(*saved));
    ARPG_REQUIRE(!arpg::test::DungeonSessionTestAccess::ground_materials(
        reloaded)[kMaterialOrdinal].active);
    return {};
}

arpg::test::Failure proximity_pickup_requires_combat_and_nearby_range()
    noexcept {
    constexpr std::uint16_t kSpawnOrdinal = 8U;
    DungeonRunState state = material_state(45U, kSpawnOrdinal, 27U);
    DungeonSession session{DungeonRules{}, state};
    const arpg::combat::RoomMonsterPlan* const plan =
        arpg::test::room_monster_plan(session);
    ARPG_REQUIRE(plan != nullptr);
    ARPG_REQUIRE(kSpawnOrdinal < plan->monster_count);
    const auto player = plan->monsters[kSpawnOrdinal].initial_position;
    arpg::test::set_player_position(session, player);
    ARPG_REQUIRE(relay_material(session, kSpawnOrdinal, 27U, player));
    arpg::test::clear_all_ground_items(session);
    session.request_nearby_pickups(player);
    ARPG_REQUIRE(!session.pending_save().has_value());
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
    session.request_nearby_pickups({player.x + 2.0F, player.y, 0.0F});
    ARPG_REQUIRE(!session.pending_save().has_value());
    session.request_nearby_pickups(player);
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->kind
        == arpg::dungeon::PendingSaveKind::material_pickup);
    return {};
}

arpg::test::Failure room_clear_vacuum_is_one_atomic_save() noexcept {
    DungeonRunState state = material_state(45U, 3U, 27U);
    DungeonSession session{DungeonRules{}, state};
    const auto player = session.snapshot().combat->player.position;
    arpg::test::install_ground_material(
        session, 6U, MaterialId::chaos, player);
    arpg::test::install_ground_material(
        session, 7U, MaterialId::coupon_6, player,
        arpg::dungeon::GroundMaterialSource::monster_coupon);
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
    arpg::test::prepare_room_clear(session);
    const auto pending = *session.pending_save();
    ARPG_REQUIRE(pending.kind == arpg::dungeon::PendingSaveKind::room_clear);
    ARPG_REQUIRE(pending.next_state.item_ownership.materials[
        arpg::items::material_index(MaterialId::chaos)] == 1U);
    ARPG_REQUIRE(pending.next_state.item_ownership.materials[
        arpg::items::material_index(MaterialId::coupon_6)] == 1U);
    ARPG_REQUIRE(session.snapshot().ground_material_count == 2U);
    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::not_committed,
        pending.expected_generation,
        pending.next_state,
        pending.kind,
    });
    ARPG_REQUIRE(session.snapshot().ground_material_count == 2U);
    ARPG_REQUIRE(session.item_state().materials[
        arpg::items::material_index(MaterialId::chaos)] == 0U);

    arpg::test::prepare_room_clear(session);
    ARPG_REQUIRE(resolve_committed(session));
    ARPG_REQUIRE(session.snapshot().ground_material_count == 0U);
    ARPG_REQUIRE(session.item_state().materials[
        arpg::items::material_index(MaterialId::chaos)] == 1U);
    ARPG_REQUIRE(session.item_state().materials[
        arpg::items::material_index(MaterialId::coupon_6)] == 1U);
    ARPG_REQUIRE(session.snapshot().material_pickup_receipt.room_vacuum);
    return {};
}

arpg::test::Failure room_clear_save_failure_retries_vacuum_after_unlock()
    noexcept {
    DungeonRunState state = material_state(45U, 3U, 27U);
    for (std::size_t word = 0U; word < 6U; ++word) {
        state.item_ownership.material_claimed_drop_bits[word] =
            (std::numeric_limits<std::uint64_t>::max)();
    }
    state.item_ownership.claimed_drop_bits.fill(
        (std::numeric_limits<std::uint64_t>::max)());
    DungeonSession session{DungeonRules{}, state};
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase
        == arpg::dungeon::RoomPhase::combat);
    arpg::test::install_ground_material(
        session, arpg::dungeon::kAbyssMaterialOrdinalBegin,
        MaterialId::chaos, {100.0F, 100.0F, 0.0F});

    for (int tick = 0; tick < 4096; ++tick) {
        const auto snapshot = session.snapshot();
        if (snapshot.phase == arpg::dungeon::RoomPhase::committing) break;
        if (snapshot.phase == arpg::dungeon::RoomPhase::combat) {
            arpg::test::force_defeat_current_wave(session);
        }
        session.tick({});
    }
    ARPG_REQUIRE(session.pending_save().has_value());
    const auto unlock = *session.pending_save();
    ARPG_REQUIRE(unlock.kind
        == arpg::dungeon::PendingSaveKind::room_unlock);
    ARPG_REQUIRE(resolve_committed(session));
    ARPG_REQUIRE(session.snapshot().phase
        == arpg::dungeon::RoomPhase::combat);
    for (const bool open : session.snapshot().exits_open) {
        ARPG_REQUIRE(open);
    }

    const auto ground_before =
        arpg::test::DungeonSessionTestAccess::ground_materials(session);
    const auto material_counts_before = session.item_state().materials;
    std::array<std::uint64_t, arpg::items::kMaterialCount>
        vacuumed_material_counts{};
    std::uint16_t ground_count_before = 0U;
    for (const auto& ground : ground_before) {
        if (!ground.active) continue;
        const std::size_t material_index =
            arpg::items::material_index(ground.material);
        ARPG_REQUIRE(material_index < vacuumed_material_counts.size());
        ++vacuumed_material_counts[material_index];
        ++ground_count_before;
    }
    ARPG_REQUIRE(ground_count_before != 0U);

    session.tick({});
    ARPG_REQUIRE(session.pending_save().has_value());
    const auto first = *session.pending_save();
    ARPG_REQUIRE(first.kind
        == arpg::dungeon::PendingSaveKind::room_clear);
    for (const bool open : session.snapshot().exits_open) {
        ARPG_REQUIRE(open);
    }

    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::not_committed,
        first.expected_generation,
        first.next_state,
        first.kind,
    });
    const auto& ground_after_rollback =
        arpg::test::DungeonSessionTestAccess::ground_materials(session);
    for (std::size_t index = 0U; index < ground_before.size(); ++index) {
        const auto& before = ground_before[index];
        const auto& after = ground_after_rollback[index];
        ARPG_REQUIRE(after.active == before.active);
        ARPG_REQUIRE(after.ordinal == before.ordinal);
        ARPG_REQUIRE(after.source == before.source);
        ARPG_REQUIRE(after.position.x == before.position.x);
        ARPG_REQUIRE(after.position.y == before.position.y);
        ARPG_REQUIRE(after.position.z == before.position.z);
        ARPG_REQUIRE(after.material == before.material);
    }
    for (const bool open : session.snapshot().exits_open) {
        ARPG_REQUIRE(open);
    }

    session.tick({});
    ARPG_REQUIRE(session.pending_save().has_value());
    const auto retry = *session.pending_save();
    ARPG_REQUIRE(retry.kind
        == arpg::dungeon::PendingSaveKind::room_clear);
    ARPG_REQUIRE(retry.expected_generation == first.expected_generation);
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        retry.next_state, first.next_state));
    for (const bool open : session.snapshot().exits_open) {
        ARPG_REQUIRE(open);
    }

    ARPG_REQUIRE(resolve_committed(session));
    ARPG_REQUIRE(session.snapshot().ground_material_count == 0U);
    for (std::size_t index = 0U;
            index < vacuumed_material_counts.size(); ++index) {
        ARPG_REQUIRE(session.item_state().materials[index]
            == material_counts_before[index]
                + vacuumed_material_counts[index]);
    }
    for (const bool open : session.snapshot().exits_open) {
        ARPG_REQUIRE(open);
    }
    return {};
}

arpg::test::Failure abyss_clear_adds_one_two_or_three_materials() noexcept {
    constexpr std::array<arpg::abyss::AbyssDanger, 3U> kDangers{{
        arpg::abyss::AbyssDanger::low,
        arpg::abyss::AbyssDanger::medium,
        arpg::abyss::AbyssDanger::high,
    }};
    for (const auto danger : kDangers) {
        DungeonRunState state = material_state(45U, 1U, 27U);
        DungeonSession session{DungeonRules{}, state};
        arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
        arpg::test::set_started_abyss_room(session, danger);
        arpg::test::prepare_room_clear(session);
        const arpg::dungeon::PendingSave* const pending =
            session.pending_save_view();
        ARPG_REQUIRE(pending != nullptr);
        ARPG_REQUIRE(pending->kind
            == arpg::dungeon::PendingSaveKind::abyss_clear);
        std::uint64_t total = 0U;
        for (const std::uint64_t count :
                pending->next_state.item_ownership.materials) {
            total += count;
        }
        ARPG_REQUIRE(total
            == arpg::dungeon::abyss_material_reward_count(danger));
        ARPG_REQUIRE(pending->next_state.item_ownership
            .material_claimed_drop_bits
            == state.item_ownership.material_claimed_drop_bits);
        std::unique_ptr<arpg::checkpoint::SaveCheckpointSlot> saved{
            new (std::nothrow) arpg::checkpoint::SaveCheckpointSlot{}};
        ARPG_REQUIRE(saved != nullptr);
        ARPG_REQUIRE(session.capture_save_checkpoint(
            *saved, 82U, &pending->next_state));
        for (std::uint8_t index = 0U;
                index < arpg::dungeon::abyss_material_reward_count(danger);
                ++index) {
            const std::uint16_t material_ordinal = static_cast<std::uint16_t>(
                arpg::dungeon::kAbyssMaterialOrdinalBegin + index);
            const std::uint16_t canonical =
                arpg::dungeon::checkpoint_material_ordinal(material_ordinal);
            ARPG_REQUIRE(!arpg::dungeon::legacy_secondary_claim_representable(
                canonical));
            ARPG_REQUIRE((saved->room_progress.secondary_claim_bits[
                    canonical / 64U]
                & (std::uint64_t{1U} << (canonical % 64U))) != 0U);
        }
    }
    return {};
}

arpg::test::Failure death_discards_ground_but_preserves_committed_counts()
    noexcept {
    DungeonRunState state = material_state(45U, 1U, 27U);
    state.item_ownership.materials[
        arpg::items::material_index(MaterialId::divine)] = 9U;
    state.item_ownership.material_claimed_drop_bits[0U] =
        std::uint64_t{1U} << 4U;
    DungeonSession session{DungeonRules{}, state};
    const auto player = session.snapshot().combat->player.position;
    arpg::test::install_ground_material(
        session, 2U, MaterialId::exalt, player);
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(session));
    arpg::test::DungeonSessionTestAccess::handle_player_defeat(session);
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->kind
        == arpg::dungeon::PendingSaveKind::death_retreat);
    ARPG_REQUIRE(session.pending_save()->next_state.item_ownership.materials[
        arpg::items::material_index(MaterialId::divine)] == 9U);
    ARPG_REQUIRE(session.pending_save()->next_state.item_ownership
        .material_claimed_drop_bits[0U] == 0U);
    ARPG_REQUIRE(resolve_committed(session));
    ARPG_REQUIRE(session.snapshot().ground_material_count == 0U);
    ARPG_REQUIRE(session.item_state().materials[
        arpg::items::material_index(MaterialId::divine)] == 9U);
    ARPG_REQUIRE(session.item_state().materials[
        arpg::items::material_index(MaterialId::exalt)] == 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"material chance basis points",
        &material_drop_chance_uses_frozen_basis_points},
    {"coupon thresholds", &coupon_thresholds_are_exact},
    {"normal material depth unlocks",
        &normal_material_rolls_respect_depth_unlocks},
    {"common and coupon independent",
        &common_material_and_coupon_use_independent_rolls},
    {"abyss material counts and pool",
        &abyss_material_rewards_use_fixed_counts_and_pool},
    {"monster material fixed ordinals",
        &monster_materials_use_independent_fixed_ordinals},
    {"material pickup atomic and reload safe",
        &material_pickup_is_atomic_and_claim_survives_reload},
    {"expanded room material pickup uses v9 claim authority",
        &expanded_room_material_pickup_uses_v9_claim_authority},
    {"material proximity requires combat and range",
        &proximity_pickup_requires_combat_and_nearby_range},
    {"room clear vacuum atomic", &room_clear_vacuum_is_one_atomic_save},
    {"room clear save failure retries vacuum",
        &room_clear_save_failure_retries_vacuum_after_unlock},
    {"abyss clear material counts",
        &abyss_clear_adds_one_two_or_three_materials},
    {"death discards unpicked materials",
        &death_discards_ground_but_preserves_committed_counts},
};

}  // namespace

arpg::test::TestSuite dungeon_material_loot_suite() noexcept {
    return arpg::test::make_suite("dungeon_material_loot", kCases);
}
