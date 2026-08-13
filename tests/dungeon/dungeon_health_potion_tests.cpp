#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "dungeon_test_support.hpp"

#include "checkpoint/room_progress_checkpoint.hpp"
#include "dungeon/health_potion_loot.hpp"
#include "dungeon/material_loot.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "combat/combat_scaling.hpp"
#include "persistence/save_store.hpp"

#include <array>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::GroundMaterialSource;

struct PotionTempDirectory final {
    std::filesystem::path path{};

    PotionTempDirectory() noexcept {
        std::error_code error;
        path = std::filesystem::temp_directory_path(error)
            / "arpg_health_potion_transaction"
            / std::to_string(static_cast<unsigned long long>(
                std::hash<std::string>{}(std::to_string(
                    reinterpret_cast<std::uintptr_t>(this)))));
        std::filesystem::remove_all(path, error);
        std::filesystem::create_directories(path, error);
        if (error) path.clear();
    }

    ~PotionTempDirectory() noexcept {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

struct DropSeedRequirements final {
    bool common{};
    bool coupon{};
    bool potion{};
};

std::uint64_t find_drop_seed(
    std::uint16_t spawn_ordinal,
    std::uint64_t depth,
    std::uint16_t affix_score,
    DropSeedRequirements required) noexcept {
    for (std::uint64_t seed = 1U; seed < 1000000U; ++seed) {
        const bool common = arpg::dungeon::roll_material_drop(
            seed, spawn_ordinal, depth, affix_score).has_value();
        const bool coupon = arpg::dungeon::roll_coupon_drop(
            seed, spawn_ordinal, depth, affix_score, false).has_value();
        const bool potion = arpg::dungeon::roll_health_potion_drop(
            seed, spawn_ordinal);
        if (common == required.common && coupon == required.coupon
                && potion == required.potion) {
            return seed;
        }
    }
    return 0U;
}

DungeonRunState potion_state(
    std::uint64_t seed, std::uint64_t depth = 90U) noexcept {
    DungeonRunState state = arpg::dungeon::make_initial_run_state(
        0x48505F504F54494FULL, DungeonRules{}).state;
    state.current_room.seed = seed;
    state.current_room.depth = depth;
    return state;
}

DungeonRunState retry_gate_state(std::uint64_t seed) {
    DungeonRunState state = potion_state(seed);
    arpg::items::ItemInstance item{};
    item.id = 7001U;
    item.base_id = 1U;
    item.rarity = arpg::items::ItemRarity::normal;
    item.item_level = 95U;
    item.required_level = 1U;
    state.item_ownership.items.push_back(item);
    state.item_ownership.next_item_sequence = 9U;
    state.item_ownership.materials[arpg::items::material_index(
        arpg::items::MaterialId::transmute)] = 1U;
    return state;
}

bool commit_pending_kind(DungeonSession& session,
    arpg::dungeon::PendingSaveKind expected_kind) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value() || pending->kind != expected_kind) return false;
    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation,
        pending->next_state,
        pending->kind,
    });
    return session.phase() != arpg::dungeon::RoomPhase::faulted;
}

bool commit_room_unlock_and_prepare_clear_save(
    DungeonSession& session) noexcept {
    if (!commit_pending_kind(
            session, arpg::dungeon::PendingSaveKind::room_unlock)) {
        return false;
    }
    if (session.phase() != arpg::dungeon::RoomPhase::combat
            || !session.snapshot().exits_unlocked) {
        return false;
    }
    arpg::test::prepare_room_clear(session);
    return session.phase() != arpg::dungeon::RoomPhase::faulted;
}

bool enter_health_potion_abyss_clear_retry(
    DungeonSession& session, std::uint16_t potion_spawn = 4U) noexcept {
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
    arpg::test::set_started_abyss_room(
        session, arpg::abyss::AbyssDanger::low);
    arpg::test::set_player_health(session, 500, 1000);
    arpg::test::quiesce_current_room_for_clear_retry(session);
    arpg::test::install_ground_health_potion(
        session, potion_spawn, {100.0F, 0.0F, 0.0F});
    session.tick({});
    if (!commit_room_unlock_and_prepare_clear_save(session)) return false;
    const arpg::dungeon::PendingSave* const pending =
        session.pending_save_view();
    if (pending == nullptr
            || pending->kind != arpg::dungeon::PendingSaveKind::abyss_clear
            || !pending->health_potion_claim.has_value()) {
        return false;
    }
    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::not_committed,
        pending->expected_generation,
        pending->next_state,
        pending->kind,
    });
    return session.phase() == arpg::dungeon::RoomPhase::combat
        && session.pending_save_view() == nullptr
        && arpg::test::DungeonSessionTestAccess::
            health_potion_abyss_clear_retry_pending(session);
}

bool relay_drop(
    DungeonSession& session,
    std::uint64_t seed,
    std::uint16_t spawn_ordinal,
    std::uint16_t affix_score,
    arpg::combat::Vec3 position) noexcept {
    arpg::test::set_current_room_seed(session, seed);
    const bool relayed = arpg::test::relay_defeated(
        session,
        static_cast<std::uint8_t>(spawn_ordinal / 96U),
        static_cast<std::uint8_t>(spawn_ordinal % 96U),
        position,
        true,
        arpg::combat::MonsterId::fire_bomber,
        spawn_ordinal,
        affix_score,
        false);
    static_cast<void>(session.try_pop_combat_event());
    return relayed;
}

bool relay_visual_drop(
    DungeonSession& session,
    std::uint64_t seed,
    std::uint16_t spawn_ordinal,
    std::uint16_t affix_score,
    arpg::combat::Vec3 position) noexcept {
    arpg::test::set_current_room_seed(session, seed);
    const bool relayed = arpg::test::relay_visual_defeated(
        session, spawn_ordinal, position, true,
        arpg::combat::MonsterId::fire_bomber, affix_score, false);
    static_cast<void>(session.try_pop_combat_event());
    return relayed;
}

const arpg::dungeon::GroundMaterialSnapshot* find_ground_material_snapshot(
    const arpg::dungeon::DungeonSnapshot& snapshot,
    std::uint16_t ordinal) noexcept {
    for (std::uint16_t index = 0U;
         index < snapshot.ground_material_count; ++index) {
        if (snapshot.ground_materials[index].ordinal == ordinal) {
            return &snapshot.ground_materials[index];
        }
    }
    return nullptr;
}

bool claim_bit_is_set(
    const arpg::dungeon::DungeonRunState& state,
    std::uint16_t ordinal) noexcept {
    const std::size_t word = static_cast<std::size_t>(ordinal / 64U);
    const std::uint64_t mask = std::uint64_t{1U} << (ordinal % 64U);
    return word < state.item_ownership.material_claimed_drop_bits.size()
        && (state.item_ownership.material_claimed_drop_bits[word] & mask) != 0U;
}

void install_potion(DungeonSession& session, std::uint16_t spawn,
    arpg::combat::Vec3 position = {100.0F, 0.0F, 0.0F}) noexcept {
    arpg::test::install_ground_health_potion(session, spawn, position);
}

arpg::test::Failure health_potion_rules_are_frozen() noexcept {
    static_assert(arpg::dungeon::kHealthPotionDropChanceBp == 3000U);
    static_assert(arpg::dungeon::kHealthPotionRestoreBp == 2500U);
    static_assert(arpg::dungeon::kHealthPotionAutoUseThresholdBp == 7500U);
    static_assert(arpg::dungeon::kGroundHealthPotionCapacity == 192U);
    ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(0U) == 1U);
    ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(191U) == 383U);
    return {};
}

arpg::test::Failure health_potion_roll_is_deterministic_and_has_both_outcomes()
    noexcept {
    // These seeds freeze both the independent HP-potion stream and the exact
    // 3000-basis-point comparison boundary for spawn ordinal 37.
    ARPG_REQUIRE(arpg::dungeon::roll_health_potion_drop(2725U, 37U));
    ARPG_REQUIRE(!arpg::dungeon::roll_health_potion_drop(1902U, 37U));
    constexpr std::array<bool, 4U> kFrozenDomainOutcomes{{
        true, false, true, false,
    }};
    for (std::uint64_t seed = 0U; seed < kFrozenDomainOutcomes.size(); ++seed) {
        ARPG_REQUIRE(arpg::dungeon::roll_health_potion_drop(seed, 37U)
            == kFrozenDomainOutcomes[seed]);
    }
    bool found_drop = false;
    bool found_miss = false;
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        const bool first = arpg::dungeon::roll_health_potion_drop(seed, 37U);
        const bool second = arpg::dungeon::roll_health_potion_drop(seed, 37U);
        ARPG_REQUIRE(first == second);
        found_drop = found_drop || first;
        found_miss = found_miss || !first;
    }
    ARPG_REQUIRE(found_drop);
    ARPG_REQUIRE(found_miss);
    return {};
}

