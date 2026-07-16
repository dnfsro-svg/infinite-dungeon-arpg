#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "dungeon/dungeon_progression.hpp"
#include "items/item_catalog.hpp"
#include "persistence/checkpoint_codec.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace {

using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonSession;
using arpg::dungeon::ExitDirection;
using arpg::dungeon::GroundItem;
using arpg::dungeon::RequestResult;
using arpg::dungeon::RoomPhase;
using arpg::dungeon::SaveDisposition;
using arpg::items::ItemInstance;

constexpr std::uint64_t kRootSeed = 0x8E10A57E5D00D123ULL;
constexpr std::size_t kStressRoomCount = 1000U;
constexpr std::size_t kRestartInterval = 37U;

enum class MismatchField : std::uint8_t {
    none,
    room_seed,
    generation,
    item_count,
    item_bytes,
    equipment_ids,
    claimed_bits,
    next_sequence,
    player_build,
    ground_active,
    ground_ordinal,
    ground_position,
    ground_content,
};

struct Comparison final {
    MismatchField field{MismatchField::none};
    std::size_t index{};
};

struct TraceResult final {
    bool completed{};
    bool perturbation_applied{};
    Comparison mismatch{};
    std::size_t mismatch_room{};
    std::size_t mismatch_step{};
    std::size_t drops{};
    std::size_t equips{};
    std::size_t recipes{};
    std::size_t restarts{};
};

bool same_build(const arpg::combat::PlayerCombatBuild& left,
    const arpg::combat::PlayerCombatBuild& right) noexcept {
    const auto& a = left.values;
    const auto& b = right.values;
    return left.weapon_physical == right.weapon_physical
        && left.local_attack_speed_bp == right.local_attack_speed_bp
        && a.flat_damage == b.flat_damage
        && a.damage_increased == b.damage_increased
        && a.damage_reduction == b.damage_reduction
        && a.damage_reduction_cap_bonus == b.damage_reduction_cap_bonus
        && a.armor == b.armor && a.evasion == b.evasion
        && a.melee_damage == b.melee_damage
        && a.max_health == b.max_health
        && a.max_health_more == b.max_health_more
        && a.max_barrier == b.max_barrier
        && a.damage_taken == b.damage_taken
        && a.movement_speed == b.movement_speed
        && a.attack_speed == b.attack_speed
        && a.impulse_scale == b.impulse_scale
        && a.jump_speed == b.jump_speed
        && a.air_control == b.air_control && a.valid == b.valid;
}

bool same_item_bytes(
    const ItemInstance& left, const ItemInstance& right) noexcept {
    static_assert(sizeof(ItemInstance) == 40U);
    return std::memcmp(&left, &right, sizeof(ItemInstance)) == 0;
}

Comparison compare_sessions(
    const DungeonSession& left, const DungeonSession& right) noexcept {
    const auto& left_state = arpg::test::stable_state(left);
    const auto& right_state = arpg::test::stable_state(right);
    if (left_state.current_room.seed != right_state.current_room.seed)
        return {MismatchField::room_seed, 0U};
    if (left_state.commit_generation != right_state.commit_generation)
        return {MismatchField::generation, 0U};

    const auto& left_items = left_state.item_ownership;
    const auto& right_items = right_state.item_ownership;
    if (left_items.items.size() != right_items.items.size())
        return {MismatchField::item_count, 0U};
    for (std::size_t index = 0U; index < left_items.items.size(); ++index) {
        if (!same_item_bytes(left_items.items[index], right_items.items[index]))
            return {MismatchField::item_bytes, index};
    }
    if (left_items.equipment.equipped_ids
            != right_items.equipment.equipped_ids)
        return {MismatchField::equipment_ids, 0U};
    if (left_items.claimed_drop_bits != right_items.claimed_drop_bits)
        return {MismatchField::claimed_bits, 0U};
    if (left_items.next_item_sequence != right_items.next_item_sequence)
        return {MismatchField::next_sequence, 0U};
    if (!same_build(
            arpg::test::player_build(left), arpg::test::player_build(right)))
        return {MismatchField::player_build, 0U};

    const auto& left_ground = arpg::test::ground_items(left);
    const auto& right_ground = arpg::test::ground_items(right);
    for (std::size_t index = 0U; index < left_ground.size(); ++index) {
        const GroundItem& a = left_ground[index];
        const GroundItem& b = right_ground[index];
        if (a.active != b.active)
            return {MismatchField::ground_active, index};
        if (!a.active) continue;
        if (a.drop_ordinal != b.drop_ordinal)
            return {MismatchField::ground_ordinal, index};
        if (a.position.x != b.position.x || a.position.y != b.position.y
                || a.position.z != b.position.z)
            return {MismatchField::ground_position, index};
        if (!same_item_bytes(a.item, b.item))
            return {MismatchField::ground_content, index};
    }
    return {};
}

