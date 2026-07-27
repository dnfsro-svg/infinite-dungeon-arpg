#include "test_framework.hpp"

#include "abyss/abyss_rules.hpp"
#include "combat/room_bounds.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_combat_template.hpp"
#include "dungeon/room_generation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

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

arpg::test::Failure legacy_abyss_sample_boundary_is_diagnostic_only() noexcept {
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    DungeonRules rules;
    const auto included = generate_room_descriptor(
        0x11E9ULL, 0U, 1U, 1U, EntrySide::initial, {}, rules);
    const auto excluded = generate_room_descriptor(
        0x38ULL, 0U, 1U, 1U, EntrySide::initial, {}, rules);
    ARPG_REQUIRE(included.samples.abyss == 99U);
    ARPG_REQUIRE(!included.room.is_abyss);
    ARPG_REQUIRE(excluded.samples.abyss == 100U);
    ARPG_REQUIRE(!excluded.room.is_abyss);
    return {};
}

arpg::test::Failure hole_and_legacy_abyss_samples_can_coexist() noexcept {
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    const auto result = generate_room_descriptor(
        0x747ULL, 0U, 1U, 1U, EntrySide::initial, {}, DungeonRules{});
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    ARPG_REQUIRE(result.samples.hole == 210U);
    ARPG_REQUIRE(result.samples.abyss == 47U);
    ARPG_REQUIRE(result.room.has_hole);
    ARPG_REQUIRE(!result.room.is_abyss);
    return {};
}

arpg::test::Failure preview_uses_fixed_direction_slots_and_target_seeds() noexcept {
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    static_assert(static_cast<std::uint8_t>(ExitDirection::up) == 0U);
    static_assert(static_cast<std::uint8_t>(ExitDirection::down) == 1U);
    static_assert(static_cast<std::uint8_t>(ExitDirection::left) == 2U);
    static_assert(static_cast<std::uint8_t>(ExitDirection::right) == 3U);

    checkpoint::RoomDescriptor current{};
    current.seed = 0x150U;
    current.index = 0U;
    const auto preview = preview_abyss_doors(current);
    constexpr std::array<ExitDirection, 4> directions{{
        ExitDirection::up,
        ExitDirection::down,
        ExitDirection::left,
        ExitDirection::right,
    }};
    for (std::size_t index = 0U; index < directions.size(); ++index) {
        const std::uint64_t target_seed = derive_door_room_seed(
            current.seed, current.index + 1U, directions[index]);
        ARPG_REQUIRE(preview[index]
            == arpg::abyss::is_abyss_roll(target_seed));
    }
    return {};
}

arpg::test::Failure preview_supports_zero_through_four_abyss_doors() noexcept {
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    constexpr std::array<std::uint64_t, 5> seeds{{
        0x0U, 0x150U, 0x1C4U, 0x2A945U, 0x5456FBDU,
    }};
    for (std::size_t expected = 0U; expected < seeds.size(); ++expected) {
        checkpoint::RoomDescriptor current{};
        current.seed = seeds[expected];
        current.index = 0U;
        std::size_t actual = 0U;
        for (const bool is_abyss : preview_abyss_doors(current)) {
            if (is_abyss) ++actual;
        }
        ARPG_REQUIRE(actual == expected);
    }
    return {};
}

arpg::test::Failure overflow_preview_has_no_announced_targets() noexcept {
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    checkpoint::RoomDescriptor current{};
    current.seed = 0x5456FBDU;
    current.index = (std::numeric_limits<std::uint64_t>::max)();
    const auto preview = preview_abyss_doors(current);
    const std::array<bool, 4> none{};
    ARPG_REQUIRE(preview == none);
    return {};
}

arpg::test::Failure initial_room_rejects_a_matching_legacy_roll() noexcept {
    using namespace arpg::dungeon;
    std::uint64_t matching_root = 0U;
    bool found = false;
    for (; matching_root < 10000U; ++matching_root) {
        if (arpg::abyss::is_abyss_roll(
                derive_initial_room_seed(matching_root, 0U))) {
            found = true;
            break;
        }
    }
    ARPG_REQUIRE(found);
    const auto initial = make_initial_run_state(matching_root, DungeonRules{});
    ARPG_REQUIRE(initial.fault == DungeonFault::none);
    ARPG_REQUIRE(initial.samples.abyss < 100U);
    ARPG_REQUIRE(!initial.state.current_room.is_abyss);
    ARPG_REQUIRE(initial.state.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::none);
    ARPG_REQUIRE(initial.state.abyss.rule == arpg::abyss::AbyssRuleId::none);
    return {};
}