arpg::test::Failure health_potion_threshold_is_integer_exact() noexcept {
    using arpg::dungeon::health_potion_auto_use_eligible;
    ARPG_REQUIRE(!health_potion_auto_use_eligible(751, 1000));
    ARPG_REQUIRE(health_potion_auto_use_eligible(750, 1000));
    ARPG_REQUIRE(health_potion_auto_use_eligible(749, 1000));
    ARPG_REQUIRE(!health_potion_auto_use_eligible(0, 1000));
    ARPG_REQUIRE(!health_potion_auto_use_eligible(1, 0));
    ARPG_REQUIRE(health_potion_auto_use_eligible(3, 4));
    ARPG_REQUIRE(!health_potion_auto_use_eligible(4, 4));
    return {};
}

arpg::test::Failure
coupon_has_priority_and_common_material_can_coexist_with_potion() noexcept {
    constexpr std::uint16_t kSpawn = 23U;
    constexpr std::uint64_t kDepth = 90U;
    constexpr std::uint16_t kScore = 18U;
    const std::uint64_t coupon_seed = find_drop_seed(
        kSpawn, kDepth, kScore, {false, true, true});
    const std::uint64_t common_potion_seed = find_drop_seed(
        kSpawn, kDepth, kScore, {true, false, true});
    ARPG_REQUIRE(coupon_seed != 0U);
    ARPG_REQUIRE(common_potion_seed != 0U);

    DungeonSession coupon_session{
        DungeonRules{}, potion_state(coupon_seed, kDepth)};
    ARPG_REQUIRE(relay_drop(coupon_session, coupon_seed, kSpawn, kScore,
        {2.0F, 3.0F, 9.0F}));
    const auto coupon_snapshot = coupon_session.snapshot();
    ARPG_REQUIRE(coupon_snapshot.ground_health_potion_count == 0U);
    const auto* secondary = find_ground_material_snapshot(
        coupon_snapshot, arpg::dungeon::health_potion_claim_ordinal(kSpawn));
    ARPG_REQUIRE(secondary != nullptr);
    ARPG_REQUIRE(secondary->source == GroundMaterialSource::monster_coupon);

    DungeonSession coexist_session{
        DungeonRules{}, potion_state(common_potion_seed, kDepth)};
    ARPG_REQUIRE(relay_drop(coexist_session, common_potion_seed, kSpawn,
        kScore, {-2.0F, 7.0F, 5.0F}));
    const auto coexist_snapshot = coexist_session.snapshot();
    ARPG_REQUIRE(coexist_snapshot.ground_health_potion_count == 1U);
    ARPG_REQUIRE(coexist_snapshot.ground_health_potions[0].claim_ordinal
        == arpg::dungeon::health_potion_claim_ordinal(kSpawn));
    const auto* common = find_ground_material_snapshot(
        coexist_snapshot, static_cast<std::uint16_t>(kSpawn * 2U));
    ARPG_REQUIRE(common != nullptr);
    ARPG_REQUIRE(common->source == GroundMaterialSource::monster_common);
    ARPG_REQUIRE(find_ground_material_snapshot(coexist_snapshot,
        arpg::dungeon::health_potion_claim_ordinal(kSpawn)) == nullptr);
    return {};
}

arpg::test::Failure
adding_potions_does_not_change_existing_material_or_coupon_rolls() noexcept {
    constexpr std::size_t kBatch = 48U;
    constexpr std::uint64_t kDepth = 90U;
    constexpr std::uint16_t kScore = 18U;
    const std::uint64_t seed = find_drop_seed(
        0U, kDepth, kScore, {false, false, true});
    ARPG_REQUIRE(seed != 0U);
    std::array<std::optional<arpg::items::MaterialId>, kBatch> common{};
    std::array<std::optional<arpg::items::MaterialId>, kBatch> coupons{};
    for (std::uint16_t spawn = 0U; spawn < kBatch; ++spawn) {
        common[spawn] = arpg::dungeon::roll_material_drop(
            seed, spawn, kDepth, kScore);
        coupons[spawn] = arpg::dungeon::roll_coupon_drop(
            seed, spawn, kDepth, kScore, false);
    }

    DungeonSession session{DungeonRules{}, potion_state(seed, kDepth)};
    for (std::uint16_t spawn = 0U; spawn < kBatch; ++spawn) {
        ARPG_REQUIRE(relay_drop(session, seed, spawn, kScore,
            {static_cast<float>(spawn), -static_cast<float>(spawn), 8.0F}));
    }
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.ground_health_potion_count != 0U);
    for (std::uint16_t spawn = 0U; spawn < kBatch; ++spawn) {
        ARPG_REQUIRE(common[spawn] == arpg::dungeon::roll_material_drop(
            seed, spawn, kDepth, kScore));
        ARPG_REQUIRE(coupons[spawn] == arpg::dungeon::roll_coupon_drop(
            seed, spawn, kDepth, kScore, false));
        const auto* actual_common = find_ground_material_snapshot(
            snapshot, static_cast<std::uint16_t>(spawn * 2U));
        ARPG_REQUIRE((actual_common != nullptr) == common[spawn].has_value());
        if (actual_common != nullptr) {
            ARPG_REQUIRE(actual_common->material == *common[spawn]);
            ARPG_REQUIRE(actual_common->source
                == GroundMaterialSource::monster_common);
        }
        const auto* actual_coupon = find_ground_material_snapshot(
            snapshot, arpg::dungeon::health_potion_claim_ordinal(spawn));
        ARPG_REQUIRE((actual_coupon != nullptr)
            == coupons[spawn].has_value());
        if (actual_coupon != nullptr) {
            ARPG_REQUIRE(actual_coupon->material == *coupons[spawn]);
            ARPG_REQUIRE(actual_coupon->source
                == GroundMaterialSource::monster_coupon);
        }
    }
    return {};
}

