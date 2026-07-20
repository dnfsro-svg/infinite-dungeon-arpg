#include "test_framework.hpp"

#include "allocation_probe.hpp"
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
using arpg::dungeon::AutoPickupPolicy;
using arpg::dungeon::RequestResult;
using arpg::dungeon::RoomPhase;
using arpg::dungeon::SaveDisposition;
using arpg::items::ItemInstance;
using arpg::items::ItemRarity;

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
    materials,
    claimed_bits,
    next_sequence,
    player_build,
    ground_active,
    ground_ordinal,
    ground_source,
    ground_abyss_reward_ordinal,
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
    const char* abort_point{"none"};
    Comparison mismatch{};
    std::size_t mismatch_room{};
    std::size_t mismatch_step{};
    std::size_t drops{};
    std::size_t equips{};
    std::size_t recipes{};
    std::size_t restarts{};
    std::size_t filtered_items_retained{};
    std::size_t relaxed_policy_pickups{};
    std::size_t hot_path_iterations{};
    std::array<std::size_t, 3> policy_attempts{};
    std::array<std::size_t, 3> policy_retained{};
    std::size_t exact_pickup_verifications{};
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
    static_assert(sizeof(ItemInstance) == 64U);
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
    if (left_items.materials != right_items.materials)
        return {MismatchField::materials, 0U};
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
        if (a.source != b.source)
            return {MismatchField::ground_source, index};
        if (a.abyss_reward_ordinal != b.abyss_reward_ordinal)
            return {MismatchField::ground_abyss_reward_ordinal, index};
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

enum class PickupAttempt : std::uint8_t {
    failed,
    retained,
    picked,
};

PickupAttempt pickup_one(DungeonSession& session,
    std::uint16_t ordinal,
    AutoPickupPolicy policy) noexcept {
    const GroundItem& ground = arpg::test::ground_items(session)[ordinal];
    if (!ground.active) return PickupAttempt::failed;
    const ItemInstance original_item = ground.item;
    const std::uint64_t original_item_id = ground.item.id;
    const auto original_position = ground.position;
    const auto original_source = ground.source;
    arpg::test::set_player_position(session, ground.position);
    session.request_nearby_pickups(ground.position, policy);
    const auto pending = session.pending_save_view();
    if (pending == nullptr) {
        const GroundItem& retained = arpg::test::ground_items(session)[ordinal];
        return retained.active && retained.drop_ordinal == ordinal
                && retained.source == original_source
                && retained.position.x == original_position.x
                && retained.position.y == original_position.y
                && retained.position.z == original_position.z
                && same_item_bytes(retained.item, original_item)
            ? PickupAttempt::retained : PickupAttempt::failed;
    }
    if (original_source != arpg::dungeon::GroundItemSource::monster_drop
            || pending->kind != arpg::dungeon::PendingSaveKind::loot_pickup
            || pending->pickup_ordinal != ordinal) {
        return PickupAttempt::failed;
    }
    if (!commit_pending(session)) return PickupAttempt::failed;

    const auto& committed_ground = arpg::test::ground_items(session);
    if (committed_ground[ordinal].active) return PickupAttempt::failed;
    for (const GroundItem& candidate : committed_ground) {
        if (candidate.active && candidate.item.id == original_item_id) {
            return PickupAttempt::failed;
        }
    }
    const auto& ownership = session.item_state();
    const auto owned = std::count_if(ownership.items.begin(),
        ownership.items.end(), [original_item_id](const ItemInstance& item) {
            return item.id == original_item_id;
        });
    const std::size_t word = ordinal / 64U;
    const std::size_t bit = ordinal % 64U;
    const bool claimed = word < ownership.claimed_drop_bits.size()
        && (ownership.claimed_drop_bits[word]
            & (std::uint64_t{1U} << bit)) != 0U;
    return owned == 1 && claimed
        ? PickupAttempt::picked : PickupAttempt::failed;
}

