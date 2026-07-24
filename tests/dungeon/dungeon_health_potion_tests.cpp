#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "dungeon_test_support.hpp"

#include "dungeon/health_potion_loot.hpp"
#include "dungeon/material_loot.hpp"
#include "dungeon/dungeon_progression.hpp"

#include <array>
#include <cstring>
#include <cstdint>
#include <optional>

namespace {

using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::GroundMaterialSource;

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
        affix_score);
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

arpg::test::Failure health_potion_rules_are_frozen() noexcept {
    ARPG_REQUIRE(arpg::dungeon::kHealthPotionDropChanceBp == 3000U);
    ARPG_REQUIRE(arpg::dungeon::kHealthPotionRestoreBp == 2500U);
    ARPG_REQUIRE(arpg::dungeon::kHealthPotionAutoUseThresholdBp == 7500U);
    ARPG_REQUIRE(arpg::dungeon::kGroundHealthPotionCapacity == 192U);
    ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(0U) == 1U);
    ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(191U) == 383U);
    return {};
}

arpg::test::Failure health_potion_roll_is_deterministic_and_has_both_outcomes()
    noexcept {
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
    ARPG_REQUIRE(relay_drop(session, initial_seed, 0U, kScore,
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
};

}  // namespace

arpg::test::TestSuite dungeon_health_potion_suite() noexcept {
    return arpg::test::make_suite("dungeon_health_potion", kCases);
}