arpg::test::Failure
ground_potion_pool_accepts_spawn_zero_and_191_without_overwrite() noexcept {
    constexpr std::uint64_t kDepth = 90U;
    constexpr std::uint16_t kScore = 18U;
    const std::uint64_t initial_seed = find_drop_seed(
        0U, kDepth, kScore, {false, false, true});
    ARPG_REQUIRE(initial_seed != 0U);
    DungeonSession session{
        DungeonRules{}, potion_state(initial_seed, kDepth)};
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    arpg::dungeon::DungeonSnapshot full{};
    {
        arpg::test::ScopedAllocationFailure fail{0U};
        for (std::uint16_t spawn = 0U;
             spawn < arpg::dungeon::kGroundHealthPotionCapacity; ++spawn) {
            const std::uint64_t seed = find_drop_seed(
                spawn, kDepth, kScore, {false, false, true});
            ARPG_REQUIRE(seed != 0U);
            ARPG_REQUIRE(relay_drop(session, seed, spawn, kScore,
                {static_cast<float>(spawn),
                    static_cast<float>(spawn + 1U), 99.0F}));
        }
        full = session.snapshot();
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    ARPG_REQUIRE(full.ground_health_potion_count
        == arpg::dungeon::kGroundHealthPotionCapacity);
    ARPG_REQUIRE(full.ground_health_potions[0].spawn_ordinal == 0U);
    ARPG_REQUIRE(full.ground_health_potions[191].spawn_ordinal == 191U);
    ARPG_REQUIRE(full.ground_health_potions[0].claim_ordinal == 1U);
    ARPG_REQUIRE(full.ground_health_potions[191].claim_ordinal == 383U);
    const auto original = full.ground_health_potions[0];
    const std::uint32_t saturation_before =
        full.diagnostics.health_potion_ground_saturation_count;

    arpg::test::clear_rolled_material_claims(session);
    ARPG_REQUIRE(relay_visual_drop(session, initial_seed, 0U, kScore,
        {-999.0F, -888.0F, 77.0F}));
    const auto after = session.snapshot();
    ARPG_REQUIRE(after.ground_health_potion_count
        == arpg::dungeon::kGroundHealthPotionCapacity);
    ARPG_REQUIRE(std::memcmp(&original, &after.ground_health_potions[0],
        sizeof(original)) == 0);
    ARPG_REQUIRE(after.diagnostics.health_potion_ground_saturation_count
        == saturation_before + 1U);
    return {};
}

arpg::test::Failure
potion_snapshot_is_sorted_by_spawn_ordinal_and_clears_on_room_exit() noexcept {
    constexpr std::uint64_t kDepth = 90U;
    constexpr std::uint16_t kScore = 18U;
    constexpr std::array<std::uint16_t, 3U> kInstallOrder{{191U, 77U, 0U}};
    const std::uint64_t initial_seed = find_drop_seed(
        kInstallOrder[0], kDepth, kScore, {false, false, true});
    ARPG_REQUIRE(initial_seed != 0U);
    DungeonSession session{
        DungeonRules{}, potion_state(initial_seed, kDepth)};
    for (const std::uint16_t spawn : kInstallOrder) {
        const std::uint64_t seed = find_drop_seed(
            spawn, kDepth, kScore, {false, false, true});
        ARPG_REQUIRE(seed != 0U);
        ARPG_REQUIRE(relay_drop(session, seed, spawn, kScore,
            {static_cast<float>(spawn), 4.0F, 17.0F}));
    }
    const auto installed = session.snapshot();
    ARPG_REQUIRE(installed.ground_health_potion_count == 3U);
    ARPG_REQUIRE(installed.ground_health_potions[0].spawn_ordinal == 0U);
    ARPG_REQUIRE(installed.ground_health_potions[1].spawn_ordinal == 77U);
    ARPG_REQUIRE(installed.ground_health_potions[2].spawn_ordinal == 191U);
    ARPG_REQUIRE(!installed.health_potion_pickup_receipt.valid);

    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
    arpg::test::prepare_room_clear(session);
    const auto clear = session.pending_save();
    ARPG_REQUIRE(clear.has_value());
    ARPG_REQUIRE(clear->kind == arpg::dungeon::PendingSaveKind::room_clear);
    ARPG_REQUIRE(commit_pending_kind(
        session, arpg::dungeon::PendingSaveKind::room_clear));
    ARPG_REQUIRE(session.snapshot().phase
        == arpg::dungeon::RoomPhase::cleared);
    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 3U);
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase
        == arpg::dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 3U);

    arpg::test::set_current_room_hole(session, true);
    ARPG_REQUIRE(session.request_descent(true));
    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 3U);
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    session.resolve_pending_transition({
        arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation,
        pending->next_state,
    });
    ARPG_REQUIRE(session.snapshot().phase
        == arpg::dungeon::RoomPhase::transitioning);
    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 0U);
    return {};
}

arpg::test::Failure
potion_above_threshold_stays_grounded_but_75_percent_ignores_distance()
    noexcept {
    DungeonSession session{DungeonRules{}, potion_state(101U)};
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_position(session, {0.0F, 0.0F, 0.0F});
    arpg::test::set_player_health(session, 751, 1000);
    install_potion(session, 7U, {100.0F, 0.0F, 0.0F});

    session.tick({});
    auto visible = session.snapshot();
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(visible.combat.has_value());
    ARPG_REQUIRE(visible.combat->player.hp == 751);
    ARPG_REQUIRE(visible.ground_health_potion_count == 1U);
    ARPG_REQUIRE(!visible.health_potion_pickup_receipt.valid);

    arpg::test::set_player_health(session, 750, 1000);
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    session.tick({});
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    const auto pending = session.pending_save();
    visible = session.snapshot();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind
        == arpg::dungeon::PendingSaveKind::health_potion_pickup);
    ARPG_REQUIRE(pending->health_potion_claim.has_value());
    ARPG_REQUIRE(pending->health_potion_claim->count == 1U);
    ARPG_REQUIRE(pending->health_potion_claim->spawn_ordinals[0] == 7U);
    ARPG_REQUIRE(visible.pending_health_potion_spawn_ordinal == 7U);
    ARPG_REQUIRE(visible.combat->player.hp == 750);
    ARPG_REQUIRE(visible.ground_health_potion_count == 1U);
    ARPG_REQUIRE(!visible.health_potion_pickup_receipt.valid);
    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(session),
        arpg::dungeon::health_potion_claim_ordinal(7U)));
    ARPG_REQUIRE(claim_bit_is_set(pending->next_state,
        arpg::dungeon::health_potion_claim_ordinal(7U)));
    return {};
}

arpg::test::Failure
potion_auto_use_scans_stable_spawn_order_and_rechecks_after_commit() noexcept {
    DungeonSession session{DungeonRules{}, potion_state(102U)};
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_health(session, 749, 1000);
    install_potion(session, 9U);
    install_potion(session, 6U);
    install_potion(session, 2U);

    session.request_nearby_pickups({-500.0F, 0.0F, 0.0F});
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->health_potion_claim.has_value());
    ARPG_REQUIRE(session.pending_save()->health_potion_claim
        ->spawn_ordinals[0] == 2U);
    ARPG_REQUIRE(arpg::test::commit_pending(session));
    ARPG_REQUIRE(session.snapshot().combat->player.hp == 999);

    session.request_nearby_pickups({500.0F, 0.0F, 0.0F});
    ARPG_REQUIRE(!session.pending_save().has_value());
    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 2U);
    arpg::test::set_player_health(session, 750, 1000);
    session.request_nearby_pickups({500.0F, 0.0F, 0.0F});
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->health_potion_claim
        ->spawn_ordinals[0] == 6U);
    return {};
}

arpg::test::Failure
committed_potion_claim_is_not_reused_by_later_death_save() noexcept {
    DungeonSession session{DungeonRules{}, potion_state(108U, 4U)};
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
    arpg::test::set_player_health(session, 750, 1000);
    install_potion(session, 3U);

    session.request_nearby_pickups({});
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->kind
        == arpg::dungeon::PendingSaveKind::health_potion_pickup);
    ARPG_REQUIRE(commit_pending_kind(
        session, arpg::dungeon::PendingSaveKind::health_potion_pickup));
    ARPG_REQUIRE(!session.pending_save().has_value());

    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(session));
    session.tick({});
    const auto death = session.pending_save();
    ARPG_REQUIRE(death.has_value());
    ARPG_REQUIRE(death->kind
        == arpg::dungeon::PendingSaveKind::death_retreat);
    ARPG_REQUIRE(!death->health_potion_claim.has_value());

    auto saved = std::make_unique<arpg::checkpoint::SaveCheckpointSlot>();
    ARPG_REQUIRE(saved != nullptr);
    saved->state.item_ownership.items.reserve(
        death->next_state.item_ownership.items.size());
    ARPG_REQUIRE(session.capture_save_checkpoint(
        *saved, 2U, &death->next_state));
    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        death->expected_generation,
        death->next_state,
        death->kind,
    });
    ARPG_REQUIRE(session.phase() == arpg::dungeon::RoomPhase::death_pending);
    return {};
}

