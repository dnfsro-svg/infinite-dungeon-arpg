#include "test_framework.hpp"

#include "checkpoint/room_checkpoint_schema.hpp"
#include "checkpoint/room_checkpoint_validation.hpp"
#include "checkpoint/room_progress_checkpoint.hpp"

#include "items/material_catalog.hpp"
#include "skills/active_skill_types.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>

namespace {

namespace checkpoint = arpg::checkpoint;

void make_valid_monster(checkpoint::MonsterCombatCheckpoint& monster,
    const std::uint16_t ordinal) noexcept {
    monster = {};
    monster.ordinal = ordinal;
    monster.id = checkpoint::MonsterId::fire_bomber;
    monster.hp = 1;
    monster.max_hp = 1;
}

arpg::items::ItemInstance normal_item(const std::uint64_t id) noexcept {
    arpg::items::ItemInstance item{};
    item.id = id;
    item.base_id = 1U;
    item.rarity = arpg::items::ItemRarity::normal;
    item.item_level = 1U;
    item.required_level = 1U;
    return item;
}

static_assert(!std::is_copy_constructible_v<
    checkpoint::RoomCombatCheckpoint>);
static_assert(!std::is_copy_assignable_v<
    checkpoint::RoomCombatCheckpoint>);
static_assert(!std::is_move_constructible_v<
    checkpoint::RoomCombatCheckpoint>);
static_assert(!std::is_move_assignable_v<
    checkpoint::RoomCombatCheckpoint>);
static_assert(!std::is_copy_constructible_v<
    checkpoint::RoomProgressCheckpoint>);
static_assert(!std::is_copy_assignable_v<
    checkpoint::RoomProgressCheckpoint>);
static_assert(!std::is_move_constructible_v<
    checkpoint::RoomProgressCheckpoint>);
static_assert(!std::is_move_assignable_v<
    checkpoint::RoomProgressCheckpoint>);
static_assert(!std::is_copy_constructible_v<
    checkpoint::SaveCheckpointSlot>);
static_assert(!std::is_copy_assignable_v<
    checkpoint::SaveCheckpointSlot>);
static_assert(!std::is_move_constructible_v<
    checkpoint::SaveCheckpointSlot>);
static_assert(!std::is_move_assignable_v<
    checkpoint::SaveCheckpointSlot>);
static_assert(checkpoint::kRoomEquipmentGroundCapacity == 1152U);
static_assert(checkpoint::kRoomSecondaryGroundCapacity == 2320U);
static_assert(checkpoint::kOrdinarySecondaryOrdinalEnd == 768U);
static_assert(checkpoint::kAbyssSecondaryOrdinalBegin == 2304U);
static_assert(checkpoint::kHealthPotionGroundCapacity == 192U);

arpg::test::Failure clear_functions_restore_canonical_state_and_reuse_items()
    noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> slot{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(slot != nullptr);

    auto& items = slot->state.item_ownership.items;
    items.reserve(8U);
    const auto* const item_data = items.data();
    const std::size_t item_capacity = items.capacity();
    ARPG_REQUIRE(item_capacity >= 8U);
    items.push_back({});

    auto& combat = slot->room_progress.combat;
    combat.tick = 91U;
    combat.evasion_rng_state[0U] = 17U;
    combat.player.facing = checkpoint::Facing::left;
    combat.player.air_attack_available = false;
    combat.attack.id = checkpoint::AttackId::j2;
    combat.monster_count = 2U;
    combat.monsters.back().ordinal = 4U;
    combat.monsters.back().id = checkpoint::MonsterId::fire_bomber;
    combat.obstacle_count = 1U;
    combat.obstacles.back().ordinal = 9U;
    combat.fire_crate_count = 1U;
    combat.player_damage_history.initialized = true;
    combat.player_damage_history.buckets.back().back() = 23U;
    combat.has_death_snapshot = true;

    checkpoint::clear_room_combat_checkpoint(combat);
    ARPG_REQUIRE(combat.tick == 0U);
    for (const std::uint64_t word : combat.evasion_rng_state) {
        ARPG_REQUIRE(word == 0U);
    }
    ARPG_REQUIRE(combat.player.facing == checkpoint::Facing::right);
    ARPG_REQUIRE(combat.player.state == checkpoint::PlayerState::idle);
    ARPG_REQUIRE(combat.player.air_attack_available);
    ARPG_REQUIRE(combat.attack.id == checkpoint::AttackId::none);
    ARPG_REQUIRE(combat.monster_count == 0U);
    ARPG_REQUIRE(combat.monsters.back().ordinal
        == checkpoint::kInvalidMonsterOrdinal);
    ARPG_REQUIRE(combat.monsters.back().id
        == checkpoint::MonsterId::count);
    ARPG_REQUIRE(combat.obstacle_count == 0U);
    ARPG_REQUIRE(combat.obstacles.back().ordinal == 0xFFFFU);
    ARPG_REQUIRE(combat.fire_crate_count == 0U);
    ARPG_REQUIRE(!combat.player_damage_history.initialized);
    ARPG_REQUIRE(combat.player_damage_history.buckets.back().back() == 0U);
    ARPG_REQUIRE(!combat.has_death_snapshot);

    auto& room = slot->room_progress;
    room.lifecycle = checkpoint::RoomProgressLifecycle::active;
    room.room_index = 31U;
    room.room_seed = 37U;
    room.defeat_bits.back() = 1U;
    room.equipment_claim_bits.back() = 2U;
    room.secondary_claim_bits.back() = 4U;
    room.equipment_ground_count = 1U;
    room.equipment_ground.back().ordinal = 6U;
    room.equipment_ground.back().reward_ordinal = 2U;
    room.secondary_ground_count = 1U;
    room.secondary_ground.back().ordinal = 7U;
    room.secondary_ground.back().material =
        arpg::items::MaterialId::reinforcement_stone;
    room.combat.player.air_attack_available = false;

    checkpoint::clear_room_progress_checkpoint(room);
    ARPG_REQUIRE(room.lifecycle == checkpoint::RoomProgressLifecycle::none);
    ARPG_REQUIRE(room.room_index == 0U);
    ARPG_REQUIRE(room.room_seed == 0U);
    ARPG_REQUIRE(room.generated_monsters == 0U);
    ARPG_REQUIRE(room.defeated_monsters == 0U);
    ARPG_REQUIRE(room.required_kills == 0U);
    ARPG_REQUIRE(!room.exits_unlocked);
    ARPG_REQUIRE(!room.full_clear);
    ARPG_REQUIRE(!room.reward_committed);
    ARPG_REQUIRE(room.defeat_bits.back() == 0U);
    ARPG_REQUIRE(room.equipment_claim_bits.back() == 0U);
    ARPG_REQUIRE(room.secondary_claim_bits.back() == 0U);
    ARPG_REQUIRE(room.equipment_ground_count == 0U);
    ARPG_REQUIRE(room.equipment_ground.back().ordinal == 0xFFFFU);
    ARPG_REQUIRE(room.equipment_ground.back().reward_ordinal == 0xFFU);
    ARPG_REQUIRE(room.secondary_ground_count == 0U);
    ARPG_REQUIRE(room.secondary_ground.back().ordinal == 0xFFFFU);
    ARPG_REQUIRE(room.secondary_ground.back().material
        == arpg::items::MaterialId::count);
    ARPG_REQUIRE(room.combat.player.air_attack_available);

    slot->state.root_seed = 41U;
    slot->state.commit_generation = 43U;
    slot->state.current_room.depth = 47U;
    slot->state.current_room.floor_room_index = 53U;
    slot->state.last_transition = checkpoint::TransitionKind::door;
    slot->state.last_direction = checkpoint::ExitDirection::left;
    slot->state.item_ownership.next_item_sequence = 59U;
    slot->state.skill_loadout = {};
    slot->persistence_revision = 61U;

    checkpoint::clear_save_checkpoint_slot(*slot);
    ARPG_REQUIRE(slot->state.root_seed == 0U);
    ARPG_REQUIRE(slot->state.commit_generation == 1U);
    ARPG_REQUIRE(slot->state.current_room.depth == 1U);
    ARPG_REQUIRE(slot->state.current_room.floor_room_index == 1U);
    ARPG_REQUIRE(slot->state.last_transition
        == checkpoint::TransitionKind::none);
    ARPG_REQUIRE(slot->state.last_direction
        == checkpoint::ExitDirection::none);
    ARPG_REQUIRE(slot->state.item_ownership.items.empty());
    ARPG_REQUIRE(slot->state.item_ownership.items.data() == item_data);
    ARPG_REQUIRE(slot->state.item_ownership.items.capacity() == item_capacity);
    ARPG_REQUIRE(slot->state.item_ownership.next_item_sequence == 1U);
    ARPG_REQUIRE(slot->persistence_revision == 0U);
    ARPG_REQUIRE(slot->room_progress.lifecycle
        == checkpoint::RoomProgressLifecycle::none);

    const auto& loadout = slot->state.skill_loadout;
    ARPG_REQUIRE(loadout.owned_active_bits == 0x03U);
    ARPG_REQUIRE(loadout.slots[0U].active
        == arpg::skills::ActiveSkillId::draw_slash);
    ARPG_REQUIRE(loadout.slots[1U].active
        == arpg::skills::ActiveSkillId::storm_swords);
    for (std::size_t index = 2U; index < loadout.slots.size(); ++index) {
        ARPG_REQUIRE(loadout.slots[index].active
            == arpg::skills::ActiveSkillId::none);
    }
    for (const auto& active : loadout.slots) {
        for (const auto support : active.supports) {
            ARPG_REQUIRE(support == arpg::skills::SupportSkillId::none);
        }
    }
    return {};
}

arpg::test::Failure type_and_capacity_contracts_are_exact() noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> slot{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(slot != nullptr);

    ARPG_REQUIRE(slot->room_progress.combat.monsters.size() == 1152U);
    ARPG_REQUIRE(slot->room_progress.combat.obstacles.size() == 400U);
    ARPG_REQUIRE(slot->room_progress.combat.fire_crates.size() == 2U);
    ARPG_REQUIRE(slot->room_progress.defeat_bits.size() == 18U);
    ARPG_REQUIRE(slot->room_progress.equipment_claim_bits.size() == 18U);
    ARPG_REQUIRE(slot->room_progress.secondary_claim_bits.size() == 37U);
    return {};
}

arpg::test::Failure required_kills_covers_rounding_edges() noexcept {
    ARPG_REQUIRE(checkpoint::required_kills(0U) == 0U);
    ARPG_REQUIRE(checkpoint::required_kills(1U) == 1U);
    ARPG_REQUIRE(checkpoint::required_kills(3U) == 1U);
    ARPG_REQUIRE(checkpoint::required_kills(4U) == 1U);
    ARPG_REQUIRE(checkpoint::required_kills(5U) == 2U);
    ARPG_REQUIRE(checkpoint::required_kills(1151U) == 288U);
    ARPG_REQUIRE(checkpoint::required_kills(1152U) == 288U);
    return {};
}

arpg::test::Failure secondary_ordinal_mappings_preserve_wire_ranges()
    noexcept {
    ARPG_REQUIRE(checkpoint::checkpoint_material_ordinal(0U) == 0U);
    ARPG_REQUIRE(checkpoint::checkpoint_material_ordinal(1U) == 2U);
    ARPG_REQUIRE(checkpoint::checkpoint_material_ordinal(383U) == 766U);
    ARPG_REQUIRE(checkpoint::checkpoint_material_ordinal(384U) == 2304U);
    ARPG_REQUIRE(checkpoint::checkpoint_material_ordinal(399U) == 2319U);

    ARPG_REQUIRE(checkpoint::material_ordinal_from_checkpoint(0U) == 0U);
    ARPG_REQUIRE(checkpoint::material_ordinal_from_checkpoint(1U) == 0U);
    ARPG_REQUIRE(checkpoint::material_ordinal_from_checkpoint(766U) == 383U);
    ARPG_REQUIRE(checkpoint::material_ordinal_from_checkpoint(767U) == 383U);
    ARPG_REQUIRE(checkpoint::material_ordinal_from_checkpoint(768U)
        == 0xFFFFU);
    ARPG_REQUIRE(checkpoint::material_ordinal_from_checkpoint(2303U)
        == 0xFFFFU);
    ARPG_REQUIRE(checkpoint::material_ordinal_from_checkpoint(2304U) == 384U);
    ARPG_REQUIRE(checkpoint::material_ordinal_from_checkpoint(2319U) == 399U);

    ARPG_REQUIRE(checkpoint::health_potion_claim_ordinal(0U) == 1U);
    ARPG_REQUIRE(checkpoint::health_potion_claim_ordinal(1U) == 3U);
    ARPG_REQUIRE(checkpoint::health_potion_claim_ordinal(383U) == 767U);
    ARPG_REQUIRE(checkpoint::health_potion_claim_ordinal(1151U) == 2303U);
    return {};
}

arpg::test::Failure equality_ignores_inactive_array_tails() noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> left{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    std::unique_ptr<checkpoint::SaveCheckpointSlot> right{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(left != nullptr);
    ARPG_REQUIRE(right != nullptr);
    checkpoint::clear_save_checkpoint_slot(*left);
    checkpoint::clear_save_checkpoint_slot(*right);

    left->room_progress.combat.monsters.back().hp = 71;
    left->room_progress.combat.obstacles.back().hp = 73U;
    left->room_progress.combat.fire_crates.back().position.x = 79.0F;
    left->room_progress.combat.death_snapshot.final_damage = 83U;
    ARPG_REQUIRE(checkpoint::same_room_combat_checkpoint(
        left->room_progress.combat, right->room_progress.combat));

    left->room_progress.equipment_ground.back().item.id = 89U;
    left->room_progress.secondary_ground.back().ordinal = 97U;
    ARPG_REQUIRE(checkpoint::same_room_progress_checkpoint(
        left->room_progress, right->room_progress));

    left->room_progress.combat.monster_count = 1U;
    right->room_progress.combat.monster_count = 1U;
    make_valid_monster(left->room_progress.combat.monsters[0U], 0U);
    make_valid_monster(right->room_progress.combat.monsters[0U], 0U);
    ARPG_REQUIRE(checkpoint::same_room_combat_checkpoint(
        left->room_progress.combat, right->room_progress.combat));
    left->room_progress.combat.monsters[0U].hp = 2;
    ARPG_REQUIRE(!checkpoint::same_room_combat_checkpoint(
        left->room_progress.combat, right->room_progress.combat));
    left->room_progress.combat.monster_count = 0U;
    right->room_progress.combat.monster_count = 0U;

    left->room_progress.equipment_ground_count = 1U;
    right->room_progress.equipment_ground_count = 1U;
    left->room_progress.equipment_ground[0U].ordinal = 0U;
    right->room_progress.equipment_ground[0U].ordinal = 0U;
    left->room_progress.equipment_ground[0U].item = normal_item(103U);
    right->room_progress.equipment_ground[0U].item = normal_item(103U);
    ARPG_REQUIRE(checkpoint::same_room_progress_checkpoint(
        left->room_progress, right->room_progress));
    right->room_progress.equipment_ground[0U].item.id = 107U;
    ARPG_REQUIRE(!checkpoint::same_room_progress_checkpoint(
        left->room_progress, right->room_progress));
    left->room_progress.equipment_ground_count = 0U;
    right->room_progress.equipment_ground_count = 0U;

    left->room_progress.combat.monster_count = static_cast<std::uint16_t>(
        left->room_progress.combat.monsters.size() + 1U);
    right->room_progress.combat.monster_count =
        left->room_progress.combat.monster_count;
    ARPG_REQUIRE(!checkpoint::same_room_combat_checkpoint(
        left->room_progress.combat, right->room_progress.combat));
    left->room_progress.combat.monster_count = 0U;
    right->room_progress.combat.monster_count = 0U;

    left->room_progress.equipment_ground_count =
        static_cast<std::uint16_t>(
            left->room_progress.equipment_ground.size() + 1U);
    right->room_progress.equipment_ground_count =
        left->room_progress.equipment_ground_count;
    ARPG_REQUIRE(!checkpoint::same_room_progress_checkpoint(
        left->room_progress, right->room_progress));
    left->room_progress.equipment_ground_count = 0U;
    right->room_progress.equipment_ground_count = 0U;

    right->room_progress.room_seed = 101U;
    ARPG_REQUIRE(!checkpoint::same_room_progress_checkpoint(
        left->room_progress, right->room_progress));
    return {};
}

void make_minimal_active(checkpoint::SaveCheckpointSlot& slot) noexcept {
    checkpoint::clear_save_checkpoint_slot(slot);
    slot.state.root_seed = 103U;
    slot.state.current_room.index = 107U;
    slot.state.current_room.seed = 109U;

    auto& room = slot.room_progress;
    room.lifecycle = checkpoint::RoomProgressLifecycle::active;
    room.room_index = slot.state.current_room.index;
    room.room_seed = slot.state.current_room.seed;
    room.monster_generator_version = 1U;
    room.monster_blueprint_hash = 113U;
    room.environment_generator_version = 1U;
    room.environment_blueprint_hash = 127U;
    room.generated_monsters = 1U;
    room.required_kills = 1U;
    room.combat.evasion_rng_state[0U] = 131U;
    room.combat.player.hp = 100;
    room.combat.player.max_hp = 100;
    room.combat.abyss_environment.expansion_stage = 0xFFU;
}

arpg::test::Failure structural_validation_rejects_bad_wire_authority()
    noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> slot{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(slot != nullptr);
    make_minimal_active(*slot);
    auto& room = slot->room_progress;
    auto& combat = room.combat;

    ARPG_REQUIRE(checkpoint::valid_room_combat_checkpoint_structural(
        combat, room.generated_monsters));
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        room, slot->state));

