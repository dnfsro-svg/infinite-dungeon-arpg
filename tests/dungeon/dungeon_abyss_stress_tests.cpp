#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat_test_support.hpp"

#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "combat/combat_world.hpp"
#include "dungeon/abyss_reward.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/encounter_director.hpp"
#include "dungeon/room_generation.hpp"
#include "persistence/checkpoint_codec.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {

namespace checkpoint = arpg::dungeon::checkpoint;

constexpr std::uint64_t kRootSeed = 0xA8B550001000CAFEULL;
constexpr std::size_t kRoomCount = 1000U;
constexpr std::array<checkpoint::ExitDirection, 4> kRoute{{
    checkpoint::ExitDirection::up,
    checkpoint::ExitDirection::right,
    checkpoint::ExitDirection::down,
    checkpoint::ExitDirection::left,
}};

struct RewardTrace final {
    std::uint8_t ordinal{};
    arpg::items::ItemSlot slot{arpg::items::ItemSlot::weapon};
    arpg::items::ItemRarity rarity{arpg::items::ItemRarity::normal};
    std::uint8_t item_level{};
    std::uint64_t item_seed{};
    std::uint64_t item_id{};

    friend bool operator==(const RewardTrace& lhs,
        const RewardTrace& rhs) noexcept {
        return lhs.ordinal == rhs.ordinal && lhs.slot == rhs.slot
            && lhs.rarity == rhs.rarity
            && lhs.item_level == rhs.item_level
            && lhs.item_seed == rhs.item_seed && lhs.item_id == rhs.item_id;
    }
};

struct ResolutionTrace final {
    bool valid{};
    std::uint64_t room_seed{};
    arpg::abyss::AbyssRuleId rule{arpg::abyss::AbyssRuleId::none};
    std::uint8_t total{};
    std::uint8_t generated{};
    std::uint8_t claimed{};
    std::uint8_t abandoned{};

    friend bool operator==(const ResolutionTrace& lhs,
        const ResolutionTrace& rhs) noexcept {
        return lhs.valid == rhs.valid && lhs.room_seed == rhs.room_seed
            && lhs.rule == rhs.rule && lhs.total == rhs.total
            && lhs.generated == rhs.generated && lhs.claimed == rhs.claimed
            && lhs.abandoned == rhs.abandoned;
    }
};

struct RoomTrace final {
    std::array<bool, 4> door_preview{};
    checkpoint::RoomDescriptor descriptor{};
    arpg::abyss::AbyssLifecycle lifecycle{arpg::abyss::AbyssLifecycle::none};
    arpg::abyss::AbyssDanger danger{arpg::abyss::AbyssDanger::low};
    arpg::abyss::AbyssRuleId rule{arpg::abyss::AbyssRuleId::none};
    std::uint32_t rules_version{};
    arpg::dungeon::RoomEncounterPlan encounter{};
    std::array<RewardTrace, 3> rewards{};
    std::uint8_t reward_count{};
    ResolutionTrace resolution{};
};

struct LongTrace final {
    std::array<RoomTrace, kRoomCount> rooms{};
    std::array<std::uint32_t, 4> door_trials{};
    std::array<std::uint32_t, 4> door_hits{};
    std::uint32_t reload_count{};
    std::uint64_t hash{1469598103934665603ULL};
    bool complete{};
};

bool same_descriptor(const checkpoint::RoomDescriptor& lhs,
    const checkpoint::RoomDescriptor& rhs) noexcept {
    return lhs.index == rhs.index && lhs.seed == rhs.seed
        && lhs.depth == rhs.depth
        && lhs.floor_room_index == rhs.floor_room_index
        && lhs.entry == rhs.entry && lhs.ecology == rhs.ecology
        && lhs.has_hole == rhs.has_hole && lhs.is_abyss == rhs.is_abyss;
}

