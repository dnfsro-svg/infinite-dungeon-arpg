#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"

namespace {

arpg::dungeon::DungeonSession make_session() noexcept {
    using namespace arpg::dungeon;
    const RunStateBuildResult built = make_initial_run_state(
        0x53544147453138ULL, DungeonRules{});
    return DungeonSession{DungeonRules{}, built.state};
}

arpg::test::Failure phase_query_tracks_room_entry() noexcept {
    using namespace arpg::dungeon;
    DungeonSession session = make_session();
    ARPG_REQUIRE(session.phase() == RoomPhase::locked);
    session.tick({});
    ARPG_REQUIRE(session.phase() == RoomPhase::combat);
    return {};
}

arpg::test::Failure phase_query_reports_fault() noexcept {
    using namespace arpg::dungeon;
    DungeonSession session = make_session();
    arpg::test::DungeonSessionTestAccess::force_fault(
        session, DungeonFault::invalid_rules);
    ARPG_REQUIRE(session.phase() == RoomPhase::faulted);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"phase tracks room entry", &phase_query_tracks_room_entry},
    {"phase reports fault", &phase_query_reports_fault},
};

}  // namespace

arpg::test::TestSuite dungeon_query_suite() noexcept {
    return arpg::test::make_suite("dungeon_query", kCases);
}
