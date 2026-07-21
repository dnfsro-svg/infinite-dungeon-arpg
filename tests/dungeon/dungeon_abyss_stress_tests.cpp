#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "combat_test_support.hpp"
#include "dungeon_test_support.hpp"

#include "abyss/abyss_rewards.hpp"
#include "abyss/abyss_rules.hpp"
#include "combat/combat_world.hpp"
#include "combat/room_bounds.hpp"
#include "dungeon/abyss_reward.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/encounter_director.hpp"
#include "dungeon/room_generation.hpp"
#include "persistence/checkpoint_codec.hpp"
#include "items/item_generation.hpp"

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
            || lhs.initial_monster_count != rhs.initial_monster_count
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
    trace.hash = fold(trace.hash, room.encounter.initial_monster_count);
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

bool commit_pending(arpg::dungeon::DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()) return false;
    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation,
        pending->next_state,
    });
    return session.snapshot().phase != arpg::dungeon::RoomPhase::faulted;
}

arpg::items::ItemInstance ground_prototype() noexcept {
    return arpg::items::generate_item({
        0xD10A600DULL,
        arpg::items::ItemSlot::weapon,
        1U,
        0xD100000000000001ULL,
        arpg::items::ItemRarity::normal,
    }).value();
}

bool same_item(const arpg::items::ItemInstance& left,
    const arpg::items::ItemInstance& right) noexcept {
    if (left.id != right.id || left.base_id != right.base_id
            || left.rarity != right.rarity
            || left.item_level != right.item_level
            || left.required_level != right.required_level
            || left.affix_count != right.affix_count
            || left.reserved != right.reserved
            || left.reinforcement != right.reinforcement
            || left.extension_reserved != right.extension_reserved) return false;
    for (std::size_t index = 0U; index < left.affixes.size(); ++index) {
        if (left.affixes[index].affix_id != right.affixes[index].affix_id
                || left.affixes[index].tier != right.affixes[index].tier
                || left.affixes[index].variant
                    != right.affixes[index].variant
                || left.affixes[index].value_roll_bp
                    != right.affixes[index].value_roll_bp) return false;
    }
    return true;
}

bool exactly_same_ground_item(const arpg::dungeon::GroundItem& left,
    const arpg::dungeon::GroundItem& right) noexcept {
    return left.active == right.active
        && left.drop_ordinal == right.drop_ordinal
        && left.source == right.source
        && left.abyss_reward_ordinal == right.abyss_reward_ordinal
        && left.position.x == right.position.x
        && left.position.y == right.position.y
        && left.position.z == right.position.z
        && same_item(left.item, right.item);
}

ResolutionTrace production_resolution(
    checkpoint::DungeonRunState& state,
    const arpg::dungeon::DungeonRules& rules) noexcept {
    const auto fail = [&state](int step) noexcept {
        std::fprintf(stderr,
            "[stage10-production-resolution] step=%d seed=%llu\n", step,
            static_cast<unsigned long long>(state.current_room.seed));
        return ResolutionTrace{};
    };
    const auto profile = arpg::abyss::reward_profile_for(
        state.abyss.danger,
        static_cast<std::uint8_t>((std::min)(
            state.current_room.depth, std::uint64_t{100U})));
    if (profile.item_count == 0U || profile.item_count > 3U) return fail(1);
    state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::cleared;
    state.abyss.reward_total = profile.item_count;
    state.abyss.generated_mask = 0U;
    state.abyss.claimed_mask = 0U;
    state.abyss.abandoned_mask = 0U;
    state.abyss.reward_revision = 0U;

    arpg::dungeon::DungeonSession session{rules, state};
    session.tick({});
    if (!commit_pending(session)) return fail(2);
    const auto materialized = session.snapshot();
    if (materialized.abyss_unpicked_rewards != 1U
            || materialized.abyss_pending_rewards
                != static_cast<std::uint8_t>(profile.item_count - 1U)) {
        return fail(3);
    }

    if (profile.item_count > 1U) {
        std::uint16_t reward_index = 0xFFFFU;
        arpg::combat::Vec3 reward_position{};
        for (std::size_t index = 0U;
             index < materialized.ground_item_count; ++index) {
            if (materialized.ground_items[index].source
                    == arpg::dungeon::GroundItemSource::abyss_chest) {
                reward_index = materialized.ground_items[index].ordinal;
                reward_position = materialized.ground_items[index].position;
                break;
            }
        }
        arpg::test::set_player_position(session, reward_position);
        if (reward_index == 0xFFFFU
                || session.request_pickup(reward_index)
                    != arpg::dungeon::RequestResult::accepted
                || !commit_pending(session)) {
            return fail(4);
        }
    }

    arpg::test::fill_ground_pool(session, ground_prototype());
    arpg::test::set_player_position(session, {arpg::combat::room_bounds::max_x, 0.0F, 0.0F});
    session.tick({1, 0});
    const auto warned = session.snapshot();
    bool warning_event = false;
    while (const auto event = session.try_pop_event()) {
        if (event->kind == arpg::dungeon::DungeonEventKind::abyss_exit_warning
                && event->transition
                    == arpg::dungeon::TransitionKind::door
                && event->direction
                    == arpg::dungeon::ExitDirection::right) {
            warning_event = true;
        }
    }
    if (!warning_event || !warned.abyss_exit_confirmation_armed) return fail(5);
    session.tick({});
    if (!session.snapshot().abyss_exit_confirmation_armed) return fail(6);
    session.tick({1, 0});
    if (!session.pending_save().has_value() || !commit_pending(session)) {
        return fail(7);
    }
    state = arpg::test::stable_state(session);
    const auto& resolution = state.last_abyss_resolution;
    return {resolution.valid, resolution.room_seed, resolution.rule,
        resolution.total, resolution.generated, resolution.claimed,
        resolution.abandoned};
}

