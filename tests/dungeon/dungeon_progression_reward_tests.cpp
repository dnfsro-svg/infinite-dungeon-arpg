#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

namespace {

using namespace arpg::dungeon;

arpg::test::Failure reset_discards_unsettled_room_experience() noexcept {
    DungeonSession session;
    session.tick({});
    arpg::test::EventSummary summary{};
    arpg::test::force_defeat_current_wave(session);
    session.tick({});
    arpg::test::drain_all_events(session, summary);
    const DungeonSnapshot before_reset = session.snapshot();
    ARPG_REQUIRE(before_reset.pending_room_experience > 0U
        || before_reset.last_room_experience > 0U);

    session.reset_current_room();
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

constexpr arpg::test::TestCase kCases[] = {
    {"reset discards pending XP", &reset_discards_unsettled_room_experience},
    {"clear settles XP once", &clear_settles_once_and_transition_carries_progression},
};

}  // namespace

arpg::test::TestSuite dungeon_progression_reward_suite() noexcept {
    return arpg::test::make_suite("dungeon_progression_reward", kCases);
}
