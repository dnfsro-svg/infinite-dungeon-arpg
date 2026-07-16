#include "dungeon_test_support.hpp"

#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_affix_generation.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/encounter_director.hpp"
#include "dungeon/room_generation.hpp"
#include "persistence/checkpoint_codec.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

using arpg::combat::MonsterAffixId;
using arpg::combat::MonsterAffixTier;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::RoomEncounterPlan;

struct DropTrace final {
    std::uint16_t ordinal{};
    std::uint64_t item_id{};
    std::uint8_t item_level{};
    arpg::items::ItemRarity rarity{arpg::items::ItemRarity::normal};
};

struct FixtureEvidence final {
    DungeonRunState initial_state{};
    DungeonRunState claimed_state{};
    RoomEncounterPlan plan{};
    DropTrace drop{};
    arpg::combat::CombatEvent defeat{};
    std::array<std::uint64_t, 3> claimed_bits{};
    std::uint32_t item_count{};
    bool completed{};
};

std::string_view affix_name(MonsterAffixId id) noexcept {
    const auto* const definition = arpg::combat::monster_affix_definition(id);
    return definition == nullptr ? "unknown" : definition->short_name;
}

const char* tier_name(MonsterAffixTier tier) noexcept {
    switch (tier) {
    case MonsterAffixTier::m1: return "M1";
    case MonsterAffixTier::m2: return "M2";
    case MonsterAffixTier::m3: return "M3";
    case MonsterAffixTier::count: return "invalid";
    }
    return "invalid";
}

bool bit_is_set(const std::array<std::uint64_t, 3>& bits,
    std::uint16_t ordinal) noexcept {
    const std::size_t word = ordinal / 64U;
    const std::uint8_t bit = static_cast<std::uint8_t>(ordinal % 64U);
    return word < bits.size() && (bits[word] & (std::uint64_t{1U} << bit)) != 0U;
}

