#include "dungeon_test_support.hpp"

#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_affix_generation.hpp"
#include "core/deterministic_rng.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/room_generation.hpp"
#include "dungeon/room_monster_plan_builder.hpp"
#include "items/item_catalog.hpp"
#include "persistence/checkpoint_codec.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
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

constexpr std::uint64_t kDropChanceDomain = 0x44524F505F43484EULL;

struct PlanTrace final {
    std::uint16_t monster_count{};
    std::uint32_t threat_total{};
    std::uint32_t generator_version{};
    std::uint64_t blueprint_hash{};
};

struct MonsterTrace final {
    arpg::combat::MonsterOrdinal ordinal{};
    std::uint16_t spawn_ordinal{};
    arpg::combat::MonsterId id{arpg::combat::MonsterId::count};
    arpg::combat::Vec3 initial_position{};
    std::uint16_t home_cell{};
    std::uint8_t roaming_leash_cells{};
    arpg::combat::MonsterAffixSet affixes{};
    std::uint16_t danger_score{};
};

struct DropTrace final {
    std::uint16_t ordinal{};
    arpg::combat::Vec3 position{};
    std::uint64_t item_id{};
    std::uint8_t item_level{};
    arpg::items::ItemRarity rarity{arpg::items::ItemRarity::normal};
};