bool generate_long_trace(LongTrace& trace, std::size_t restart_interval) noexcept {
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

        const arpg::dungeon::EncounterBuildRequest request{
            state.current_room.seed,
            state.current_room.depth,
            state.current_room.ecology,
            state.current_room.entry,
            state.current_room.has_hole,
            static_cast<std::uint8_t>(
                state.current_room.is_abyss ? 18U : 12U),
        };
        const auto encounter = state.current_room.is_abyss
            ? arpg::dungeon::build_abyss_encounter_plan(
                request, rules.encounter)
            : arpg::dungeon::build_encounter_plan(request, rules.encounter);
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
            room.resolution = production_resolution(state, rules);
            if (!room.resolution.valid
                    || room.resolution.room_seed != room.descriptor.seed
                    || room.resolution.rule != selected->rule
                    || room.resolution.total != room.reward_count) {
                std::fprintf(stderr,
                    "[stage10-resolution-fail] room=%zu seed=%llu valid=%u "
                    "resolution_seed=%llu rule=%u/%u total=%u/%u\n",
                    room_index,
                    static_cast<unsigned long long>(room.descriptor.seed),
                    static_cast<unsigned>(room.resolution.valid),
                    static_cast<unsigned long long>(
                        room.resolution.room_seed),
                    static_cast<unsigned>(room.resolution.rule),
                    static_cast<unsigned>(selected->rule),
                    static_cast<unsigned>(room.resolution.total),
                    static_cast<unsigned>(room.reward_count));
                return false;
            }
        }
        fold_room(trace, room);

        if (restart_interval != 0U
                && (room_index + 1U) % restart_interval == 0U) {
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

arpg::combat::CombatEncounterConfig extreme_config() noexcept {
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
    return config;
}

checkpoint::DungeonRunState extreme_cleared_state() noexcept {
    auto state = arpg::dungeon::make_initial_run_state(
        0xA8E600DULL, arpg::dungeon::DungeonRules{}).state;
    for (std::uint64_t seed = 1U; seed != 0U; ++seed) {
        const auto selected = arpg::abyss::select_abyss_rule(seed, 1U);
        if (!arpg::abyss::is_abyss_roll(seed)
                || !selected.has_value()
                || selected->rule
                    != arpg::abyss::AbyssRuleId::chaos_expansion) {
            continue;
        }
        state.current_room.seed = seed;
        state.current_room.entry = checkpoint::EntrySide::left;
        state.current_room.is_abyss = true;
        state.last_transition = checkpoint::TransitionKind::door;
        state.last_direction = checkpoint::ExitDirection::right;
        state.abyss.lifecycle = arpg::abyss::AbyssLifecycle::cleared;
        state.abyss.danger = selected->danger;
        state.abyss.rule = selected->rule;
        state.abyss.rules_version = selected->rules_version;
        state.abyss.reward_total = 3U;
        return state;
    }
    return {};
}

arpg::test::Failure thousand_room_abyss_trace_matches_golden_and_reload() noexcept {
    auto first = std::make_unique<LongTrace>();
    auto second = std::make_unique<LongTrace>();
    ARPG_REQUIRE(generate_long_trace(*first, 0U));
    ARPG_REQUIRE(generate_long_trace(*second, 37U));
    ARPG_REQUIRE(first->complete && first->reload_count == 0U);
    ARPG_REQUIRE(second->complete && second->reload_count == 27U);
    ARPG_REQUIRE(same_long_trace(*first, *second));

    const std::uint32_t total_trials = first->door_trials[0]
        + first->door_trials[1] + first->door_trials[2]
        + first->door_trials[3];
    const std::uint32_t total_hits = first->door_hits[0]
        + first->door_hits[1] + first->door_hits[2] + first->door_hits[3];
    std::printf("[stage10-abyss-stress] hits=%u/%u directions=%u/%u/%u/%u "
        "reloads=%u hash=0x%016llx\n", total_hits, total_trials,
        first->door_hits[0], first->door_hits[1], first->door_hits[2],
        first->door_hits[3], second->reload_count,
        static_cast<unsigned long long>(first->hash));

    ARPG_REQUIRE(total_trials == 4000U);
    ARPG_REQUIRE(total_hits >= 20U && total_hits <= 70U);
    constexpr std::array<std::uint32_t, 4> kGoldenTrials{{
        1000U, 1000U, 1000U, 1000U}};
    constexpr std::array<std::uint32_t, 4> kGoldenHits{{9U, 11U, 9U, 7U}};
    ARPG_REQUIRE(first->door_trials == kGoldenTrials);
    ARPG_REQUIRE(first->door_hits == kGoldenHits);
    ARPG_REQUIRE(first->hash == 0xbe1ba6dc7d9c29a3ULL);
    return {};
}

arpg::test::Failure extreme_abyss_pools_run_600_ticks_without_allocation() noexcept {
    arpg::dungeon::DungeonSession session{
        arpg::dungeon::DungeonRules{}, extreme_cleared_state()};
    arpg::test::install_combat_world(session, extreme_config());
    arpg::combat::CombatWorld* const world =
        arpg::test::mutable_combat_world(session);
    ARPG_REQUIRE(world != nullptr);
    const auto initial_world = world->snapshot();
    ARPG_REQUIRE(initial_world.monster_count
        == arpg::combat::kMonsterCapacity);
    const arpg::combat::MonsterHandle owner{
        0U, initial_world.monsters[0].generation};
    arpg::test::CombatWorldTestAccess::fill_projectiles(*world, owner);
    arpg::test::CombatWorldTestAccess::fill_hazards(*world, owner);
    arpg::test::fill_ground_pool(session, ground_prototype());
    session.tick({});
    const auto saturated = session.snapshot();
    ARPG_REQUIRE(saturated.combat.has_value());
    ARPG_REQUIRE(saturated.combat->monster_count
        == arpg::combat::kMonsterCapacity);
    ARPG_REQUIRE(saturated.combat->projectile_count
        == arpg::combat::kProjectileCapacity);
    ARPG_REQUIRE(saturated.combat->hazard_count
        == arpg::combat::kHazardCapacity);
    std::size_t ground_count = 0U;
    for (const auto& ground : arpg::test::ground_items(session)) {
        ground_count += ground.active;
    }
    ARPG_REQUIRE(ground_count == arpg::dungeon::kGroundDropCapacity);

    const std::uint32_t projectile_saturation_before =
        saturated.combat->diagnostics.projectile_saturation_count;
    const std::uint32_t hazard_saturation_before =
        saturated.combat->diagnostics.hazard_saturation_count;
    const std::uint32_t ground_saturation_before =
        saturated.diagnostics.ground_saturation_count;
    const auto ground_before = arpg::test::ground_items(session);
    const std::uint64_t before = arpg::test::allocation_count();
    for (int tick = 0; tick < 600; ++tick) session.tick({});
    const std::uint64_t allocations = arpg::test::allocation_count() - before;
    const auto after = session.snapshot();

    ARPG_REQUIRE(allocations == 0U);
    ARPG_REQUIRE(after.combat.has_value());
    ARPG_REQUIRE(after.combat->monster_count <= arpg::combat::kMonsterCapacity);
    ARPG_REQUIRE(after.combat->projectile_count
        == arpg::combat::kProjectileCapacity);
    ARPG_REQUIRE(after.combat->hazard_count == arpg::combat::kHazardCapacity);
    ARPG_REQUIRE(after.combat->diagnostics.projectile_saturation_count
        > projectile_saturation_before);
    ARPG_REQUIRE(after.combat->diagnostics.hazard_saturation_count
        > hazard_saturation_before);
    ARPG_REQUIRE(after.diagnostics.ground_saturation_count
        - ground_saturation_before == 600U);
    ARPG_REQUIRE(after.combat->diagnostics.projectile_invalid_owner_count == 0U);
    ARPG_REQUIRE(after.combat->diagnostics.hazard_invalid_owner_count == 0U);
    ARPG_REQUIRE(after.combat->diagnostics.event_overflow_count == 0U);
    ground_count = 0U;
    const auto& ground_after = arpg::test::ground_items(session);
    for (std::size_t index = 0U; index < ground_after.size(); ++index) {
        ARPG_REQUIRE(exactly_same_ground_item(
            ground_before[index], ground_after[index]));
    }
    for (const auto& ground : ground_after) {
        ground_count += ground.active;
    }
    ARPG_REQUIRE(ground_count == arpg::dungeon::kGroundDropCapacity);
    ARPG_REQUIRE(after.abyss_pending_rewards == 3U);
    std::printf("[stage10-abyss-stress] ticks=600 allocations=%llu "
        "pools=%zu/%zu/%zu/%zu saturation=%u/%u/%u\n",
        static_cast<unsigned long long>(allocations),
        after.combat->monster_count, after.combat->projectile_count,
        after.combat->hazard_count, ground_count,
        after.combat->diagnostics.projectile_saturation_count,
        after.combat->diagnostics.hazard_saturation_count,
        after.diagnostics.ground_saturation_count);
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
