#include "test_framework.hpp"

#include "items/item_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

using namespace arpg::items;
using arpg::modifiers::ModifierOperation;
using arpg::modifiers::StatId;

static_assert(std::is_same_v<
    std::underlying_type_t<ItemSlot>, std::uint8_t>);
static_assert(std::is_same_v<
    std::underlying_type_t<ItemRarity>, std::uint8_t>);
static_assert(std::is_same_v<
    std::underlying_type_t<AffixKind>, std::uint8_t>);
static_assert(std::is_same_v<decltype(AffixRoll::affix_id), std::uint16_t>);
static_assert(std::is_same_v<decltype(ItemInstance::id), std::uint64_t>);
static_assert(std::is_same_v<
    decltype(ItemInstance::affixes), std::array<AffixRoll, 6>>);
static_assert(std::is_same_v<
    decltype(ItemInstance::reserved), std::array<std::uint8_t, 3>>);
static_assert(std::is_standard_layout_v<ItemInstance>);
static_assert(std::is_trivially_copyable_v<ItemInstance>);
static_assert(sizeof(ItemInstance) == 40U);
static_assert(offsetof(ItemInstance, id) == 0U);
static_assert(offsetof(ItemInstance, affixes) == 12U);
static_assert(offsetof(ItemInstance, affix_count) == 36U);
static_assert(offsetof(ItemInstance, reserved) == 37U);
static_assert(std::is_same_v<
    decltype(EquipmentState::equipped_ids), std::array<std::uint64_t, 6>>);
static_assert(std::is_same_v<
    decltype(ItemOwnershipState::claimed_drop_bits),
    std::array<std::uint64_t, 3>>);
static_assert(static_cast<std::uint8_t>(ItemSlot::weapon) == 0U);
static_assert(static_cast<std::uint8_t>(ItemSlot::accessory) == 5U);
static_assert(static_cast<std::uint8_t>(ItemSlot::count) == 6U);

constexpr std::uint8_t slots(std::initializer_list<ItemSlot> values) noexcept {
    std::uint8_t result = 0U;
    for (const ItemSlot slot : values)
        result = static_cast<std::uint8_t>(result | slot_bit(slot));
    return result;
}

struct ExpectedBase final {
    std::uint8_t id;
    std::string_view name;
    ItemSlot slot;
    ItemEffectKind effect;
    StatId stat;
    ModifierOperation operation;
    std::array<std::int32_t, 8> values;
};

struct ExpectedAffix final {
    std::uint16_t id;
    AffixKind kind;
    std::uint8_t slot_mask;
    ItemEffectKind effect;
    StatId stat;
    ModifierOperation operation;
    std::array<std::int32_t, 8> values;
};

constexpr std::array<std::int32_t, 8> kElementFlat{{1, 2, 3, 4, 5, 6, 8, 10}};
constexpr std::array<std::int32_t, 8> kDamageIncreased{{500, 800, 1200,
    1600, 2100, 2700, 3400, 4200}};
constexpr std::array<std::int32_t, 8> kRating{{25, 60, 150, 400, 1000,
    3000, 10000, 30000}};
constexpr std::array<std::int32_t, 8> kReduction{{300, 500, 700, 900,
    1100, 1400, 1700, 2000}};

