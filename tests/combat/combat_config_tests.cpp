#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"

namespace {

arpg::test::Failure configured_facing_survives_reset() noexcept {
    using namespace arpg::combat;
    CombatLabConfig config;
    config.initial_facing = Facing::left;
    CombatWorld world{config};
    ARPG_REQUIRE(world.snapshot().player.facing == Facing::left);
    world.tick(MovementInput{1, 0});
    world.reset();
    ARPG_REQUIRE(world.snapshot().player.facing == Facing::left);
    return {};
}

arpg::test::Failure disabled_respawn_stays_defeated() noexcept {
    using namespace arpg::combat;
    using arpg::test::finish_attack;
    CombatLabConfig config;
    config.dummy_spawns = {{{2.13F, 0.0F, 0.0F},
                            {6.0F, 2.0F, 0.0F},
                            {7.0F, 2.0F, 0.0F}}};
    config.respawn_defeated_dummies = false;
    CombatWorld world{config};
    for (int hit = 0; hit < 11; ++hit) {
        ARPG_REQUIRE(world.queue_action(Action::light));
        world.tick(MovementInput{});
        ARPG_REQUIRE(finish_attack(world, 80));
    }
    for (int tick = 0; tick < 120; ++tick) {
        world.tick(MovementInput{});
    }
    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.monsters[0].hp == 0);
    ARPG_REQUIRE(snapshot.monsters[0].reaction == ReactionState::defeated);
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::respawned);
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"configured facing survives reset", &configured_facing_survives_reset},
    {"disabled respawn stays defeated", &disabled_respawn_stays_defeated},
};

}  // namespace

arpg::test::TestSuite combat_config_suite() noexcept {
    return arpg::test::make_suite("combat_config", kCases);
}