arpg::test::Failure ecology_bias_does_not_change_door_preview() noexcept {
    using namespace arpg::dungeon;
    using namespace arpg::dungeon::checkpoint;
    const auto unbiased = generate_room_descriptor(
        0x2A945U, 9U, 12U, 3U, EntrySide::initial, {}, DungeonRules{});
    const auto biased = generate_room_descriptor(
        0x2A945U,
        9U,
        12U,
        3U,
        EntrySide::initial,
        std::array<std::uint32_t, 4>{{19U, 23U, 29U, 31U}},
        DungeonRules{});
    ARPG_REQUIRE(unbiased.fault == DungeonFault::none);
    ARPG_REQUIRE(biased.fault == DungeonFault::none);
    ARPG_REQUIRE(preview_abyss_doors(unbiased.room)
        == preview_abyss_doors(biased.room));
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

arpg::test::Failure entry_templates_derive_spawns_from_square_bounds() noexcept {
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
    ARPG_REQUIRE(same_position(left->player_spawn,
        room_bounds::min_x + 1.50F, 0.0F));
    ARPG_REQUIRE(left->initial_facing == Facing::right);
    ARPG_REQUIRE(same_position(left->dummy_spawns[0], 2.30F, -0.35F));
    ARPG_REQUIRE(same_position(left->dummy_spawns[1], 2.80F, 0.0F));
    ARPG_REQUIRE(same_position(left->dummy_spawns[2], 3.30F, 0.35F));
    ARPG_REQUIRE(same_position(right->player_spawn,
        room_bounds::max_x - 1.50F, 0.0F));
    ARPG_REQUIRE(right->initial_facing == Facing::left);
    ARPG_REQUIRE(same_position(right->dummy_spawns[0], -2.30F, -0.35F));
    ARPG_REQUIRE(same_position(right->dummy_spawns[1], -2.80F, 0.0F));
    ARPG_REQUIRE(same_position(right->dummy_spawns[2], -3.30F, 0.35F));
    ARPG_REQUIRE(same_position(top->player_spawn, 0.0F,
        room_bounds::min_y + 0.75F));
    ARPG_REQUIRE(same_position(top->dummy_spawns[0], 2.30F, 2.30F));
    ARPG_REQUIRE(same_position(top->dummy_spawns[1], 2.80F, 2.30F));
    ARPG_REQUIRE(same_position(top->dummy_spawns[2], 3.30F, 2.30F));
    ARPG_REQUIRE(same_position(bottom->player_spawn, 0.0F,
        room_bounds::max_y - 0.75F));
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
    {"legacy abyss sample boundary is diagnostic only", &legacy_abyss_sample_boundary_is_diagnostic_only},
    {"hole and legacy abyss samples can coexist", &hole_and_legacy_abyss_samples_can_coexist},
    {"bias only changes ecology stream", &bias_only_changes_ecology_stream},
    {"entry templates derive spawns from square bounds",
     &entry_templates_derive_spawns_from_square_bounds},
    {"non v1 combat template is rejected", &non_v1_combat_template_is_rejected},
    {"preview uses fixed direction slots and target seeds", &preview_uses_fixed_direction_slots_and_target_seeds},
    {"preview supports zero through four abyss doors", &preview_supports_zero_through_four_abyss_doors},
    {"overflow preview has no announced targets", &overflow_preview_has_no_announced_targets},
    {"initial room rejects a matching legacy roll", &initial_room_rejects_a_matching_legacy_roll},
    {"ecology bias does not change door preview", &ecology_bias_does_not_change_door_preview},
};

}  // namespace

arpg::test::TestSuite room_generation_suite() noexcept {
    return arpg::test::make_suite("room_generation", kCases);
}
