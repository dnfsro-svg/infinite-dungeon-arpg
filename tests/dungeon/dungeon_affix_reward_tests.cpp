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

bool affix_drop_hits(const DungeonRunState& state,
    std::uint16_t ordinal, std::uint16_t score) noexcept {
    auto ordinal_stream = arpg::core::DeterministicRng::derive_stream(
        state.current_room.seed, ordinal);
    auto chance = arpg::core::DeterministicRng::derive_stream(
        ordinal_stream.next_u64(), kDropChanceDomain);
    return chance.next_bounded(10000U).value()
        < arpg::dungeon::affix_drop_chance_bp(score);
}

bool legacy_drop_hits(const DungeonRunState& state,
    std::uint16_t ordinal) noexcept {
    auto ordinal_stream = arpg::core::DeterministicRng::derive_stream(
        state.current_room.seed, ordinal);
    auto chance = arpg::core::DeterministicRng::derive_stream(
        ordinal_stream.next_u64(), kDropChanceDomain);
    return chance.next_bounded(100U).value() == 0U;
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
    {"affix rewards use event fields", &defeat_reward_uses_only_stable_event_fields},
    {"affix rewards relay once", &duplicate_event_and_ineligible_event_do_not_reward_twice},
};

}  // namespace

arpg::test::TestSuite dungeon_affix_reward_suite() noexcept {
    return arpg::test::make_suite("dungeon_affix_reward", kCases);
}
