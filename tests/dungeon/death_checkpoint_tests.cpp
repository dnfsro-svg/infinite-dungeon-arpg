#include "test_framework.hpp"

#include "combat/combat_types.hpp"
#include "combat/monster_affix_types.hpp"
#include "core/deterministic_rng.hpp"
#include "dungeon/death_checkpoint.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/room_generation.hpp"
#include "modifiers/damage_types.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;
namespace combat = arpg::combat;
namespace dungeon = arpg::dungeon;

using checkpoint::DeathCheckpoint;
using checkpoint::DeathDamageType;
using checkpoint::DeathLifecycle;
using checkpoint::DeathSourceKind;
using checkpoint::DungeonElement;
using checkpoint::EntrySide;
using dungeon::DungeonFault;
using dungeon::DungeonRules;

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

std::uint64_t first(std::uint64_t seed, std::uint64_t domain) noexcept {
    auto stream = arpg::core::DeterministicRng::derive_stream(seed, domain);
    return stream.next_u64();
}

std::uint64_t expected_retreat_seed(
    const checkpoint::DungeonRunState& current,
    std::uint64_t next_death_sequence,
    std::uint64_t target_depth) noexcept {
    constexpr std::uint64_t kDeathRetreatDomain = 0x44454154485F5254ULL;
    std::uint64_t seed = first(current.current_room.seed, kDeathRetreatDomain);
    seed = first(seed, current.commit_generation);
    seed = first(seed, next_death_sequence);
    seed = first(seed, target_depth);
    return seed == 0U ? 0xD34D5EEDULL : seed;
}

DeathCheckpoint valid_pending() noexcept {
    DeathCheckpoint death{};
    death.lifecycle = DeathLifecycle::pending_continue;
    death.data_version = checkpoint::kDeathCheckpointDataVersion;
    death.death_depth = 3U;
    death.death_floor_room_index = 7U;
    death.death_ecology = DungeonElement::lightning;
    death.death_was_abyss = false;
    death.source_kind = DeathSourceKind::monster_affix;
    death.source_monster_id = static_cast<std::uint8_t>(
        combat::MonsterId::fire_charger);
    death.source_detail_id = static_cast<std::uint16_t>(
        combat::MonsterAffixId::burning_ground);
    death.damage_type = DeathDamageType::fire;
    death.raw_damage = 50U;
    death.barrier_loss = 10U;
    death.health_loss = 20U;
    death.final_damage = 30U;
    death.recent_damage = {{0U, 30U, 0U, 0U, 0U}};
    death.hp = 0;
    death.max_hp = 100;
    death.barrier = 0;
    death.max_barrier = 50;
    death.armor = 100;
    death.evasion = 200;
    death.armor_reduction_bp = 1000;
    death.evasion_rate_bp = 2000;
    death.damage_reduction = {{100, -200, 300, 400}};
    death.damage_reduction_cap = {{7500, 7600, 7700, 7800}};
    death.target_room = {8U, 9U, 2U, 0U,
        EntrySide::initial, DungeonElement::water, true, false};
    return death;
}

arpg::test::Failure canonical_none_is_header_only_and_zeroed() noexcept {
    constexpr DeathCheckpoint death{};
    static_assert(checkpoint::valid_death_checkpoint_structural(death));
    ARPG_REQUIRE(death.lifecycle == DeathLifecycle::none);
    ARPG_REQUIRE(death.source_kind == DeathSourceKind::unknown);
    ARPG_REQUIRE(death.source_monster_id == 0xFFU);
    ARPG_REQUIRE(death.damage_type == DeathDamageType::physical);
    ARPG_REQUIRE(death.death_ecology == DungeonElement::fire);
    ARPG_REQUIRE(death.target_room.index == 0U);
    ARPG_REQUIRE(death.target_room.seed == 0U);
    ARPG_REQUIRE(death.target_room.depth == 0U);
    ARPG_REQUIRE(death.target_room.floor_room_index == 0U);
    ARPG_REQUIRE(death.target_room.entry == EntrySide::initial);
    ARPG_REQUIRE(death.target_room.ecology == DungeonElement::fire);
    ARPG_REQUIRE(!death.target_room.has_hole);
    ARPG_REQUIRE(!death.target_room.is_abyss);
    return {};
}