bool commit_pending(DungeonSession& session) noexcept {
    const auto* const pending = session.pending_save_view();
    if (pending == nullptr) return false;
    session.resolve_pending_save({SaveDisposition::committed,
        pending->expected_generation, pending->next_state});
    return session.snapshot().phase != RoomPhase::faulted;
}

void drain_observable_events(DungeonSession& session) noexcept {
    while (session.try_pop_event().has_value()) {}
    while (session.try_pop_combat_event().has_value()) {}
}

bool record_comparison(TraceResult& result,
    const DungeonSession& left,
    const DungeonSession& right,
    std::size_t room,
    std::size_t step) noexcept {
    const Comparison comparison = compare_sessions(left, right);
    if (comparison.field == MismatchField::none) return true;
    result.mismatch = comparison;
    result.mismatch_room = room;
    result.mismatch_step = step;
    return false;
}

std::vector<std::uint16_t> active_ordinals(
    const DungeonSession& session) {
    std::vector<std::uint16_t> result;
    const auto& ground = arpg::test::ground_items(session);
    for (std::size_t index = 0U; index < ground.size(); ++index) {
        if (ground[index].active)
            result.push_back(static_cast<std::uint16_t>(index));
    }
    return result;
}

bool pickup_one(
    DungeonSession& session, std::uint16_t ordinal) noexcept {
    const GroundItem& ground = arpg::test::ground_items(session)[ordinal];
    if (!ground.active) return false;
    arpg::test::set_player_position(session, ground.position);
    session.request_nearby_pickups(ground.position);
    const auto pending = session.pending_save_view();
    return pending != nullptr && commit_pending(session);
}

const arpg::items::BaseDefinition* base_for(const ItemInstance& item) noexcept {
    return arpg::items::base_definition(item.base_id);
}

bool equipped(const arpg::items::EquipmentState& equipment,
    std::uint64_t item_id) noexcept {
    return std::find(equipment.equipped_ids.begin(),
        equipment.equipped_ids.end(), item_id)
        != equipment.equipped_ids.end();
}

std::array<std::uint64_t, 3> first_recipe(
    const arpg::items::ItemOwnershipState& state) noexcept {
    for (std::size_t first = 0U; first < state.items.size(); ++first) {
        const ItemInstance& a = state.items[first];
        const auto* a_base = base_for(a);
        if (a_base == nullptr || equipped(state.equipment, a.id)) continue;
        for (std::size_t second = first + 1U;
             second < state.items.size(); ++second) {
            const ItemInstance& b = state.items[second];
            const auto* b_base = base_for(b);
            if (b_base == nullptr || equipped(state.equipment, b.id)
                    || b_base->slot != a_base->slot || b.rarity != a.rarity)
                continue;
            for (std::size_t third = second + 1U;
                 third < state.items.size(); ++third) {
                const ItemInstance& c = state.items[third];
                const auto* c_base = base_for(c);
                if (c_base != nullptr && !equipped(state.equipment, c.id)
                        && c_base->slot == a_base->slot
                        && c.rarity == a.rarity)
                    return {a.id, b.id, c.id};
            }
        }
    }
    return {};
}