    combat.player.facing = static_cast<checkpoint::Facing>(0);
    ARPG_REQUIRE(!checkpoint::valid_room_combat_checkpoint_structural(
        combat, room.generated_monsters));
    combat.player.facing = checkpoint::Facing::right;

    combat.monster_count = static_cast<std::uint16_t>(
        combat.monsters.size() + 1U);
    ARPG_REQUIRE(!checkpoint::valid_room_combat_checkpoint_structural(
        combat, room.generated_monsters));
    combat.monster_count = 0U;

    combat.evasion_rng_state = {};
    ARPG_REQUIRE(!checkpoint::valid_room_combat_checkpoint_structural(
        combat, room.generated_monsters));
    combat.evasion_rng_state[0U] = 131U;

    ++slot->state.current_room.seed;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        room, slot->state));
    --slot->state.current_room.seed;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        room, slot->state));
    return {};
}

arpg::test::Failure maximum_population_and_ground_records_validate()
    noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> slot{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(slot != nullptr);
    make_minimal_active(*slot);
    auto& room = slot->room_progress;
    auto& combat = room.combat;

    room.generated_monsters = static_cast<std::uint32_t>(
        checkpoint::kRoomEquipmentGroundCapacity);
    room.required_kills = checkpoint::required_kills(
        room.generated_monsters);
    combat.monster_count = static_cast<std::uint16_t>(
        combat.monsters.size());
    for (std::uint16_t ordinal = 0U; ordinal < combat.monster_count;
            ++ordinal) {
        make_valid_monster(combat.monsters[ordinal], ordinal);
    }
    ARPG_REQUIRE(checkpoint::valid_room_combat_checkpoint_structural(
        combat, room.generated_monsters));
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        room, slot->state));

    combat.monster_count = 0U;
    room.generated_monsters = 1U;
    room.required_kills = 1U;
    room.equipment_ground_count = 1U;
    room.equipment_ground[0U].ordinal = 0U;
    room.equipment_ground[0U].source = 0U;
    room.equipment_ground[0U].reward_ordinal = 0xFFU;
    room.equipment_ground[0U].item = normal_item(137U);
    room.secondary_ground_count = 2U;
    room.secondary_ground[0U].tag = checkpoint::SecondaryGroundTag::material;
    room.secondary_ground[0U].ordinal = 0U;
    room.secondary_ground[0U].source = 0U;
    room.secondary_ground[0U].material =
        arpg::items::MaterialId::reinforcement_stone;
    room.secondary_ground[1U].tag =
        checkpoint::SecondaryGroundTag::health_potion;
    room.secondary_ground[1U].ordinal = 1U;
    room.secondary_ground[1U].source = 0U;
    room.secondary_ground[1U].material = arpg::items::MaterialId::count;
    ARPG_REQUIRE(checkpoint::valid_room_progress_checkpoint_structural(
        room, slot->state));

    room.secondary_claim_bits[0U] = 1U << 1U;
    ARPG_REQUIRE(!checkpoint::valid_room_progress_checkpoint_structural(
        room, slot->state));
    return {};
}