constexpr std::array<ExpectedBase, 6> kExpectedBases{{
    {1U, "Iron Blade", ItemSlot::weapon,
        ItemEffectKind::local_weapon_physical_flat, StatId::count,
        ModifierOperation::flat, {{2, 3, 4, 5, 7, 9, 12, 16}}},
    {2U, "Guard Helm", ItemSlot::helmet, ItemEffectKind::global_modifier,
        StatId::max_health, ModifierOperation::flat,
        {{3, 5, 8, 12, 17, 23, 30, 38}}},
    {3U, "Ward Coat", ItemSlot::chest, ItemEffectKind::global_modifier,
        StatId::max_barrier, ModifierOperation::flat,
        {{4, 7, 11, 16, 22, 29, 37, 46}}},
    {4U, "Striker Gloves", ItemSlot::gloves,
        ItemEffectKind::global_modifier, StatId::attack_speed,
        ModifierOperation::increased, {{100, 200, 300, 400, 500, 600, 700, 800}}},
    {5U, "Runner Boots", ItemSlot::boots, ItemEffectKind::global_modifier,
        StatId::move_speed, ModifierOperation::increased,
        {{100, 200, 300, 400, 500, 600, 700, 800}}},
    {6U, "Element Charm", ItemSlot::accessory,
        ItemEffectKind::all_element_damage_reduction, StatId::count,
        ModifierOperation::flat, {{100, 200, 300, 400, 500, 600, 800, 1000}}},
}};

