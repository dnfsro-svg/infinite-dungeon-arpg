#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "core/deterministic_rng.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"

#include <cstdint>
#include <limits>

namespace {

using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;

constexpr std::uint64_t kDropChanceDomain = 0x44524F505F43484EULL;
constexpr std::uint64_t kPositiveAffixTraceRootSeed = 1U;
constexpr std::uint16_t kPositiveAffixScore = 27U;
constexpr std::uint16_t kPositiveAffixHitOrdinal = 6U;
constexpr std::uint16_t kPositiveAffixMissOrdinal = 0U;

std::uint64_t drop_roll(const DungeonRunState& state,
    std::uint16_t ordinal, std::uint64_t bound) noexcept {
    auto ordinal_stream = arpg::core::DeterministicRng::derive_stream(
        state.current_room.seed, ordinal);
    auto chance = arpg::core::DeterministicRng::derive_stream(
        ordinal_stream.next_u64(), kDropChanceDomain);
    return chance.next_bounded(bound).value();
}

bool affix_drop_hits(const DungeonRunState& state,
    std::uint16_t ordinal, std::uint16_t score) noexcept {
    return drop_roll(state, ordinal, 10000U)
        < arpg::dungeon::affix_drop_chance_bp(score);
}

bool legacy_drop_hits(const DungeonRunState& state,
    std::uint16_t ordinal) noexcept {
    return drop_roll(state, ordinal, 100U) == 0U;
}

DungeonRunState state_for_direct_ordinal_trace(
    std::uint16_t ordinal) noexcept {
    const DungeonRules rules;
    for (std::uint64_t root = 1U; root < 10000U; ++root) {
        const DungeonRunState state =
            arpg::dungeon::make_initial_run_state(root, rules).state;
        if (!legacy_drop_hits(state, 0U)
                && affix_drop_hits(state, ordinal, 27U)) {
            return state;
        }
    }
    return {};
}

arpg::test::Failure dangerous_affix_reward_formulas_are_frozen() noexcept {
    using arpg::dungeon::affix_drop_chance_bp;
    using arpg::dungeon::affix_experience;
    using arpg::dungeon::affix_item_level;

    ARPG_REQUIRE(affix_drop_chance_bp(0U) == 100U);
    ARPG_REQUIRE(affix_drop_chance_bp(1U) == 250U);
    ARPG_REQUIRE(affix_drop_chance_bp(9U) == 1450U);
    ARPG_REQUIRE(affix_drop_chance_bp(27U) == 4150U);
    ARPG_REQUIRE(affix_item_level(95U, 0U) == 95U);
    ARPG_REQUIRE(affix_item_level(95U, 1U) == 96U);
    ARPG_REQUIRE(affix_item_level(95U, 9U) == 98U);
    ARPG_REQUIRE(affix_item_level(95U, 27U) == 100U);
    ARPG_REQUIRE(affix_experience(40U, 0U) == 40U);
    ARPG_REQUIRE(affix_experience(40U, 1U) == 44U);
    ARPG_REQUIRE(affix_experience(40U, 9U) == 76U);
    ARPG_REQUIRE(affix_experience(40U, 27U) == 148U);
    ARPG_REQUIRE(affix_experience((std::numeric_limits<std::uint64_t>::max)(),
        27U) == (std::numeric_limits<std::uint64_t>::max)());
    return {};
}

arpg::test::Failure positive_affix_drop_uses_bp_roll_and_can_miss() noexcept {
    const DungeonRules rules;
    const DungeonRunState state = arpg::dungeon::make_initial_run_state(
        kPositiveAffixTraceRootSeed, rules).state;
    constexpr std::uint16_t kChanceBp = 4150U;
    ARPG_REQUIRE(state.root_seed == kPositiveAffixTraceRootSeed);
    ARPG_REQUIRE(arpg::dungeon::affix_drop_chance_bp(kPositiveAffixScore)
        == kChanceBp);
    ARPG_REQUIRE(drop_roll(state, kPositiveAffixHitOrdinal, 10000U) == 279U);
    ARPG_REQUIRE(drop_roll(state, kPositiveAffixHitOrdinal, 100U) == 79U);
    ARPG_REQUIRE(drop_roll(state, kPositiveAffixHitOrdinal, 10000U)
        < kChanceBp);
    ARPG_REQUIRE(drop_roll(state, kPositiveAffixHitOrdinal, 100U)
        >= kChanceBp / 100U);
    ARPG_REQUIRE(drop_roll(state, kPositiveAffixMissOrdinal, 10000U)
        == 7592U);
    ARPG_REQUIRE(drop_roll(state, kPositiveAffixMissOrdinal, 10000U)
        >= kChanceBp);

    DungeonSession session{rules, state};
    const auto player = session.snapshot().combat->player.position;
    ARPG_REQUIRE(arpg::test::relay_defeated(session, 0U,
        kPositiveAffixHitOrdinal, player, true,
        arpg::combat::MonsterId::fire_bomber,
        kPositiveAffixHitOrdinal, kPositiveAffixScore));
    ARPG_REQUIRE(session.snapshot().ground_item_count == 1U);
    ARPG_REQUIRE(session.snapshot().ground_items[0].ordinal
        == kPositiveAffixHitOrdinal);
    ARPG_REQUIRE(arpg::test::relay_defeated(session, 0U,
        kPositiveAffixMissOrdinal, player, true,
        arpg::combat::MonsterId::fire_bomber,
        kPositiveAffixMissOrdinal, kPositiveAffixScore));
    ARPG_REQUIRE(session.snapshot().ground_item_count == 1U);
    return {};
}

arpg::test::Failure defeat_reward_uses_only_stable_event_fields() noexcept {
    const DungeonRules rules;
    const DungeonRunState state = state_for_direct_ordinal_trace(111U);
    ARPG_REQUIRE(state.root_seed != 0U);
    DungeonSession session{rules, state};

    ARPG_REQUIRE(arpg::test::relay_defeated(session, 0U, 0xFFU,
        {7.0F, 3.0F, 0.0F}, true,
        arpg::combat::MonsterId::fire_bomber, 111U, 27U));

    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.pending_room_experience == 74U);
    ARPG_REQUIRE(snapshot.ground_item_count == 1U);
    ARPG_REQUIRE(snapshot.ground_items[0].ordinal == 111U);
    ARPG_REQUIRE(arpg::test::ground_items(session)[111U].item.item_level == 10U);
    return {};
}

