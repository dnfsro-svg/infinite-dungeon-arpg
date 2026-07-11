#include "test_framework.hpp"

#include "dungeon/room_generation.hpp"

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

arpg::test::Failure direction_ids_and_opposite_entries_are_fixed() noexcept {
    using namespace arpg::dungeon;
    std::uint8_t up_id = static_cast<std::uint8_t>(ExitDirection::up);
    std::uint8_t down_id = static_cast<std::uint8_t>(ExitDirection::down);
    std::uint8_t left_id = static_cast<std::uint8_t>(ExitDirection::left);
    std::uint8_t right_id = static_cast<std::uint8_t>(ExitDirection::right);
    std::uint8_t none_id = static_cast<std::uint8_t>(ExitDirection::none);
    ARPG_REQUIRE(up_id == 0U);
    ARPG_REQUIRE(down_id == 1U);
    ARPG_REQUIRE(left_id == 2U);
    ARPG_REQUIRE(right_id == 3U);
    ARPG_REQUIRE(none_id == 0xFFU);
    ARPG_REQUIRE(entry_side_for_exit(ExitDirection::up) == EntrySide::bottom);
    ARPG_REQUIRE(entry_side_for_exit(ExitDirection::down) == EntrySide::top);
    ARPG_REQUIRE(entry_side_for_exit(ExitDirection::left) == EntrySide::right);
    ARPG_REQUIRE(entry_side_for_exit(ExitDirection::right) == EntrySide::left);
    return {};
}

arpg::test::Failure initial_descriptor_uses_stage_two_template() noexcept {
    using namespace arpg::combat;
    using namespace arpg::dungeon;
    DungeonSessionConfig config;
    config.root_seed = 0x123456789ABCDEF0ULL;
    config.initial_room_index = 7U;

    const RoomDescriptor room = make_initial_room(config);
    ARPG_REQUIRE(room.index == config.initial_room_index);
    ARPG_REQUIRE(room.seed == derive_initial_room_seed(
        config.root_seed, config.initial_room_index));
    ARPG_REQUIRE(room.entry == EntrySide::initial);
    ARPG_REQUIRE(same_position(room.combat.player_spawn, 0.0F, 0.0F));
    ARPG_REQUIRE(same_position(room.combat.dummy_spawns[0], 2.30F, -0.35F));
    ARPG_REQUIRE(same_position(room.combat.dummy_spawns[1], 2.80F, 0.0F));
    ARPG_REQUIRE(same_position(room.combat.dummy_spawns[2], 3.30F, 0.35F));
    ARPG_REQUIRE(room.combat.initial_facing == Facing::right);
    ARPG_REQUIRE(!room.combat.respawn_defeated_dummies);
    return {};
}

arpg::test::Failure side_entries_mirror_spawn_and_facing() noexcept {
    using namespace arpg::combat;
    using namespace arpg::dungeon;
    const RoomDescriptor current = make_initial_room({});

    const auto from_right = make_next_room(current, ExitDirection::left);
    ARPG_REQUIRE(from_right.has_value());
    ARPG_REQUIRE(from_right->index == current.index + 1U);
    ARPG_REQUIRE(from_right->seed == derive_next_room_seed(
        current.seed, current.index + 1U, ExitDirection::left));
    ARPG_REQUIRE(from_right->entry == EntrySide::right);
    ARPG_REQUIRE(same_position(from_right->combat.player_spawn, 6.50F, 0.0F));
    ARPG_REQUIRE(from_right->combat.initial_facing == Facing::left);
    ARPG_REQUIRE(same_position(from_right->combat.dummy_spawns[0], -2.30F, -0.35F));
    ARPG_REQUIRE(same_position(from_right->combat.dummy_spawns[1], -2.80F, 0.0F));
    ARPG_REQUIRE(same_position(from_right->combat.dummy_spawns[2], -3.30F, 0.35F));
    ARPG_REQUIRE(!from_right->combat.respawn_defeated_dummies);

    const auto from_left = make_next_room(current, ExitDirection::right);
    ARPG_REQUIRE(from_left.has_value());
    ARPG_REQUIRE(from_left->index == current.index + 1U);
    ARPG_REQUIRE(from_left->seed == derive_next_room_seed(
        current.seed, current.index + 1U, ExitDirection::right));
    ARPG_REQUIRE(from_left->entry == EntrySide::left);
    ARPG_REQUIRE(same_position(from_left->combat.player_spawn, -6.50F, 0.0F));
    ARPG_REQUIRE(from_left->combat.initial_facing == Facing::right);
    ARPG_REQUIRE(same_position(from_left->combat.dummy_spawns[0], 2.30F, -0.35F));
    ARPG_REQUIRE(same_position(from_left->combat.dummy_spawns[1], 2.80F, 0.0F));
    ARPG_REQUIRE(same_position(from_left->combat.dummy_spawns[2], 3.30F, 0.35F));
    ARPG_REQUIRE(!from_left->combat.respawn_defeated_dummies);
    return {};
}