bool apply_fixed_transactions(DungeonSession& left,
    DungeonSession& right,
    std::size_t room,
    std::size_t& equips,
    std::size_t& recipes) noexcept {
    const auto& items = left.item_state().items;
    if (!items.empty()) {
        const ItemInstance& chosen = items[room % items.size()];
        const RequestResult left_result = left.request_equip(chosen.id);
        const RequestResult right_result = right.request_equip(chosen.id);
        if (left_result != right_result || left_result == RequestResult::faulted)
            return false;
        if (left_result == RequestResult::accepted) {
            if (!commit_pending(left) || !commit_pending(right)) return false;
            ++equips;
        }
    }

    const auto recipe = first_recipe(left.item_state());
    if (recipe[0] == 0U) return true;
    const RequestResult left_result = left.request_recipe(recipe);
    const RequestResult right_result = right.request_recipe(recipe);
    if (left_result != RequestResult::accepted
            || right_result != RequestResult::accepted)
        return false;
    if (!commit_pending(left) || !commit_pending(right)) return false;
    ++recipes;
    return true;
}

bool transition_room(
    DungeonSession& left, DungeonSession& right, std::size_t room) noexcept {
    arpg::test::set_phase(left, RoomPhase::awaiting_exit);
    arpg::test::set_phase(right, RoomPhase::awaiting_exit);
    constexpr std::array<ExitDirection, 4> kDirections{{
        ExitDirection::up,
        ExitDirection::right,
        ExitDirection::down,
        ExitDirection::left,
    }};
    const ExitDirection direction = kDirections[room % kDirections.size()];
    arpg::test::attempt_exit(left, direction);
    arpg::test::attempt_exit(right, direction);
    if (!commit_pending(left) || !commit_pending(right)) return false;
    left.tick({});
    right.tick({});
    drain_observable_events(left);
    drain_observable_events(right);
    return left.snapshot().phase == RoomPhase::locked
        && right.snapshot().phase == RoomPhase::locked;
}

bool restart_right_session(
    std::unique_ptr<DungeonSession>& right,
    const DungeonRules& rules) noexcept {
    const auto encoded = arpg::persistence::encode_checkpoint(
        arpg::test::stable_state(*right));
    if (!encoded.has_value()) return false;
    const auto decoded = arpg::persistence::decode_checkpoint(
        encoded->data(), encoded->size());
    if (decoded.error != arpg::persistence::CodecError::none) return false;
    right = std::make_unique<DungeonSession>(rules, decoded.state);
    return right->snapshot().phase == RoomPhase::locked;
}