arpg::test::Failure death_history_effect_and_timing_rules_validate()
    noexcept {
    std::unique_ptr<checkpoint::SaveCheckpointSlot> slot{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    ARPG_REQUIRE(slot != nullptr);
    make_minimal_active(*slot);
    auto& combat = slot->room_progress.combat;

    combat.player.hp = 0;
    combat.has_death_snapshot = true;
    combat.player_damage_history.initialized = true;
    combat.player_damage_history.active_tick = 0U;
    combat.player_damage_history.buckets[0U][0U] = 10U;
    combat.death_snapshot.tick = 0U;
    combat.death_snapshot.raw_damage = 10U;
    combat.death_snapshot.health_loss = 10U;
    combat.death_snapshot.final_damage = 10U;
    combat.death_snapshot.recent_damage[0U] = 10U;
    combat.death_snapshot.defense.hp = 0;
    combat.death_snapshot.defense.max_hp = combat.player.max_hp;
    ARPG_REQUIRE(checkpoint::valid_room_combat_checkpoint_structural(
        combat, 1U));

    combat.player_damage_history.buckets[0U][1U] =
        (std::numeric_limits<std::uint64_t>::max)();
    combat.player_damage_history.buckets[1U][1U] = 1U;
    ARPG_REQUIRE(!checkpoint::valid_room_combat_checkpoint_structural(
        combat, 1U));

    make_minimal_active(*slot);
    auto& living_combat = slot->room_progress.combat;
    living_combat.monster_count = 1U;
    make_valid_monster(living_combat.monsters[0U], 0U);
    ARPG_REQUIRE(checkpoint::valid_room_combat_checkpoint_structural(
        living_combat, 1U));

    living_combat.monsters[0U].effects.command_count = 17U;
    ARPG_REQUIRE(!checkpoint::valid_room_combat_checkpoint_structural(
        living_combat, 1U));
    living_combat.monsters[0U].effects = {};
    living_combat.monsters[0U].affixes.count = 1U;
    living_combat.monsters[0U].affixes.values[0U] = {
        checkpoint::MonsterAffixId::burning_ground,
        checkpoint::MonsterAffixTier::m1};
    living_combat.monsters[0U].burning_ground_ticks = 180U;
    ARPG_REQUIRE(!checkpoint::valid_room_combat_checkpoint_structural(
        living_combat, 1U));
    living_combat.monsters[0U].burning_ground_ticks = 179U;
    ARPG_REQUIRE(checkpoint::valid_room_combat_checkpoint_structural(
        living_combat, 1U));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"clear restores canonical state and reuses items",
        &clear_functions_restore_canonical_state_and_reuse_items},
    {"type and capacity contracts are exact",
        &type_and_capacity_contracts_are_exact},
    {"required kills covers rounding edges",
        &required_kills_covers_rounding_edges},
    {"secondary ordinal mappings preserve wire ranges",
        &secondary_ordinal_mappings_preserve_wire_ranges},
    {"equality ignores inactive array tails",
        &equality_ignores_inactive_array_tails},
    {"structural validation rejects bad wire authority",
        &structural_validation_rejects_bad_wire_authority},
    {"maximum population and ground records validate",
        &maximum_population_and_ground_records_validate},
    {"death history effect and timing rules validate",
        &death_history_effect_and_timing_rules_validate},
};

}  // namespace

arpg::test::TestSuite checkpoint_schema_suite() noexcept {
    return arpg::test::make_suite("checkpoint_schema", kCases);
}