arpg::test::Failure
dead_player_or_death_snapshot_never_requests_or_consumes_potion() noexcept {
    DungeonSession dead{DungeonRules{}, potion_state(103U)};
    arpg::test::set_phase(dead, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_health(dead, 0, 1000);
    install_potion(dead, 3U);
    dead.request_nearby_pickups({});
    ARPG_REQUIRE(!dead.pending_save().has_value());
    ARPG_REQUIRE(dead.snapshot().combat->player.hp == 0);
    ARPG_REQUIRE(dead.snapshot().ground_health_potion_count == 1U);
    ARPG_REQUIRE(!dead.snapshot().health_potion_pickup_receipt.valid);

    DungeonSession death_snapshot{DungeonRules{}, potion_state(104U)};
    ARPG_REQUIRE(arpg::test::kill_current_player_through_combat(death_snapshot));
    arpg::test::set_player_health(death_snapshot, 750, 1000);
    arpg::test::set_phase(
        death_snapshot, arpg::dungeon::RoomPhase::awaiting_exit);
    install_potion(death_snapshot, 3U);
    death_snapshot.request_nearby_pickups({});
    ARPG_REQUIRE(!death_snapshot.pending_save().has_value());
    ARPG_REQUIRE(death_snapshot.snapshot().combat->player.hp == 750);
    ARPG_REQUIRE(death_snapshot.snapshot().ground_health_potion_count == 1U);
    ARPG_REQUIRE(!death_snapshot.snapshot().health_potion_pickup_receipt.valid);

    DungeonSession no_combat{DungeonRules{}, potion_state(105U)};
    arpg::test::set_phase(no_combat, arpg::dungeon::RoomPhase::awaiting_exit);
    install_potion(no_combat, 3U);
    arpg::test::clear_combat_world(no_combat);
    no_combat.request_nearby_pickups({});
    ARPG_REQUIRE(!no_combat.pending_save().has_value());
    ARPG_REQUIRE(no_combat.snapshot().ground_health_potion_count == 1U);
    ARPG_REQUIRE(!no_combat.snapshot().health_potion_pickup_receipt.valid);

    DungeonSession wrong_phase{DungeonRules{}, potion_state(106U)};
    arpg::test::set_player_health(wrong_phase, 750, 1000);
    arpg::test::set_phase(
        wrong_phase, arpg::dungeon::RoomPhase::transitioning);
    install_potion(wrong_phase, 3U);
    wrong_phase.request_nearby_pickups({});
    ARPG_REQUIRE(!wrong_phase.pending_save().has_value());
    ARPG_REQUIRE(wrong_phase.snapshot().combat->player.hp == 750);
    ARPG_REQUIRE(wrong_phase.snapshot().ground_health_potion_count == 1U);
    ARPG_REQUIRE(!wrong_phase.snapshot().health_potion_pickup_receipt.valid);
    return {};
}

arpg::test::Failure
committed_pickup_heals_caps_removes_and_publishes_exact_receipt() noexcept {
    DungeonSession session{DungeonRules{}, potion_state(107U)};
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_health(session, 750, 1000);
    install_potion(session, 9U);
    install_potion(session, 2U);
    const std::uint64_t generation_before =
        session.snapshot().commit_generation;
    const std::uint64_t allocations_before_request =
        arpg::test::allocation_count();

    session.request_nearby_pickups({-1000.0F, -1000.0F, 0.0F});
    ARPG_REQUIRE(arpg::test::allocation_count()
        == allocations_before_request);
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(session.snapshot().combat->player.hp == 750);
    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 2U);
    ARPG_REQUIRE(!session.snapshot().health_potion_pickup_receipt.valid);
    const arpg::dungeon::PendingSaveResult receipt{
        arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation,
        pending->next_state,
    };
    const std::uint64_t allocations_before_resolve =
        arpg::test::allocation_count();
    {
        arpg::test::ScopedAllocationFailure fail{0U};
        session.resolve_pending_save(receipt);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before_resolve);

    const auto committed = session.snapshot();
    ARPG_REQUIRE(committed.phase
        == arpg::dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(committed.combat->player.hp == 1000);
    ARPG_REQUIRE(committed.ground_health_potion_count == 1U);
    ARPG_REQUIRE(committed.ground_health_potions[0].spawn_ordinal == 9U);
    ARPG_REQUIRE(committed.health_potion_pickup_receipt.valid);
    ARPG_REQUIRE(!committed.health_potion_pickup_receipt.room_clear);
    ARPG_REQUIRE(committed.health_potion_pickup_receipt.restored_hp == 250);
    ARPG_REQUIRE(committed.health_potion_pickup_receipt.consumed_count == 1U);
    ARPG_REQUIRE(committed.health_potion_pickup_receipt.commit_generation
        == generation_before + 1U);
    ARPG_REQUIRE(claim_bit_is_set(arpg::test::stable_state(session),
        arpg::dungeon::health_potion_claim_ordinal(2U)));
    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(session),
        arpg::dungeon::health_potion_claim_ordinal(9U)));
    return {};
}

arpg::test::Failure
failed_or_indeterminate_save_never_heals_removes_or_reports_success()
    noexcept {
    DungeonSession rollback{DungeonRules{}, potion_state(201U)};
    arpg::test::set_phase(
        rollback, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_health(rollback, 500, 1000);
    install_potion(rollback, 4U);
    rollback.request_nearby_pickups({});
    const auto first = rollback.pending_save();
    ARPG_REQUIRE(first.has_value());
    const DungeonRunState stable_before = arpg::test::stable_state(rollback);
    rollback.resolve_pending_save({
        arpg::dungeon::SaveDisposition::not_committed,
        first->expected_generation,
        first->next_state,
        first->kind,
    });
    auto visible = rollback.snapshot();
    ARPG_REQUIRE(visible.phase
        == arpg::dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(visible.combat->player.hp == 500);
    ARPG_REQUIRE(visible.ground_health_potion_count == 1U);
    ARPG_REQUIRE(!visible.health_potion_pickup_receipt.valid);
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        arpg::test::stable_state(rollback), stable_before));
    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(rollback),
        arpg::dungeon::health_potion_claim_ordinal(4U)));

    rollback.request_nearby_pickups({999.0F, 999.0F, 0.0F});
    const auto retry = rollback.pending_save();
    ARPG_REQUIRE(retry.has_value());
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        retry->next_state, first->next_state));
    ARPG_REQUIRE(retry->health_potion_claim.has_value());
    ARPG_REQUIRE(retry->health_potion_claim->spawn_ordinals
        == first->health_potion_claim->spawn_ordinals);
    ARPG_REQUIRE(retry->health_potion_claim->count
        == first->health_potion_claim->count);
    ARPG_REQUIRE(retry->health_potion_claim->expected_hp
        == first->health_potion_claim->expected_hp);
    ARPG_REQUIRE(retry->health_potion_claim->expected_max_hp
        == first->health_potion_claim->expected_max_hp);

    DungeonSession indeterminate{DungeonRules{}, potion_state(202U)};
    arpg::test::set_phase(
        indeterminate, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_health(indeterminate, 500, 1000);
    install_potion(indeterminate, 4U);
    indeterminate.request_nearby_pickups({});
    indeterminate.resolve_pending_save({
        arpg::dungeon::SaveDisposition::indeterminate, 0U, {}});
    visible = indeterminate.snapshot();
    ARPG_REQUIRE(visible.phase == arpg::dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(visible.diagnostics.fault
        == arpg::dungeon::DungeonFault::save_commit_indeterminate);
    ARPG_REQUIRE(visible.combat->player.hp == 500);
    ARPG_REQUIRE(visible.ground_health_potion_count == 1U);
    ARPG_REQUIRE(!visible.health_potion_pickup_receipt.valid);
    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(indeterminate),
        arpg::dungeon::health_potion_claim_ordinal(4U)));

    DungeonSession oversized{DungeonRules{}, potion_state(206U)};
    arpg::test::set_phase(oversized, arpg::dungeon::RoomPhase::combat);
    arpg::test::set_player_health(oversized, 1, 1000);
    install_potion(oversized, 2U);
    install_potion(oversized, 3U);
    install_potion(oversized, 6U);
    arpg::test::prepare_room_clear(oversized);
    ARPG_REQUIRE(oversized.pending_save().has_value());
    ARPG_REQUIRE(oversized.pending_save()->health_potion_claim.has_value());
    ARPG_REQUIRE(oversized.pending_save()->health_potion_claim->count == 3U);
    const DungeonRunState oversized_stable_before =
        arpg::test::stable_state(oversized);
    arpg::test::set_pending_health_potion_claim_count(oversized, 5U);
    arpg::test::set_pending_next_material_claim_bit(oversized,
        arpg::dungeon::health_potion_claim_ordinal(0U));
    arpg::test::set_pending_next_material_claim_bit(oversized,
        arpg::dungeon::health_potion_claim_ordinal(5U));
    ARPG_REQUIRE(!arpg::test::pending_material_cache_consistent(oversized));
    const auto oversized_pending = oversized.pending_save();
    ARPG_REQUIRE(oversized_pending.has_value());
    oversized.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        oversized_pending->expected_generation,
        oversized_pending->next_state,
        oversized_pending->kind,
    });
    visible = oversized.snapshot();
    ARPG_REQUIRE(visible.phase == arpg::dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(visible.diagnostics.fault
        == arpg::dungeon::DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(visible.combat->player.hp == 1);
    ARPG_REQUIRE(visible.ground_health_potion_count == 3U);
    ARPG_REQUIRE(!visible.health_potion_pickup_receipt.valid);
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        arpg::test::stable_state(oversized), oversized_stable_before));
    return {};
}