TraceResult run_trace(std::size_t room_count, bool perturb_pickup_order) {
    TraceResult result{};
    const DungeonRules rules{};
    auto initial = arpg::dungeon::make_initial_run_state(kRootSeed, rules);
    if (initial.fault != arpg::dungeon::DungeonFault::none) return result;
    auto left = std::make_unique<DungeonSession>(rules, initial.state);
    auto right = std::make_unique<DungeonSession>(rules, initial.state);
    std::size_t step = 0U;
    if (!record_comparison(result, *left, *right, 0U, step++)) return result;

    for (std::size_t room = 0U; room < room_count; ++room) {
        for (std::uint8_t wave = 0U; wave < 2U; ++wave) {
            for (std::uint8_t target = 0U; target < 96U; ++target) {
                const std::uint16_t ordinal = static_cast<std::uint16_t>(
                    static_cast<std::uint16_t>(wave) * 96U + target);
                const arpg::combat::Vec3 position{
                    static_cast<float>(ordinal) * 4.0F,
                    static_cast<float>(wave) * 4.0F,
                    0.0F};
                if (!arpg::test::relay_defeated(
                        *left, wave, target, position)
                        || !arpg::test::relay_defeated(
                            *right, wave, target, position))
                    return result;
                drain_observable_events(*left);
                drain_observable_events(*right);
                if (!record_comparison(
                        result, *left, *right, room, step++))
                    return result;
            }
        }

        auto left_order = active_ordinals(*left);
        auto right_order = left_order;
        result.drops += left_order.size();
        if (perturb_pickup_order && !result.perturbation_applied
                && right_order.size() >= 2U) {
            std::swap(right_order[0], right_order[1]);
            result.perturbation_applied = true;
        }
        for (std::size_t index = 0U; index < left_order.size(); ++index) {
            if (!pickup_one(*left, left_order[index])
                    || !pickup_one(*right, right_order[index]))
                return result;
            if (!record_comparison(
                    result, *left, *right, room, step++)) {
                result.completed = perturb_pickup_order
                    && result.perturbation_applied;
                return result;
            }
        }

        if (!apply_fixed_transactions(
                *left, *right, room, result.equips, result.recipes))
            return result;
        if (!record_comparison(result, *left, *right, room, step++))
            return result;
        if (!transition_room(*left, *right, room)) return result;
        if (!record_comparison(result, *left, *right, room, step++))
            return result;

        if ((room + 1U) % kRestartInterval == 0U) {
            if (!restart_right_session(right, rules)) return result;
            ++result.restarts;
            if (!record_comparison(result, *left, *right, room, step++))
                return result;
        }
    }
    result.completed = true;
    return result;
}

const char* mismatch_name(MismatchField field) noexcept {
    switch (field) {
    case MismatchField::none: return "none";
    case MismatchField::room_seed: return "room_seed";
    case MismatchField::generation: return "generation";
    case MismatchField::item_count: return "item_count";
    case MismatchField::item_bytes: return "item_bytes";
    case MismatchField::equipment_ids: return "equipment_ids";
    case MismatchField::claimed_bits: return "claimed_bits";
    case MismatchField::next_sequence: return "next_sequence";
    case MismatchField::player_build: return "player_build";
    case MismatchField::ground_active: return "ground_active";
    case MismatchField::ground_ordinal: return "ground_ordinal";
    case MismatchField::ground_position: return "ground_position";
    case MismatchField::ground_content: return "ground_content";
    }
    return "unknown";
}

arpg::test::Failure pickup_order_perturbation_is_detected() noexcept {
    const TraceResult result = run_trace(128U, true);
    ARPG_REQUIRE(result.completed);
    ARPG_REQUIRE(result.perturbation_applied);
    ARPG_REQUIRE(result.mismatch.field != MismatchField::none);
    ARPG_REQUIRE(result.mismatch_room < 128U);
    return {};
}

arpg::test::Failure one_thousand_room_equipment_trace_is_deterministic() noexcept {
    const TraceResult result = run_trace(kStressRoomCount, false);
    if (!result.completed || result.mismatch.field != MismatchField::none) {
        std::fprintf(stderr,
            "stage8 stress mismatch room=%zu step=%zu field=%s index=%zu\n",
            result.mismatch_room, result.mismatch_step,
            mismatch_name(result.mismatch.field), result.mismatch.index);
    }
    ARPG_REQUIRE(result.completed);
    ARPG_REQUIRE(result.mismatch.field == MismatchField::none);
    ARPG_REQUIRE(result.drops > 0U);
    ARPG_REQUIRE(result.equips > 0U);
    ARPG_REQUIRE(result.recipes > 0U);
    ARPG_REQUIRE(result.restarts == kStressRoomCount / kRestartInterval);
    return {};
}

const arpg::test::TestCase kCases[] = {
    {"pickup order perturbation is detected", &pickup_order_perturbation_is_detected},
    {"1000 room equipment trace is deterministic",
        &one_thousand_room_equipment_trace_is_deterministic},
};

}  // namespace

arpg::test::TestSuite dungeon_equipment_stress_suite() noexcept {
    return arpg::test::make_suite("dungeon_equipment_stress", kCases);
}
