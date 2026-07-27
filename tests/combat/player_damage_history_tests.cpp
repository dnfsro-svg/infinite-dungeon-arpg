#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat/player_damage_history.hpp"
#include "modifiers/damage_types.hpp"

#include <cstdint>
#include <limits>

namespace {

using namespace arpg::combat;
using namespace arpg::modifiers;

constexpr std::size_t kPhysical = damage_index(DamageType::physical);
constexpr std::size_t kFire = damage_index(DamageType::fire);
constexpr std::size_t kWater = damage_index(DamageType::water);
constexpr std::size_t kLightning = damage_index(DamageType::lightning);
constexpr std::size_t kChaos = damage_index(DamageType::chaos);

arpg::test::Failure packet_resolution_preserves_final_damage_by_type() noexcept {
    PlayerCombatBuild build{};
    build.values.armor = 100;
    build.values.damage_reduction[element_index(DamageType::fire)] = 5000;
    build.values.damage_reduction[element_index(DamageType::water)] = -6000;
    build.values.damage_reduction[element_index(DamageType::lightning)] = 7500;
    build.values.damage_reduction[element_index(DamageType::chaos)] = 9500;
    build.values.damage_reduction_cap_bonus[element_index(DamageType::chaos)] =
        2000;
    build.values.damage_taken = 20000;

    const auto resolved = resolve_player_damage_packet(
        DamagePacket{{10, 10, 10, 1, 100}}, build);
    ARPG_REQUIRE(resolved.has_value());
    ARPG_REQUIRE(resolved->by_type[kPhysical] == 16U);
    ARPG_REQUIRE(resolved->by_type[kFire] == 10U);
    ARPG_REQUIRE(resolved->by_type[kWater] == 32U);
    ARPG_REQUIRE(resolved->by_type[kLightning] == 2U);
    ARPG_REQUIRE(resolved->by_type[kChaos] == 10U);
    ARPG_REQUIRE(resolved->total == 70U);
    ARPG_REQUIRE(resolve_player_damage(
        DamagePacket{{10, 10, 10, 1, 100}}, build) == 70);
    return {};
}

arpg::test::Failure packet_resolution_handles_int_max_without_wrapping() noexcept {
    constexpr int maximum = (std::numeric_limits<int>::max)();
    const auto resolved = resolve_player_damage_packet(
        DamagePacket{{maximum, maximum, maximum, maximum, maximum}},
        PlayerCombatBuild{});
    ARPG_REQUIRE(resolved.has_value());
    for (const std::uint64_t component : resolved->by_type) {
        ARPG_REQUIRE(component == static_cast<std::uint64_t>(maximum));
    }
    ARPG_REQUIRE(resolved->total
        == static_cast<std::uint64_t>(maximum) * kDamageTypeCount);
    ARPG_REQUIRE(!resolve_player_damage(
        DamagePacket{{maximum, maximum, maximum, maximum, maximum}},
        PlayerCombatBuild{}).has_value());
    return {};
}

arpg::test::Failure damage_taken_preserves_total_and_distributes_remainder() noexcept {
    PlayerCombatBuild build{};
    build.values.damage_taken = 5000;

    const auto resolved = resolve_player_damage_packet(
        DamagePacket{{1, 1, 0, 0, 0}}, build);
    ARPG_REQUIRE(resolved.has_value());
    ARPG_REQUIRE(resolved->total == 1U);
    ARPG_REQUIRE(resolved->by_type[kPhysical] == 1U);
    ARPG_REQUIRE(resolved->by_type[kFire] == 0U);
    ARPG_REQUIRE(resolved->by_type[kWater] == 0U);
    ARPG_REQUIRE(resolved->by_type[kLightning] == 0U);
    ARPG_REQUIRE(resolved->by_type[kChaos] == 0U);
    ARPG_REQUIRE(resolve_player_damage(
        DamagePacket{{1, 1, 0, 0, 0}}, build) == 1);
    return {};
}

arpg::test::Failure history_records_caller_provided_damage() noexcept {
    PlayerDamageHistory history{};
    history.begin_tick(42U);
    ResolvedPlayerDamage actual_damage{};
    actual_damage.by_type = {{40U, 30U, 20U, 10U, 0U}};
    actual_damage.total = 100U;
    history.record(actual_damage);

    const auto totals = history.totals();
    ARPG_REQUIRE(totals == actual_damage.by_type);
    return {};
}

arpg::test::Failure same_tick_records_accumulate_by_type() noexcept {
    PlayerDamageHistory history{};
    history.begin_tick(7U);
    history.record(ResolvedPlayerDamage{{{1U, 2U, 3U, 4U, 5U}}, 15U});
    history.begin_tick(7U);
    history.record(ResolvedPlayerDamage{{{5U, 4U, 3U, 2U, 1U}}, 15U});

    const auto totals = history.totals();
    for (const std::uint64_t component : totals) {
        ARPG_REQUIRE(component == 6U);
    }
    return {};
}

arpg::test::Failure history_keeps_300_ticks_and_evicts_on_tick_301() noexcept {
    PlayerDamageHistory history{};
    history.begin_tick(1U);
    history.record(ResolvedPlayerDamage{{{9U, 0U, 0U, 0U, 0U}}, 9U});
    history.begin_tick(300U);
    ARPG_REQUIRE(history.totals()[kPhysical] == 9U);

    history.begin_tick(301U);
    ARPG_REQUIRE(history.totals()[kPhysical] == 0U);
    return {};
}

arpg::test::Failure backward_tick_resets_the_entire_history() noexcept {
    PlayerDamageHistory history{};
    history.begin_tick(50U);
    history.record(ResolvedPlayerDamage{{{3U, 0U, 0U, 0U, 0U}}, 3U});
    history.begin_tick(49U);

    ARPG_REQUIRE(history.totals()[kPhysical] == 0U);
    return {};
}

arpg::test::Failure jump_of_300_ticks_resets_the_entire_history() noexcept {
    PlayerDamageHistory history{};
    history.begin_tick(10U);
    history.record(ResolvedPlayerDamage{{{0U, 4U, 0U, 0U, 0U}}, 4U});
    history.begin_tick(310U);

    ARPG_REQUIRE(history.totals()[kFire] == 0U);
    return {};
}

arpg::test::Failure history_accumulation_saturates_instead_of_wrapping() noexcept {
    constexpr std::uint64_t maximum =
        (std::numeric_limits<std::uint64_t>::max)();
    PlayerDamageHistory history{};
    history.begin_tick(1U);
    history.record(ResolvedPlayerDamage{{{maximum, 0U, 0U, 0U, 0U}}, maximum});
    history.record(ResolvedPlayerDamage{{{1U, 0U, 0U, 0U, 0U}}, 1U});
    history.begin_tick(2U);
    history.record(ResolvedPlayerDamage{{{1U, 0U, 0U, 0U, 0U}}, 1U});

    ARPG_REQUIRE(history.totals()[kPhysical] == maximum);
    return {};
}

arpg::test::Failure history_hot_path_performs_no_heap_allocations() noexcept {
    const std::uint64_t before = arpg::test::allocation_count();
    PlayerDamageHistory history{};
    for (std::uint64_t tick = 1U; tick <= 600U; ++tick) {
        history.begin_tick(tick);
        history.record(ResolvedPlayerDamage{{{tick, 2U, 3U, 4U, 5U}},
                                             tick + 14U});
    }
    const auto totals = history.totals();
    const std::uint64_t after = arpg::test::allocation_count();

    ARPG_REQUIRE(totals[kPhysical] != 0U);
    ARPG_REQUIRE(after == before);
    return {};
}

arpg::test::Failure history_checkpoint_round_trip_is_exact() noexcept {
    PlayerDamageHistory source{};
    source.begin_tick(412U);
    source.record(ResolvedPlayerDamage{{{9U, 8U, 7U, 6U, 5U}}, 35U});
    source.begin_tick(413U);
    source.record(ResolvedPlayerDamage{{{1U, 2U, 3U, 4U, 5U}}, 15U});

    PlayerDamageHistoryCheckpoint checkpoint{};
    const std::uint64_t before = arpg::test::allocation_count();
    source.capture_checkpoint(checkpoint);
    PlayerDamageHistory restored{};
    ARPG_REQUIRE(restored.restore_checkpoint(checkpoint));
    const std::uint64_t after = arpg::test::allocation_count();

    ARPG_REQUIRE(restored.totals() == source.totals());
    ARPG_REQUIRE(restored.active_tick() == 413U);
    ARPG_REQUIRE(restored.initialized());
    ARPG_REQUIRE(after == before);
    return {};
}

arpg::test::Failure malformed_history_checkpoint_is_rejected() noexcept {
    PlayerDamageHistory history{};
    PlayerDamageHistoryCheckpoint uninitialized_with_data{};
    uninitialized_with_data.buckets[0U][kPhysical] = 1U;
    ARPG_REQUIRE(!history.restore_checkpoint(uninitialized_with_data));

    PlayerDamageHistoryCheckpoint overflow{};
    overflow.initialized = true;
    overflow.active_tick = 300U;
    overflow.buckets[0U][kPhysical] =
        (std::numeric_limits<std::uint64_t>::max)();
    overflow.buckets[1U][kPhysical] = 1U;
    ARPG_REQUIRE(!history.restore_checkpoint(overflow));
    ARPG_REQUIRE(!history.initialized());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"packet resolution preserves types", &packet_resolution_preserves_final_damage_by_type},
    {"packet resolution handles int max", &packet_resolution_handles_int_max_without_wrapping},
    {"damage taken distributes remainder", &damage_taken_preserves_total_and_distributes_remainder},
    {"history records caller provided damage", &history_records_caller_provided_damage},
    {"same tick records accumulate", &same_tick_records_accumulate_by_type},
    {"300 tick inclusive window", &history_keeps_300_ticks_and_evicts_on_tick_301},
    {"backward tick resets history", &backward_tick_resets_the_entire_history},
    {"300 tick jump resets history", &jump_of_300_ticks_resets_the_entire_history},
    {"history saturates", &history_accumulation_saturates_instead_of_wrapping},
    {"history allocates nothing", &history_hot_path_performs_no_heap_allocations},
    {"history checkpoint round trip", &history_checkpoint_round_trip_is_exact},
    {"malformed history checkpoint", &malformed_history_checkpoint_is_rejected},
};

}  // namespace

arpg::test::TestSuite player_damage_history_suite() noexcept {
    return arpg::test::make_suite("player_damage_history", kCases);
}