arpg::test::Failure
mismatched_pending_or_replaced_ground_faults_before_health_side_effects()
    noexcept {
    const auto assert_unchanged_fault = [](const DungeonSession& session,
        int expected_hp, std::uint16_t expected_ground) noexcept {
        const auto visible = session.snapshot();
        return visible.phase == arpg::dungeon::RoomPhase::faulted
            && visible.diagnostics.fault
                == arpg::dungeon::DungeonFault::save_receipt_mismatch
            && visible.combat.has_value()
            && visible.combat->player.hp == expected_hp
            && visible.ground_health_potion_count == expected_ground
            && !visible.health_potion_pickup_receipt.valid;
    };

    DungeonSession wrong_generation{DungeonRules{}, potion_state(203U)};
    arpg::test::set_phase(
        wrong_generation, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_health(wrong_generation, 500, 1000);
    install_potion(wrong_generation, 5U);
    wrong_generation.request_nearby_pickups({});
    const auto generation_pending = wrong_generation.pending_save();
    ARPG_REQUIRE(generation_pending.has_value());
    wrong_generation.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        generation_pending->expected_generation + 1U,
        generation_pending->next_state,
        generation_pending->kind,
    });
    ARPG_REQUIRE(assert_unchanged_fault(wrong_generation, 500, 1U));
    ARPG_REQUIRE(!claim_bit_is_set(
        arpg::test::stable_state(wrong_generation),
        arpg::dungeon::health_potion_claim_ordinal(5U)));

    DungeonSession wrong_kind{DungeonRules{}, potion_state(204U)};
    arpg::test::set_phase(
        wrong_kind, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_health(wrong_kind, 500, 1000);
    install_potion(wrong_kind, 5U);
    wrong_kind.request_nearby_pickups({});
    const auto kind_pending = wrong_kind.pending_save();
    ARPG_REQUIRE(kind_pending.has_value());
    wrong_kind.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        kind_pending->expected_generation,
        kind_pending->next_state,
        arpg::dungeon::PendingSaveKind::material_pickup,
    });
    ARPG_REQUIRE(assert_unchanged_fault(wrong_kind, 500, 1U));
    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(wrong_kind),
        arpg::dungeon::health_potion_claim_ordinal(5U)));

    DungeonSession replaced{DungeonRules{}, potion_state(205U)};
    arpg::test::set_phase(
        replaced, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_health(replaced, 500, 1000);
    install_potion(replaced, 5U);
    replaced.request_nearby_pickups({});
    const auto replaced_pending = replaced.pending_save();
    ARPG_REQUIRE(replaced_pending.has_value());
    arpg::test::replace_ground_health_potion(replaced, 5U, {
        true, 6U, arpg::dungeon::health_potion_claim_ordinal(6U),
        {17.0F, 18.0F, 0.0F}});
    replaced.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        replaced_pending->expected_generation,
        replaced_pending->next_state,
        replaced_pending->kind,
    });
    ARPG_REQUIRE(assert_unchanged_fault(replaced, 500, 1U));
    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(replaced),
        arpg::dungeon::health_potion_claim_ordinal(5U)));

    DungeonSession polluted{DungeonRules{}, potion_state(207U)};
    arpg::test::set_phase(
        polluted, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_health(polluted, 500, 1000);
    install_potion(polluted, 5U);
    polluted.request_nearby_pickups({});
    ARPG_REQUIRE(polluted.pending_save().has_value());
    const DungeonRunState polluted_stable_before =
        arpg::test::stable_state(polluted);
    arpg::test::offset_pending_next_room_depth(polluted, 1U);
    const auto polluted_pending = polluted.pending_save();
    ARPG_REQUIRE(polluted_pending.has_value());
    polluted.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        polluted_pending->expected_generation,
        polluted_pending->next_state,
        polluted_pending->kind,
    });
    ARPG_REQUIRE(assert_unchanged_fault(polluted, 500, 1U));
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        arpg::test::stable_state(polluted), polluted_stable_before));
    return {};
}