arpg::test::Failure structural_validation_rejects_noncanonical_none() noexcept {
    DeathCheckpoint death{};
    death.target_room.depth = 1U;
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(death));
    death = {};
    death.data_version = 1U;
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(death));
    death = {};
    death.source_monster_id = 0U;
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(death));
    return {};
}

arpg::test::Failure structural_validation_checks_pending_payload() noexcept {
    const DeathCheckpoint valid = valid_pending();
    ARPG_REQUIRE(checkpoint::valid_death_checkpoint_structural(valid));

    DeathCheckpoint changed = valid;
    changed.lifecycle = static_cast<DeathLifecycle>(0xFFU);
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(changed));
    changed = valid;
    changed.source_kind = static_cast<DeathSourceKind>(0xFFU);
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(changed));
    changed = valid;
    changed.damage_type = static_cast<DeathDamageType>(0xFFU);
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(changed));
    changed = valid;
    changed.final_damage = 29U;
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(changed));
    changed = valid;
    changed.recent_damage[4] =
        (std::numeric_limits<std::uint64_t>::max)();
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(changed));
    changed = valid;
    changed.hp = 1;
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(changed));
    changed = valid;
    changed.barrier = 1;
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(changed));
    changed = valid;
    changed.armor_reduction_bp = 10001;
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(changed));
    changed = valid;
    changed.damage_reduction[1] = -6001;
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(changed));
    changed = valid;
    changed.target_room.entry = EntrySide::left;
    ARPG_REQUIRE(!checkpoint::valid_death_checkpoint_structural(changed));
    return {};
}

arpg::test::Failure combat_snapshot_converts_without_combat_types_leaking() noexcept {
    combat::CombatDeathSnapshot snapshot{};
    snapshot.source = {combat::PlayerDamageSourceKind::monster_affix,
        combat::MonsterId::fire_charger,
        static_cast<std::uint16_t>(combat::MonsterAffixId::burning_ground)};
    snapshot.primary_type = arpg::modifiers::DamageType::fire;
    snapshot.raw_damage = 50U;
    snapshot.barrier_loss = 10U;
    snapshot.health_loss = 20U;
    snapshot.final_damage = 30U;
    snapshot.recent_damage = {{0U, 30U, 0U, 0U, 0U}};
    snapshot.defense = {0, 100, 0, 50, 100, 200, 1000, 2000,
        {{100, -200, 300, 400}}, {{7500, 7600, 7700, 7800}}};
    const checkpoint::RoomDescriptor death_room{
        7U, 77U, 3U, 7U, EntrySide::left,
        DungeonElement::lightning, false, true};
    const checkpoint::RoomDescriptor target{
        8U, 88U, 2U, 0U, EntrySide::initial,
        DungeonElement::water, true, false};

    const DeathCheckpoint converted = dungeon::make_death_checkpoint(
        snapshot, death_room, target);
    ARPG_REQUIRE(converted.lifecycle == DeathLifecycle::pending_continue);
    ARPG_REQUIRE(converted.data_version
        == checkpoint::kDeathCheckpointDataVersion);
    ARPG_REQUIRE(converted.death_depth == death_room.depth);
    ARPG_REQUIRE(converted.death_floor_room_index
        == death_room.floor_room_index);
    ARPG_REQUIRE(converted.death_ecology == death_room.ecology);
    ARPG_REQUIRE(converted.death_was_abyss);
    ARPG_REQUIRE(converted.source_kind == DeathSourceKind::monster_affix);
    ARPG_REQUIRE(converted.source_monster_id
        == static_cast<std::uint8_t>(combat::MonsterId::fire_charger));
    ARPG_REQUIRE(converted.source_detail_id
        == static_cast<std::uint16_t>(combat::MonsterAffixId::burning_ground));
    ARPG_REQUIRE(converted.damage_type == DeathDamageType::fire);
    ARPG_REQUIRE(converted.recent_damage == snapshot.recent_damage);
    ARPG_REQUIRE(converted.hp == snapshot.defense.hp);
    ARPG_REQUIRE(converted.damage_reduction
        == snapshot.defense.damage_reduction);
    ARPG_REQUIRE(same_room(converted.target_room, target));
    return {};
}