bool same_plan_and_affixes(const RoomEncounterPlan& left,
    const RoomEncounterPlan& right) noexcept {
    if (left.wave_count != right.wave_count
            || left.total_budget != right.total_budget) return false;
    for (std::uint8_t wave = 0U; wave < left.wave_count; ++wave) {
        const auto& a = left.waves[wave];
        const auto& b = right.waves[wave];
        if (a.spawn_count != b.spawn_count || a.spent_budget != b.spent_budget) {
            return false;
        }
        for (std::uint8_t spawn = 0U; spawn < a.spawn_count; ++spawn) {
            const auto& source = a.spawns[spawn];
            const auto& restored = b.spawns[spawn];
            if (source.id != restored.id
                    || source.spawn_ordinal != restored.spawn_ordinal
                    || source.position.x != restored.position.x
                    || source.position.y != restored.position.y
                    || source.position.z != restored.position.z
                    || source.affixes.count != restored.affixes.count) {
                return false;
            }
            for (std::uint8_t affix = 0U;
                 affix < source.affixes.count; ++affix) {
                if (source.affixes.values[affix].id
                        != restored.affixes.values[affix].id
                    || source.affixes.values[affix].tier
                        != restored.affixes.values[affix].tier) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool item_matches_drop(const arpg::items::ItemOwnershipState& items,
    const DropTrace& drop) noexcept {
    for (const auto& item : items.items) {
        if (item.id == drop.item_id) {
            return item.item_level == drop.item_level && item.rarity == drop.rarity;
        }
    }
    return false;
}

bool has_affix(const RoomEncounterPlan& plan) noexcept {
    for (std::uint8_t wave = 0U; wave < plan.wave_count; ++wave) {
        for (std::uint8_t spawn = 0U;
             spawn < plan.waves[wave].spawn_count; ++spawn) {
            if (plan.waves[wave].spawns[spawn].affixes.count != 0U) return true;
        }
    }
    return false;
}

std::optional<DungeonRunState> deep40_state_with_affix(
    const DungeonRules& rules, std::uint64_t root) noexcept {
    const auto built = arpg::dungeon::make_initial_run_state(root, rules);
    if (built.fault != arpg::dungeon::DungeonFault::none) return std::nullopt;
    DungeonRunState state = built.state;
    state.current_room.index = 39U;
    state.current_room.depth = 40U;
    state.current_room.floor_room_index = 40U;
    state.current_room.seed = arpg::dungeon::derive_initial_room_seed(root, 39U);
    const auto plan = arpg::dungeon::build_encounter_plan(
        state.current_room.seed, state.current_room.depth,
        state.current_room.ecology, rules.encounter);
    return plan.fault == arpg::dungeon::DungeonFault::none && has_affix(plan.plan)
        ? std::optional<DungeonRunState>{state} : std::nullopt;
}

bool commit_pending(DungeonSession& session) noexcept {
    const auto* const pending = session.pending_save_view();
    if (pending == nullptr) return true;
    session.resolve_pending_save({arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation, pending->next_state});
    return session.snapshot().phase != arpg::dungeon::RoomPhase::faulted;
}

std::optional<FixtureEvidence> run_real_deep40_fixture(const DungeonRules& rules) noexcept {
    for (std::uint64_t root = 1U; root < 10000U; ++root) {
        const auto initial = deep40_state_with_affix(rules, root);
        if (!initial.has_value()) continue;

        FixtureEvidence evidence{};
        evidence.initial_state = *initial;
        DungeonSession session{rules, *initial};
        evidence.plan = arpg::test::encounter_plan(session);
        if (!has_affix(evidence.plan)) continue;

        constexpr int kMaximumTicks = 4096;
        for (int tick = 0; tick < kMaximumTicks; ++tick) {
            const auto snapshot = session.snapshot();
            if (snapshot.phase == arpg::dungeon::RoomPhase::faulted) break;
            if (snapshot.phase == arpg::dungeon::RoomPhase::combat
                    && !arpg::test::defeat_next_live_monster(session)) {
                break;
            }
            session.tick({});
            while (const auto event = session.try_pop_combat_event()) {
                if (event->kind == arpg::combat::CombatEventKind::defeated
                        && event->affix_score > 0U) {
                    evidence.defeat = *event;
                }
            }
            while (session.try_pop_event().has_value()) {}

            for (const auto& drop : arpg::test::ground_items(session)) {
                if (!drop.active || drop.drop_ordinal != evidence.defeat.spawn_ordinal) {
                    continue;
                }
                evidence.drop = {drop.drop_ordinal, drop.item.id,
                    drop.item.item_level, drop.item.rarity};
                arpg::test::set_player_position(session, drop.position);
                session.request_nearby_pickups(drop.position);
                if (!commit_pending(session)) break;
                evidence.claimed_bits = session.item_state().claimed_drop_bits;
                evidence.item_count = static_cast<std::uint32_t>(
                    session.item_state().items.size());
                evidence.claimed_state = arpg::test::stable_state(session);
                evidence.completed = bit_is_set(evidence.claimed_bits,
                    evidence.drop.ordinal) && item_matches_drop(session.item_state(),
                        evidence.drop);
                if (evidence.completed) return evidence;
            }
            if (!commit_pending(session)) break;
        }
    }
    return std::nullopt;
}

void print_plan(const RoomEncounterPlan& plan) noexcept {
    std::cout << "plan waves=" << static_cast<unsigned>(plan.wave_count)
              << " budget=" << static_cast<unsigned>(plan.total_budget) << '\n';
    for (std::uint8_t wave = 0U; wave < plan.wave_count; ++wave) {
        const auto& entries = plan.waves[wave];
        for (std::uint8_t spawn = 0U; spawn < entries.spawn_count; ++spawn) {
            const auto& source = entries.spawns[spawn];
            std::cout << "plan wave=" << static_cast<unsigned>(wave)
                      << " spawn=" << static_cast<unsigned>(spawn)
                      << " ordinal=" << source.spawn_ordinal
                      << " monster=" << static_cast<unsigned>(source.id)
                      << " pos=" << source.position.x << ',' << source.position.y
                      << " affixes=";
            for (std::uint8_t affix = 0U; affix < source.affixes.count; ++affix) {
                if (affix != 0U) std::cout << ',';
                std::cout << affix_name(source.affixes.values[affix].id)
                          << '-' << tier_name(source.affixes.values[affix].tier);
            }
            std::cout << " score="
                      << arpg::combat::monster_affix_danger_score(source.affixes)
                      << '\n';
        }
    }
}

}  // namespace

int main() {
    const DungeonRules rules{};
    const auto fixture = run_real_deep40_fixture(rules);
    if (!fixture.has_value()) return 2;

    print_plan(fixture->plan);
    std::cout << "deep40 root=" << fixture->initial_state.root_seed
              << " room_index=" << fixture->initial_state.current_room.index
              << " room_seed=" << fixture->initial_state.current_room.seed
              << " depth=" << fixture->initial_state.current_room.depth << '\n'
              << "defeat kind=" << static_cast<unsigned>(fixture->defeat.kind)
              << " tick=" << fixture->defeat.tick
              << " target=" << static_cast<unsigned>(fixture->defeat.target_index)
              << " monster=" << static_cast<unsigned>(fixture->defeat.monster_id)
              << " ordinal=" << fixture->defeat.spawn_ordinal
              << " score=" << fixture->defeat.affix_score
              << " reward_eligible=" << fixture->defeat.reward_eligible << '\n'
              << "drop ordinal=" << fixture->drop.ordinal
              << " item_id=" << fixture->drop.item_id
              << " item_level=" << static_cast<unsigned>(fixture->drop.item_level)
              << " rarity=" << static_cast<unsigned>(fixture->drop.rarity) << '\n'
              << "claim ordinal=" << fixture->drop.ordinal
              << " claimed_bits=" << fixture->claimed_bits[0] << ','
              << fixture->claimed_bits[1] << ',' << fixture->claimed_bits[2]
              << " item_count=" << fixture->item_count << '\n';

    DungeonSession original{rules, fixture->claimed_state};
    const auto replay = run_real_deep40_fixture(rules);
    if (!replay.has_value()) return 3;
    const auto encoded = arpg::persistence::encode_checkpoint(
        arpg::test::stable_state(original));
    if (!encoded.has_value()) return 4;
    const auto decoded = arpg::persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    if (decoded.error != arpg::persistence::CodecError::none) return 5;
    DungeonSession reloaded{rules, decoded.state};
    const bool state_equal = arpg::dungeon::same_run_state(
        arpg::test::stable_state(original), decoded.state);
    const bool plan_equal = same_plan_and_affixes(arpg::test::encounter_plan(original),
        arpg::test::encounter_plan(reloaded));
    const bool drop_equal = fixture->drop.ordinal == replay->drop.ordinal
        && fixture->drop.item_id == replay->drop.item_id
        && fixture->drop.item_level == replay->drop.item_level
        && fixture->drop.rarity == replay->drop.rarity
        && item_matches_drop(reloaded.item_state(), fixture->drop);
    const bool claim_equal = fixture->claimed_bits
        == reloaded.item_state().claimed_drop_bits
        && bit_is_set(reloaded.item_state().claimed_drop_bits,
            fixture->drop.ordinal);
    const bool consistent = state_equal && plan_equal && drop_equal && claim_equal;
    std::cout << "v4 state_equal=" << state_equal
              << " plan_and_affixes_equal=" << plan_equal
              << " drop_equal=" << drop_equal
              << " claim_equal=" << claim_equal
              << " consistent=" << consistent << '\n';
    return consistent ? 0 : 6;
}
