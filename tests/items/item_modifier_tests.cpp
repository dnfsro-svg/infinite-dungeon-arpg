#include "test_framework.hpp"

#include "items/item_catalog.hpp"
#include "items/item_modifiers.hpp"
#include "modifiers/player_modifier_values.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using namespace arpg::items;
using arpg::modifiers::Modifier;
using arpg::modifiers::ModifierOperation;
using arpg::modifiers::StatId;

ItemInstance normal_item(std::uint64_t id,
    std::uint8_t base_id,
    std::uint8_t level = 95U) noexcept {
    ItemInstance item{};
    item.id = id;
    item.base_id = base_id;
    item.rarity = ItemRarity::normal;
    item.item_level = level;
    item.required_level = 1U;
    return item;
}

const Modifier* find_modifier(const EquipmentProjection& projection,
    StatId stat,
    ModifierOperation operation) noexcept {
    for (std::size_t index = 0U; index < projection.modifier_count; ++index) {
        if (projection.modifiers[index].stat == stat
            && projection.modifiers[index].operation == operation)
            return &projection.modifiers[index];
    }
    return nullptr;
}

arpg::test::Failure weapon_local_values_use_frozen_floor_order() noexcept {
    ItemInstance weapon = normal_item(1U, 1U);
    weapon.rarity = ItemRarity::rare;
    weapon.required_level = 95U;
    weapon.affixes[0] = {1U, 1U, 0xFFU};
    weapon.affixes[1] = {2U, 1U, 0xFFU};
    weapon.affixes[2] = {101U, 1U, 0xFFU};
    weapon.affix_count = 3U;
    ARPG_REQUIRE(validate_item(weapon));
    ItemOwnershipState state{};
    state.items.push_back(weapon);
    state.equipment.equipped_ids[0] = weapon.id;
    const auto projection = project_equipment(state);
    ARPG_REQUIRE(projection.valid);
    ARPG_REQUIRE(projection.weapon_physical == 39);
    ARPG_REQUIRE(projection.local_attack_speed_bp == 1200);
    ARPG_REQUIRE(projection.modifier_count == 0U);
    return {};
}

arpg::test::Failure base_intrinsics_project_through_global_modifier_path() noexcept {
    ItemOwnershipState state{};
    for (std::uint8_t slot = 0U;
         slot < static_cast<std::uint8_t>(ItemSlot::count); ++slot) {
        state.items.push_back(normal_item(slot + 1U, slot + 1U));
        state.equipment.equipped_ids[slot] = slot + 1U;
    }
    const auto projection = project_equipment(state);
    ARPG_REQUIRE(projection.valid);
    ARPG_REQUIRE(projection.weapon_physical == 16);
    ARPG_REQUIRE(projection.local_attack_speed_bp == 0);
    ARPG_REQUIRE(projection.modifier_count == 8U);
    const auto values = arpg::modifiers::evaluate_player_modifiers(
        {projection.modifiers.data(), projection.modifier_count});
    ARPG_REQUIRE(values.valid);
    ARPG_REQUIRE(values.max_health == 38 * arpg::modifiers::kFixedOne);
    ARPG_REQUIRE(values.max_barrier == 46 * arpg::modifiers::kFixedOne);
    ARPG_REQUIRE(values.attack_speed == 10800);
    ARPG_REQUIRE(values.movement_speed == 10800);
    for (const std::int32_t reduction : values.damage_reduction)
        ARPG_REQUIRE(reduction == 1000);

    for (std::size_t index = 0U; index < projection.modifier_count; ++index) {
        const auto id = projection.modifiers[index].id;
        ARPG_REQUIRE(id != 0U);
        ARPG_REQUIRE((id & 0x80000000U) != 0U);
        ARPG_REQUIRE(id < 1000U || id > 1189U);
        for (std::size_t prior = 0U; prior < index; ++prior)
            ARPG_REQUIRE(projection.modifiers[prior].id != id);
    }
    return {};
}