arpg::test::Failure retreat_target_clamps_depth_and_uses_named_stream() noexcept {
    auto current = dungeon::make_initial_run_state(7U, DungeonRules{}).state;
    current.current_room.index = 41U;
    current.current_room.seed = 0x123456789ABCDEF0ULL;
    current.current_room.depth = 1U;
    current.commit_generation = 9U;
    current.death_sequence = 3U;

    const auto depth_one = dungeon::make_death_retreat_target(
        current, 4U, DungeonRules{});
    ARPG_REQUIRE(depth_one.fault == DungeonFault::none);
    ARPG_REQUIRE(depth_one.room.index == 42U);
    ARPG_REQUIRE(depth_one.room.seed == expected_retreat_seed(current, 4U, 1U));
    ARPG_REQUIRE(depth_one.room.seed != 0U);
    ARPG_REQUIRE(depth_one.room.depth == 1U);
    ARPG_REQUIRE(depth_one.room.floor_room_index == 0U);
    ARPG_REQUIRE(depth_one.room.entry == EntrySide::initial);
    ARPG_REQUIRE(!depth_one.room.is_abyss);

    current.current_room.depth = 8U;
    const auto depth_eight = dungeon::make_death_retreat_target(
        current, 4U, DungeonRules{});
    ARPG_REQUIRE(depth_eight.fault == DungeonFault::none);
    ARPG_REQUIRE(depth_eight.room.depth == 7U);
    ARPG_REQUIRE(depth_eight.room.seed
        == expected_retreat_seed(current, 4U, 7U));
    return {};
}

arpg::test::Failure retreat_target_is_replayable_and_preserves_ordinary_streams() noexcept {
    auto current = dungeon::make_initial_run_state(123U, DungeonRules{}).state;
    current.current_room.index = 5U;
    current.current_room.seed = 456U;
    current.current_room.depth = 4U;
    current.commit_generation = 7U;
    current.death_sequence = 10U;
    current.biases = {{9U, 8U, 7U, 6U}};
    const auto first_target = dungeon::make_death_retreat_target(
        current, 11U, DungeonRules{});
    const auto repeated = dungeon::make_death_retreat_target(
        current, 11U, DungeonRules{});
    ARPG_REQUIRE(first_target.fault == DungeonFault::none);
    ARPG_REQUIRE(repeated.fault == DungeonFault::none);
    ARPG_REQUIRE(same_room(first_target.room, repeated.room));

    const auto ordinary = dungeon::generate_room_descriptor(
        first_target.room.seed, 6U, 3U, 0U, EntrySide::initial,
        std::array<std::uint32_t, 4>{}, DungeonRules{});
    ARPG_REQUIRE(ordinary.fault == DungeonFault::none);
    ARPG_REQUIRE(first_target.room.ecology == ordinary.room.ecology);
    ARPG_REQUIRE(first_target.room.has_hole == ordinary.room.has_hole);
    ARPG_REQUIRE(!first_target.room.is_abyss);
    return {};
}

arpg::test::Failure each_retreat_input_changes_the_named_stream() noexcept {
    auto current = dungeon::make_initial_run_state(7U, DungeonRules{}).state;
    current.current_room.index = 10U;
    current.current_room.seed = 20U;
    current.current_room.depth = 5U;
    current.commit_generation = 30U;
    current.death_sequence = 40U;
    const auto base = dungeon::make_death_retreat_target(
        current, 41U, DungeonRules{});
    ARPG_REQUIRE(base.fault == DungeonFault::none);

    auto changed = current;
    ++changed.current_room.seed;
    ARPG_REQUIRE(dungeon::make_death_retreat_target(
        changed, 41U, DungeonRules{}).room.seed != base.room.seed);
    changed = current;
    ++changed.commit_generation;
    ARPG_REQUIRE(dungeon::make_death_retreat_target(
        changed, 41U, DungeonRules{}).room.seed != base.room.seed);
    ARPG_REQUIRE(dungeon::make_death_retreat_target(
        current, 42U, DungeonRules{}).room.seed != base.room.seed);
    changed = current;
    ++changed.current_room.depth;
    ARPG_REQUIRE(dungeon::make_death_retreat_target(
        changed, 41U, DungeonRules{}).room.seed != base.room.seed);
    return {};
}

