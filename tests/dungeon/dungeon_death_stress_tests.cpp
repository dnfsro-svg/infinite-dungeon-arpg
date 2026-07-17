#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "abyss/abyss_rules.hpp"
#include "core/deterministic_rng.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/room_generation.hpp"
#include "items/item_types.hpp"
#include "persistence/checkpoint_codec.hpp"

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <utility>

namespace arpg::test {

struct DungeonDeathStressFixture final {
    static void saturate_ground_pool(
        dungeon::DungeonSession& session,
        const items::ItemInstance& prototype) noexcept {
        for (std::uint16_t index = 0U;
             index < session.ground_items_.size(); ++index) {
            items::ItemInstance item = prototype;
            item.id += index;
            session.ground_items_[index] = {
                true, index, dungeon::GroundItemSource::monster_drop,
                0xFFU, {100.0F + static_cast<float>(index),
                    100.0F, 0.0F}, item};
        }
    }
};

}  // namespace arpg::test

namespace {

namespace abyss = arpg::abyss;
namespace checkpoint = arpg::dungeon::checkpoint;
namespace combat = arpg::combat;
namespace core = arpg::core;
namespace dungeon = arpg::dungeon;
namespace persistence = arpg::persistence;

constexpr std::size_t kDeathCount = 1000U;
constexpr std::uint64_t kHashOffset = 1469598103934665603ULL;
constexpr std::uint64_t kHashPrime = 1099511628211ULL;
constexpr std::uint64_t kDropChanceDomain = 0x44524F505F43484EULL;
constexpr std::uint64_t kDropSlotDomain = 0x44524F505F534C54ULL;
constexpr std::uint64_t kDropContentDomain = 0x44524F505F49544DULL;

void fold(std::uint64_t& hash, std::uint64_t value) noexcept {
    hash ^= value;
    hash *= kHashPrime;
}

void fold_room(
    std::uint64_t& hash,
    const checkpoint::RoomDescriptor& room) noexcept {
    fold(hash, room.index);
    fold(hash, room.seed);
    fold(hash, room.depth);
    fold(hash, room.floor_room_index);
    fold(hash, static_cast<std::uint8_t>(room.entry));
    fold(hash, static_cast<std::uint8_t>(room.ecology));
    fold(hash, room.has_hole);
    fold(hash, room.is_abyss);
}

std::uint64_t death_hash(
    const checkpoint::DeathCheckpoint& death) noexcept {
    std::uint64_t hash = kHashOffset;
    fold(hash, static_cast<std::uint8_t>(death.lifecycle));
    fold(hash, death.death_depth);
    fold(hash, death.death_floor_room_index);
    fold(hash, static_cast<std::uint8_t>(death.death_ecology));
    fold(hash, death.death_was_abyss);
    fold(hash, static_cast<std::uint8_t>(death.source_kind));
    fold(hash, death.source_monster_id);
    fold(hash, death.source_detail_id);
    fold(hash, static_cast<std::uint8_t>(death.damage_type));
    fold(hash, death.raw_damage);
    fold(hash, death.barrier_loss);
    fold(hash, death.health_loss);
    fold(hash, death.final_damage);
    for (const std::uint64_t value : death.recent_damage) fold(hash, value);
    fold(hash, static_cast<std::uint32_t>(death.hp));
    fold(hash, static_cast<std::uint32_t>(death.max_hp));
    fold(hash, static_cast<std::uint32_t>(death.barrier));
    fold(hash, static_cast<std::uint32_t>(death.max_barrier));
    fold(hash, static_cast<std::uint64_t>(death.armor));
    fold(hash, static_cast<std::uint64_t>(death.evasion));
    fold(hash, static_cast<std::uint32_t>(death.armor_reduction_bp));
    fold(hash, static_cast<std::uint32_t>(death.evasion_rate_bp));
    for (const std::int32_t value : death.damage_reduction) {
        fold(hash, static_cast<std::uint32_t>(value));
    }
    for (const std::int32_t value : death.damage_reduction_cap) {
        fold(hash, static_cast<std::uint32_t>(value));
    }
    fold_room(hash, death.target_room);
    return hash;
}

bool same_death(
    const checkpoint::DeathCheckpoint& left,
    const checkpoint::DeathCheckpoint& right) noexcept {
    checkpoint::DungeonRunState a{};
    checkpoint::DungeonRunState b{};
    a.death = left;
    b.death = right;
    return dungeon::same_run_state(a, b);
}

bool same_permanent_player_state(
    const checkpoint::DungeonRunState& left,
    const checkpoint::DungeonRunState& right) noexcept {
    if (left.root_seed != right.root_seed
            || left.progression.level != right.progression.level
            || left.progression.experience != right.progression.experience
            || left.progression.earned_passive_points
                != right.progression.earned_passive_points
            || left.progression.unspent_passive_points
                != right.progression.unspent_passive_points
            || left.passive_tree.allocated_bits
                != right.passive_tree.allocated_bits
            || left.item_ownership.items.size()
                != right.item_ownership.items.size()
            || left.item_ownership.equipment.equipped_ids
                != right.item_ownership.equipment.equipped_ids
            || left.item_ownership.claimed_drop_bits
                != right.item_ownership.claimed_drop_bits
            || left.item_ownership.next_item_sequence
                != right.item_ownership.next_item_sequence) {
        return false;
    }
    for (std::size_t index = 0U;
         index < left.item_ownership.items.size(); ++index) {
        const auto& a = left.item_ownership.items[index];
        const auto& b = right.item_ownership.items[index];
        if (a.id != b.id || a.base_id != b.base_id || a.rarity != b.rarity
                || a.item_level != b.item_level
                || a.required_level != b.required_level
                || a.affix_count != b.affix_count
                || a.reserved != b.reserved) {
            return false;
        }
        for (std::size_t affix = 0U; affix < a.affixes.size(); ++affix) {
            if (a.affixes[affix].affix_id != b.affixes[affix].affix_id
                    || a.affixes[affix].tier != b.affixes[affix].tier
                    || a.affixes[affix].variant
                        != b.affixes[affix].variant) {
                return false;
            }
        }
    }
    return true;
}

bool codec_round_trip(checkpoint::DungeonRunState& state) noexcept {
    const auto encoded = persistence::encode_checkpoint(state);
    if (!encoded.has_value()) return false;
    const auto decoded = persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    if (decoded.error != persistence::CodecError::none
            || decoded.migrated
            || !dungeon::same_run_state(state, decoded.state)) {
        return false;
    }
    state = decoded.state;
    return true;
}

std::uint64_t available_abyss_seed(
    std::uint64_t start,
    std::uint64_t depth) noexcept {
    for (std::uint64_t seed = start; seed != 0U; ++seed) {
        if (abyss::is_abyss_roll(seed)
                && abyss::select_abyss_rule(seed, depth).has_value()) {
            return seed;
        }
    }
    return 0U;
}

checkpoint::DungeonRunState case_root(
    std::size_t index,
    bool abyss_death) noexcept {
    const dungeon::DungeonRules rules{};
    const std::uint64_t depth = index % 3U == 0U
        ? 1U : 8U + static_cast<std::uint64_t>(index % 29U);
    auto built = dungeon::make_initial_run_state(
        0xD34D000000000000ULL + index, rules);
    checkpoint::DungeonRunState state = built.state;
    const std::uint64_t seed = abyss_death
        ? available_abyss_seed(0xA8000000ULL + index * 97U, depth)
        : (depth == 1U
            ? 0x0D1E000000000000ULL
            : 0x0D1E000000000083ULL);
    const std::uint64_t floor_room = depth == 1U
        ? 0U : 1U + static_cast<std::uint64_t>(index % 11U);
    const auto generated = dungeon::generate_room_descriptor(
        seed, 100U + index, depth, floor_room,
        checkpoint::EntrySide::left, {}, rules);
    if (generated.fault != dungeon::DungeonFault::none) return {};
    state.current_room = generated.room;
    state.current_room.is_abyss = abyss_death;
    state.last_transition = checkpoint::TransitionKind::door;
    state.last_direction = checkpoint::ExitDirection::right;
    if (abyss_death) {
        const auto selection = abyss::select_abyss_rule(seed, depth);
        if (!selection.has_value()) return {};
        state.abyss.lifecycle = abyss::AbyssLifecycle::available;
        state.abyss.danger = selection->danger;
        state.abyss.rule = selection->rule;
        state.abyss.rules_version = selection->rules_version;
    }
    return state;
}

arpg::items::ItemInstance ground_prototype() noexcept {
    arpg::items::ItemInstance item{};
    item.id = 0x100000000ULL;
    item.base_id = 0U;
    item.rarity = arpg::items::ItemRarity::normal;
    item.item_level = 1U;
    item.required_level = 1U;
    return item;
}

combat::MovementInput move_toward_nearest_monster(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    if (!snapshot.combat.has_value()) return {};
    const combat::Vec3 player = snapshot.combat->player.position;
    const combat::MonsterSnapshot* nearest = nullptr;
    float nearest_distance = 0.0F;
    for (const auto& monster : snapshot.combat->monsters) {
        if (!monster.active || monster.hp <= 0) continue;
        const float dx = monster.position.x - player.x;
        const float dy = monster.position.y - player.y;
        const float distance = dx * dx + dy * dy;
        if (nearest == nullptr || distance < nearest_distance) {
            nearest = &monster;
            nearest_distance = distance;
        }
    }
    if (nearest == nullptr) return {};
    combat::MovementInput movement{};
    const float dx = nearest->position.x - player.x;
    const float dy = nearest->position.y - player.y;
    if (dx > 0.25F) movement.x = 1;
    else if (dx < -0.25F) movement.x = -1;
    if (dy > 0.20F) movement.y = 1;
    else if (dy < -0.20F) movement.y = -1;
    return movement;
}

struct Trace final {
    std::array<checkpoint::DeathCheckpoint, kDeathCount> deaths{};
    std::array<std::uint64_t, kDeathCount> death_sequences{};
    std::uint64_t cumulative_hash{kHashOffset};
    std::uint64_t combat_allocations{};
    std::uint64_t dungeon_prepare_allocations{};
    std::uint64_t continue_prepare_allocations{};
    std::uint64_t dungeon_commit_allocations{};
    std::uint32_t codec_round_trips{};
    std::uint32_t pending_restarts{};
    std::uint32_t continue_restarts{};
    checkpoint::DungeonRunState final_state{};
    bool complete{};
};

void resolve_committed(
    dungeon::DungeonSession& session,
    const dungeon::PendingSaveResult& receipt) noexcept {
    session.resolve_pending_save(receipt);
}

dungeon::PendingSaveResult committed_receipt(
    const dungeon::PendingSave& pending) {
    return {dungeon::SaveDisposition::committed,
        pending.expected_generation, pending.next_state, pending.kind};
}

bool run_trace(Trace& trace, bool restart_rhythm) noexcept {
    const dungeon::DungeonRules rules{};
    checkpoint::DungeonRunState state = case_root(2U, true);
    if (state.root_seed == 0U || state.current_room.seed == 0U) return false;
    state.progression = {2U, 7U, 1U, 1U};
    state.item_ownership.claimed_drop_bits = {{1U, 2U, 4U}};
    state.item_ownership.next_item_sequence = 17U;
    const checkpoint::DungeonRunState permanent_baseline = state;
    auto session = std::make_unique<dungeon::DungeonSession>(rules, state);
    const dungeon::PendingSave* const abyss_start =
        session->pending_save_view();
    if (abyss_start == nullptr
            || abyss_start->kind != dungeon::PendingSaveKind::abyss_start) {
        return false;
    }
    const auto abyss_start_receipt = committed_receipt(*abyss_start);
    checkpoint::DungeonRunState started_state = abyss_start->next_state;
    resolve_committed(*session, abyss_start_receipt);
    state = std::move(started_state);
    if (session->snapshot().phase == dungeon::RoomPhase::faulted) return false;

    for (std::size_t index = 0U; index < kDeathCount; ++index) {
        const auto fail = [index](const char* step) noexcept {
            std::fprintf(stderr,
                "[stage11-death-stress-fail] index=%zu step=%s\n",
                index, step);
            return false;
        };
        const bool abyss_death = index == 0U;

        std::uint32_t room_start_ticks = 0U;
        while (session->snapshot().phase != dungeon::RoomPhase::combat
                && room_start_ticks < 3U) {
            session->tick({});
            while (session->try_pop_event().has_value()) {
            }
            while (session->try_pop_combat_event().has_value()) {
            }
            ++room_start_ticks;
        }
        if (session->snapshot().phase != dungeon::RoomPhase::combat) {
            return fail("room-start");
        }

        const bool saturated = index == 499U;
        std::uint32_t saturation_before = 0U;
        if (saturated) {
            arpg::test::DungeonDeathStressFixture::saturate_ground_pool(
                *session, ground_prototype());
            const auto filled = session->snapshot();
            if (filled.ground_item_count != dungeon::kGroundDropCapacity) {
                return fail("ground-fill");
            }
            saturation_before = filled.diagnostics.ground_saturation_count;
        }

        const std::uint64_t combat_before = arpg::test::allocation_count();
        std::uint32_t combat_ticks = 0U;
        combat::MovementInput combat_movement{};
        while (session->pending_save_view() == nullptr
                && combat_ticks < 20000U) {
            if (combat_ticks % 15U == 0U) {
                combat_movement =
                    move_toward_nearest_monster(session->snapshot());
            }
            session->tick(combat_movement);
            while (session->try_pop_event().has_value()) {
            }
            while (session->try_pop_combat_event().has_value()) {
            }
            ++combat_ticks;
        }
        const std::uint64_t combat_delta = arpg::test::allocation_count()
            - combat_before;
        trace.combat_allocations += combat_delta;
        if (combat_delta != 0U) return fail("combat-allocation");
        if (session->pending_save_view() == nullptr) {
            const auto timeout = session->snapshot();
            std::fprintf(stderr,
                "[stage11-death-timeout] phase=%u hp=%d remaining=%u "
                "room=%llu seed=%llu depth=%llu\n",
                static_cast<unsigned>(timeout.phase),
                timeout.combat.has_value() ? timeout.combat->player.hp : -1,
                timeout.remaining_targets,
                static_cast<unsigned long long>(timeout.room_index),
                static_cast<unsigned long long>(state.current_room.seed),
                static_cast<unsigned long long>(timeout.depth));
            return fail("public-combat-death-timeout");
        }
        trace.dungeon_prepare_allocations += combat_delta;

        const auto death_pending = session->pending_save();
        if (!death_pending.has_value()
                || death_pending->kind
                    != dungeon::PendingSaveKind::death_retreat
                || !death_pending->death_snapshot.has_value()
                || death_pending->next_state.death.lifecycle
                    != checkpoint::DeathLifecycle::pending_continue) {
            return fail("death-pending");
        }
        if (death_pending->next_state.death.death_was_abyss != abyss_death) {
            return fail("abyss-kind");
        }
        const std::uint64_t expected_target_depth =
            state.current_room.depth > 1U ? state.current_room.depth - 1U : 1U;
        if (death_pending->next_state.death.target_room.depth
                != expected_target_depth) {
            return fail("target-depth");
        }
        if (saturated) {
            const auto current = session->snapshot();
            if (current.diagnostics.ground_saturation_count
                    != saturation_before
                    || death_pending->next_state.item_ownership.items.size()
                        != state.item_ownership.items.size()) {
                return fail("ground-checkpoint");
            }
        }
        trace.deaths[index] = death_pending->next_state.death;
        trace.death_sequences[index] =
            death_pending->next_state.death_sequence;
        if (trace.death_sequences[index] != index + 1U) {
            return fail("death-sequence");
        }
        if (!same_permanent_player_state(
                permanent_baseline, death_pending->next_state)) {
            return fail("death-permanent-state");
        }
        const std::uint64_t current_death_hash = death_hash(trace.deaths[index]);
        fold(trace.cumulative_hash, current_death_hash);
        fold(trace.cumulative_hash,
            death_pending->next_state.death.target_room.seed);

        const auto death_receipt = committed_receipt(*death_pending);
        const std::uint64_t commit_before = arpg::test::allocation_count();
        resolve_committed(*session, death_receipt);
        const std::uint64_t commit_delta =
            arpg::test::allocation_count() - commit_before;
        trace.dungeon_commit_allocations += commit_delta;
        if (commit_delta != 0U) return fail("death-commit-allocation");
        state = death_pending->next_state;
        if (session->snapshot().phase == dungeon::RoomPhase::faulted) {
            return fail("death-commit");
        }
        if (session->snapshot().ground_item_count != 0U) {
            return fail("ground-not-cleared");
        }
        if (saturated && session->snapshot().diagnostics.ground_saturation_count
                != saturation_before) {
            return fail("ground-diagnostics-changed");
        }

        if (restart_rhythm && (index + 1U) % 17U == 0U) {
            if (!codec_round_trip(state)) return fail("codec-17");
            ++trace.codec_round_trips;
        }
        if (restart_rhythm && (index + 1U) % 31U == 0U) {
            if (!codec_round_trip(state)) return fail("codec-31");
            session = std::make_unique<dungeon::DungeonSession>(rules, state);
            ++trace.pending_restarts;
        }

        const std::uint64_t continue_prepare_before =
            arpg::test::allocation_count();
        const dungeon::RequestResult continue_result =
            session->request_death_continue();
        const std::uint64_t continue_prepare_delta =
            arpg::test::allocation_count() - continue_prepare_before;
        trace.continue_prepare_allocations += continue_prepare_delta;
        if (continue_prepare_delta != 0U) {
            return fail("continue-prepare-allocation");
        }
        if (continue_result != dungeon::RequestResult::accepted) {
            return fail("continue-request");
        }
        const dungeon::PendingSave* const continue_pending =
            session->pending_save_view();
        if (continue_pending == nullptr
                || continue_pending->kind
                    != dungeon::PendingSaveKind::death_continue
                || continue_pending->next_state.death.lifecycle
                    != checkpoint::DeathLifecycle::none) {
            return fail("continue-pending");
        }
        const auto continue_receipt = committed_receipt(*continue_pending);
        checkpoint::DungeonRunState continued_state =
            continue_pending->next_state;
        const std::uint64_t continue_commit_before =
            arpg::test::allocation_count();
        resolve_committed(*session, continue_receipt);
        const std::uint64_t continue_commit_delta =
            arpg::test::allocation_count() - continue_commit_before;
        trace.dungeon_commit_allocations += continue_commit_delta;
        if (continue_commit_delta != 0U) {
            return fail("continue-commit-allocation");
        }
        state = std::move(continued_state);
        if (session->snapshot().phase == dungeon::RoomPhase::faulted) {
            return fail("continue-commit");
        }
        if (state.death_sequence != index + 1U
                || !same_permanent_player_state(permanent_baseline, state)) {
            return fail("continue-stable-state");
        }
        fold(trace.cumulative_hash, state.current_room.seed);
        fold(trace.cumulative_hash, state.commit_generation);

        if (restart_rhythm && (index + 1U) % 43U == 0U) {
            if (!codec_round_trip(state)) return fail("codec-43");
            session = std::make_unique<dungeon::DungeonSession>(rules, state);
            if (session->snapshot().phase == dungeon::RoomPhase::faulted) {
                return fail("continue-restart");
            }
            ++trace.continue_restarts;
        }
    }
    trace.final_state = state;
    trace.complete = true;
    return true;
}

std::array<std::uint64_t, 3U> golden_drop_triplet(
    std::uint64_t seed,
    std::uint16_t ordinal) noexcept {
    auto ordinal_stream = core::DeterministicRng::derive_stream(seed, ordinal);
    const std::uint64_t ordinal_key = ordinal_stream.next_u64();
    auto chance = core::DeterministicRng::derive_stream(
        ordinal_key, kDropChanceDomain);
    auto slot = core::DeterministicRng::derive_stream(
        ordinal_key, kDropSlotDomain);
    auto content = core::DeterministicRng::derive_stream(
        ordinal_key, kDropContentDomain);
    return {{chance.next_u64(), slot.next_u64(), content.next_u64()}};
}

std::uint64_t golden_stream_hash() noexcept {
    std::uint64_t hash = kHashOffset;
    const auto initial = dungeon::make_initial_run_state(
        0x57A9E11DULL, dungeon::DungeonRules{}).state;
    checkpoint::RoomDescriptor room = initial.current_room;
    constexpr std::array<checkpoint::ExitDirection, 4U> directions{{
        checkpoint::ExitDirection::up,
        checkpoint::ExitDirection::down,
        checkpoint::ExitDirection::left,
        checkpoint::ExitDirection::right,
    }};
    for (std::uint64_t ordinal = 0U; ordinal < 64U; ++ordinal) {
        const auto direction = directions[ordinal % directions.size()];
        fold(hash, dungeon::derive_door_room_seed(
            room.seed, ordinal + 1U, direction));
        fold(hash, dungeon::derive_descent_room_seed(
            room.seed, ordinal + 1U));
        const auto abyss_doors = dungeon::preview_abyss_doors(room);
        for (const bool value : abyss_doors) fold(hash, value);

        const auto drop = golden_drop_triplet(
            room.seed, static_cast<std::uint16_t>(ordinal));
        fold(hash, drop[0]);
        fold(hash, drop[1]);
        fold(hash, drop[2]);

        room.seed = dungeon::derive_door_room_seed(
            room.seed, room.index + 1U, direction);
        ++room.index;
    }
    return hash;
}

std::size_t working_set_bytes() noexcept {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)) != FALSE) {
        return counters.WorkingSetSize;
    }
