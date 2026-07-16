#include "test_framework.hpp"

#include "abyss/abyss_rules.hpp"
#include "dungeon/dungeon_progression.hpp"

#include <array>
#include <cstdint>
#include <limits>

namespace {

namespace dungeon = arpg::dungeon;
namespace checkpoint = arpg::dungeon::checkpoint;
using dungeon::DungeonFault;
using dungeon::DungeonRules;
using checkpoint::DungeonElement;
using checkpoint::EntrySide;
using checkpoint::ExitDirection;
using checkpoint::TransitionKind;

bool same_room(
    const checkpoint::RoomDescriptor& lhs,
    const checkpoint::RoomDescriptor& rhs) noexcept {
    return lhs.index == rhs.index
        && lhs.seed == rhs.seed
        && lhs.depth == rhs.depth
        && lhs.floor_room_index == rhs.floor_room_index
        && lhs.entry == rhs.entry
        && lhs.ecology == rhs.ecology
        && lhs.has_hole == rhs.has_hole
        && lhs.is_abyss == rhs.is_abyss;
}

arpg::test::Failure initial_state_is_one_commit_and_three_samples() noexcept {
    const auto result = dungeon::make_initial_run_state(
        0x0123456789ABCDEFULL, DungeonRules{});
    ARPG_REQUIRE(result.fault == DungeonFault::none);
    ARPG_REQUIRE(result.state.root_seed == 0x0123456789ABCDEFULL);
    ARPG_REQUIRE(result.state.commit_generation == 1U);
    ARPG_REQUIRE((result.state.biases == std::array<std::uint32_t, 4>{}));
    ARPG_REQUIRE(result.state.current_room.index == 0U);
    ARPG_REQUIRE(result.state.current_room.seed == 0xCA5A07A71C3153C4ULL);
    ARPG_REQUIRE(result.state.current_room.depth == 1U);
    ARPG_REQUIRE(result.state.current_room.floor_room_index == 1U);
    ARPG_REQUIRE(result.state.current_room.entry == EntrySide::initial);
    ARPG_REQUIRE(result.state.last_transition == TransitionKind::none);
    ARPG_REQUIRE(result.state.last_direction == ExitDirection::none);
    ARPG_REQUIRE(result.samples.ecology == 96U);
    ARPG_REQUIRE(result.samples.hole == 6033U);
    ARPG_REQUIRE(result.samples.abyss == 2160U);
    return {};
}

arpg::test::Failure each_door_advances_counters_and_bias() noexcept {
    const auto initial = dungeon::make_initial_run_state(7U, DungeonRules{});
    ARPG_REQUIRE(initial.fault == DungeonFault::none);
    constexpr std::array<ExitDirection, 4> directions{{
        ExitDirection::up,
        ExitDirection::down,
        ExitDirection::left,
        ExitDirection::right,
    }};
    for (std::size_t index = 0; index < directions.size(); ++index) {
        const auto next = dungeon::make_door_transition(
            initial.state, directions[index], DungeonRules{});
        ARPG_REQUIRE(next.fault == DungeonFault::none);
        ARPG_REQUIRE(next.state.commit_generation == 2U);
        ARPG_REQUIRE(next.state.current_room.index == 1U);
        ARPG_REQUIRE(next.state.current_room.depth == 1U);
        ARPG_REQUIRE(next.state.current_room.floor_room_index == 2U);
        ARPG_REQUIRE(next.state.biases[index] == 1U);
        ARPG_REQUIRE(next.state.last_transition == TransitionKind::door);
        ARPG_REQUIRE(next.state.last_direction == directions[index]);
    }
    return {};
}

arpg::test::Failure door_bias_is_applied_before_room_generation() noexcept {
    const auto initial = dungeon::make_initial_run_state(
        0x0123456789ABCDEFULL, DungeonRules{});
    const auto next = dungeon::make_door_transition(
        initial.state, ExitDirection::up, DungeonRules{});
    const std::array<std::uint32_t, 4> expected_biases{{1U, 0U, 0U, 0U}};
    const auto expected = dungeon::generate_room_descriptor(
        dungeon::derive_door_room_seed(
            initial.state.current_room.seed, 1U, ExitDirection::up),
        1U,
        1U,
        2U,
        EntrySide::bottom,
        expected_biases,
        DungeonRules{});
    ARPG_REQUIRE(next.fault == DungeonFault::none);
    ARPG_REQUIRE(expected.fault == DungeonFault::none);
    ARPG_REQUIRE(next.state.current_room.ecology == expected.room.ecology);
    ARPG_REQUIRE(next.samples.ecology == expected.samples.ecology);
    ARPG_REQUIRE(next.samples.hole == expected.samples.hole);
    ARPG_REQUIRE(next.samples.abyss == expected.samples.abyss);
    return {};
}

arpg::test::Failure descent_resets_floor_bias_and_entry() noexcept {
    const auto initial = dungeon::make_initial_run_state(7U, DungeonRules{});
    const auto door = dungeon::make_door_transition(
        initial.state, ExitDirection::right, DungeonRules{});
    const auto descent = dungeon::make_descent_transition(
        door.state, DungeonRules{});
    ARPG_REQUIRE(descent.fault == DungeonFault::none);
    ARPG_REQUIRE(descent.state.commit_generation == 3U);
    ARPG_REQUIRE(descent.state.current_room.index == 2U);
    ARPG_REQUIRE(descent.state.current_room.depth == 2U);
    ARPG_REQUIRE(descent.state.current_room.floor_room_index == 1U);
    ARPG_REQUIRE((descent.state.biases == std::array<std::uint32_t, 4>{}));
    ARPG_REQUIRE(descent.state.current_room.entry == EntrySide::initial);
    ARPG_REQUIRE(descent.state.last_transition == TransitionKind::descent);
    ARPG_REQUIRE(descent.state.last_direction == ExitDirection::none);
    return {};
}

arpg::test::Failure checked_limits_preserve_input_state() noexcept {
    const auto initial = dungeon::make_initial_run_state(7U, DungeonRules{});
    ARPG_REQUIRE(initial.fault == DungeonFault::none);

    auto generation = initial.state;
    generation.commit_generation = (std::numeric_limits<std::uint64_t>::max)();
    auto result = dungeon::make_door_transition(
        generation, ExitDirection::up, DungeonRules{});
    ARPG_REQUIRE(result.fault == DungeonFault::commit_generation_overflow);
    ARPG_REQUIRE(dungeon::same_run_state(result.state, generation));

    auto index = initial.state;
    index.current_room.index = (std::numeric_limits<std::uint64_t>::max)();
    result = dungeon::make_door_transition(index, ExitDirection::up, DungeonRules{});
    ARPG_REQUIRE(result.fault == DungeonFault::room_index_overflow);
    ARPG_REQUIRE(dungeon::same_run_state(result.state, index));

    auto depth = initial.state;
    depth.current_room.depth = (std::numeric_limits<std::uint64_t>::max)();
    result = dungeon::make_descent_transition(depth, DungeonRules{});
    ARPG_REQUIRE(result.fault == DungeonFault::depth_overflow);
    ARPG_REQUIRE(dungeon::same_run_state(result.state, depth));

    auto floor = initial.state;
    floor.current_room.floor_room_index = (std::numeric_limits<std::uint64_t>::max)();
    result = dungeon::make_door_transition(floor, ExitDirection::up, DungeonRules{});
    ARPG_REQUIRE(result.fault == DungeonFault::floor_room_overflow);
    ARPG_REQUIRE(dungeon::same_run_state(result.state, floor));

    auto bias = initial.state;
    bias.biases[0] = (std::numeric_limits<std::uint32_t>::max)();
    result = dungeon::make_door_transition(bias, ExitDirection::up, DungeonRules{});
    ARPG_REQUIRE(result.fault == DungeonFault::bias_overflow);
    ARPG_REQUIRE(dungeon::same_run_state(result.state, bias));
    return {};
}

arpg::test::Failure same_root_and_route_are_replayable() noexcept {
    const auto first = dungeon::make_initial_run_state(7U, DungeonRules{});
    const auto second = dungeon::make_initial_run_state(7U, DungeonRules{});
    const auto first_door = dungeon::make_door_transition(
        first.state, ExitDirection::left, DungeonRules{});
    const auto second_door = dungeon::make_door_transition(
        second.state, ExitDirection::left, DungeonRules{});
    ARPG_REQUIRE(dungeon::same_run_state(first.state, second.state));
    ARPG_REQUIRE(dungeon::same_run_state(first_door.state, second_door.state));
    ARPG_REQUIRE(first_door.samples.ecology == second_door.samples.ecology);
    ARPG_REQUIRE(first_door.samples.hole == second_door.samples.hole);
    ARPG_REQUIRE(first_door.samples.abyss == second_door.samples.abyss);
    return {};
}

arpg::test::Failure uncalled_door_domains_do_not_change_selected_samples() noexcept {
    const auto initial = dungeon::make_initial_run_state(7U, DungeonRules{});
    const auto selected = dungeon::make_door_transition(
        initial.state, ExitDirection::down, DungeonRules{});
    const auto direct = dungeon::generate_room_descriptor(
        dungeon::derive_door_room_seed(
            initial.state.current_room.seed, 1U, ExitDirection::down),
        1U,
        1U,
        2U,
        EntrySide::top,
        std::array<std::uint32_t, 4>{{0U, 1U, 0U, 0U}},
        DungeonRules{});
    ARPG_REQUIRE(selected.fault == DungeonFault::none);
    ARPG_REQUIRE(direct.fault == DungeonFault::none);
    ARPG_REQUIRE(selected.samples.ecology == direct.samples.ecology);
    ARPG_REQUIRE(selected.samples.hole == direct.samples.hole);
    ARPG_REQUIRE(selected.samples.abyss == direct.samples.abyss);
    return {};
}

arpg::test::Failure ordinary_door_builder_clears_previous_abyss_checkpoint() noexcept {
    auto current = dungeon::make_initial_run_state(7U, DungeonRules{}).state;
    current.current_room.index = 0U;
    current.current_room.seed = 0U;
    current.abyss.lifecycle = arpg::abyss::AbyssLifecycle::cleared;
    current.abyss.danger = arpg::abyss::AbyssDanger::high;
    current.abyss.rule = arpg::abyss::AbyssRuleId::life_sacrifice;
    current.abyss.rules_version = arpg::abyss::kAbyssRulesVersion;
    current.abyss.reward_total = 3U;
    current.abyss.generated_mask = 7U;
    current.abyss.claimed_mask = 3U;
    current.abyss.abandoned_mask = 4U;
    current.abyss.reward_revision = 9U;

    const auto next = dungeon::make_door_transition(
        current, ExitDirection::right, DungeonRules{});
    ARPG_REQUIRE(next.fault == DungeonFault::none);
    ARPG_REQUIRE(!next.state.current_room.is_abyss);
    ARPG_REQUIRE(next.state.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::none);
    ARPG_REQUIRE(next.state.abyss.rule == arpg::abyss::AbyssRuleId::none);
    ARPG_REQUIRE(next.state.abyss.rules_version == 0U);
    ARPG_REQUIRE(next.state.abyss.reward_total == 0U);
    ARPG_REQUIRE(next.state.abyss.generated_mask == 0U);
    ARPG_REQUIRE(next.state.abyss.claimed_mask == 0U);
    ARPG_REQUIRE(next.state.abyss.abandoned_mask == 0U);
    ARPG_REQUIRE(next.state.abyss.reward_revision == 0U);
    return {};
}

arpg::test::Failure abyss_door_builder_produces_complete_selection() noexcept {
    auto current = dungeon::make_initial_run_state(7U, DungeonRules{}).state;
    current.current_room.index = 0U;
    current.current_room.seed = 0x150U;
    current.current_room.depth = 27U;
    current.abyss.lifecycle = arpg::abyss::AbyssLifecycle::failed;
    current.abyss.rule = arpg::abyss::AbyssRuleId::abyss_fury;
    const auto preview = dungeon::preview_abyss_doors(current.current_room);
    ExitDirection selected = ExitDirection::none;
    for (std::size_t index = 0U; index < preview.size(); ++index) {
        if (preview[index]) selected = static_cast<ExitDirection>(index);
    }
    ARPG_REQUIRE(selected != ExitDirection::none);

    const auto next = dungeon::make_door_transition(
        current, selected, DungeonRules{});
    ARPG_REQUIRE(next.fault == DungeonFault::none);
    ARPG_REQUIRE(next.state.current_room.is_abyss);
    const auto selection = arpg::abyss::select_abyss_rule(
        next.state.current_room.seed, next.state.current_room.depth);
    ARPG_REQUIRE(selection.has_value());
    ARPG_REQUIRE(next.state.abyss.lifecycle
        == arpg::abyss::AbyssLifecycle::available);
    ARPG_REQUIRE(next.state.abyss.danger == selection->danger);
    ARPG_REQUIRE(next.state.abyss.rule == selection->rule);
    ARPG_REQUIRE(next.state.abyss.rules_version == selection->rules_version);
    ARPG_REQUIRE(next.state.abyss.reward_total == 0U);
    ARPG_REQUIRE(next.state.abyss.generated_mask == 0U);
    ARPG_REQUIRE(next.state.abyss.claimed_mask == 0U);
    ARPG_REQUIRE(next.state.abyss.abandoned_mask == 0U);
    ARPG_REQUIRE(next.state.abyss.reward_revision == 0U);
    return {};
}

arpg::test::Failure same_run_state_compares_all_abyss_fields() noexcept {
    using State = checkpoint::DungeonRunState;
    using Mutation = void (*)(State&) noexcept;
    struct Case final {
        const char* name;
        Mutation mutate;
    };
    State original = dungeon::make_initial_run_state(7U, DungeonRules{}).state;
    original.current_room.is_abyss = true;
    original.abyss.lifecycle = arpg::abyss::AbyssLifecycle::started;
    original.abyss.danger = arpg::abyss::AbyssDanger::medium;
    original.abyss.rule = arpg::abyss::AbyssRuleId::abyss_bulwark;
    original.abyss.rules_version = arpg::abyss::kAbyssRulesVersion;
    original.abyss.reward_total = 2U;
    original.abyss.generated_mask = 1U;
    original.abyss.claimed_mask = 1U;
    original.abyss.abandoned_mask = 2U;
    original.abyss.reward_revision = 4U;
    original.last_abyss_resolution = {
        true, 9U, arpg::abyss::AbyssRuleId::thunderstorm, 3U, 2U, 1U, 1U};
    constexpr std::array<Case, 16U> cases{{
        {"lifecycle", [](State& s) noexcept {
            s.abyss.lifecycle = arpg::abyss::AbyssLifecycle::cleared;
        }},
        {"danger", [](State& s) noexcept {
            s.abyss.danger = arpg::abyss::AbyssDanger::high;
        }},
        {"rule", [](State& s) noexcept {
            s.abyss.rule = arpg::abyss::AbyssRuleId::exhausted_recovery;
        }},
        {"rules version", [](State& s) noexcept { ++s.abyss.rules_version; }},
        {"reward total", [](State& s) noexcept {
            s.abyss.reward_total = 3U;
        }},
        {"generated mask", [](State& s) noexcept {
            s.abyss.generated_mask = 3U;
        }},
        {"claimed mask", [](State& s) noexcept {
            s.abyss.claimed_mask = 0U;
        }},
        {"abandoned mask", [](State& s) noexcept {
            s.abyss.abandoned_mask = 0U;
        }},
        {"reward revision", [](State& s) noexcept { ++s.abyss.reward_revision; }},
        {"resolution valid", [](State& s) noexcept {
            s.last_abyss_resolution.valid = false;
        }},
        {"resolution seed", [](State& s) noexcept {
            ++s.last_abyss_resolution.room_seed;
        }},
        {"resolution rule", [](State& s) noexcept {
            s.last_abyss_resolution.rule =
                arpg::abyss::AbyssRuleId::swift_pursuit;
        }},
        {"resolution total", [](State& s) noexcept {
            s.last_abyss_resolution.total = 2U;
        }},
        {"resolution generated", [](State& s) noexcept {
            s.last_abyss_resolution.generated = 3U;
        }},
        {"resolution claimed", [](State& s) noexcept {
            s.last_abyss_resolution.claimed = 0U;
        }},
        {"resolution abandoned", [](State& s) noexcept {
            s.last_abyss_resolution.abandoned = 0U;
        }},
    }};
    ARPG_REQUIRE(dungeon::same_run_state(original, original));
    for (const Case& entry : cases) {
        static_cast<void>(entry.name);
        State changed = original;
        entry.mutate(changed);
        ARPG_REQUIRE(!dungeon::same_run_state(original, changed));
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"initial state is one commit and three samples", &initial_state_is_one_commit_and_three_samples},
    {"each door advances counters and bias", &each_door_advances_counters_and_bias},
    {"door bias is applied before room generation", &door_bias_is_applied_before_room_generation},
    {"descent resets floor bias and entry", &descent_resets_floor_bias_and_entry},
    {"checked limits preserve input state", &checked_limits_preserve_input_state},
    {"same root and route are replayable", &same_root_and_route_are_replayable},
    {"uncalled door domains do not change selected samples", &uncalled_door_domains_do_not_change_selected_samples},
    {"ordinary door builder clears previous abyss checkpoint", &ordinary_door_builder_clears_previous_abyss_checkpoint},
    {"abyss door builder produces complete selection", &abyss_door_builder_produces_complete_selection},
    {"same run state compares all abyss fields", &same_run_state_compares_all_abyss_fields},
};

}  // namespace

arpg::test::TestSuite dungeon_progression_suite() noexcept {
    return arpg::test::make_suite("dungeon_progression", kCases);
}
