#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat_test_support.hpp"
#include "dungeon_test_support.hpp"

#include "combat/combat_world.hpp"
#include "combat/monster_affix_generation.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "persistence/checkpoint_codec.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace {

using arpg::combat::CombatEncounterConfig;
using arpg::combat::CombatWorld;
using arpg::combat::MonsterAffixId;
using arpg::combat::MonsterAffixSet;
using arpg::combat::MonsterAffixTier;
using arpg::combat::MonsterHandle;
using arpg::combat::MonsterId;
using arpg::combat::MonsterSpawnSpec;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::RoomPhase;
using arpg::dungeon::SaveDisposition;

constexpr std::uint64_t kRootSeed = 0x9A0FF10900000001ULL;
constexpr std::size_t kTraceRoomCapacity = 1000U;
constexpr std::size_t kTraceMonsterCapacity =
    arpg::combat::kEncounterWaveCapacity
    * arpg::combat::kEncounterSpawnCapacity;
constexpr std::uint16_t kNoClaimedDrop = 0xFFFFU;

struct AllocationProbe final {
    void begin() noexcept { before_ = arpg::test::allocation_count(); }
    [[nodiscard]] std::size_t end() const noexcept {
        return static_cast<std::size_t>(
            arpg::test::allocation_count() - before_);
    }

private:
    std::uint64_t before_{};
};

struct MonsterTrace final {
    MonsterId id{MonsterId::count};
    std::uint32_t position_x{};
    std::uint32_t position_y{};
    std::uint32_t position_z{};
    std::array<MonsterAffixId, 3> affix_ids{};
    std::array<MonsterAffixTier, 3> affix_tiers{};
    std::uint8_t affix_count{};
    std::uint16_t danger_score{};

    friend bool operator==(const MonsterTrace& left,
        const MonsterTrace& right) noexcept {
        return left.id == right.id
            && left.position_x == right.position_x
            && left.position_y == right.position_y
            && left.position_z == right.position_z
            && left.affix_ids == right.affix_ids
            && left.affix_tiers == right.affix_tiers
            && left.affix_count == right.affix_count
            && left.danger_score == right.danger_score;
    }
};

struct DropTrace final {
    std::uint16_t ordinal{};
    std::uint64_t item_id{};
    std::uint8_t item_level{};
    arpg::items::ItemRarity rarity{arpg::items::ItemRarity::normal};

    friend bool operator==(const DropTrace& left,
        const DropTrace& right) noexcept {
        return left.ordinal == right.ordinal
            && left.item_id == right.item_id
            && left.item_level == right.item_level
            && left.rarity == right.rarity;
    }
};

struct RoomTrace final {
    std::uint64_t room_seed{};
    std::array<MonsterTrace, kTraceMonsterCapacity> monsters{};
    std::uint16_t monster_count{};
    std::uint64_t cumulative_experience{};
    std::array<DropTrace, arpg::dungeon::kGroundDropCapacity> drops{};
    std::uint16_t drop_count{};
    std::array<std::uint64_t, 3> claimed_drop_bits{};
    std::uint16_t claimed_slot{kNoClaimedDrop};
    std::uint64_t item_summary_hash{};
    std::uint32_t item_count{};

    friend bool operator==(const RoomTrace& left,
        const RoomTrace& right) noexcept {
        return left.room_seed == right.room_seed
            && left.monster_count == right.monster_count
            && left.monsters == right.monsters
            && left.cumulative_experience == right.cumulative_experience
            && left.drop_count == right.drop_count
            && left.drops == right.drops
            && left.claimed_drop_bits == right.claimed_drop_bits
            && left.claimed_slot == right.claimed_slot
            && left.item_summary_hash == right.item_summary_hash
            && left.item_count == right.item_count;
    }
};

struct Stage9Trace final {
    std::unique_ptr<std::array<RoomTrace, kTraceRoomCapacity>> rooms{
        std::make_unique<std::array<RoomTrace, kTraceRoomCapacity>>()};
    std::uint16_t room_count{};
    bool completed{};

    Stage9Trace() = default;
    Stage9Trace(Stage9Trace&&) noexcept = default;
    Stage9Trace& operator=(Stage9Trace&&) noexcept = default;
    Stage9Trace(const Stage9Trace&) = delete;
    Stage9Trace& operator=(const Stage9Trace&) = delete;

    friend bool operator==(const Stage9Trace& left,
        const Stage9Trace& right) noexcept {
        return left.room_count == right.room_count
            && left.completed == right.completed
            && *left.rooms == *right.rooms;
    }
};