arpg::test::Failure duplicate_event_and_ineligible_event_do_not_reward_twice() noexcept {
    const DungeonRules rules;
    const DungeonRunState state = state_for_direct_ordinal_trace(111U);
    ARPG_REQUIRE(state.root_seed != 0U);
    DungeonSession session{rules, state};

    ARPG_REQUIRE(arpg::test::relay_defeated(session, 0U, 0xFFU,
        {7.0F, 3.0F, 0.0F}, true,
        arpg::combat::MonsterId::fire_bomber, 111U, 27U));
    ARPG_REQUIRE(arpg::test::relay_defeated(session, 1U, 0xFEU,
        {9.0F, 4.0F, 0.0F}, true,
        arpg::combat::MonsterId::fire_bomber, 111U, 27U));
    ARPG_REQUIRE(arpg::test::relay_defeated(session, 0U, 0U,
        {10.0F, 5.0F, 0.0F}, false,
        arpg::combat::MonsterId::fire_bomber, 112U, 27U));

    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.pending_room_experience == 74U);
    ARPG_REQUIRE(snapshot.ground_item_count == 1U);
    ARPG_REQUIRE(snapshot.ground_items[0].ordinal == 111U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"affix reward formulas", &dangerous_affix_reward_formulas_are_frozen},
    {"positive affix drop bp roll and miss",
        &positive_affix_drop_uses_bp_roll_and_can_miss},
    {"affix rewards use event fields", &defeat_reward_uses_only_stable_event_fields},
    {"affix rewards relay once", &duplicate_event_and_ineligible_event_do_not_reward_twice},
};

}  // namespace

arpg::test::TestSuite dungeon_affix_reward_suite() noexcept {
    return arpg::test::make_suite("dungeon_affix_reward", kCases);
}