arpg::test::Failure vertical_entries_use_horizontal_target_rows() noexcept {
    using namespace arpg::combat;
    using namespace arpg::dungeon;
    const RoomDescriptor current = make_initial_room({});

    const auto from_bottom = make_next_room(current, ExitDirection::up);
    ARPG_REQUIRE(from_bottom.has_value());
    ARPG_REQUIRE(from_bottom->index == current.index + 1U);
    ARPG_REQUIRE(from_bottom->seed == derive_next_room_seed(
        current.seed, current.index + 1U, ExitDirection::up));
    ARPG_REQUIRE(from_bottom->entry == EntrySide::bottom);
    ARPG_REQUIRE(same_position(from_bottom->combat.player_spawn, 0.0F, 2.75F));
    ARPG_REQUIRE(from_bottom->combat.initial_facing == Facing::right);
    ARPG_REQUIRE(same_position(from_bottom->combat.dummy_spawns[0], 2.30F, -2.30F));
    ARPG_REQUIRE(same_position(from_bottom->combat.dummy_spawns[1], 2.80F, -2.30F));
    ARPG_REQUIRE(same_position(from_bottom->combat.dummy_spawns[2], 3.30F, -2.30F));
    ARPG_REQUIRE(!from_bottom->combat.respawn_defeated_dummies);

    const auto from_top = make_next_room(current, ExitDirection::down);
    ARPG_REQUIRE(from_top.has_value());
    ARPG_REQUIRE(from_top->index == current.index + 1U);
    ARPG_REQUIRE(from_top->seed == derive_next_room_seed(
        current.seed, current.index + 1U, ExitDirection::down));
    ARPG_REQUIRE(from_top->entry == EntrySide::top);
    ARPG_REQUIRE(same_position(from_top->combat.player_spawn, 0.0F, -2.75F));
    ARPG_REQUIRE(from_top->combat.initial_facing == Facing::right);
    ARPG_REQUIRE(same_position(from_top->combat.dummy_spawns[0], 2.30F, 2.30F));
    ARPG_REQUIRE(same_position(from_top->combat.dummy_spawns[1], 2.80F, 2.30F));
    ARPG_REQUIRE(same_position(from_top->combat.dummy_spawns[2], 3.30F, 2.30F));
    ARPG_REQUIRE(!from_top->combat.respawn_defeated_dummies);
    return {};
}

arpg::test::Failure seed_derivation_is_deterministic_and_complete() noexcept {
    using namespace arpg::dungeon;
    constexpr std::uint64_t root_seed = 0x0102030405060708ULL;
    constexpr std::uint64_t high_bit = std::uint64_t{1} << 63U;

    const auto initial = derive_initial_room_seed(root_seed, 9U);
    ARPG_REQUIRE(initial == derive_initial_room_seed(root_seed, 9U));
    ARPG_REQUIRE(initial != derive_initial_room_seed(root_seed, high_bit | 9U));
    ARPG_REQUIRE(derive_next_room_seed(initial, 10U, ExitDirection::up)
        == derive_next_room_seed(initial, 10U, ExitDirection::up));
    ARPG_REQUIRE(derive_next_room_seed(initial, 10U, ExitDirection::up)
        != derive_next_room_seed(initial, 10U, ExitDirection::down));
    ARPG_REQUIRE(derive_next_room_seed(initial, 10U, ExitDirection::left)
        != derive_next_room_seed(initial, 10U, ExitDirection::right));
    ARPG_REQUIRE(derive_next_room_seed(initial, 10U, ExitDirection::up)
        != derive_next_room_seed(initial, high_bit | 10U, ExitDirection::up));

    RoomDescriptor maximum = make_initial_room({});
    maximum.index = (std::numeric_limits<std::uint64_t>::max)();
    ARPG_REQUIRE(!make_next_room(maximum, ExitDirection::right).has_value());
    ARPG_REQUIRE(!make_next_room(make_initial_room({}), ExitDirection::none).has_value());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"direction ids and opposite entries are fixed", &direction_ids_and_opposite_entries_are_fixed},
    {"initial descriptor uses stage two template", &initial_descriptor_uses_stage_two_template},
    {"side entries mirror spawn and facing", &side_entries_mirror_spawn_and_facing},
    {"vertical entries use horizontal target rows", &vertical_entries_use_horizontal_target_rows},
    {"seed derivation is deterministic and complete", &seed_derivation_is_deterministic_and_complete},
};

}  // namespace

arpg::test::TestSuite room_generation_suite() noexcept {
    return arpg::test::make_suite("room_generation", kCases);
}