#endif
    return 0U;
}

arpg::test::Failure thousand_deaths_are_deterministic_through_restarts() noexcept {
    constexpr std::uint64_t kGoldenStreams = 0xe999456db183d756ULL;
    const std::uint64_t streams_before = golden_stream_hash();
    std::printf("[stage11-death-streams] hash=0x%016llx\n",
        static_cast<unsigned long long>(streams_before));
    ARPG_REQUIRE(streams_before == kGoldenStreams);
    const std::size_t working_before = working_set_bytes();

    auto direct = std::make_unique<Trace>();
    auto restarted = std::make_unique<Trace>();
    ARPG_REQUIRE(run_trace(*direct, false));
    ARPG_REQUIRE(run_trace(*restarted, true));
    ARPG_REQUIRE(direct->complete && restarted->complete);
    ARPG_REQUIRE(restarted->codec_round_trips == kDeathCount / 17U);
    ARPG_REQUIRE(restarted->pending_restarts == kDeathCount / 31U);
    ARPG_REQUIRE(restarted->continue_restarts == kDeathCount / 43U);
    for (std::size_t index = 0U; index < kDeathCount; ++index) {
        if (!same_death(direct->deaths[index], restarted->deaths[index])) {
            const auto& a = direct->deaths[index];
            const auto& b = restarted->deaths[index];
            std::fprintf(stderr,
                "[stage11-death-divergence] index=%zu direct=0x%016llx "
                "restarted=0x%016llx target=%llu/%llu raw=%llu/%llu "
                "loss=%llu/%llu source=%u/%u detail=%u/%u "
                "hp=%d/%d armor=%lld/%lld recent0=%llu/%llu\n",
                index,
                static_cast<unsigned long long>(
                    death_hash(direct->deaths[index])),
                static_cast<unsigned long long>(
                    death_hash(restarted->deaths[index])),
                static_cast<unsigned long long>(a.target_room.seed),
                static_cast<unsigned long long>(b.target_room.seed),
                static_cast<unsigned long long>(a.raw_damage),
                static_cast<unsigned long long>(b.raw_damage),
                static_cast<unsigned long long>(a.final_damage),
                static_cast<unsigned long long>(b.final_damage),
                static_cast<unsigned>(a.source_kind),
                static_cast<unsigned>(b.source_kind),
                static_cast<unsigned>(a.source_detail_id),
                static_cast<unsigned>(b.source_detail_id),
                a.max_hp, b.max_hp,
                static_cast<long long>(a.armor),
                static_cast<long long>(b.armor),
                static_cast<unsigned long long>(a.recent_damage[0]),
                static_cast<unsigned long long>(b.recent_damage[0]));
        }
        ARPG_REQUIRE(same_death(direct->deaths[index],
            restarted->deaths[index]));
        ARPG_REQUIRE(direct->deaths[index].target_room.seed
            == restarted->deaths[index].target_room.seed);
        ARPG_REQUIRE(direct->death_sequences[index] == index + 1U);
        ARPG_REQUIRE(restarted->death_sequences[index] == index + 1U);
    }
    ARPG_REQUIRE(direct->cumulative_hash == restarted->cumulative_hash);
    ARPG_REQUIRE(dungeon::same_run_state(
        direct->final_state, restarted->final_state));
    ARPG_REQUIRE(direct->final_state.death_sequence == kDeathCount);
    ARPG_REQUIRE(direct->final_state.death.lifecycle
        == checkpoint::DeathLifecycle::none);
    ARPG_REQUIRE(golden_stream_hash() == streams_before);

    const std::size_t working_after = working_set_bytes();
    std::printf("[stage11-death-stress] deaths=%zu hash=0x%016llx "
        "reloads=%u/%u/%u allocations=%llu/%llu/%llu/%llu "
        "working-set=%zu->%zu\n",
        kDeathCount,
        static_cast<unsigned long long>(direct->cumulative_hash),
        restarted->codec_round_trips, restarted->pending_restarts,
        restarted->continue_restarts,
        static_cast<unsigned long long>(
            direct->combat_allocations + restarted->combat_allocations),
        static_cast<unsigned long long>(direct->dungeon_prepare_allocations
            + restarted->dungeon_prepare_allocations),
        static_cast<unsigned long long>(direct->continue_prepare_allocations
            + restarted->continue_prepare_allocations),
        static_cast<unsigned long long>(direct->dungeon_commit_allocations
            + restarted->dungeon_commit_allocations),
        working_before, working_after);
    return {};
}

arpg::test::Failure drop_golden_uses_production_ordinal_key() noexcept {
    constexpr std::array<std::uint64_t, 3U> kExpected{{
        0x7af3c1e15bc7b138ULL,
        0x563976ed1fc9f0c6ULL,
        0xa9e93dbac431f276ULL,
    }};
    ARPG_REQUIRE(golden_drop_triplet(1U, 0U) == kExpected);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"drop golden uses production ordinal key",
        &drop_golden_uses_production_ordinal_key},
    {"1000 deaths deterministic through 17 31 43 restarts",
        &thousand_deaths_are_deterministic_through_restarts},
};

}  // namespace

arpg::test::TestSuite dungeon_death_stress_suite() noexcept {
    return arpg::test::make_suite("dungeon_death_stress", kCases);
}