bool same_encounter(const arpg::dungeon::RoomEncounterPlan& lhs,
    const arpg::dungeon::RoomEncounterPlan& rhs) noexcept {
    if (lhs.wave_count != rhs.wave_count
            || lhs.total_budget != rhs.total_budget) return false;
    for (std::size_t wave = 0U; wave < lhs.wave_count; ++wave) {
        const auto& a = lhs.waves[wave];
        const auto& b = rhs.waves[wave];
        if (a.spawn_count != b.spawn_count
                || a.spent_budget != b.spent_budget) return false;
        for (std::size_t spawn = 0U; spawn < a.spawn_count; ++spawn) {
            const auto& x = a.spawns[spawn];
            const auto& y = b.spawns[spawn];
            if (x.id != y.id || x.position.x != y.position.x
                    || x.position.y != y.position.y
                    || x.position.z != y.position.z
                    || !(x.affixes == y.affixes)
                    || x.spawn_ordinal != y.spawn_ordinal) return false;
        }
    }
    return true;
}

bool same_room_trace(const RoomTrace& lhs, const RoomTrace& rhs) noexcept {
    return lhs.door_preview == rhs.door_preview
        && same_descriptor(lhs.descriptor, rhs.descriptor)
        && lhs.lifecycle == rhs.lifecycle && lhs.danger == rhs.danger
        && lhs.rule == rhs.rule && lhs.rules_version == rhs.rules_version
        && same_encounter(lhs.encounter, rhs.encounter)
        && lhs.rewards == rhs.rewards
        && lhs.reward_count == rhs.reward_count
        && lhs.resolution == rhs.resolution;
}

std::uint64_t fold(std::uint64_t hash, std::uint64_t value) noexcept {
    return (hash ^ value) * 1099511628211ULL;
}