const std::array<ExpectedAffix, 24> kExpectedAffixes{{
    {1U, AffixKind::prefix, slots({ItemSlot::weapon}),
        ItemEffectKind::local_weapon_physical_flat, StatId::count,
        ModifierOperation::flat, {{1, 2, 3, 4, 5, 7, 9, 12}}},
    {2U, AffixKind::prefix, slots({ItemSlot::weapon}),
        ItemEffectKind::local_weapon_physical_increased, StatId::count,
        ModifierOperation::flat, kDamageIncreased},
    {3U, AffixKind::prefix, slots({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::fire_flat_damage, ModifierOperation::flat, kElementFlat},
    {4U, AffixKind::prefix, slots({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::water_flat_damage, ModifierOperation::flat, kElementFlat},
    {5U, AffixKind::prefix, slots({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::lightning_flat_damage, ModifierOperation::flat, kElementFlat},
    {6U, AffixKind::prefix, slots({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::chaos_flat_damage, ModifierOperation::flat, kElementFlat},
    {7U, AffixKind::prefix, slots({ItemSlot::weapon, ItemSlot::helmet,
        ItemSlot::chest, ItemSlot::gloves, ItemSlot::boots, ItemSlot::accessory}),
        ItemEffectKind::global_modifier, StatId::fire_damage,
        ModifierOperation::increased, kDamageIncreased},
    {8U, AffixKind::prefix, slots({ItemSlot::weapon, ItemSlot::helmet,
        ItemSlot::chest, ItemSlot::gloves, ItemSlot::boots, ItemSlot::accessory}),
        ItemEffectKind::global_modifier, StatId::water_damage,
        ModifierOperation::increased, kDamageIncreased},
    {9U, AffixKind::prefix, slots({ItemSlot::weapon, ItemSlot::helmet,
        ItemSlot::chest, ItemSlot::gloves, ItemSlot::boots, ItemSlot::accessory}),
        ItemEffectKind::global_modifier, StatId::lightning_damage,
        ModifierOperation::increased, kDamageIncreased},
    {10U, AffixKind::prefix, slots({ItemSlot::weapon, ItemSlot::helmet,
        ItemSlot::chest, ItemSlot::gloves, ItemSlot::boots, ItemSlot::accessory}),
        ItemEffectKind::global_modifier, StatId::chaos_damage,
        ModifierOperation::increased, kDamageIncreased},
    {11U, AffixKind::prefix, slots({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::gloves, ItemSlot::boots, ItemSlot::accessory}),
        ItemEffectKind::global_modifier, StatId::max_health,
        ModifierOperation::flat, {{4, 7, 11, 16, 22, 29, 37, 46}}},
    {12U, AffixKind::prefix, slots({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::gloves, ItemSlot::boots, ItemSlot::accessory}),
        ItemEffectKind::global_modifier, StatId::max_barrier,
        ModifierOperation::flat, {{3, 5, 8, 12, 17, 23, 30, 38}}},
    {101U, AffixKind::suffix, slots({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::slot_dependent_attack_speed,
        StatId::count, ModifierOperation::flat,
        {{200, 300, 400, 500, 600, 800, 1000, 1200}}},
    {102U, AffixKind::suffix, slots({ItemSlot::boots, ItemSlot::accessory}),
        ItemEffectKind::global_modifier, StatId::move_speed,
        ModifierOperation::increased,
        {{200, 300, 400, 500, 600, 700, 800, 1000}}},
    {103U, AffixKind::suffix, slots({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::gloves, ItemSlot::boots, ItemSlot::accessory}),
        ItemEffectKind::global_modifier, StatId::evasion,
        ModifierOperation::flat, kRating},
    {104U, AffixKind::suffix, slots({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::gloves, ItemSlot::boots}), ItemEffectKind::global_modifier,
        StatId::armor, ModifierOperation::flat, kRating},
    {105U, AffixKind::suffix, slots({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::impulse_scale, ModifierOperation::increased,
        {{400, 600, 800, 1000, 1300, 1600, 2000, 2500}}},
    {106U, AffixKind::suffix, slots({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::melee_damage, ModifierOperation::increased,
        {{400, 600, 900, 1200, 1500, 1900, 2400, 3000}}},
    {107U, AffixKind::suffix, slots({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::boots, ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::fire_damage_reduction, ModifierOperation::flat, kReduction},
    {108U, AffixKind::suffix, slots({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::boots, ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::water_damage_reduction, ModifierOperation::flat, kReduction},
    {109U, AffixKind::suffix, slots({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::boots, ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::lightning_damage_reduction, ModifierOperation::flat, kReduction},
    {110U, AffixKind::suffix, slots({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::boots, ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::chaos_damage_reduction, ModifierOperation::flat, kReduction},
    {111U, AffixKind::suffix, slots({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::max_health, ModifierOperation::increased,
        {{300, 500, 700, 900, 1200, 1500, 1900, 2400}}},
    {112U, AffixKind::suffix, slots({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::accessory}),
        ItemEffectKind::variant_element_damage_reduction_cap, StatId::count,
        ModifierOperation::flat, {{100, 100, 200, 200, 300, 400, 500, 600}}},
}};

bool base_matches(const BaseDefinition& actual,
    const ExpectedBase& expected) noexcept {
    return actual.id == expected.id && actual.name == expected.name
        && actual.slot == expected.slot && actual.effect == expected.effect
        && actual.stat == expected.stat && actual.operation == expected.operation
        && actual.values == expected.values;
}

bool affix_matches(const AffixDefinition& actual,
    const ExpectedAffix& expected) noexcept {
    return actual.id == expected.id && actual.group_id == expected.id
        && actual.kind == expected.kind && actual.slot_mask == expected.slot_mask
        && actual.effect == expected.effect && actual.stat == expected.stat
        && actual.operation == expected.operation && actual.values == expected.values;
}

ItemInstance normal_item(std::uint64_t id, std::uint8_t base_id) noexcept {
    ItemInstance item{};
    item.id = id;
    item.base_id = base_id;
    item.rarity = ItemRarity::normal;
    item.item_level = 1U;
    item.required_level = 1U;
    return item;
}

void set_roll(ItemInstance& item, std::size_t index, std::uint16_t id,
    std::uint8_t tier, std::uint8_t variant = 0xFFU) noexcept {
    item.affixes[index] = {id, tier, variant};
    item.affix_count = static_cast<std::uint8_t>(index + 1U);
}

ItemInstance valid_magic_weapon() noexcept {
    ItemInstance item = normal_item(1U, 1U);
    item.rarity = ItemRarity::magic;
    item.item_level = 95U;
    item.required_level = 95U;
    set_roll(item, 0U, 1U, 1U);
    return item;
}

ItemInstance valid_rare_accessory() noexcept {
    ItemInstance item = normal_item(1U, 6U);
    item.rarity = ItemRarity::rare;
    item.item_level = 95U;
    item.required_level = 95U;
    set_roll(item, 0U, 3U, 8U);
    set_roll(item, 1U, 7U, 7U);
    set_roll(item, 2U, 11U, 6U);
    set_roll(item, 3U, 101U, 5U);
    set_roll(item, 4U, 102U, 4U);
    set_roll(item, 5U, 112U, 1U, 3U);
    return item;
}

arpg::test::Failure stable_item_types_have_required_defaults() noexcept {
    const ItemOwnershipState ownership{};
    const std::array<std::uint64_t, 6> empty_equipment{};
    const std::array<std::uint64_t, 3> empty_claims{};
    ARPG_REQUIRE(ownership.items.empty());
    ARPG_REQUIRE(ownership.equipment.equipped_ids == empty_equipment);
    ARPG_REQUIRE(ownership.claimed_drop_bits == empty_claims);
    ARPG_REQUIRE(ownership.next_item_sequence == 1U);
    return {};
}

arpg::test::Failure tier_constants_use_persistent_tier_encoding() noexcept {
    constexpr std::array<std::uint8_t, 8> minimum_t8_to_t1{{
        1U, 16U, 30U, 45U, 60U, 75U, 88U, 95U}};
    constexpr std::array<std::uint32_t, 8> weight_t8_to_t1{{
        64U, 48U, 36U, 27U, 20U, 15U, 11U, 8U}};
    for (std::uint8_t tier = 1U; tier <= 8U; ++tier) {
        const std::size_t index = static_cast<std::size_t>(8U - tier);
        ARPG_REQUIRE(tier_minimum_level(tier) == minimum_t8_to_t1[index]);
        ARPG_REQUIRE(tier_base_weight(tier) == weight_t8_to_t1[index]);
    }
    ARPG_REQUIRE(tier_minimum_level(0U) == 0U);
    ARPG_REQUIRE(tier_minimum_level(9U) == 0U);
    ARPG_REQUIRE(tier_base_weight(0U) == 0U);
    ARPG_REQUIRE(tier_base_weight(9U) == 0U);
    return {};
}

arpg::test::Failure bases_match_frozen_catalog() noexcept {
    ARPG_REQUIRE(base_definition(0U) == nullptr);
    ARPG_REQUIRE(base_definition(7U) == nullptr);
    for (const auto& expected : kExpectedBases) {
        const BaseDefinition* actual = base_definition(expected.id);
        ARPG_REQUIRE(actual != nullptr);
        ARPG_REQUIRE(base_matches(*actual, expected));
    }
    return {};
}

arpg::test::Failure affixes_match_frozen_catalog() noexcept {
    ARPG_REQUIRE(affix_definition(0U) == nullptr);
    ARPG_REQUIRE(affix_definition(13U) == nullptr);
    ARPG_REQUIRE(affix_definition(100U) == nullptr);
    ARPG_REQUIRE(affix_definition(113U) == nullptr);
    for (const auto& expected : kExpectedAffixes) {
        const AffixDefinition* actual = affix_definition(expected.id);
        ARPG_REQUIRE(actual != nullptr);
        ARPG_REQUIRE(affix_matches(*actual, expected));
    }
    return {};
}

arpg::test::Failure every_slot_has_enough_affix_candidates() noexcept {
    for (std::uint8_t raw_slot = 0U;
         raw_slot < static_cast<std::uint8_t>(ItemSlot::count); ++raw_slot) {
        const auto slot = static_cast<ItemSlot>(raw_slot);
        std::size_t prefixes = 0U;
        std::size_t suffixes = 0U;
        for (const auto& expected : kExpectedAffixes) {
            if ((expected.slot_mask & slot_bit(slot)) == 0U)
                continue;
            prefixes += expected.kind == AffixKind::prefix ? 1U : 0U;
            suffixes += expected.kind == AffixKind::suffix ? 1U : 0U;
        }
        ARPG_REQUIRE(prefixes >= 3U);
        ARPG_REQUIRE(suffixes >= 3U);
    }
    ARPG_REQUIRE(validate_catalog());
    return {};
}

arpg::test::Failure item_rarity_controls_affix_count() noexcept {
    ItemInstance item = normal_item(0U, 1U);
    ARPG_REQUIRE(validate_item(item));
    item.rarity = ItemRarity::magic;
    ARPG_REQUIRE(!validate_item(item));
    set_roll(item, 0U, 1U, 8U);
    ARPG_REQUIRE(validate_item(item));
    set_roll(item, 1U, 2U, 8U);
    ARPG_REQUIRE(validate_item(item));
    set_roll(item, 2U, 3U, 8U);
    ARPG_REQUIRE(!validate_item(item));
    item.rarity = ItemRarity::rare;
    ARPG_REQUIRE(validate_item(item));
    item.affix_count = 2U;
    ARPG_REQUIRE(!validate_item(item));
    item.affix_count = 7U;
    ARPG_REQUIRE(!validate_item(item));
    item.rarity = static_cast<ItemRarity>(3U);
    ARPG_REQUIRE(!validate_item(item));
    return {};
}

arpg::test::Failure item_rejects_bad_base_slot_group_and_kind_counts() noexcept {
    ItemInstance item = valid_magic_weapon();
    item.base_id = 0U;
    ARPG_REQUIRE(!validate_item(item));
    item.base_id = 2U;
    ARPG_REQUIRE(!validate_item(item));

    item = valid_magic_weapon();
    set_roll(item, 1U, 1U, 8U);
    ARPG_REQUIRE(!validate_item(item));

    item = valid_rare_accessory();
    item.affixes[3] = {8U, 8U, 0xFFU};
    ARPG_REQUIRE(!validate_item(item));

    item = valid_rare_accessory();
    item.affixes[0] = {101U, 8U, 0xFFU};
    item.affixes[1] = {102U, 8U, 0xFFU};
    item.affixes[2] = {103U, 8U, 0xFFU};
    item.affixes[3] = {107U, 8U, 0xFFU};
    ARPG_REQUIRE(!validate_item(item));
    return {};
}

arpg::test::Failure item_enforces_tier_variant_and_required_level() noexcept {
    ItemInstance item = valid_magic_weapon();
    item.affixes[0].tier = 0U;
    ARPG_REQUIRE(!validate_item(item));
    item.affixes[0].tier = 9U;
    ARPG_REQUIRE(!validate_item(item));

    item = valid_magic_weapon();
    item.affixes[0].variant = 0U;
    ARPG_REQUIRE(!validate_item(item));
    item.affixes[0].variant = 0xFFU;
    item.required_level = 94U;
    ARPG_REQUIRE(!validate_item(item));
    item.required_level = 95U;
    item.item_level = 94U;
    ARPG_REQUIRE(!validate_item(item));

    item = normal_item(0U, 2U);
    item.required_level = 2U;
    ARPG_REQUIRE(!validate_item(item));

    item = valid_rare_accessory();
    item.affixes[5].variant = 4U;
    ARPG_REQUIRE(!validate_item(item));
    for (std::uint8_t variant = 0U; variant < 4U; ++variant) {
        item.affixes[5].variant = variant;
        ARPG_REQUIRE(validate_item(item));
    }
    return {};
}

arpg::test::Failure item_requires_unused_rolls_to_be_zero() noexcept {
    ItemInstance item = valid_magic_weapon();
    item.affixes[1].affix_id = 1U;
    ARPG_REQUIRE(!validate_item(item));
    item.affixes[1] = {};
    item.affixes[1].tier = 1U;
    ARPG_REQUIRE(!validate_item(item));
    item.affixes[1] = {};
    item.affixes[1].variant = 0xFFU;
    ARPG_REQUIRE(!validate_item(item));
    return {};
}

arpg::test::Failure item_requires_reserved_bytes_to_be_zero() noexcept {
    ItemInstance item = valid_magic_weapon();
    ARPG_REQUIRE(validate_item(item));
    for (std::size_t index = 0U; index < item.reserved.size(); ++index) {
        item.reserved[index] = static_cast<std::uint8_t>(index + 1U);
        ARPG_REQUIRE(!validate_item(item));
        item.reserved[index] = 0U;
    }
    ARPG_REQUIRE(validate_item(item));
    return {};
}

arpg::test::Failure valid_normal_magic_and_rare_items_are_accepted() noexcept {
    ARPG_REQUIRE(validate_item(normal_item(0U, 1U)));
    ARPG_REQUIRE(validate_item(valid_magic_weapon()));
    ARPG_REQUIRE(validate_item(valid_rare_accessory()));
    return {};
}

arpg::test::Failure ownership_enforces_ids_and_equipment_references() noexcept {
    ItemOwnershipState state{};
    for (std::uint8_t index = 0U; index < 6U; ++index) {
        state.items.push_back(normal_item(index + 1U, index + 1U));
        state.equipment.equipped_ids[index] = index + 1U;
    }
    ARPG_REQUIRE(validate_ownership(state));

    ItemOwnershipState invalid = state;
    invalid.items[0].id = 0U;
    ARPG_REQUIRE(!validate_ownership(invalid));
    invalid = state;
    invalid.items[1].id = invalid.items[0].id;
    ARPG_REQUIRE(!validate_ownership(invalid));
    invalid = state;
    invalid.equipment.equipped_ids[0] = 999U;
    ARPG_REQUIRE(!validate_ownership(invalid));
    invalid = state;
    std::swap(invalid.equipment.equipped_ids[0],
        invalid.equipment.equipped_ids[1]);
    ARPG_REQUIRE(!validate_ownership(invalid));
    invalid = state;
    invalid.equipment.equipped_ids[1] = invalid.equipment.equipped_ids[0];
    ARPG_REQUIRE(!validate_ownership(invalid));
    invalid = state;
    invalid.items[0].required_level = 2U;
    ARPG_REQUIRE(!validate_ownership(invalid));
    return {};
}

arpg::test::Failure ownership_enforces_exact_capacity_boundary() noexcept {
    ItemOwnershipState state{};
    state.items.reserve(65536U);
    for (std::uint32_t id = 1U; id <= 65535U; ++id)
        state.items.push_back(normal_item(id, 1U));
    ARPG_REQUIRE(validate_ownership(state));
    state.items.push_back(normal_item(65536U, 1U));
    ARPG_REQUIRE(!validate_ownership(state));
    return {};
}

arpg::test::Failure ownership_requires_nonzero_next_item_sequence() noexcept {
    ItemOwnershipState state{};
    ARPG_REQUIRE(validate_ownership(state));
    state.next_item_sequence = 0U;
    ARPG_REQUIRE(!validate_ownership(state));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"stable item type defaults", &stable_item_types_have_required_defaults},
    {"persistent tier constants", &tier_constants_use_persistent_tier_encoding},
    {"frozen base catalog", &bases_match_frozen_catalog},
    {"frozen affix catalog", &affixes_match_frozen_catalog},
    {"slot candidate coverage", &every_slot_has_enough_affix_candidates},
    {"rarity affix counts", &item_rarity_controls_affix_count},
    {"base slot group and kind validation",
        &item_rejects_bad_base_slot_group_and_kind_counts},
    {"tier variant and level validation",
        &item_enforces_tier_variant_and_required_level},
    {"unused roll zero validation", &item_requires_unused_rolls_to_be_zero},
    {"reserved byte zero validation",
        &item_requires_reserved_bytes_to_be_zero},
    {"valid item acceptance", &valid_normal_magic_and_rare_items_are_accepted},
    {"ownership identity and equipment validation",
        &ownership_enforces_ids_and_equipment_references},
    {"ownership capacity", &ownership_enforces_exact_capacity_boundary},
    {"ownership sequence", &ownership_requires_nonzero_next_item_sequence},
};

}  // namespace

arpg::test::TestSuite item_catalog_suite() noexcept {
    return arpg::test::make_suite("item_catalog", kCases);
}
