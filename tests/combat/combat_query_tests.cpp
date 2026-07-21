#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"

namespace {

arpg::test::Failure player_position_query_matches_snapshot() noexcept {
    using namespace arpg::combat;
    CombatWorld world{};

    Vec3 queried = world.player_position();
    CombatSnapshot visible = world.snapshot();
    ARPG_REQUIRE(queried.x == visible.player.position.x);
    ARPG_REQUIRE(queried.y == visible.player.position.y);
    ARPG_REQUIRE(queried.z == visible.player.position.z);

    world.tick(MovementInput{1, -1});
    queried = world.player_position();
    visible = world.snapshot();
    ARPG_REQUIRE(queried.x == visible.player.position.x);
    ARPG_REQUIRE(queried.y == visible.player.position.y);
    ARPG_REQUIRE(queried.z == visible.player.position.z);
    return {};
}

arpg::test::Failure living_monster_query_excludes_defeated_targets() noexcept {
    using namespace arpg::combat;
    CombatLabConfig config{};
    config.respawn_defeated_dummies = false;
    CombatWorld world{config};

    ARPG_REQUIRE(world.living_monster_count() == 3U);
    arpg::test::CombatWorldTestAccess::defeat_monster(world, 0U, true);

    const CombatSnapshot visible = world.snapshot();
    std::size_t expected = 0U;
    for (const MonsterSnapshot& monster : visible.monsters) {
        if (monster.active && monster.hp > 0) ++expected;
    }
    ARPG_REQUIRE(expected == 2U);
    ARPG_REQUIRE(world.living_monster_count() == expected);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"player position matches snapshot", &player_position_query_matches_snapshot},
    {"living count excludes defeated", &living_monster_query_excludes_defeated_targets},
};

}  // namespace

arpg::test::TestSuite combat_query_suite() noexcept {
    return arpg::test::make_suite("combat_query", kCases);
}