std::uint32_t float_bits(float value) noexcept {
    std::uint32_t result{};
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

std::uint64_t fold(std::uint64_t hash, std::uint64_t value) noexcept {
    return (hash ^ value) * 1099511628211ULL;
}

void drain_events(DungeonSession& session) noexcept {
    while (session.try_pop_event().has_value()) {}
    while (session.try_pop_combat_event().has_value()) {}
}

bool commit_pending(DungeonSession& session) noexcept {
    const auto* const pending = session.pending_save_view();
    if (pending == nullptr) return false;
    session.resolve_pending_save({SaveDisposition::committed,
        pending->expected_generation, pending->next_state});
    return session.snapshot().phase != RoomPhase::faulted;
}

bool reload_v4(std::unique_ptr<DungeonSession>& session,
    const DungeonRules& rules) noexcept {
    const auto encoded = arpg::persistence::encode_checkpoint(
        arpg::test::stable_state(*session));
    if (!encoded.has_value()) return false;
    const auto decoded = arpg::persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    if (decoded.error != arpg::persistence::CodecError::none) return false;
    session = std::make_unique<DungeonSession>(rules, decoded.state);
    return session->snapshot().phase == RoomPhase::locked;
}

void record_item_summary(RoomTrace& trace,
    const DungeonSession& session) noexcept {
    trace.item_summary_hash = 1469598103934665603ULL;
    const auto& items = session.item_state().items;
    trace.item_count = static_cast<std::uint32_t>(items.size());
    for (const auto& item : items) {
        trace.item_summary_hash = fold(trace.item_summary_hash, item.id);
        trace.item_summary_hash = fold(trace.item_summary_hash, item.item_level);
        trace.item_summary_hash = fold(trace.item_summary_hash,
            static_cast<std::uint8_t>(item.rarity));
    }
}

bool record_room_plan(RoomTrace& trace, const DungeonSession& session) noexcept {
    trace.room_seed = arpg::test::stable_state(session).current_room.seed;
    const auto& plan = arpg::test::encounter_plan(session);
    for (std::uint8_t wave = 0U; wave < plan.wave_count; ++wave) {
        const auto& entries = plan.waves[wave];
        for (std::uint8_t index = 0U; index < entries.spawn_count; ++index) {
            if (trace.monster_count >= trace.monsters.size()) return false;
            const MonsterSpawnSpec& source = entries.spawns[index];
            MonsterTrace& target = trace.monsters[trace.monster_count++];
            target.id = source.id;
            target.position_x = float_bits(source.position.x);
            target.position_y = float_bits(source.position.y);
            target.position_z = float_bits(source.position.z);
            target.affix_count = source.affixes.count;
            target.danger_score = arpg::combat::monster_affix_danger_score(
                source.affixes);
            for (std::uint8_t affix = 0U; affix < source.affixes.count; ++affix) {
                target.affix_ids[affix] = source.affixes.values[affix].id;
                target.affix_tiers[affix] = source.affixes.values[affix].tier;
            }
        }
    }
    return true;
}

bool relay_all_defeats(DungeonSession& session) noexcept {
    const auto& plan = arpg::test::encounter_plan(session);
    for (std::uint8_t wave = 0U; wave < plan.wave_count; ++wave) {
        const auto& entries = plan.waves[wave];
        for (std::uint8_t index = 0U; index < entries.spawn_count; ++index) {
            const MonsterSpawnSpec& spawn = entries.spawns[index];
            const std::uint16_t ordinal = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(wave) * arpg::combat::kEncounterSpawnCapacity
                + index);
            if (!arpg::test::relay_defeated(session, wave, index, spawn.position,
                    true, spawn.id, ordinal,
                    arpg::combat::monster_affix_danger_score(spawn.affixes))) {
                return false;
            }
            drain_events(session);
        }
    }
    return true;
}

bool record_drops_and_claim(RoomTrace& trace, DungeonSession& session,
    std::size_t room) noexcept {
    const auto& ground = arpg::test::ground_items(session);
    std::uint16_t claimed = kNoClaimedDrop;
    for (const auto& drop : ground) {
        if (!drop.active) continue;
        if (trace.drop_count >= trace.drops.size()) return false;
        trace.drops[trace.drop_count++] = {drop.drop_ordinal, drop.item.id,
            drop.item.item_level, drop.item.rarity};
        if (claimed == kNoClaimedDrop && room % 3U == 0U) {
            claimed = drop.drop_ordinal;
        }
    }
    if (claimed != kNoClaimedDrop) {
        const auto& item = arpg::test::ground_items(session)[claimed];
        arpg::test::set_player_position(session, item.position);
        session.request_nearby_pickups(item.position);
        if (!commit_pending(session)) return false;
        trace.claimed_slot = claimed;
    }
    trace.claimed_drop_bits = session.item_state().claimed_drop_bits;
    record_item_summary(trace, session);
    return true;
}

