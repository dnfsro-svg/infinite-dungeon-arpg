#include "test_framework.hpp"

#include "combat_test_support.hpp"

#include "combat/combat_world.hpp"
#include "combat/monster_ai_common.hpp"

#include "abyss/abyss_rules.hpp"

#include <limits>

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

arpg::test::Failure encounter_config_exposes_neutral_abyss_rules() noexcept {
    using namespace arpg::combat;
    const CombatEncounterConfig config{};
    ARPG_REQUIRE(config.abyss.rule == arpg::abyss::AbyssRuleId::none);
    ARPG_REQUIRE(config.abyss.monster_move_bp == 10000U);
    ARPG_REQUIRE(config.abyss.player_max_health_bp == 10000U);
    return {};
}

arpg::test::Failure basis_point_scaling_has_locked_integer_edges() noexcept {
    using namespace arpg::combat;
    ARPG_REQUIRE(scale_basis_points(1, 5500U) == 0);
    ARPG_REQUIRE(scale_basis_points(
        1, 5500U, BasisPointRounding::ceil) == 1);
    ARPG_REQUIRE(scale_basis_points(
        101, 7000U, BasisPointRounding::ceil) == 71);
    ARPG_REQUIRE(scale_ticks_ratio(0U, 8500U) == 0U);
    ARPG_REQUIRE(scale_ticks_ratio(1U, 1U) == 1U);
    ARPG_REQUIRE(scale_ticks_ratio(42U, 8500U) == 36U);
    ARPG_REQUIRE(scale_ticks_ratio(12U, 10000U, 14500U) == 9U);
    ARPG_REQUIRE(scale_ticks_ratio(
        (std::numeric_limits<std::uint16_t>::max)(), 65535U)
        == (std::numeric_limits<std::uint16_t>::max)());
    ARPG_REQUIRE(scale_basis_points(
        (std::numeric_limits<int>::max)(), 65535U)
        == (std::numeric_limits<int>::max)());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"configured facing survives reset", &configured_facing_survives_reset},
    {"disabled respawn stays defeated", &disabled_respawn_stays_defeated},
    {"encounter exposes neutral abyss config",
     &encounter_config_exposes_neutral_abyss_rules},
    {"basis point integer edges", &basis_point_scaling_has_locked_integer_edges},
};

}  // namespace

arpg::test::TestSuite combat_config_suite() noexcept {
    return arpg::test::make_suite("combat_config", kCases);
}