struct FixtureEvidence final {
    DungeonRunState initial_state{};
    DungeonRunState claimed_state{};
    PlanTrace plan{};
    MonsterTrace target{};
    DropTrace drop{};
    arpg::combat::CombatEvent defeat{};
    std::array<std::uint64_t, 3> claimed_bits{};
    std::uint32_t item_count{};
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

bool same_position(arpg::combat::Vec3 left,
    arpg::combat::Vec3 right) noexcept {
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

PlanTrace plan_trace(const arpg::combat::RoomMonsterPlan& plan) noexcept {
    return {plan.monster_count, plan.threat_total,
        plan.generator_version, plan.blueprint_hash};
}

bool same_plan_trace(const PlanTrace& left, const PlanTrace& right) noexcept {
    return left.monster_count == right.monster_count
        && left.threat_total == right.threat_total
        && left.generator_version == right.generator_version
        && left.blueprint_hash == right.blueprint_hash;
}

bool trace_matches_blueprint(const MonsterTrace& trace,
    const arpg::combat::RoomMonsterBlueprint& blueprint) noexcept {
    return trace.spawn_ordinal == blueprint.spawn_ordinal
        && trace.id == blueprint.id
        && same_position(trace.initial_position, blueprint.initial_position)
        && trace.home_cell == blueprint.home_cell
        && trace.roaming_leash_cells == blueprint.roaming_leash_cells
        && trace.affixes == blueprint.affixes
        && trace.danger_score
            == arpg::combat::monster_affix_danger_score(blueprint.affixes);
}

bool same_monster_trace(const MonsterTrace& left,
    const MonsterTrace& right) noexcept {
    return left.ordinal == right.ordinal
        && left.spawn_ordinal == right.spawn_ordinal
        && left.id == right.id
        && same_position(left.initial_position, right.initial_position)
        && left.home_cell == right.home_cell
        && left.roaming_leash_cells == right.roaming_leash_cells
        && left.affixes == right.affixes
        && left.danger_score == right.danger_score;
}

bool defeat_matches_target(const arpg::combat::CombatEvent& event,
    const MonsterTrace& target) noexcept {
    return event.kind == arpg::combat::CombatEventKind::defeated
        && event.attack == arpg::combat::AttackId::j1
        && event.target_ordinal == target.ordinal
        && same_position(event.position, target.initial_position)
        && event.monster_id == target.id
        && event.spawn_ordinal == target.spawn_ordinal
        && event.affix_score == target.danger_score
        && event.reward_eligible;
}

bool same_defeat_trace(const arpg::combat::CombatEvent& left,
    const arpg::combat::CombatEvent& right) noexcept {
    return left.kind == right.kind && left.tick == right.tick
        && left.attack == right.attack
        && left.target_ordinal == right.target_ordinal
        && same_position(left.position, right.position)
        && left.monster_id == right.monster_id
        && left.spawn_ordinal == right.spawn_ordinal
        && left.affix_score == right.affix_score
        && left.reward_eligible == right.reward_eligible;
}

bool same_drop_trace(const DropTrace& left, const DropTrace& right) noexcept {
    return left.ordinal == right.ordinal
        && same_position(left.position, right.position)
        && left.item_id == right.item_id
        && left.item_level == right.item_level
        && left.rarity == right.rarity;
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

std::uint64_t drop_roll(const DungeonRunState& state,
    std::uint16_t ordinal, std::uint64_t bound) noexcept {
    auto ordinal_stream = arpg::core::DeterministicRng::derive_stream(
        state.current_room.seed, ordinal);
    auto chance = arpg::core::DeterministicRng::derive_stream(
        ordinal_stream.next_u64(), kDropChanceDomain);
    return chance.next_bounded(bound).value();
}

bool affix_drop_hits(const DungeonRunState& state,
    std::uint16_t ordinal, std::uint16_t score) noexcept {
    return drop_roll(state, ordinal, 10000U)
        < arpg::dungeon::affix_drop_chance_bp(score);
}

std::optional<MonsterTrace> select_target(
    const arpg::combat::RoomMonsterPlan& plan,
    const DungeonRunState& state) noexcept {
    const std::size_t candidate_count = (std::min)(
        static_cast<std::size_t>(plan.monster_count),
        arpg::dungeon::kGroundDropCapacity);
    for (std::size_t index = 0U; index < candidate_count; ++index) {
        const auto ordinal = static_cast<arpg::combat::MonsterOrdinal>(index);
        const arpg::combat::RoomMonsterBlueprint& source =
            plan.monsters[index];
        const std::uint16_t score =
            arpg::combat::monster_affix_danger_score(source.affixes);
        if (source.spawn_ordinal != ordinal || source.affixes.count == 0U
                || score == 0U
                || !affix_drop_hits(state, source.spawn_ordinal, score)) {
            continue;
        }
        return MonsterTrace{ordinal, source.spawn_ordinal, source.id,
            source.initial_position, source.home_cell,
            source.roaming_leash_cells, source.affixes, score};
    }
    return std::nullopt;
}

std::optional<DungeonRunState> deep40_state(
    const DungeonRules& rules, std::uint64_t root) noexcept {
    const auto built = arpg::dungeon::make_initial_run_state(root, rules);
    if (built.fault != arpg::dungeon::DungeonFault::none) return std::nullopt;
    DungeonRunState state = built.state;
    state.current_room.index = 39U;
    state.current_room.depth = 40U;
    state.current_room.floor_room_index = 40U;
    state.current_room.seed = arpg::dungeon::derive_initial_room_seed(root, 39U);
    return state;
}

bool commit_pending(DungeonSession& session) noexcept {
    const auto* const pending = session.pending_save_view();
    if (pending == nullptr
            || pending->kind != arpg::dungeon::PendingSaveKind::loot_pickup
            || session.snapshot().phase != arpg::dungeon::RoomPhase::committing) {
        return false;
    }
    session.resolve_pending_save({arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation, pending->next_state});
    return session.snapshot().phase == arpg::dungeon::RoomPhase::combat;
}

std::optional<FixtureEvidence> run_real_deep40_fixture(const DungeonRules& rules) noexcept {
    for (std::uint64_t root = 1U; root < 10000U; ++root) {
        const auto initial = deep40_state(rules, root);
        if (!initial.has_value()) continue;

        DungeonSession session{rules, *initial};
        const arpg::combat::RoomMonsterPlan* const plan =
            arpg::test::room_monster_plan(session);
        if (plan == nullptr || plan->monster_count == 0U) return std::nullopt;
        const std::optional<MonsterTrace> target = select_target(*plan, *initial);
        if (!target.has_value()) continue;

        FixtureEvidence evidence{};
        evidence.initial_state = *initial;
        evidence.plan = plan_trace(*plan);
        evidence.target = *target;
        if (!trace_matches_blueprint(
                evidence.target, plan->monsters[evidence.target.ordinal])) {
            return std::nullopt;
        }

        const std::size_t initial_item_count = session.item_state().items.size();
        session.tick({});
        if (session.snapshot().phase != arpg::dungeon::RoomPhase::combat) {
            return std::nullopt;
        }
        while (session.try_pop_combat_event().has_value()) {}
        while (session.try_pop_event().has_value()) {}

        const auto defeated = arpg::test::defeat_room_monster_by_ordinal(
            session, evidence.target.ordinal);
        if (!defeated.has_value()
                || !defeat_matches_target(*defeated, evidence.target)) {
            return std::nullopt;
        }
        evidence.defeat = *defeated;

        const auto& ground_items = arpg::test::ground_items(session);
        if (evidence.target.spawn_ordinal >= ground_items.size()) {
            return std::nullopt;
        }
        const arpg::dungeon::GroundItem& ground =
            ground_items[evidence.target.spawn_ordinal];
        if (!ground.active
                || ground.source != arpg::dungeon::GroundItemSource::monster_drop
                || ground.drop_ordinal != evidence.target.spawn_ordinal
                || !arpg::items::validate_item(ground.item)) {
            return std::nullopt;
        }
        evidence.drop = {ground.drop_ordinal, ground.position, ground.item.id,
            ground.item.item_level, ground.item.rarity};

        arpg::test::set_player_position(session, ground.position);
        if (session.request_pickup(evidence.target.spawn_ordinal)
                != arpg::dungeon::RequestResult::accepted
                || !commit_pending(session)) {
            return std::nullopt;
        }
        evidence.claimed_bits = session.item_state().claimed_drop_bits;
        evidence.item_count = static_cast<std::uint32_t>(
            session.item_state().items.size());
        if (!bit_is_set(evidence.claimed_bits, evidence.target.spawn_ordinal)
                || session.item_state().items.size() != initial_item_count + 1U
                || !item_matches_drop(session.item_state(), evidence.drop)) {
            return std::nullopt;
        }
        evidence.claimed_state = arpg::test::stable_state(session);
        return evidence;
    }
    return std::nullopt;
}

void print_plan(const PlanTrace& plan, const MonsterTrace& target) noexcept {
    std::cout << "plan monsters=" << plan.monster_count
              << " threat=" << plan.threat_total
              << " generator=" << plan.generator_version
              << " hash=" << plan.blueprint_hash << '\n'
              << "target ordinal=" << target.ordinal
              << " monster=" << static_cast<unsigned>(target.id)
              << " pos=" << target.initial_position.x << ','
              << target.initial_position.y << ',' << target.initial_position.z
              << " home_cell=" << target.home_cell
              << " leash=" << static_cast<unsigned>(target.roaming_leash_cells)
              << " affixes=";
    for (std::uint8_t affix = 0U; affix < target.affixes.count; ++affix) {
        if (affix != 0U) std::cout << ',';
        std::cout << affix_name(target.affixes.values[affix].id)
                  << '-' << tier_name(target.affixes.values[affix].tier);
    }
    std::cout << " score=" << target.danger_score << '\n';
}

}  // namespace

int main() {
    const DungeonRules rules{};
    const auto fixture = run_real_deep40_fixture(rules);
    if (!fixture.has_value()) return 2;

    print_plan(fixture->plan, fixture->target);
    std::cout << "deep40 root=" << fixture->initial_state.root_seed
              << " room_index=" << fixture->initial_state.current_room.index
              << " room_seed=" << fixture->initial_state.current_room.seed
              << " depth=" << fixture->initial_state.current_room.depth << '\n'
              << "defeat kind=" << static_cast<unsigned>(fixture->defeat.kind)
              << " tick=" << fixture->defeat.tick
              << " target="
              << static_cast<unsigned>(fixture->defeat.target_ordinal)
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
    const arpg::combat::RoomMonsterPlan* const original_plan =
        arpg::test::room_monster_plan(original);
    const arpg::combat::RoomMonsterPlan* const reloaded_plan =
        arpg::test::room_monster_plan(reloaded);
    const bool plan_equal = original_plan != nullptr && reloaded_plan != nullptr
        && arpg::dungeon::room_monster_plan_equal_fields(
            *original_plan, *reloaded_plan)
        && same_plan_trace(fixture->plan, plan_trace(*original_plan))
        && same_plan_trace(fixture->plan, plan_trace(*reloaded_plan))
        && fixture->target.ordinal < original_plan->monster_count
        && fixture->target.ordinal < reloaded_plan->monster_count
        && trace_matches_blueprint(fixture->target,
            original_plan->monsters[fixture->target.ordinal])
        && trace_matches_blueprint(fixture->target,
            reloaded_plan->monsters[fixture->target.ordinal]);
    const bool replay_equal = fixture->initial_state.root_seed
            == replay->initial_state.root_seed
        && same_plan_trace(fixture->plan, replay->plan)
        && same_monster_trace(fixture->target, replay->target)
        && same_defeat_trace(fixture->defeat, replay->defeat)
        && same_drop_trace(fixture->drop, replay->drop);
    const bool drop_equal = replay_equal
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