bool run_filter_hot_path_probe(TraceResult& result) noexcept {
    GroundItem ground{};
    ground.active = true;
    std::size_t eligible_count = 0U;
    constexpr std::size_t kIterationCount = 100000U;
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t iteration = 0U;
         iteration < kIterationCount; ++iteration) {
        ground.source = iteration % 11U == 0U
            ? arpg::dungeon::GroundItemSource::abyss_chest
            : arpg::dungeon::GroundItemSource::monster_drop;
        ground.item.rarity = static_cast<ItemRarity>(iteration % 3U);
        const AutoPickupPolicy policy{
            static_cast<ItemRarity>((iteration / 3U) % 3U)};
        if (arpg::dungeon::auto_pickup_eligible(ground, policy)) {
            ++eligible_count;
        }
    }
    result.hot_path_iterations = kIterationCount;
    return eligible_count > 0U
        && arpg::test::allocation_count() == before;
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
        if (left_result != right_result || left_result == RequestResult::faulted) {
            std::fprintf(stderr,
                "stage8 fixed transaction equip failed room=%zu left=%u right=%u id=%llu base=%u level=%u rarity=%u left_fault=%u right_fault=%u\n",
                room, static_cast<unsigned>(left_result),
                static_cast<unsigned>(right_result),
                static_cast<unsigned long long>(chosen.id),
                static_cast<unsigned>(chosen.base_id),
                static_cast<unsigned>(chosen.item_level),
                static_cast<unsigned>(chosen.rarity),
                static_cast<unsigned>(left.snapshot().diagnostics.fault),
                static_cast<unsigned>(right.snapshot().diagnostics.fault));
            return false;
        }
        if (left_result == RequestResult::accepted) {
            if (!commit_pending(left) || !commit_pending(right)) {
                std::fprintf(stderr,
                    "stage8 fixed transaction equip commit failed room=%zu\n",
                    room);
                return false;
            }
            ++equips;
        }
    }

    const auto recipe = first_recipe(left.item_state());
    if (recipe[0] == 0U) return true;
    const RequestResult left_result = left.request_recipe(recipe);
    const RequestResult right_result = right.request_recipe(recipe);
    if (left_result != RequestResult::accepted
            || right_result != RequestResult::accepted) {
        std::fprintf(stderr,
            "stage8 fixed transaction recipe failed room=%zu left=%u right=%u ids=%llu,%llu,%llu\n",
            room, static_cast<unsigned>(left_result),
            static_cast<unsigned>(right_result),
            static_cast<unsigned long long>(recipe[0]),
            static_cast<unsigned long long>(recipe[1]),
            static_cast<unsigned long long>(recipe[2]));
        return false;
    }
    if (!commit_pending(left) || !commit_pending(right)) {
        std::fprintf(stderr,
            "stage8 fixed transaction recipe commit failed room=%zu\n", room);
        return false;
    }
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
    if (left.snapshot().phase == RoomPhase::committing
            || right.snapshot().phase == RoomPhase::committing) {
        if (!commit_pending(left) || !commit_pending(right)) return false;
    }
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
    if (initial.fault != arpg::dungeon::DungeonFault::none) {
        result.abort_point = "initial_state";
        return result;
    }
    auto left = std::make_unique<DungeonSession>(rules, initial.state);
    auto right = std::make_unique<DungeonSession>(rules, initial.state);
    std::size_t step = 0U;
    if (!run_filter_hot_path_probe(result)) {
        result.abort_point = "filter_hot_path";
        return result;
    }
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
                    {
                        result.abort_point = "relay_defeated";
                        return result;
                    }
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
        std::array<std::uint16_t, arpg::dungeon::kGroundDropCapacity>
            retained_ordinals{};
        std::size_t retained_count = 0U;
        for (std::size_t index = 0U; index < left_order.size(); ++index) {
            const AutoPickupPolicy policy{perturb_pickup_order
                ? ItemRarity::normal
                : static_cast<ItemRarity>((room + index) % 3U)};
            const std::size_t policy_index = static_cast<std::size_t>(
                policy.minimum_rarity);
            if (!perturb_pickup_order) {
                if (policy_index >= result.policy_attempts.size()) return result;
                ++result.policy_attempts[policy_index];
            }
            const bool left_eligible = arpg::dungeon::auto_pickup_eligible(
                arpg::test::ground_items(*left)[left_order[index]], policy);
            const bool right_eligible = arpg::dungeon::auto_pickup_eligible(
                arpg::test::ground_items(*right)[right_order[index]], policy);
            if (left_eligible != right_eligible) {
                result.abort_point = "pickup_eligibility";
                return result;
            }
            const PickupAttempt left_attempt =
                pickup_one(*left, left_order[index], policy);
            const PickupAttempt right_attempt =
                pickup_one(*right, right_order[index], policy);
            const PickupAttempt expected = left_eligible
                ? PickupAttempt::picked : PickupAttempt::retained;
            if (left_attempt != expected || right_attempt != expected) {
                result.abort_point = "pickup_attempt";
                return result;
            }
            if (!left_eligible) {
                retained_ordinals[retained_count++] = left_order[index];
                ++result.filtered_items_retained;
                if (!perturb_pickup_order) {
                    ++result.policy_retained[policy_index];
                }
            } else if (!perturb_pickup_order) {
                ++result.exact_pickup_verifications;
            }
            if (!record_comparison(
                    result, *left, *right, room, step++)) {
                result.completed = perturb_pickup_order
                    && result.perturbation_applied;
                result.abort_point = "retained_pickup";
                return result;
            }
        }
        for (std::size_t index = 0U; index < retained_count; ++index) {
            const std::uint16_t ordinal = retained_ordinals[index];
            if (pickup_one(*left, ordinal, {}) != PickupAttempt::picked
                    || pickup_one(*right, ordinal, {})
                        != PickupAttempt::picked) {
                return result;
            }
            ++result.relaxed_policy_pickups;
            ++result.exact_pickup_verifications;
            if (!record_comparison(
                    result, *left, *right, room, step++)) {
                result.completed = perturb_pickup_order
                    && result.perturbation_applied;
                return result;
            }
        }

        if (!apply_fixed_transactions(
                *left, *right, room, result.equips, result.recipes)) {
            result.abort_point = "fixed_transactions";
            return result;
        }
        if (!record_comparison(result, *left, *right, room, step++))
            return result;
        if (!transition_room(*left, *right, room)) {
            result.abort_point = "transition_room";
            return result;
        }
        if (!record_comparison(result, *left, *right, room, step++))
            return result;

        if ((room + 1U) % kRestartInterval == 0U) {
            if (!restart_right_session(right, rules)) {
                result.abort_point = "restart_right_session";
                return result;
            }
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
    case MismatchField::materials: return "materials";
    case MismatchField::claimed_bits: return "claimed_bits";
    case MismatchField::next_sequence: return "next_sequence";
    case MismatchField::player_build: return "player_build";
    case MismatchField::ground_active: return "ground_active";
    case MismatchField::ground_ordinal: return "ground_ordinal";
    case MismatchField::ground_source: return "ground_source";
    case MismatchField::ground_abyss_reward_ordinal:
        return "ground_abyss_reward_ordinal";
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
            "stage8 stress mismatch room=%zu step=%zu field=%s index=%zu abort=%s\n",
            result.mismatch_room, result.mismatch_step,
            mismatch_name(result.mismatch.field), result.mismatch.index,
            result.abort_point);
    }
    ARPG_REQUIRE(result.completed);
    ARPG_REQUIRE(result.mismatch.field == MismatchField::none);
    ARPG_REQUIRE(result.drops > 0U);
    ARPG_REQUIRE(result.equips > 0U);
    ARPG_REQUIRE(result.recipes > 0U);
    ARPG_REQUIRE(result.restarts == kStressRoomCount / kRestartInterval);
    ARPG_REQUIRE(result.filtered_items_retained > 0U);
    ARPG_REQUIRE(result.relaxed_policy_pickups
        == result.filtered_items_retained);
    ARPG_REQUIRE(result.hot_path_iterations == 100000U);
    ARPG_REQUIRE(result.policy_attempts[0] > 0U);
    ARPG_REQUIRE(result.policy_attempts[1] > 0U);
    ARPG_REQUIRE(result.policy_attempts[2] > 0U);
    ARPG_REQUIRE(result.policy_retained[0] == 0U);
    ARPG_REQUIRE(result.policy_retained[1] > 0U);
    ARPG_REQUIRE(result.policy_retained[2] > 0U);
    ARPG_REQUIRE(result.policy_retained[0] + result.policy_retained[1]
        + result.policy_retained[2] == result.filtered_items_retained);
    ARPG_REQUIRE(result.exact_pickup_verifications > 0U);
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
