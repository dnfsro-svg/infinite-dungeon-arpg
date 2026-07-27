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
#include <cstdio>
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
    std::uint16_t spawn_ordinal{};
    std::uint32_t position_x{};
    std::uint32_t position_y{};
    std::uint32_t position_z{};
    std::array<MonsterAffixId, 3> affix_ids{};
    std::array<MonsterAffixTier, 3> affix_tiers{};
    std::uint8_t affix_count{};
    std::uint16_t danger_score{};
    bool defeat_seen{};
    arpg::combat::CombatEventKind defeat_kind{};
    std::uint64_t defeat_tick{};
    arpg::combat::AttackId defeat_attack{arpg::combat::AttackId::none};
    arpg::combat::MonsterOrdinal defeat_target_ordinal{
        arpg::combat::kInvalidMonsterOrdinal};
    arpg::combat::FeedbackLevel defeat_feedback{};
    std::uint32_t defeat_position_x{};
    std::uint32_t defeat_position_y{};
    std::uint32_t defeat_position_z{};
    MonsterId defeat_monster_id{MonsterId::count};
    std::uint16_t defeat_spawn_ordinal{};
    std::uint16_t defeat_affix_score{};
    bool defeat_reward_eligible{};

    friend bool operator==(const MonsterTrace& left,
        const MonsterTrace& right) noexcept {
        return left.id == right.id
            && left.spawn_ordinal == right.spawn_ordinal
            && left.position_x == right.position_x
            && left.position_y == right.position_y
            && left.position_z == right.position_z
            && left.affix_ids == right.affix_ids
            && left.affix_tiers == right.affix_tiers
            && left.affix_count == right.affix_count
            && left.danger_score == right.danger_score
            && left.defeat_seen == right.defeat_seen
            && left.defeat_kind == right.defeat_kind
            && left.defeat_tick == right.defeat_tick
            && left.defeat_attack == right.defeat_attack
            && left.defeat_target_ordinal == right.defeat_target_ordinal
            && left.defeat_feedback == right.defeat_feedback
            && left.defeat_position_x == right.defeat_position_x
            && left.defeat_position_y == right.defeat_position_y
            && left.defeat_position_z == right.defeat_position_z
            && left.defeat_monster_id == right.defeat_monster_id
            && left.defeat_spawn_ordinal == right.defeat_spawn_ordinal
            && left.defeat_affix_score == right.defeat_affix_score
            && left.defeat_reward_eligible == right.defeat_reward_eligible;
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
    auto restored = decoded.state;
    if (restored.abyss.lifecycle
            == arpg::abyss::AbyssLifecycle::started) {
        ++restored.commit_generation;
        restored.current_room.is_abyss = false;
        restored.abyss.lifecycle = arpg::abyss::AbyssLifecycle::failed;
    }
    session = std::make_unique<DungeonSession>(rules, restored);
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
            target.spawn_ordinal = source.spawn_ordinal;
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

bool record_defeat_payload(RoomTrace& trace,
    const arpg::combat::CombatEvent& event) noexcept {
    if (event.kind != arpg::combat::CombatEventKind::defeated) return true;
    for (std::uint16_t index = 0U; index < trace.monster_count; ++index) {
        MonsterTrace& monster = trace.monsters[index];
        if (monster.spawn_ordinal != event.spawn_ordinal) continue;
        if (monster.defeat_seen || monster.id != event.monster_id
                || monster.danger_score != event.affix_score) {
            return false;
        }
        monster.defeat_seen = true;
        monster.defeat_kind = event.kind;
        monster.defeat_tick = event.tick;
        monster.defeat_attack = event.attack;
        monster.defeat_target_ordinal = event.target_ordinal;
        monster.defeat_feedback = event.feedback;
        monster.defeat_position_x = float_bits(event.position.x);
        monster.defeat_position_y = float_bits(event.position.y);
        monster.defeat_position_z = float_bits(event.position.z);
        monster.defeat_monster_id = event.monster_id;
        monster.defeat_spawn_ordinal = event.spawn_ordinal;
        monster.defeat_affix_score = event.affix_score;
        monster.defeat_reward_eligible = event.reward_eligible;
        return true;
    }
    return false;
}

bool all_defeat_payloads_recorded(const RoomTrace& trace) noexcept {
    for (std::uint16_t index = 0U; index < trace.monster_count; ++index) {
        const MonsterTrace& monster = trace.monsters[index];
        if (!monster.defeat_seen
                || monster.defeat_kind != arpg::combat::CombatEventKind::defeated
                || monster.defeat_monster_id != monster.id
                || monster.defeat_spawn_ordinal != monster.spawn_ordinal
                || monster.defeat_affix_score != monster.danger_score
                || !monster.defeat_reward_eligible) {
            return false;
        }
    }
    return true;
}

bool defeat_all_generated_monsters(DungeonSession& session,
    RoomTrace& trace) noexcept {
    constexpr int kMaximumTicks = 4096;
    for (int tick = 0; tick < kMaximumTicks; ++tick) {
        const auto snapshot = session.snapshot();
        if (snapshot.phase == RoomPhase::cleared
                || snapshot.phase == RoomPhase::awaiting_exit) {
            drain_events(session);
            const bool recorded = all_defeat_payloads_recorded(trace);
            if (!recorded && arpg::test::trace_enabled()) {
                std::fprintf(stderr, "[TRACE] defeated payload mismatch room monsters=%u phase=%u tick=%d\n",
                    static_cast<unsigned>(trace.monster_count),
                    static_cast<unsigned>(snapshot.phase), tick);
            }
            return recorded;
        }
        if (snapshot.phase == RoomPhase::faulted
                || snapshot.phase == RoomPhase::committing) {
            if (arpg::test::trace_enabled()) {
                std::fprintf(stderr, "[TRACE] combat trace stopped phase=%u tick=%d\n",
                    static_cast<unsigned>(snapshot.phase), tick);
            }
            return false;
        }
        if (snapshot.phase == RoomPhase::combat
                && !arpg::test::defeat_next_live_monster(session)) {
            if (arpg::test::trace_enabled()) {
                std::fprintf(stderr, "[TRACE] no live monster phase=%u tick=%d\n",
                    static_cast<unsigned>(snapshot.phase), tick);
            }
            return false;
        }
        session.tick({});
        if (session.pending_save_view() != nullptr && !commit_pending(session)) {
            return false;
        }
        while (const auto event = session.try_pop_combat_event()) {
            if (!record_defeat_payload(trace, *event)) {
                if (arpg::test::trace_enabled()) {
                    std::fprintf(stderr, "[TRACE] rejected combat event kind=%u ordinal=%u score=%u tick=%d\n",
                        static_cast<unsigned>(event->kind),
                        static_cast<unsigned>(event->spawn_ordinal),
                        static_cast<unsigned>(event->affix_score), tick);
                }
                return false;
            }
        }
        while (session.try_pop_event().has_value()) {}
    }
    return false;
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
    const ExitDirection direction = kDirections[room % kDirections.size()];
    if (direction == ExitDirection::none) return false;
    const arpg::combat::Vec3 door_position =
        arpg::test::exit_boundary_position(direction);
    for (int attempt = 0; attempt < 16; ++attempt) {
        if (session.snapshot().phase == RoomPhase::committing) {
            if (!commit_pending(session)) return false;
            if (session.snapshot().phase == RoomPhase::transitioning) break;
            continue;
        }
        arpg::test::set_phase(session, RoomPhase::awaiting_exit);
        arpg::test::set_player_position(session, door_position);
        arpg::test::attempt_exit(session, direction);
        if (session.snapshot().phase == RoomPhase::committing) continue;
        if (session.snapshot().abyss_exit_confirmation_armed) {
            session.tick({});
            if (session.snapshot().phase != RoomPhase::committing
                    && session.snapshot().abyss_exit_confirmation_armed) {
                arpg::test::attempt_exit(session, direction);
            }
        }
    }
    if (session.snapshot().phase != RoomPhase::transitioning) return false;
    session.tick({});
    if (session.snapshot().phase == RoomPhase::committing
            && !commit_pending(session)) return false;
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
                || !defeat_all_generated_monsters(*session, trace)) return result;
        cumulative_experience += session->snapshot().last_room_experience;
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
        arpg::combat::kEncounterSpawnCapacity);
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
    ARPG_REQUIRE(initial.monster_count
        == arpg::combat::kEncounterSpawnCapacity);
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
