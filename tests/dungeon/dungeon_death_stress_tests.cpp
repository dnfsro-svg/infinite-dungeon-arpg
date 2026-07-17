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

struct Trace final {
    std::array<checkpoint::DeathCheckpoint, kDeathCount> deaths{};
    std::uint64_t cumulative_hash{kHashOffset};
    std::uint64_t combat_allocations{};
    std::uint64_t dungeon_prepare_allocations{};
    std::uint64_t dungeon_commit_allocations{};
    std::uint32_t codec_round_trips{};
    std::uint32_t pending_restarts{};
    std::uint32_t continue_restarts{};
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
    for (std::size_t index = 0U; index < kDeathCount; ++index) {
        const auto fail = [index](const char* step) noexcept {
            std::fprintf(stderr,
                "[stage11-death-stress-fail] index=%zu step=%s\n",
                index, step);
            return false;
        };
        const bool abyss_death = index % 3U == 2U;
        checkpoint::DungeonRunState state = case_root(index, abyss_death);
        if (state.root_seed == 0U || state.current_room.seed == 0U) {
            return fail("case-root");
        }
        auto session = std::make_unique<dungeon::DungeonSession>(rules, state);

        if (abyss_death) {
            const auto start = session->pending_save();
            const auto receipt = start.has_value()
                ? committed_receipt(*start) : dungeon::PendingSaveResult{};
            if (!start.has_value()
                    || start->kind != dungeon::PendingSaveKind::abyss_start) {
                return fail("abyss-start");
            }
            resolve_committed(*session, receipt);
            if (session->snapshot().phase == dungeon::RoomPhase::faulted) {
                return fail("abyss-start-commit");
            }
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
        while (session->pending_save_view() == nullptr
                && combat_ticks < 20000U) {
            session->tick({});
            while (session->try_pop_combat_event().has_value()) {
            }
            ++combat_ticks;
        }
        const std::uint64_t combat_delta = arpg::test::allocation_count()
            - combat_before;
        trace.combat_allocations += combat_delta;
        if (combat_delta != 0U) return fail("combat-allocation");
        if (session->pending_save_view() == nullptr) {
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

        if (session->request_death_continue()
                != dungeon::RequestResult::accepted) {
            return fail("continue-request");
        }
        const auto continue_pending = session->pending_save();
        if (!continue_pending.has_value()
                || continue_pending->kind
                    != dungeon::PendingSaveKind::death_continue
                || continue_pending->next_state.death.lifecycle
                    != checkpoint::DeathLifecycle::none) {
            return fail("continue-pending");
        }
        const auto continue_receipt = committed_receipt(*continue_pending);
        const std::uint64_t continue_commit_before =
            arpg::test::allocation_count();
        resolve_committed(*session, continue_receipt);
        const std::uint64_t continue_commit_delta =
            arpg::test::allocation_count() - continue_commit_before;
        trace.dungeon_commit_allocations += continue_commit_delta;
        if (continue_commit_delta != 0U) {
            return fail("continue-commit-allocation");
        }
        state = continue_pending->next_state;
        if (session->snapshot().phase == dungeon::RoomPhase::faulted) {
            return fail("continue-commit");
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
    trace.complete = true;
    return true;
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

        auto ordinal_stream = core::DeterministicRng::derive_stream(
            room.seed, ordinal);
        auto chance = core::DeterministicRng::derive_stream(
            ordinal_stream.next_u64(), kDropChanceDomain);
        auto slot = core::DeterministicRng::derive_stream(
            ordinal_stream.next_u64(), kDropSlotDomain);
        auto content = core::DeterministicRng::derive_stream(
            ordinal_stream.next_u64(), kDropContentDomain);
        fold(hash, chance.next_u64());
        fold(hash, slot.next_u64());
        fold(hash, content.next_u64());

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
    constexpr std::uint64_t kGoldenStreams = 0xcf472b0e7d8ea761ULL;
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
    ARPG_REQUIRE(restarted->codec_round_trips == 58U);
    ARPG_REQUIRE(restarted->pending_restarts == 32U);
    ARPG_REQUIRE(restarted->continue_restarts == 23U);
    ARPG_REQUIRE(direct->cumulative_hash == restarted->cumulative_hash);
    for (std::size_t index = 0U; index < kDeathCount; ++index) {
        ARPG_REQUIRE(same_death(direct->deaths[index],
            restarted->deaths[index]));
        ARPG_REQUIRE(direct->deaths[index].target_room.seed
            == restarted->deaths[index].target_room.seed);
    }
    ARPG_REQUIRE(golden_stream_hash() == streams_before);

    const std::size_t working_after = working_set_bytes();
    std::printf("[stage11-death-stress] deaths=%zu hash=0x%016llx "
        "reloads=%u/%u/%u allocations=%llu/%llu/%llu "
        "working-set=%zu->%zu\n",
        kDeathCount,
        static_cast<unsigned long long>(direct->cumulative_hash),
        restarted->codec_round_trips, restarted->pending_restarts,
        restarted->continue_restarts,
        static_cast<unsigned long long>(
            direct->combat_allocations + restarted->combat_allocations),
        static_cast<unsigned long long>(direct->dungeon_prepare_allocations
            + restarted->dungeon_prepare_allocations),
        static_cast<unsigned long long>(direct->dungeon_commit_allocations
            + restarted->dungeon_commit_allocations),
        working_before, working_after);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"1000 deaths deterministic through 17 31 43 restarts",
        &thousand_deaths_are_deterministic_through_restarts},
};

}  // namespace

arpg::test::TestSuite dungeon_death_stress_suite() noexcept {
    return arpg::test::make_suite("dungeon_death_stress", kCases);
}
