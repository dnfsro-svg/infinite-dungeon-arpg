#include "test_framework.hpp"

#include "combat/combat_collision.hpp"
#include "combat/combat_world.hpp"

#include <array>
#include <cstddef>

namespace {

using namespace arpg::combat;

void tick_n(CombatWorld& world, int count) noexcept {
    for (int tick = 0; tick < count; ++tick) {
        world.tick(MovementInput{});
    }
}

arpg::test::Failure inclusive_xyz_mirror_and_depth_are_deterministic() noexcept {
    constexpr Aabb local{{0.20F, -0.65F, 0.10F}, {1.50F, 0.65F, 1.50F}};
    const Aabb right = make_world_aabb(
        local, Vec3{1.0F, 2.0F, 3.0F}, Facing::right);
    const Aabb left = make_world_aabb(
        local, Vec3{1.0F, 2.0F, 3.0F}, Facing::left);

    ARPG_REQUIRE(arpg::test::near(right.minimum.x, 1.20, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(right.maximum.x, 2.50, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(left.minimum.x, -0.50, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(left.maximum.x, 0.80, 1.0e-4));

    const Aabb boundary{
        {right.maximum.x, right.minimum.y, right.maximum.z},
        {right.maximum.x, right.minimum.y, right.maximum.z}};
    ARPG_REQUIRE(overlaps_inclusive(right, boundary));
    constexpr float epsilon = 1.0e-4F;
    Aabb miss = boundary;
    miss.minimum.x += epsilon;
    miss.maximum.x += epsilon;
    ARPG_REQUIRE(!overlaps_inclusive(right, miss));
    miss = boundary;
    miss.minimum.y -= epsilon;
    miss.maximum.y -= epsilon;
    ARPG_REQUIRE(!overlaps_inclusive(right, miss));
    miss = boundary;
    miss.minimum.z += epsilon;
    miss.maximum.z += epsilon;
    ARPG_REQUIRE(!overlaps_inclusive(right, miss));

    const Aabb depth_touch = make_dummy_hurtbox(
        DummyKind::normal, Vec3{2.0F, right.maximum.y + 0.40F, 3.0F});
    ARPG_REQUIRE(overlaps_inclusive(right, depth_touch));
    const Aabb depth_miss = make_dummy_hurtbox(
        DummyKind::normal,
        Vec3{2.0F, right.maximum.y + 0.40F + epsilon, 3.0F});
    ARPG_REQUIRE(!overlaps_inclusive(right, depth_miss));

    const Vec3 invalid_position{4.0F, 5.0F, 6.0F};
    const Aabb invalid = make_dummy_hurtbox(
        static_cast<DummyKind>(0xFF), invalid_position);
    ARPG_REQUIRE(arpg::test::near(invalid.minimum.x, invalid_position.x));
    ARPG_REQUIRE(arpg::test::near(invalid.minimum.y, invalid_position.y));
    ARPG_REQUIRE(arpg::test::near(invalid.minimum.z, invalid_position.z));
    ARPG_REQUIRE(arpg::test::near(invalid.maximum.x, invalid_position.x));
    ARPG_REQUIRE(arpg::test::near(invalid.maximum.y, invalid_position.y));
    ARPG_REQUIRE(arpg::test::near(invalid.maximum.z, invalid_position.z));
    return {};
}

arpg::test::Failure attack_assist_is_single_bounded_and_x_only() noexcept {
    CombatLabConfig nearest_config;
    nearest_config.dummy_spawns = {{{2.13F, 0.20F, 0.0F},
                                    {2.31F, -0.20F, 0.0F},
                                    {6.00F, 0.0F, 0.0F}}};
    CombatWorld nearest{nearest_config};
    ARPG_REQUIRE(nearest.queue_action(Action::light));
    nearest.tick(MovementInput{});
    CombatSnapshot snapshot = nearest.snapshot();
    ARPG_REQUIRE(arpg::test::near(snapshot.player.position.x, 0.18, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(snapshot.player.position.y, 0.0, 1.0e-4));
    nearest.tick(MovementInput{});
    ARPG_REQUIRE(arpg::test::near(
        nearest.snapshot().player.position.x, 0.18, 1.0e-4));

    CombatLabConfig capped_config;
    capped_config.dummy_spawns = {{{2.30F, 0.20F, 0.0F},
                                   {6.00F, 0.0F, 0.0F},
                                   {7.00F, 0.0F, 0.0F}}};
    CombatWorld capped{capped_config};
    ARPG_REQUIRE(capped.queue_action(Action::light));
    capped.tick(MovementInput{});
    snapshot = capped.snapshot();
    ARPG_REQUIRE(arpg::test::near(snapshot.player.position.x, 0.28, 1.0e-4));
    ARPG_REQUIRE(arpg::test::near(snapshot.player.position.y, 0.0, 1.0e-4));

    CombatLabConfig defeated_config;
    defeated_config.dummy_spawns = {{{2.30F, 0.0F, 0.0F},
                                     {3.29F, 0.0F, 0.0F},
                                     {7.00F, 3.0F, 0.0F}}};
    CombatWorld defeated{defeated_config};
    for (int attack = 0; attack < 4; ++attack) {
        ARPG_REQUIRE(defeated.queue_action(Action::heavy));
        defeated.tick(MovementInput{});
        for (int tick = 0;
             tick < 80
             && defeated.snapshot().player.active_attack != AttackId::none;
             ++tick) {
            defeated.tick(MovementInput{});
        }
        ARPG_REQUIRE(
            defeated.snapshot().player.active_attack == AttackId::none);
    }
    ARPG_REQUIRE(defeated.snapshot().dummies[0].hp == 0);
    ARPG_REQUIRE(defeated.snapshot().dummies[1].hp > 0);
    ARPG_REQUIRE(defeated.queue_action(Action::light));
    defeated.tick(MovementInput{});
    ARPG_REQUIRE(arpg::test::near(
        defeated.snapshot().player.position.x, 1.24, 1.0e-4));
    return {};
}

arpg::test::Failure one_attack_hits_one_target_once_across_active_ticks() noexcept {
    CombatLabConfig config;
    config.dummy_spawns = {{{1.20F, 0.0F, 0.0F},
                            {5.00F, 2.0F, 0.0F},
                            {6.00F, -2.0F, 0.0F}}};
    CombatWorld world{config};
    ARPG_REQUIRE(world.queue_action(Action::light));
    world.tick(MovementInput{});
    tick_n(world, 5);

    CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.dummies[0].hp == 272);
    ARPG_REQUIRE(snapshot.player.hit_stop_ticks == 3);
    ARPG_REQUIRE(snapshot.dummies[0].hit_stop_ticks == 3);

    int hit_events = 0;
    int summaries = 0;
    while (const auto event = world.try_pop_event()) {
        if (event->kind == CombatEventKind::hit) {
            ++hit_events;
            ARPG_REQUIRE(event->target_index == 0);
            ARPG_REQUIRE(event->value == 28);
        } else if (event->kind == CombatEventKind::impact_summary) {
            ++summaries;
            ARPG_REQUIRE(event->hit_count == 1);
            ARPG_REQUIRE(event->feedback == FeedbackLevel::light);
        }
    }
    ARPG_REQUIRE(hit_events == 1);
    ARPG_REQUIRE(summaries == 1);

    tick_n(world, 8);
    snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.dummies[0].hp == 272);
    while (const auto event = world.try_pop_event()) {
        ARPG_REQUIRE(event->kind != CombatEventKind::hit);
        ARPG_REQUIRE(event->kind != CombatEventKind::impact_summary);
    }
    return {};
}

arpg::test::Failure three_targets_resolve_independently_with_one_summary() noexcept {
    CombatLabConfig config;
    config.dummy_spawns = {{{1.20F, 0.0F, 0.0F},
                            {1.20F, 0.0F, 0.0F},
                            {1.20F, 0.0F, 0.0F}}};
    CombatWorld world{config};
    ARPG_REQUIRE(world.queue_action(Action::heavy));
    world.tick(MovementInput{});
    tick_n(world, 14);

    const CombatSnapshot snapshot = world.snapshot();
    ARPG_REQUIRE(snapshot.dummies[0].hp == 210);
    ARPG_REQUIRE(snapshot.dummies[1].hp == 360);
    ARPG_REQUIRE(snapshot.dummies[2].hp == 610);
    ARPG_REQUIRE(snapshot.dummies[2].break_value == 80);
    ARPG_REQUIRE(snapshot.player.hit_stop_ticks == 7);
    ARPG_REQUIRE(snapshot.dummies[0].hit_stop_ticks == 7);
    ARPG_REQUIRE(snapshot.dummies[1].hit_stop_ticks == 7);
    ARPG_REQUIRE(snapshot.dummies[2].hit_stop_ticks == 7);

    std::array<std::uint8_t, kDummyCount> hit_order{{0xFF, 0xFF, 0xFF}};
    int hit_events = 0;
    int summaries = 0;
    int event_index = 0;
    while (const auto event = world.try_pop_event()) {
        if (event->kind == CombatEventKind::swing) {
            ARPG_REQUIRE(event_index == 0);
        } else if (event->kind == CombatEventKind::hit) {
            ARPG_REQUIRE(event_index == hit_events + 1);
            ARPG_REQUIRE(event->target_index < kDummyCount);
            hit_order[static_cast<std::size_t>(hit_events)] =
                event->target_index;
            ++hit_events;
        } else if (event->kind == CombatEventKind::impact_summary) {
            ARPG_REQUIRE(event_index == 4);
            ++summaries;
            ARPG_REQUIRE(event->hit_count == 3);
            ARPG_REQUIRE(event->feedback == FeedbackLevel::heavy);
        } else {
            ARPG_REQUIRE(false);
        }
        ++event_index;
    }
    ARPG_REQUIRE(hit_events == 3);
    ARPG_REQUIRE(hit_order[0] == 0);
    ARPG_REQUIRE(hit_order[1] == 1);
    ARPG_REQUIRE(hit_order[2] == 2);
    ARPG_REQUIRE(summaries == 1);
    ARPG_REQUIRE(event_index == 5);
    return {};
}

arpg::test::Failure hit_confirm_opens_j1_cancel_at_eight_not_thirteen() noexcept {
    CombatLabConfig hit_config;
    hit_config.dummy_spawns = {{{1.20F, 0.0F, 0.0F},
                                {5.00F, 2.0F, 0.0F},
                                {6.00F, -2.0F, 0.0F}}};
    CombatWorld hit{hit_config};
    ARPG_REQUIRE(hit.queue_action(Action::light));
    hit.tick(MovementInput{});
    tick_n(hit, 5);
    ARPG_REQUIRE(hit.snapshot().player.attack_elapsed_ticks == 5);
    ARPG_REQUIRE(hit.snapshot().dummies[0].hp == 272);
    tick_n(hit, 3);
    tick_n(hit, 3);
    ARPG_REQUIRE(hit.snapshot().player.attack_elapsed_ticks == 8);
    ARPG_REQUIRE(hit.queue_action(Action::light));
    hit.tick(MovementInput{});
    ARPG_REQUIRE(hit.snapshot().player.active_attack == AttackId::j2);
    ARPG_REQUIRE(hit.snapshot().player.attack_elapsed_ticks == 0);

    CombatLabConfig whiff_config;
    whiff_config.dummy_spawns = {{{5.00F, 2.0F, 0.0F},
                                  {6.00F, -2.0F, 0.0F},
                                  {7.00F, 2.0F, 0.0F}}};
    CombatWorld whiff{whiff_config};
    ARPG_REQUIRE(whiff.queue_action(Action::light));
    whiff.tick(MovementInput{});
    tick_n(whiff, 8);
    ARPG_REQUIRE(whiff.queue_action(Action::light));
    whiff.tick(MovementInput{});
    ARPG_REQUIRE(whiff.snapshot().player.active_attack == AttackId::j1);
    ARPG_REQUIRE(whiff.snapshot().player.attack_elapsed_ticks == 9);
    tick_n(whiff, 4);
    ARPG_REQUIRE(whiff.snapshot().player.attack_elapsed_ticks == 13);
    whiff.tick(MovementInput{});
    ARPG_REQUIRE(whiff.snapshot().player.active_attack == AttackId::j2);
    ARPG_REQUIRE(whiff.snapshot().player.attack_elapsed_ticks == 0);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"inclusive XYZ, facing mirror, and depth",
     &inclusive_xyz_mirror_and_depth_are_deterministic},
    {"single bounded X-only attack assist",
     &attack_assist_is_single_bounded_and_x_only},
    {"one target once per attack serial",
     &one_attack_hits_one_target_once_across_active_ticks},
    {"three targets and one impact summary",
     &three_targets_resolve_independently_with_one_summary},
    {"J1 hit-confirm cancel window",
     &hit_confirm_opens_j1_cancel_at_eight_not_thirteen},
};

}  // namespace

arpg::test::TestSuite hit_resolution_suite() noexcept {
    return {"hit_resolution", kCases, sizeof(kCases) / sizeof(kCases[0])};
}