std::uint32_t float_bits(float value) noexcept {
    std::uint32_t result{};
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

void fold_room(LongTrace& trace, const RoomTrace& room) noexcept {
    for (bool door : room.door_preview) trace.hash = fold(trace.hash, door);
    trace.hash = fold(trace.hash, room.descriptor.index);
    trace.hash = fold(trace.hash, room.descriptor.seed);
    trace.hash = fold(trace.hash, room.descriptor.depth);
    trace.hash = fold(trace.hash, room.descriptor.floor_room_index);
    trace.hash = fold(trace.hash, static_cast<std::uint8_t>(room.descriptor.entry));
    trace.hash = fold(trace.hash, static_cast<std::uint8_t>(room.descriptor.ecology));
    trace.hash = fold(trace.hash, room.descriptor.has_hole);
    trace.hash = fold(trace.hash, room.descriptor.is_abyss);
    trace.hash = fold(trace.hash, static_cast<std::uint8_t>(room.lifecycle));
    trace.hash = fold(trace.hash, static_cast<std::uint8_t>(room.danger));
    trace.hash = fold(trace.hash, static_cast<std::uint8_t>(room.rule));
    trace.hash = fold(trace.hash, room.rules_version);
    trace.hash = fold(trace.hash, room.encounter.wave_count);
    trace.hash = fold(trace.hash, room.encounter.total_budget);
    for (std::size_t wave = 0U; wave < room.encounter.wave_count; ++wave) {
        const auto& entries = room.encounter.waves[wave];
        trace.hash = fold(trace.hash, entries.spawn_count);
        trace.hash = fold(trace.hash, entries.spent_budget);
        for (std::size_t spawn = 0U; spawn < entries.spawn_count; ++spawn) {
            const auto& monster = entries.spawns[spawn];
            trace.hash = fold(trace.hash, static_cast<std::uint8_t>(monster.id));
            trace.hash = fold(trace.hash, monster.spawn_ordinal);
            trace.hash = fold(trace.hash, float_bits(monster.position.x));
            trace.hash = fold(trace.hash, float_bits(monster.position.y));
            trace.hash = fold(trace.hash, float_bits(monster.position.z));
            trace.hash = fold(trace.hash, monster.affixes.count);
            for (std::size_t affix = 0U; affix < monster.affixes.count; ++affix) {
                trace.hash = fold(trace.hash,
                    static_cast<std::uint8_t>(monster.affixes.values[affix].id));
                trace.hash = fold(trace.hash,
                    static_cast<std::uint8_t>(monster.affixes.values[affix].tier));
            }
        }
    }
    trace.hash = fold(trace.hash, room.reward_count);
    for (std::size_t index = 0U; index < room.reward_count; ++index) {
        const RewardTrace& reward = room.rewards[index];
        trace.hash = fold(trace.hash, reward.ordinal);
        trace.hash = fold(trace.hash, static_cast<std::uint8_t>(reward.slot));
        trace.hash = fold(trace.hash, static_cast<std::uint8_t>(reward.rarity));
        trace.hash = fold(trace.hash, reward.item_level);
        trace.hash = fold(trace.hash, reward.item_seed);
        trace.hash = fold(trace.hash, reward.item_id);
    }
    trace.hash = fold(trace.hash, room.resolution.valid);
    trace.hash = fold(trace.hash, room.resolution.room_seed);
    trace.hash = fold(trace.hash,
        static_cast<std::uint8_t>(room.resolution.rule));
    trace.hash = fold(trace.hash, room.resolution.total);
    trace.hash = fold(trace.hash, room.resolution.generated);
    trace.hash = fold(trace.hash, room.resolution.claimed);
    trace.hash = fold(trace.hash, room.resolution.abandoned);
}

bool encode_decode(checkpoint::DungeonRunState& state) noexcept {
    const auto encoded = arpg::persistence::encode_checkpoint(state);
    if (!encoded.has_value()) return false;
    const auto decoded = arpg::persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    if (decoded.error != arpg::persistence::CodecError::none
            || !arpg::dungeon::same_run_state(state, decoded.state)) {
        return false;
    }
    state = decoded.state;
    return true;
}

bool generate_long_trace(LongTrace& trace) noexcept {
    const arpg::dungeon::DungeonRules rules{};
    auto initial = arpg::dungeon::make_initial_run_state(kRootSeed, rules);
    if (initial.fault != arpg::dungeon::DungeonFault::none) return false;
    checkpoint::DungeonRunState state = initial.state;

    for (std::size_t room_index = 0U; room_index < kRoomCount; ++room_index) {
        RoomTrace& room = trace.rooms[room_index];
        room.door_preview = arpg::dungeon::preview_abyss_doors(
            state.current_room);
        for (std::size_t direction = 0U; direction < 4U; ++direction) {
            ++trace.door_trials[direction];
            if (room.door_preview[direction]) ++trace.door_hits[direction];
        }

        const checkpoint::ExitDirection direction =
            kRoute[room_index % kRoute.size()];
        const bool previewed = room.door_preview[
            static_cast<std::size_t>(direction)];
        auto next = arpg::dungeon::make_door_transition(state, direction, rules);
        if (next.fault != arpg::dungeon::DungeonFault::none
                || next.state.current_room.is_abyss != previewed) return false;
        state = next.state;
        room.descriptor = state.current_room;
        room.lifecycle = state.abyss.lifecycle;
        room.danger = state.abyss.danger;
        room.rule = state.abyss.rule;
        room.rules_version = state.abyss.rules_version;

        const auto encounter = state.current_room.is_abyss
            ? arpg::dungeon::build_abyss_encounter_plan(
                state.current_room.seed, state.current_room.depth,
                state.current_room.ecology, rules.encounter)
            : arpg::dungeon::build_encounter_plan(
                state.current_room.seed, state.current_room.depth,
                state.current_room.ecology, rules.encounter);
        if (encounter.fault != arpg::dungeon::DungeonFault::none) return false;
        room.encounter = encounter.plan;

        if (state.current_room.is_abyss) {
            const auto selected = arpg::abyss::select_abyss_rule(
                state.current_room.seed, state.current_room.depth);
            if (!selected.has_value() || selected->danger != state.abyss.danger
                    || selected->rule != state.abyss.rule
                    || selected->rules_version != state.abyss.rules_version) {
                return false;
            }
            const auto profile = arpg::abyss::reward_profile_for(
                selected->danger, 1U);
            if (profile.item_count == 0U
                    || profile.item_count > room.rewards.size()) return false;
            room.reward_count = profile.item_count;
            for (std::uint8_t ordinal = 0U;
                 ordinal < room.reward_count; ++ordinal) {
                const auto reward = arpg::dungeon::derive_abyss_reward_slot(
                    state.current_room.seed, selected->danger,
                    1U, ordinal);
                if (!reward.has_value()) return false;
                room.rewards[ordinal] = {reward->slot_index,
                    reward->item_slot, reward->rarity, reward->item_level,
                    reward->item_seed, reward->item_id};
            }
            const std::uint8_t claimed = room.reward_count > 1U ? 1U : 0U;
            room.resolution = {true, state.current_room.seed,
                selected->rule, room.reward_count, claimed, claimed,
                static_cast<std::uint8_t>(room.reward_count - claimed)};
        }
        fold_room(trace, room);

        if ((room_index + 1U) % 37U == 0U) {
            if (!encode_decode(state)) return false;
            ++trace.reload_count;
        }
    }
    trace.complete = true;
    return true;
}

bool same_long_trace(const LongTrace& lhs, const LongTrace& rhs) noexcept {
    if (lhs.door_trials != rhs.door_trials
            || lhs.door_hits != rhs.door_hits
            || lhs.reload_count != rhs.reload_count
            || lhs.hash != rhs.hash || lhs.complete != rhs.complete) return false;
    for (std::size_t room = 0U; room < kRoomCount; ++room) {
        if (!same_room_trace(lhs.rooms[room], rhs.rooms[room])) return false;
    }
    return true;
}

arpg::combat::MonsterAffixSet extreme_affixes() noexcept {
    arpg::combat::MonsterAffixSet result{};
    result.values = {{
        {arpg::combat::MonsterAffixId::multishot,
            arpg::combat::MonsterAffixTier::m3},
        {arpg::combat::MonsterAffixId::burning_ground,
            arpg::combat::MonsterAffixTier::m3},
        {arpg::combat::MonsterAffixId::chain_lightning,
            arpg::combat::MonsterAffixTier::m3},
    }};
    result.count = 3U;
    return result;
}

arpg::combat::CombatWorld make_extreme_world() noexcept {
    arpg::combat::CombatEncounterConfig config{};
    config.abyss = arpg::abyss::combat_config_for(
        arpg::abyss::AbyssRuleId::chaos_expansion);
    config.wave.spawn_count = static_cast<std::uint8_t>(
        arpg::combat::kMonsterCapacity);
    for (std::size_t index = 0U; index < config.wave.spawn_count; ++index) {
        config.wave.spawns[index] = {
            arpg::combat::MonsterId::lightning_shooter,
            {10.0F + static_cast<float>(index % 12U),
             4.0F + static_cast<float>(index / 12U), 0.0F},
            extreme_affixes(), static_cast<std::uint16_t>(index)};
    }
    return arpg::combat::CombatWorld{config};
}

std::array<arpg::dungeon::GroundItem,
    arpg::dungeon::kGroundDropCapacity> make_full_ground_pool() noexcept {
    std::array<arpg::dungeon::GroundItem,
        arpg::dungeon::kGroundDropCapacity> result{};
    for (std::size_t index = 0U; index < result.size(); ++index) {
        arpg::items::ItemInstance item{};
        item.id = 0xA800000000000000ULL + index + 1U;
        item.item_level = 1U;
        result[index] = {true, static_cast<std::uint16_t>(index),
            arpg::dungeon::GroundItemSource::monster_drop, 0xFFU,
            {100.0F + static_cast<float>(index), 100.0F, 0.0F}, item};
    }
    return result;
}

arpg::test::Failure thousand_room_abyss_trace_matches_golden_and_reload() noexcept {
    auto first = std::make_unique<LongTrace>();
    auto second = std::make_unique<LongTrace>();
    ARPG_REQUIRE(generate_long_trace(*first));
    ARPG_REQUIRE(generate_long_trace(*second));
    ARPG_REQUIRE(first->complete && first->reload_count == 27U);
    ARPG_REQUIRE(same_long_trace(*first, *second));

    const std::uint32_t total_trials = first->door_trials[0]
        + first->door_trials[1] + first->door_trials[2]
        + first->door_trials[3];
    const std::uint32_t total_hits = first->door_hits[0]
        + first->door_hits[1] + first->door_hits[2] + first->door_hits[3];
    std::printf("[stage10-abyss-stress] hits=%u/%u directions=%u/%u/%u/%u "
        "reloads=%u hash=0x%016llx\n", total_hits, total_trials,
        first->door_hits[0], first->door_hits[1], first->door_hits[2],
        first->door_hits[3], first->reload_count,
        static_cast<unsigned long long>(first->hash));

    ARPG_REQUIRE(total_trials == 4000U);
    ARPG_REQUIRE(total_hits >= 20U && total_hits <= 70U);
    constexpr std::array<std::uint32_t, 4> kGoldenTrials{{
        1000U, 1000U, 1000U, 1000U}};
    constexpr std::array<std::uint32_t, 4> kGoldenHits{{12U, 6U, 5U, 8U}};
    ARPG_REQUIRE(first->door_trials == kGoldenTrials);
    ARPG_REQUIRE(first->door_hits == kGoldenHits);
    ARPG_REQUIRE(first->hash == 0x51b4ebccf356d01bULL);
    return {};
}

arpg::test::Failure extreme_abyss_pools_run_600_ticks_without_allocation() noexcept {
    arpg::combat::CombatWorld world = make_extreme_world();
    auto ground = make_full_ground_pool();
    const auto initial = world.snapshot();
    ARPG_REQUIRE(initial.monster_count == arpg::combat::kMonsterCapacity);
    const arpg::combat::MonsterHandle owner{
        0U, initial.monsters[0].generation};
    arpg::test::CombatWorldTestAccess::fill_projectiles(world, owner);
    arpg::test::CombatWorldTestAccess::fill_hazards(world, owner);
    const auto saturated = world.snapshot();
    ARPG_REQUIRE(saturated.projectile_count
        == arpg::combat::kProjectileCapacity);
    ARPG_REQUIRE(saturated.hazard_count == arpg::combat::kHazardCapacity);
    for (std::size_t index = 0U; index < ground.size(); ++index) {
        ARPG_REQUIRE(ground[index].active);
        ARPG_REQUIRE(ground[index].drop_ordinal == index);
    }

    const std::uint32_t projectile_saturation_before =
        saturated.diagnostics.projectile_saturation_count;
    const std::uint32_t hazard_saturation_before =
        saturated.diagnostics.hazard_saturation_count;
    const std::uint64_t before = arpg::test::allocation_count();
    for (int tick = 0; tick < 600; ++tick) world.tick({});
    const std::uint64_t allocations = arpg::test::allocation_count() - before;
    const auto after = world.snapshot();

    ARPG_REQUIRE(allocations == 0U);
    ARPG_REQUIRE(after.monster_count <= arpg::combat::kMonsterCapacity);
    ARPG_REQUIRE(after.projectile_count
        == arpg::combat::kProjectileCapacity);
    ARPG_REQUIRE(after.hazard_count == arpg::combat::kHazardCapacity);
    ARPG_REQUIRE(after.diagnostics.projectile_saturation_count
        > projectile_saturation_before);
    ARPG_REQUIRE(after.diagnostics.hazard_saturation_count
        > hazard_saturation_before);
    ARPG_REQUIRE(after.diagnostics.projectile_invalid_owner_count == 0U);
    ARPG_REQUIRE(after.diagnostics.hazard_invalid_owner_count == 0U);
    ARPG_REQUIRE(after.diagnostics.event_overflow_count == 0U);
    for (std::size_t index = 0U; index < ground.size(); ++index) {
        ARPG_REQUIRE(ground[index].active);
        ARPG_REQUIRE(ground[index].item.id
            == 0xA800000000000000ULL + index + 1U);
    }
    std::printf("[stage10-abyss-stress] ticks=600 allocations=%llu "
        "pools=%zu/%zu/%zu saturation=%u/%u\n",
        static_cast<unsigned long long>(allocations), after.monster_count,
        after.projectile_count, after.hazard_count,
        after.diagnostics.projectile_saturation_count,
        after.diagnostics.hazard_saturation_count);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"1000 room abyss trace matches golden through reloads",
        &thousand_room_abyss_trace_matches_golden_and_reload},
    {"extreme abyss pools run 600 ticks without allocation",
        &extreme_abyss_pools_run_600_ticks_without_allocation},
};

}  // namespace

arpg::test::TestSuite dungeon_abyss_stress_suite() noexcept {
    return arpg::test::make_suite("dungeon_abyss_stress", kCases);
}