bool transition_room(DungeonSession& session, std::size_t room) noexcept {
    constexpr std::array<ExitDirection, 4> kDirections{{
        ExitDirection::up, ExitDirection::right,
        ExitDirection::down, ExitDirection::left,
    }};
    arpg::test::set_phase(session, RoomPhase::awaiting_exit);
    arpg::test::attempt_exit(session, kDirections[room % kDirections.size()]);
    if (!commit_pending(session)) return false;
    session.tick({});
    drain_events(session);
    return session.snapshot().phase == RoomPhase::locked;
}

Stage9Trace run_stage9_trace(std::unique_ptr<DungeonSession>& session,
    const DungeonRules& rules, std::size_t room_count,
    std::size_t restart_interval) noexcept {
    Stage9Trace result{};
    std::uint64_t cumulative_experience{};
    if (room_count > result.rooms->size() || restart_interval == 0U) return result;
    for (std::size_t room = 0U; room < room_count; ++room) {
        RoomTrace& trace = (*result.rooms)[room];
        if (!record_room_plan(trace, *session)
                || !relay_all_defeats(*session)) return result;
        cumulative_experience += session->snapshot().pending_room_experience;
        trace.cumulative_experience = cumulative_experience;
        if (!record_drops_and_claim(trace, *session, room)
                || !transition_room(*session, room)) return result;
        if ((room + 1U) % restart_interval == 0U
                && !reload_v4(session, rules)) return result;
        result.room_count = static_cast<std::uint16_t>(room + 1U);
    }
    result.completed = true;
    return result;
}

MonsterAffixSet high_risk_affixes() noexcept {
    MonsterAffixSet result{};
    result.values = {{
        {MonsterAffixId::multishot, MonsterAffixTier::m3},
        {MonsterAffixId::burning_ground, MonsterAffixTier::m3},
        {MonsterAffixId::chain_lightning, MonsterAffixTier::m3},
    }};
    result.count = 3U;
    return result;
}

CombatWorld make_high_risk_world() noexcept {
    CombatEncounterConfig config{};
    config.wave.spawn_count = static_cast<std::uint8_t>(
        arpg::combat::kMonsterCapacity);
    for (std::size_t index = 0U; index < config.wave.spawn_count; ++index) {
        config.wave.spawns[index] = {MonsterId::lightning_shooter,
            {static_cast<float>(index + 3U), 0.0F, 0.0F},
            high_risk_affixes(), static_cast<std::uint16_t>(index)};
    }
    return CombatWorld{config};
}

arpg::test::Failure one_thousand_room_affix_trace_is_deterministic() noexcept {
    const DungeonRules rules{};
    const auto initial = arpg::dungeon::make_initial_run_state(kRootSeed, rules);
    ARPG_REQUIRE(initial.fault == arpg::dungeon::DungeonFault::none);
    auto left = std::make_unique<DungeonSession>(rules, initial.state);
    auto right = std::make_unique<DungeonSession>(rules, initial.state);

    const Stage9Trace left_trace = run_stage9_trace(left, rules, 1000U, 37U);
    const Stage9Trace right_trace = run_stage9_trace(right, rules, 1000U, 37U);

    ARPG_REQUIRE(left_trace.completed);
    ARPG_REQUIRE(left_trace.room_count == 1000U);
    ARPG_REQUIRE(left_trace == right_trace);
    return {};
}

arpg::test::Failure saturated_high_risk_affix_world_allocates_nothing() noexcept {
    CombatWorld world = make_high_risk_world();
    const auto initial = world.snapshot();
    ARPG_REQUIRE(initial.monster_count == arpg::combat::kMonsterCapacity);
    const MonsterHandle owner{0U, initial.monsters[0].generation};
    arpg::test::CombatWorldTestAccess::fill_projectiles(world, owner);
    arpg::test::CombatWorldTestAccess::fill_hazards(world, owner);
    ARPG_REQUIRE(world.snapshot().projectile_count == arpg::combat::kProjectileCapacity);
    ARPG_REQUIRE(world.snapshot().hazard_count == arpg::combat::kHazardCapacity);

    AllocationProbe probe;
    probe.begin();
    for (int tick = 0; tick < 600; ++tick) {
        world.tick(arpg::combat::MovementInput{});
    }
    const std::size_t allocations = probe.end();
    ARPG_REQUIRE(allocations == 0U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"1000 room affix trace is deterministic",
        &one_thousand_room_affix_trace_is_deterministic},
    {"saturated high risk affix world allocates nothing",
        &saturated_high_risk_affix_world_allocates_nothing},
};

}  // namespace

arpg::test::TestSuite dungeon_affix_stress_suite() noexcept {
    return arpg::test::make_suite("dungeon_affix_stress", kCases);
}