arpg::test::Failure
committed_claim_without_runtime_resolve_does_not_replay_after_reload()
    noexcept {
    constexpr std::uint16_t kSpawn = 8U;
    constexpr std::uint64_t kDepth = 90U;
    constexpr std::uint16_t kScore = 18U;
    const std::uint64_t seed = find_drop_seed(
        kSpawn, kDepth, kScore, {false, false, true});
    ARPG_REQUIRE(seed != 0U);
    PotionTempDirectory directory;
    ARPG_REQUIRE(!directory.path.empty());
    arpg::persistence::SaveStore store({directory.path});
    auto seeded = store.commit(potion_state(seed, kDepth));
    ARPG_REQUIRE(seeded.state
        == arpg::persistence::SaveCommitState::committed);

    DungeonSession before_crash{
        DungeonRules{}, std::move(seeded.verified_state)};
    ARPG_REQUIRE(relay_drop(before_crash, seed, kSpawn, kScore,
        {2.0F, 3.0F, 0.0F}));
    ARPG_REQUIRE(before_crash.snapshot().ground_health_potion_count == 1U);
    arpg::test::set_phase(
        before_crash, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_health(before_crash, 500, 1000);
    before_crash.request_nearby_pickups({});
    const arpg::dungeon::PendingSave* const pending =
        before_crash.pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    auto durable = store.commit(pending->next_state);
    ARPG_REQUIRE(durable.state
        == arpg::persistence::SaveCommitState::committed);

    auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == arpg::persistence::SaveLoadState::ready);
    ARPG_REQUIRE(claim_bit_is_set(loaded.checkpoint,
        arpg::dungeon::health_potion_claim_ordinal(kSpawn)));
    DungeonSession reloaded{
        DungeonRules{}, std::move(loaded.checkpoint)};
    arpg::test::set_phase(
        reloaded, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::set_player_health(reloaded, 500, 1000);
    ARPG_REQUIRE(relay_drop(reloaded, seed, kSpawn, kScore,
        {2.0F, 3.0F, 0.0F}));
    reloaded.tick({});
    const auto visible = reloaded.snapshot();
    ARPG_REQUIRE(visible.combat->player.hp == 500);
    ARPG_REQUIRE(visible.ground_health_potion_count == 0U);
    ARPG_REQUIRE(!visible.pending_health_potion_spawn_ordinal.has_value());
    ARPG_REQUIRE(!visible.health_potion_pickup_receipt.valid);
    ARPG_REQUIRE(claim_bit_is_set(arpg::test::stable_state(reloaded),
        arpg::dungeon::health_potion_claim_ordinal(kSpawn)));
    return {};
}

arpg::test::Failure
final_kill_low_health_folds_sorted_potions_into_clear_transaction() noexcept {
    constexpr std::uint16_t kMaterialOrdinal = 40U;
    const std::size_t chaos_index = arpg::items::material_index(
        arpg::items::MaterialId::chaos);
    DungeonSession normal{DungeonRules{}, potion_state(301U)};
    arpg::test::set_phase(normal, arpg::dungeon::RoomPhase::combat);
    arpg::test::set_player_health(normal, 500, 1000);
    install_potion(normal, 8U);
    install_potion(normal, 3U);
    arpg::test::install_ground_material(normal, kMaterialOrdinal,
        arpg::items::MaterialId::chaos, {4.0F, 5.0F, 0.0F});
    arpg::test::prepare_room_clear(normal);
    const auto first = normal.pending_save();
    ARPG_REQUIRE(first.has_value());
    ARPG_REQUIRE(first->kind == arpg::dungeon::PendingSaveKind::room_clear);
    ARPG_REQUIRE(first->health_potion_claim.has_value());
    ARPG_REQUIRE(first->health_potion_claim->count == 2U);
    ARPG_REQUIRE(first->health_potion_claim->spawn_ordinals[0] == 3U);
    ARPG_REQUIRE(first->health_potion_claim->spawn_ordinals[1] == 8U);
    ARPG_REQUIRE(first->next_state.item_ownership.materials[chaos_index]
        == arpg::test::stable_state(normal)
            .item_ownership.materials[chaos_index] + 1U);
    ARPG_REQUIRE(claim_bit_is_set(first->next_state, kMaterialOrdinal));
    ARPG_REQUIRE(claim_bit_is_set(first->next_state,
        arpg::dungeon::health_potion_claim_ordinal(3U)));
    ARPG_REQUIRE(claim_bit_is_set(first->next_state,
        arpg::dungeon::health_potion_claim_ordinal(8U)));
    ARPG_REQUIRE(normal.snapshot().combat->player.hp == 500);
    ARPG_REQUIRE(normal.snapshot().ground_health_potion_count == 2U);
    ARPG_REQUIRE(normal.snapshot().ground_material_count == 1U);
    ARPG_REQUIRE(!normal.snapshot().health_potion_pickup_receipt.valid);

    normal.resolve_pending_save({
        arpg::dungeon::SaveDisposition::not_committed,
        first->expected_generation,
        first->next_state,
        first->kind,
    });
    ARPG_REQUIRE(normal.snapshot().phase
        == arpg::dungeon::RoomPhase::combat);
    ARPG_REQUIRE(normal.snapshot().combat->player.hp == 500);
    ARPG_REQUIRE(normal.snapshot().ground_health_potion_count == 2U);
    ARPG_REQUIRE(normal.snapshot().ground_material_count == 1U);
    ARPG_REQUIRE(!normal.snapshot().health_potion_pickup_receipt.valid);
    arpg::test::prepare_room_clear(normal);
    const auto retry = normal.pending_save();
    ARPG_REQUIRE(retry.has_value());
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        retry->next_state, first->next_state));
    ARPG_REQUIRE(retry->health_potion_claim->spawn_ordinals
        == first->health_potion_claim->spawn_ordinals);

    DungeonSession abyss{DungeonRules{}, potion_state(302U)};
    arpg::test::set_phase(abyss, arpg::dungeon::RoomPhase::combat);
    arpg::test::set_started_abyss_room(
        abyss, arpg::abyss::AbyssDanger::low);
    arpg::test::set_player_health(abyss, 500, 1000);
    ARPG_REQUIRE(arpg::test::install_delayed_abyss_environment_hazard(abyss));
    arpg::test::quiesce_current_room_for_clear_retry(abyss);
    install_potion(abyss, 4U);
    arpg::test::install_ground_material(abyss, kMaterialOrdinal,
        arpg::items::MaterialId::chaos, {4.0F, 5.0F, 0.0F});
    abyss.tick({});
    ARPG_REQUIRE(commit_room_unlock_and_prepare_clear_save(abyss));
    const auto abyss_pending = abyss.pending_save();
    ARPG_REQUIRE(abyss_pending.has_value());
    ARPG_REQUIRE(abyss_pending->kind
        == arpg::dungeon::PendingSaveKind::abyss_clear);
    ARPG_REQUIRE(abyss_pending->health_potion_claim.has_value());
    ARPG_REQUIRE(abyss_pending->health_potion_claim->count == 1U);
    ARPG_REQUIRE(abyss_pending->health_potion_claim->spawn_ordinals[0] == 4U);
    ARPG_REQUIRE(claim_bit_is_set(
        abyss_pending->next_state, kMaterialOrdinal));
    ARPG_REQUIRE(claim_bit_is_set(abyss_pending->next_state,
        arpg::dungeon::health_potion_claim_ordinal(4U)));
    ARPG_REQUIRE(abyss_pending->next_state.item_ownership
        .materials[chaos_index]
        > arpg::test::stable_state(abyss)
            .item_ownership.materials[chaos_index]);
    ARPG_REQUIRE(abyss.snapshot().combat->player.hp == 500);
    ARPG_REQUIRE(abyss.snapshot().ground_health_potion_count == 1U);
    ARPG_REQUIRE(!abyss.snapshot().health_potion_pickup_receipt.valid);
    const DungeonRunState abyss_stable_before =
        arpg::test::stable_state(abyss);
    const auto abyss_before_failure = abyss.snapshot();
    abyss.resolve_pending_save({
        arpg::dungeon::SaveDisposition::not_committed,
        abyss_pending->expected_generation,
        abyss_pending->next_state,
        abyss_pending->kind,
    });
    const auto abyss_after_failure = abyss.snapshot();
    ARPG_REQUIRE(abyss_after_failure.phase
        == arpg::dungeon::RoomPhase::combat);
    ARPG_REQUIRE(abyss_after_failure.combat->player.hp
        == abyss_before_failure.combat->player.hp);
    ARPG_REQUIRE(abyss_after_failure.ground_health_potion_count
        == abyss_before_failure.ground_health_potion_count);
    ARPG_REQUIRE(abyss_after_failure.ground_material_count
        == abyss_before_failure.ground_material_count);
    for (std::uint16_t index = 0U;
            index < abyss_after_failure.ground_health_potion_count; ++index) {
        const auto& before = abyss_before_failure.ground_health_potions[index];
        const auto& after = abyss_after_failure.ground_health_potions[index];
        ARPG_REQUIRE(after.spawn_ordinal == before.spawn_ordinal);
        ARPG_REQUIRE(after.claim_ordinal == before.claim_ordinal);
        ARPG_REQUIRE(after.position.x == before.position.x);
        ARPG_REQUIRE(after.position.y == before.position.y);
        ARPG_REQUIRE(after.position.z == before.position.z);
    }
    for (std::uint16_t index = 0U;
            index < abyss_after_failure.ground_material_count; ++index) {
        const auto& before = abyss_before_failure.ground_materials[index];
        const auto& after = abyss_after_failure.ground_materials[index];
        ARPG_REQUIRE(after.ordinal == before.ordinal);
        ARPG_REQUIRE(after.source == before.source);
        ARPG_REQUIRE(after.material == before.material);
        ARPG_REQUIRE(after.position.x == before.position.x);
        ARPG_REQUIRE(after.position.y == before.position.y);
        ARPG_REQUIRE(after.position.z == before.position.z);
    }
    const auto& receipt_before =
        abyss_before_failure.health_potion_pickup_receipt;
    const auto& receipt_after =
        abyss_after_failure.health_potion_pickup_receipt;
    ARPG_REQUIRE(receipt_after.valid == receipt_before.valid);
    ARPG_REQUIRE(receipt_after.room_clear == receipt_before.room_clear);
    ARPG_REQUIRE(receipt_after.consumed_count == receipt_before.consumed_count);
    ARPG_REQUIRE(receipt_after.commit_generation
        == receipt_before.commit_generation);
    ARPG_REQUIRE(receipt_after.restored_hp == receipt_before.restored_hp);
    for (const bool exit_open : abyss_after_failure.exits_open) {
        ARPG_REQUIRE(exit_open);
    }
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        arpg::test::stable_state(abyss), abyss_stable_before));
    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(abyss),
        arpg::dungeon::health_potion_claim_ordinal(4U)));
    ARPG_REQUIRE(abyss_before_failure.combat->hazard_count == 1U);
    const auto hazard_before_retry =
        abyss_before_failure.combat->hazards[0];
    ARPG_REQUIRE(hazard_before_retry.active);
    ARPG_REQUIRE(hazard_before_retry.telegraph_ticks == 1U);
    const bool accepted_combat_action =
        abyss.queue_action(arpg::combat::Action::light);
    abyss.tick({});
    const auto abyss_after_retry = abyss.snapshot();
    ARPG_REQUIRE(abyss_after_retry.combat->player.hp
        == abyss_before_failure.combat->player.hp);
    ARPG_REQUIRE(abyss_after_retry.combat->hazard_count == 1U);
    const auto hazard_after_retry = abyss_after_retry.combat->hazards[0];
    ARPG_REQUIRE(hazard_after_retry.telegraph_ticks
        == hazard_before_retry.telegraph_ticks);
    ARPG_REQUIRE(hazard_after_retry.active_ticks
        == hazard_before_retry.active_ticks);
    ARPG_REQUIRE(hazard_after_retry.lifetime_ticks
        == hazard_before_retry.lifetime_ticks);
    ARPG_REQUIRE(hazard_after_retry.player_latched
        == hazard_before_retry.player_latched);
    ARPG_REQUIRE(!accepted_combat_action);
    const auto abyss_retry = abyss.pending_save();
    ARPG_REQUIRE(abyss_retry.has_value());
    ARPG_REQUIRE(abyss_retry->kind == abyss_pending->kind);
    ARPG_REQUIRE(abyss_retry->expected_generation
        == abyss_pending->expected_generation);
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        abyss_retry->next_state, abyss_pending->next_state));
    ARPG_REQUIRE(abyss_retry->health_potion_claim.has_value());
    ARPG_REQUIRE(abyss_retry->health_potion_claim->spawn_ordinals
        == abyss_pending->health_potion_claim->spawn_ordinals);
    ARPG_REQUIRE(abyss_retry->health_potion_claim->count
        == abyss_pending->health_potion_claim->count);
    ARPG_REQUIRE(abyss_retry->health_potion_claim->expected_hp
        == abyss_pending->health_potion_claim->expected_hp);
    ARPG_REQUIRE(abyss_retry->health_potion_claim->expected_max_hp
        == abyss_pending->health_potion_claim->expected_max_hp);
    return {};
}