arpg::test::Failure retreat_overflows_are_explicit_and_readable() noexcept {
    auto current = dungeon::make_initial_run_state(7U, DungeonRules{}).state;
    current.death_sequence = 5U;
    current.current_room.index =
        (std::numeric_limits<std::uint64_t>::max)();
    ARPG_REQUIRE(dungeon::make_death_retreat_target(
        current, 6U, DungeonRules{}).fault
        == DungeonFault::room_index_overflow);
    current.current_room.index = 0U;
    current.commit_generation =
        (std::numeric_limits<std::uint64_t>::max)();
    ARPG_REQUIRE(dungeon::make_death_retreat_target(
        current, 6U, DungeonRules{}).fault
        == DungeonFault::commit_generation_overflow);
    current.commit_generation = 1U;
    current.death_sequence =
        (std::numeric_limits<std::uint64_t>::max)();
    ARPG_REQUIRE(dungeon::make_death_retreat_target(
        current, (std::numeric_limits<std::uint64_t>::max)(), DungeonRules{}).fault
        == DungeonFault::death_sequence_overflow);
    ARPG_REQUIRE(dungeon::dungeon_fault_name(
        DungeonFault::death_sequence_overflow)
        == std::string_view{"death_sequence_overflow"});
    return {};
}

arpg::test::Failure dungeon_semantics_validate_catalogs_and_regeneration() noexcept {
    auto current = dungeon::make_initial_run_state(7U, DungeonRules{}).state;
    current.current_room.index = 10U;
    current.current_room.seed = 20U;
    current.current_room.depth = 5U;
    current.current_room.floor_room_index = 3U;
    current.commit_generation = 30U;
    current.death_sequence = 40U;
    const auto target = dungeon::make_death_retreat_target(
        current, 41U, DungeonRules{});
    ARPG_REQUIRE(target.fault == DungeonFault::none);

    combat::CombatDeathSnapshot snapshot{};
    snapshot.source = {combat::PlayerDamageSourceKind::monster_affix,
        combat::MonsterId::fire_charger,
        static_cast<std::uint16_t>(combat::MonsterAffixId::burning_ground)};
    snapshot.primary_type = arpg::modifiers::DamageType::fire;
    snapshot.raw_damage = 50U;
    snapshot.health_loss = 30U;
    snapshot.final_damage = 30U;
    snapshot.recent_damage = {{0U, 30U, 0U, 0U, 0U}};
    snapshot.defense.max_hp = 100;
    snapshot.defense.max_barrier = 50;
    snapshot.defense.damage_reduction_cap = {{7500, 7500, 7500, 7500}};
    DeathCheckpoint death = dungeon::make_death_checkpoint(
        snapshot, current.current_room, target.room);
    ARPG_REQUIRE(dungeon::valid_death_checkpoint_dungeon(
        death, current.current_room, current.commit_generation,
        41U, DungeonRules{}));

    death.source_detail_id = 0xFFFFU;
    ARPG_REQUIRE(!dungeon::valid_death_checkpoint_dungeon(
        death, current.current_room, current.commit_generation,
        41U, DungeonRules{}));
    death = dungeon::make_death_checkpoint(
        snapshot, current.current_room, target.room);
    ++death.target_room.seed;
    ARPG_REQUIRE(!dungeon::valid_death_checkpoint_dungeon(
        death, current.current_room, current.commit_generation,
        41U, DungeonRules{}));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"canonical none is header only and zeroed", &canonical_none_is_header_only_and_zeroed},
    {"structural rejects noncanonical none", &structural_validation_rejects_noncanonical_none},
    {"structural checks pending payload", &structural_validation_checks_pending_payload},
    {"combat snapshot converts to stable checkpoint", &combat_snapshot_converts_without_combat_types_leaking},
    {"retreat clamps depth and uses named stream", &retreat_target_clamps_depth_and_uses_named_stream},
    {"retreat replay and ordinary streams", &retreat_target_is_replayable_and_preserves_ordinary_streams},
    {"each retreat input changes stream", &each_retreat_input_changes_the_named_stream},
    {"retreat overflow faults are explicit", &retreat_overflows_are_explicit_and_readable},
    {"dungeon semantics validate catalogs and target", &dungeon_semantics_validate_catalogs_and_regeneration},
};

}  // namespace

arpg::test::TestSuite death_checkpoint_suite() noexcept {
    return arpg::test::make_suite("death_checkpoint", kCases);
}
