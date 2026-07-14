#include "test_framework.hpp"

#include "dungeon/room_combat_template.hpp"
#include "dungeon/room_generation.hpp"

#include <array>
#include <cstdint>

namespace {

bool same_position(
    const arpg::combat::Vec3& actual,
    float x,
    float y,
    float z = 0.0F) noexcept {
    return actual.x == x && actual.y == y && actual.z == z;
}

arpg::test::Failure golden_seed_chain_is_stable() noexcept {
    using namespace arpg::dungeon;
    constexpr std::uint64_t root = 0x0123456789ABCDEFULL;
    const auto initial = derive_initial_room_seed(root, 0U);
    ARPG_REQUIRE(initial == 0xCA5A07A71C3153C4ULL);
    const auto up = derive_door_room_seed(
        initial, 1U, ExitDirection::up);
    ARPG_REQUIRE(up == 0xCF92F9DC3E32DA47ULL);
    const auto right = derive_door_room_seed(
        up, 2U, ExitDirection::right);
    ARPG_REQUIRE(right == 0xF71A3E545FA8D5CCULL);
    ARPG_REQUIRE(derive_descent_room_seed(right, 3U)
        == 0x21DD351FA20839E8ULL);
    ARPG_REQUIRE(derive_next_room_seed(initial, 1U, ExitDirection::up)
        == up);
    return {};
}

arpg::test::Failure ecology_and_three_stream_samples_are_fixed() noexcept {
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    DungeonRules rules;
    const auto result = generate_room_descriptor(
        0xCF92F9DC3E32DA47ULL,
        2U,
        1U,
        2U,
        EntrySide::initial,
        std::array<std::uint32_t, 4>{{1U, 0U, 0U, 0U}},
        rules);
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    ARPG_REQUIRE(result.samples.ecology == 83U);
    ARPG_REQUIRE(result.samples.hole == 7295U);
    ARPG_REQUIRE(result.samples.abyss == 8629U);
    ARPG_REQUIRE(result.room.ecology == DungeonElement::fire);
    return {};
}

arpg::test::Failure hole_threshold_boundary_is_left_closed() noexcept {
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    DungeonRules rules;
    const auto included = generate_room_descriptor(
        0xAA3ULL, 0U, 1U, 1U, EntrySide::initial, {}, rules);
    const auto excluded = generate_room_descriptor(
        0x2D80ULL, 0U, 1U, 1U, EntrySide::initial, {}, rules);
    ARPG_REQUIRE(included.samples.hole == 999U);
    ARPG_REQUIRE(included.room.has_hole);
    ARPG_REQUIRE(excluded.samples.hole == 1000U);
    ARPG_REQUIRE(!excluded.room.has_hole);
    return {};
}

arpg::test::Failure abyss_threshold_boundary_is_left_closed() noexcept {
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    DungeonRules rules;
    const auto included = generate_room_descriptor(
        0x11E9ULL, 0U, 1U, 1U, EntrySide::initial, {}, rules);
    const auto excluded = generate_room_descriptor(
        0x38ULL, 0U, 1U, 1U, EntrySide::initial, {}, rules);
    ARPG_REQUIRE(included.samples.abyss == 99U);
    ARPG_REQUIRE(included.room.is_abyss);
    ARPG_REQUIRE(excluded.samples.abyss == 100U);
    ARPG_REQUIRE(!excluded.room.is_abyss);
    return {};
}

arpg::test::Failure hole_and_abyss_can_coexist() noexcept {
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    const auto result = generate_room_descriptor(
        0x747ULL, 0U, 1U, 1U, EntrySide::initial, {}, DungeonRules{});
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    ARPG_REQUIRE(result.samples.hole == 210U);
    ARPG_REQUIRE(result.samples.abyss == 47U);
    ARPG_REQUIRE(result.room.has_hole);
    ARPG_REQUIRE(result.room.is_abyss);
    return {};
}

arpg::test::Failure bias_only_changes_ecology_stream() noexcept {
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    const auto unbiased = generate_room_descriptor(
        0xCF92F9DC3E32DA47ULL,
        2U,
        1U,
        2U,
        EntrySide::initial,
        {},
        DungeonRules{});
    const auto biased = generate_room_descriptor(
        0xCF92F9DC3E32DA47ULL,
        2U,
        1U,
        2U,
        EntrySide::initial,
        std::array<std::uint32_t, 4>{{1U, 0U, 0U, 0U}},
        DungeonRules{});
    ARPG_REQUIRE(unbiased.fault == DungeonFault::none);
    ARPG_REQUIRE(biased.fault == DungeonFault::none);
    ARPG_REQUIRE(unbiased.samples.hole == biased.samples.hole);
    ARPG_REQUIRE(unbiased.samples.abyss == biased.samples.abyss);
    ARPG_REQUIRE(unbiased.room.has_hole == biased.room.has_hole);
    ARPG_REQUIRE(unbiased.room.is_abyss == biased.room.is_abyss);
    return {};
}

arpg::test::Failure entry_templates_preserve_stage_two_layout() noexcept {
    using namespace arpg::combat;
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    const auto initial = make_combat_lab_config(EntrySide::initial, 1U);
    const auto left = make_combat_lab_config(EntrySide::left, 1U);
    const auto right = make_combat_lab_config(EntrySide::right, 1U);
    const auto top = make_combat_lab_config(EntrySide::top, 1U);
    const auto bottom = make_combat_lab_config(EntrySide::bottom, 1U);
    ARPG_REQUIRE(initial.has_value() && left.has_value() && right.has_value()
        && top.has_value() && bottom.has_value());
    ARPG_REQUIRE(same_position(initial->player_spawn, 0.0F, 0.0F));
    ARPG_REQUIRE(initial->initial_facing == Facing::right);
    ARPG_REQUIRE(same_position(initial->dummy_spawns[0], 2.30F, -0.35F));
    ARPG_REQUIRE(same_position(initial->dummy_spawns[1], 2.80F, 0.0F));
    ARPG_REQUIRE(same_position(initial->dummy_spawns[2], 3.30F, 0.35F));
    ARPG_REQUIRE(same_position(left->player_spawn, -10.50F, 0.0F));
    ARPG_REQUIRE(left->initial_facing == Facing::right);
    ARPG_REQUIRE(same_position(left->dummy_spawns[0], 2.30F, -0.35F));
    ARPG_REQUIRE(same_position(left->dummy_spawns[1], 2.80F, 0.0F));
    ARPG_REQUIRE(same_position(left->dummy_spawns[2], 3.30F, 0.35F));
    ARPG_REQUIRE(same_position(right->player_spawn, 10.50F, 0.0F));
    ARPG_REQUIRE(right->initial_facing == Facing::left);
    ARPG_REQUIRE(same_position(right->dummy_spawns[0], -2.30F, -0.35F));
    ARPG_REQUIRE(same_position(right->dummy_spawns[1], -2.80F, 0.0F));
    ARPG_REQUIRE(same_position(right->dummy_spawns[2], -3.30F, 0.35F));
    ARPG_REQUIRE(same_position(top->player_spawn, 0.0F, -4.75F));
    ARPG_REQUIRE(same_position(top->dummy_spawns[0], 2.30F, 2.30F));
    ARPG_REQUIRE(same_position(top->dummy_spawns[1], 2.80F, 2.30F));
    ARPG_REQUIRE(same_position(top->dummy_spawns[2], 3.30F, 2.30F));
    ARPG_REQUIRE(same_position(bottom->player_spawn, 0.0F, 4.75F));
    ARPG_REQUIRE(same_position(bottom->dummy_spawns[0], 2.30F, -2.30F));
    ARPG_REQUIRE(same_position(bottom->dummy_spawns[1], 2.80F, -2.30F));
    ARPG_REQUIRE(same_position(bottom->dummy_spawns[2], 3.30F, -2.30F));
    ARPG_REQUIRE(!initial->respawn_defeated_dummies);
    ARPG_REQUIRE(!left->respawn_defeated_dummies);
    ARPG_REQUIRE(!right->respawn_defeated_dummies);
    ARPG_REQUIRE(!top->respawn_defeated_dummies);
    ARPG_REQUIRE(!bottom->respawn_defeated_dummies);
    return {};
}

arpg::test::Failure non_v1_combat_template_is_rejected() noexcept {
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    ARPG_REQUIRE(!make_combat_lab_config(EntrySide::initial, 0U).has_value());
    ARPG_REQUIRE(!make_combat_lab_config(EntrySide::initial, 2U).has_value());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"golden seed chain is stable", &golden_seed_chain_is_stable},
    {"ecology and three stream samples are fixed", &ecology_and_three_stream_samples_are_fixed},
    {"hole threshold boundary is left closed", &hole_threshold_boundary_is_left_closed},
    {"abyss threshold boundary is left closed", &abyss_threshold_boundary_is_left_closed},
    {"hole and abyss can coexist", &hole_and_abyss_can_coexist},
    {"bias only changes ecology stream", &bias_only_changes_ecology_stream},
    {"entry templates preserve stage two layout", &entry_templates_preserve_stage_two_layout},
    {"non v1 combat template is rejected", &non_v1_combat_template_is_rejected},
};

}  // namespace

arpg::test::TestSuite room_generation_suite() noexcept {
    return arpg::test::make_suite("room_generation", kCases);
}