arpg::test::Failure
abyss_clear_health_retry_gates_all_public_mutation_entries() noexcept {
    const auto retry_is_intact = [](const DungeonSession& session) noexcept {
        return session.phase() == arpg::dungeon::RoomPhase::combat
            && session.pending_save_view() == nullptr
            && session.snapshot().diagnostics.fault
                == arpg::dungeon::DungeonFault::none
            && arpg::test::DungeonSessionTestAccess::
                health_potion_abyss_clear_retry_pending(session);
    };

    auto nearby = std::make_unique<DungeonSession>(
        DungeonRules{}, retry_gate_state(306U));
    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(*nearby));
    const auto nearby_before = nearby->snapshot();
    const DungeonRunState nearby_stable_before =
        arpg::test::stable_state(*nearby);
    nearby->request_nearby_pickups({100.0F, 0.0F, 0.0F});
    ARPG_REQUIRE(retry_is_intact(*nearby));
    ARPG_REQUIRE(nearby->snapshot().ground_health_potion_count
        == nearby_before.ground_health_potion_count);
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        arpg::test::stable_state(*nearby), nearby_stable_before));

    auto equipment = std::make_unique<DungeonSession>(
        DungeonRules{}, retry_gate_state(307U));
    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(*equipment));
    ARPG_REQUIRE(equipment->request_equip(7001U)
        == arpg::dungeon::RequestResult::rejected);
    ARPG_REQUIRE(retry_is_intact(*equipment));

    auto crafting = std::make_unique<DungeonSession>(
        DungeonRules{}, retry_gate_state(308U));
    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(*crafting));
    ARPG_REQUIRE(crafting->request_craft(
        arpg::items::MaterialId::transmute, 7001U)
        == arpg::dungeon::RequestResult::rejected);
    ARPG_REQUIRE(retry_is_intact(*crafting));

    auto loadout = std::make_unique<DungeonSession>(
        DungeonRules{}, retry_gate_state(309U));
    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(*loadout));
    ARPG_REQUIRE(loadout->request_remove_active_skill(0U)
        == arpg::dungeon::RequestResult::rejected);
    ARPG_REQUIRE(retry_is_intact(*loadout));

    auto reset = std::make_unique<DungeonSession>(
        DungeonRules{}, retry_gate_state(310U));
    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(*reset));
    ARPG_REQUIRE(reset->reset_current_room()
        == arpg::dungeon::RequestResult::rejected);
    ARPG_REQUIRE(retry_is_intact(*reset));

    const arpg::dungeon::PendingSaveResult stray_result{
        arpg::dungeon::SaveDisposition::not_committed,
        0U,
        {},
        arpg::dungeon::PendingSaveKind::transition,
    };
    auto save_resolve = std::make_unique<DungeonSession>(
        DungeonRules{}, retry_gate_state(311U));
    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(*save_resolve));
    save_resolve->resolve_pending_save(stray_result);
    ARPG_REQUIRE(retry_is_intact(*save_resolve));

    auto transition_resolve = std::make_unique<DungeonSession>(
        DungeonRules{}, retry_gate_state(312U));
    ARPG_REQUIRE(enter_health_potion_abyss_clear_retry(
        *transition_resolve));
    transition_resolve->resolve_pending_transition(stray_result);
    ARPG_REQUIRE(retry_is_intact(*transition_resolve));

    nearby->tick({});
    const auto rebuilt = nearby->pending_save();
    ARPG_REQUIRE(rebuilt.has_value());
    ARPG_REQUIRE(rebuilt->kind
        == arpg::dungeon::PendingSaveKind::abyss_clear);
    ARPG_REQUIRE(rebuilt->health_potion_claim.has_value());
    ARPG_REQUIRE(rebuilt->health_potion_claim->count == 1U);
    ARPG_REQUIRE(rebuilt->health_potion_claim->spawn_ordinals[0] == 4U);
    return {};
}

arpg::test::Failure
clear_batch_selects_only_until_health_is_strictly_above_75_percent()
    noexcept {
    DungeonSession session{DungeonRules{}, potion_state(303U)};
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
    arpg::test::set_player_health(session, 1, 1000);
    install_potion(session, 9U);
    install_potion(session, 2U);
    install_potion(session, 6U);
    install_potion(session, 3U);
    arpg::test::prepare_room_clear(session);
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == arpg::dungeon::PendingSaveKind::room_clear);
    ARPG_REQUIRE(pending->health_potion_claim.has_value());
    ARPG_REQUIRE(pending->health_potion_claim->count == 3U);
    ARPG_REQUIRE(pending->health_potion_claim->spawn_ordinals[0] == 2U);
    ARPG_REQUIRE(pending->health_potion_claim->spawn_ordinals[1] == 3U);
    ARPG_REQUIRE(pending->health_potion_claim->spawn_ordinals[2] == 6U);
    ARPG_REQUIRE(session.snapshot().combat->player.hp == 1);
    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 4U);
    ARPG_REQUIRE(!session.snapshot().health_potion_pickup_receipt.valid);
    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation,
        pending->next_state,
        pending->kind,
    });
    const auto committed = session.snapshot();
    ARPG_REQUIRE(committed.phase == arpg::dungeon::RoomPhase::cleared);
    ARPG_REQUIRE(committed.combat->player.hp == 751);
    ARPG_REQUIRE(committed.ground_health_potion_count == 1U);
    ARPG_REQUIRE(committed.ground_health_potions[0].spawn_ordinal == 9U);
    ARPG_REQUIRE(committed.health_potion_pickup_receipt.valid);
    ARPG_REQUIRE(committed.health_potion_pickup_receipt.room_clear);
    ARPG_REQUIRE(committed.health_potion_pickup_receipt.consumed_count == 3U);
    ARPG_REQUIRE(committed.health_potion_pickup_receipt.restored_hp == 750);
    ARPG_REQUIRE(claim_bit_is_set(arpg::test::stable_state(session),
        arpg::dungeon::health_potion_claim_ordinal(2U)));
    ARPG_REQUIRE(claim_bit_is_set(arpg::test::stable_state(session),
        arpg::dungeon::health_potion_claim_ordinal(3U)));
    ARPG_REQUIRE(claim_bit_is_set(arpg::test::stable_state(session),
        arpg::dungeon::health_potion_claim_ordinal(6U)));
    ARPG_REQUIRE(!claim_bit_is_set(arpg::test::stable_state(session),
        arpg::dungeon::health_potion_claim_ordinal(9U)));

    DungeonSession wrong_kind_normal{DungeonRules{}, potion_state(306U)};
    arpg::test::set_phase(
        wrong_kind_normal, arpg::dungeon::RoomPhase::combat);
    arpg::test::set_player_health(wrong_kind_normal, 500, 1000);
    install_potion(wrong_kind_normal, 4U);
    arpg::test::prepare_room_clear(wrong_kind_normal);
    const auto wrong_kind_pending = wrong_kind_normal.pending_save();
    ARPG_REQUIRE(wrong_kind_pending.has_value());
    const DungeonRunState wrong_kind_stable_before =
        arpg::test::stable_state(wrong_kind_normal);
    wrong_kind_normal.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        wrong_kind_pending->expected_generation,
        wrong_kind_pending->next_state,
        arpg::dungeon::PendingSaveKind::material_pickup,
    });
    const auto wrong_kind_visible = wrong_kind_normal.snapshot();
    ARPG_REQUIRE(wrong_kind_visible.phase
        == arpg::dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(wrong_kind_visible.diagnostics.fault
        == arpg::dungeon::DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(wrong_kind_visible.combat->player.hp == 500);
    ARPG_REQUIRE(wrong_kind_visible.ground_health_potion_count == 1U);
    ARPG_REQUIRE(!wrong_kind_visible.health_potion_pickup_receipt.valid);
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        arpg::test::stable_state(wrong_kind_normal),
        wrong_kind_stable_before));
    return {};
}