arpg::test::Failure slot_dependent_and_variant_affixes_map_globally() noexcept {
    ItemInstance gloves = normal_item(11U, 4U);
    gloves.rarity = ItemRarity::magic;
    gloves.required_level = 95U;
    gloves.affixes[0] = {101U, 1U, 0xFFU};
    gloves.affix_count = 1U;
    ItemInstance accessory = normal_item(12U, 6U);
    accessory.rarity = ItemRarity::magic;
    accessory.required_level = 95U;
    accessory.affixes[0] = {112U, 1U, 2U};
    accessory.affix_count = 1U;
    ARPG_REQUIRE(validate_item(gloves));
    ARPG_REQUIRE(validate_item(accessory));
    ItemOwnershipState state{};
    state.items.push_back(gloves);
    state.items.push_back(accessory);
    state.equipment.equipped_ids[3] = gloves.id;
    state.equipment.equipped_ids[5] = accessory.id;
    const auto projection = project_equipment(state);
    ARPG_REQUIRE(projection.valid);
    ARPG_REQUIRE(projection.local_attack_speed_bp == 0);
    ARPG_REQUIRE(projection.modifier_count == 7U);
    const auto* speed = find_modifier(
        projection, StatId::attack_speed, ModifierOperation::increased);
    const auto* cap = find_modifier(projection,
        StatId::lightning_damage_reduction_cap, ModifierOperation::flat);
    ARPG_REQUIRE(speed != nullptr);
    ARPG_REQUIRE(cap != nullptr);
    const auto values = arpg::modifiers::evaluate_player_modifiers(
        {projection.modifiers.data(), projection.modifier_count});
    ARPG_REQUIRE(values.valid);
    ARPG_REQUIRE(values.attack_speed == 12000);
    ARPG_REQUIRE(values.damage_reduction_cap_bonus[2] == 600);
    return {};
}

arpg::test::Failure projection_validates_ownership_and_ignores_unequipped() noexcept {
    ItemOwnershipState state{};
    state.items.push_back(normal_item(21U, 2U));
    state.items.push_back(normal_item(22U, 3U));
    state.equipment.equipped_ids[1] = 21U;
    const auto projection = project_equipment(state);
    ARPG_REQUIRE(projection.valid);
    ARPG_REQUIRE(projection.modifier_count == 1U);
    ARPG_REQUIRE(find_modifier(projection,
        StatId::max_health, ModifierOperation::flat) != nullptr);
    ARPG_REQUIRE(find_modifier(projection,
        StatId::max_barrier, ModifierOperation::flat) == nullptr);

    state.equipment.equipped_ids[1] = 999U;
    const auto invalid = project_equipment(state);
    ARPG_REQUIRE(!invalid.valid);
    ARPG_REQUIRE(invalid.modifier_count == 0U);
    return {};
}

arpg::test::Failure detailed_projection_distinguishes_invalid_state() noexcept {
    ItemOwnershipState state{};
    state.items.push_back(normal_item(31U, 2U));
    state.equipment.equipped_ids[1] = 999U;
    const auto invalid = project_equipment_detailed(state);
    ARPG_REQUIRE(invalid.status == EquipmentProjectionStatus::invalid_state);
    ARPG_REQUIRE(!invalid.projection.valid);

    state.equipment.equipped_ids[1] = 31U;
    const auto valid = project_equipment_detailed(state);
    ARPG_REQUIRE(valid.status == EquipmentProjectionStatus::valid);
    ARPG_REQUIRE(valid.projection.valid);
    return {};
}

arpg::test::Failure equipment_override_projects_without_copying_ownership() noexcept {
    ItemOwnershipState state{};
    state.items.push_back(normal_item(41U, 1U));
    state.items.push_back(normal_item(42U, 2U));
    const ItemInstance* const original_data = state.items.data();
    EquipmentState override_equipment{};
    override_equipment.equipped_ids[0] = 41U;
    override_equipment.equipped_ids[1] = 42U;
    const auto projection = project_equipment_detailed(
        state, override_equipment);
    ARPG_REQUIRE(projection.status == EquipmentProjectionStatus::valid);
    ARPG_REQUIRE(projection.projection.valid);
    ARPG_REQUIRE(projection.projection.weapon_physical == 16);
    ARPG_REQUIRE(find_modifier(projection.projection,
        StatId::max_health, ModifierOperation::flat) != nullptr);
    ARPG_REQUIRE(state.items.data() == original_data);
    ARPG_REQUIRE(state.equipment.equipped_ids[0] == 0U);

    override_equipment.equipped_ids[2] = 42U;
    const auto duplicate = project_equipment_detailed(
        state, override_equipment);
    ARPG_REQUIRE(duplicate.status == EquipmentProjectionStatus::invalid_state);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"weapon local floor formula", &weapon_local_values_use_frozen_floor_order},
    {"base intrinsic projection",
        &base_intrinsics_project_through_global_modifier_path},
    {"slot-dependent and variant mapping",
        &slot_dependent_and_variant_affixes_map_globally},
    {"ownership validation and equipped-only projection",
        &projection_validates_ownership_and_ignores_unequipped},
    {"detailed projection status",
        &detailed_projection_distinguishes_invalid_state},
    {"equipment override projection",
        &equipment_override_projects_without_copying_ownership},
};

}  // namespace

arpg::test::TestSuite item_modifier_suite() noexcept {
    return arpg::test::make_suite("item_modifiers", kCases);
}
