#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "core/deterministic_rng.hpp"
#include "dungeon/dungeon_progression.hpp"

#include <cstdint>

namespace {

using namespace arpg::dungeon;

constexpr std::uint64_t kDropChanceDomain = 0x44524F505F43484EULL;

DungeonRunState state_with_first_ordinal_drop() noexcept {
    DungeonRules rules;
    for (std::uint64_t root = 1U; root < 10000U; ++root) {
        const DungeonRunState state = make_initial_run_state(root, rules).state;
        auto ordinal_stream = arpg::core::DeterministicRng::derive_stream(
            state.current_room.seed, 0U);
        auto chance = arpg::core::DeterministicRng::derive_stream(
            ordinal_stream.next_u64(), kDropChanceDomain);
        if (chance.next_bounded(100U).value() == 0U) return state;
    }
    return {};
}

arpg::test::Failure reset_discards_unsettled_room_experience() noexcept {
    DungeonSession session;
    session.tick({});
    arpg::test::EventSummary summary{};
    ARPG_REQUIRE(arpg::test::defeat_next_live_monster(session));
    session.tick({});
    arpg::test::drain_all_events(session, summary);
    const DungeonSnapshot before_reset = session.snapshot();
    ARPG_REQUIRE(before_reset.pending_room_experience > 0U
        || before_reset.last_room_experience > 0U);

    static_cast<void>(session.reset_current_room());
    const DungeonSnapshot reset = session.snapshot();
    ARPG_REQUIRE(reset.pending_room_experience == 0U);
    ARPG_REQUIRE(reset.progression.level == 1U);
    ARPG_REQUIRE(reset.progression.experience == 0U);
    return {};
}

arpg::test::Failure clear_settles_once_and_transition_carries_progression() noexcept {
    DungeonSession session;
    arpg::test::EventSummary summary{};
    ARPG_REQUIRE(arpg::test::drive_until_cleared(session, summary));
    const DungeonSnapshot cleared = session.snapshot();
    ARPG_REQUIRE(cleared.pending_room_experience == 0U);
    ARPG_REQUIRE(cleared.last_room_experience > 0U);
    ARPG_REQUIRE(cleared.progression.level > 1U);
    ARPG_REQUIRE(cleared.progression.earned_passive_points
        == cleared.progression.level - 1U);

    for (int tick = 0; tick < 10; ++tick) {
        session.tick({});
    }
    const DungeonSnapshot stable = session.snapshot();
    ARPG_REQUIRE(stable.progression.level == cleared.progression.level);
    ARPG_REQUIRE(stable.progression.experience == cleared.progression.experience);

    ARPG_REQUIRE(session.request_descent(false) == false);
    return {};
}

arpg::test::Failure reward_ineligible_defeat_relays_without_player_rewards() noexcept {
    const DungeonRunState state = state_with_first_ordinal_drop();
    ARPG_REQUIRE(state.current_room.depth != 0U);
    DungeonSession session{DungeonRules{}, state};
    const DungeonSnapshot before = session.snapshot();

    ARPG_REQUIRE(arpg::test::relay_defeated(
        session, 0U, 0U, {7.0F, 3.0F, 0.0F}, false));

    const DungeonSnapshot after = session.snapshot();
    ARPG_REQUIRE(after.ground_item_count == 0U);
    ARPG_REQUIRE(after.pending_room_experience == before.pending_room_experience);
    const auto event = session.try_pop_combat_event();
    ARPG_REQUIRE(event.has_value());
    ARPG_REQUIRE(event->kind == arpg::combat::CombatEventKind::defeated);
    ARPG_REQUIRE(!event->reward_eligible);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"reset discards pending XP", &reset_discards_unsettled_room_experience},
    {"clear settles XP once", &clear_settles_once_and_transition_carries_progression},
    {"reward-ineligible defeats do not reward player", &reward_ineligible_defeat_relays_without_player_rewards},
};

}  // namespace

arpg::test::TestSuite dungeon_progression_reward_suite() noexcept {
    return arpg::test::make_suite("dungeon_progression_reward", kCases);
}