arpg::test::Failure
final_kill_high_health_keeps_potion_until_room_transition() noexcept {
    DungeonSession session{DungeonRules{}, potion_state(304U)};
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
    arpg::test::set_player_health(session, 751, 1000);
    install_potion(session, 5U);
    arpg::test::prepare_room_clear(session);
    const auto clear = session.pending_save();
    ARPG_REQUIRE(clear.has_value());
    ARPG_REQUIRE(clear->kind == arpg::dungeon::PendingSaveKind::room_clear);
    ARPG_REQUIRE(commit_pending_kind(
        session, arpg::dungeon::PendingSaveKind::room_clear));
    ARPG_REQUIRE(session.snapshot().phase == arpg::dungeon::RoomPhase::cleared);
    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 1U);
    ARPG_REQUIRE(session.snapshot().combat->player.hp == 751);
    ARPG_REQUIRE(!session.snapshot().health_potion_pickup_receipt.valid);
    session.tick({});
    ARPG_REQUIRE(session.snapshot().phase
        == arpg::dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 1U);
    arpg::test::set_current_room_hole(session, true);
    ARPG_REQUIRE(session.request_descent(true));
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 1U);
    session.resolve_pending_transition({
        arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation,
        pending->next_state,
    });
    ARPG_REQUIRE(session.snapshot().phase
        == arpg::dungeon::RoomPhase::transitioning);
    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 0U);
    return {};
}

arpg::test::Failure
abyss_clear_potion_uses_post_clear_actual_max_health() noexcept {
    DungeonSession session{DungeonRules{}, potion_state(305U)};
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::combat);
    const int base_max = session.snapshot().combat->player.max_hp;
    arpg::test::set_started_life_sacrifice_abyss_room(session);
    const int sacrificed_max = session.snapshot().combat->player.max_hp;
    ARPG_REQUIRE(sacrificed_max > 0);
    ARPG_REQUIRE(sacrificed_max < base_max);
    arpg::test::set_player_health(session, 1, sacrificed_max);
    install_potion(session, 7U);
    arpg::test::prepare_room_clear(session);
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == arpg::dungeon::PendingSaveKind::abyss_clear);
    ARPG_REQUIRE(pending->health_potion_claim.has_value());
    ARPG_REQUIRE(pending->health_potion_claim->expected_max_hp
        == sacrificed_max);
    ARPG_REQUIRE(session.snapshot().combat->player.hp == 1);
    ARPG_REQUIRE(session.snapshot().ground_health_potion_count == 1U);
    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation,
        pending->next_state,
        pending->kind,
    });
    const auto committed = session.snapshot();
    const int mapped = arpg::abyss::map_resource_ratio(
        1, sacrificed_max, base_max, true).value_or(0);
    const int expected_restore = arpg::combat::scale_basis_points(
        base_max, arpg::dungeon::kHealthPotionRestoreBp,
        arpg::combat::BasisPointRounding::ceil);
    ARPG_REQUIRE(committed.phase == arpg::dungeon::RoomPhase::cleared);
    ARPG_REQUIRE(committed.combat->player.max_hp == base_max);
    ARPG_REQUIRE(committed.combat->player.hp == mapped + expected_restore);
    ARPG_REQUIRE(committed.health_potion_pickup_receipt.valid);
    ARPG_REQUIRE(committed.health_potion_pickup_receipt.room_clear);
    ARPG_REQUIRE(committed.health_potion_pickup_receipt.consumed_count == 1U);
    ARPG_REQUIRE(committed.health_potion_pickup_receipt.restored_hp
        == expected_restore);
    ARPG_REQUIRE(committed.ground_health_potion_count == 0U);

    DungeonSession wrong_kind_abyss{DungeonRules{}, potion_state(307U)};
    arpg::test::set_phase(
        wrong_kind_abyss, arpg::dungeon::RoomPhase::combat);
    arpg::test::set_started_abyss_room(
        wrong_kind_abyss, arpg::abyss::AbyssDanger::low);
    arpg::test::set_player_health(wrong_kind_abyss, 500, 1000);
    install_potion(wrong_kind_abyss, 4U);
    arpg::test::prepare_room_clear(wrong_kind_abyss);
    const auto wrong_kind_pending = wrong_kind_abyss.pending_save();
    ARPG_REQUIRE(wrong_kind_pending.has_value());
    const DungeonRunState wrong_kind_stable_before =
        arpg::test::stable_state(wrong_kind_abyss);
    wrong_kind_abyss.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        wrong_kind_pending->expected_generation,
        wrong_kind_pending->next_state,
        arpg::dungeon::PendingSaveKind::room_clear,
    });
    const auto wrong_kind_visible = wrong_kind_abyss.snapshot();
    ARPG_REQUIRE(wrong_kind_visible.phase
        == arpg::dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(wrong_kind_visible.diagnostics.fault
        == arpg::dungeon::DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(wrong_kind_visible.combat->player.hp == 500);
    ARPG_REQUIRE(wrong_kind_visible.ground_health_potion_count == 1U);
    ARPG_REQUIRE(!wrong_kind_visible.health_potion_pickup_receipt.valid);
    ARPG_REQUIRE(arpg::dungeon::same_run_state(
        arpg::test::stable_state(wrong_kind_abyss),
        wrong_kind_stable_before));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"health potion rules frozen", &health_potion_rules_are_frozen},
    {"health potion deterministic roll",
        &health_potion_roll_is_deterministic_and_has_both_outcomes},
    {"health potion exact threshold", &health_potion_threshold_is_integer_exact},
    {"coupon priority and common potion coexistence",
        &coupon_has_priority_and_common_material_can_coexist_with_potion},
    {"potions preserve material and coupon rolls",
        &adding_potions_does_not_change_existing_material_or_coupon_rolls},
    {"fixed potion pool boundaries and no overwrite",
        &ground_potion_pool_accepts_spawn_zero_and_191_without_overwrite},
    {"potion snapshot sorted and room exit clears",
        &potion_snapshot_is_sorted_by_spawn_ordinal_and_clears_on_room_exit},
    {"potion threshold and distance-free auto-use",
        &potion_above_threshold_stays_grounded_but_75_percent_ignores_distance},
    {"potion stable scan and post-commit recheck",
        &potion_auto_use_scans_stable_spawn_order_and_rechecks_after_commit},
    {"potion claim is not reused by later death",
        &committed_potion_claim_is_not_reused_by_later_death_save},
    {"dead player and death snapshot reject potion",
        &dead_player_or_death_snapshot_never_requests_or_consumes_potion},
    {"committed potion publishes exact receipt",
        &committed_pickup_heals_caps_removes_and_publishes_exact_receipt},
    {"failed potion save outcomes stay atomic",
        &failed_or_indeterminate_save_never_heals_removes_or_reports_success},
    {"potion cache mismatches fault before side effects",
        &mismatched_pending_or_replaced_ground_faults_before_health_side_effects},
    {"durable potion claim never replays after reload",
        &committed_claim_without_runtime_resolve_does_not_replay_after_reload},
    {"final kill folds sorted potions into clear save",
        &final_kill_low_health_folds_sorted_potions_into_clear_transaction},
    {"abyss clear health retry gates public mutations",
        &abyss_clear_health_retry_gates_all_public_mutation_entries},
    {"clear potion batch stops strictly above threshold",
        &clear_batch_selects_only_until_health_is_strictly_above_75_percent},
    {"high health clear preserves potion until transition",
        &final_kill_high_health_keeps_potion_until_room_transition},
    {"abyss clear potion uses post-clear max health",
        &abyss_clear_potion_uses_post_clear_actual_max_health},
};

}  // namespace

arpg::test::TestSuite dungeon_health_potion_suite() noexcept {
    return arpg::test::make_suite("dungeon_health_potion", kCases);
}
