#include "items/item_catalog.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace arpg::items {
namespace {

using modifiers::ModifierOperation;
using modifiers::StatId;

constexpr std::array<std::uint8_t, 8> kTierMinimumLevels{{
    1U, 16U, 30U, 45U, 60U, 75U, 88U, 95U}};
constexpr std::array<std::uint32_t, 8> kTierBaseWeights{{
    64U, 48U, 36U, 27U, 20U, 15U, 11U, 8U}};

constexpr std::uint8_t mask(std::initializer_list<ItemSlot> slots) noexcept {
    std::uint8_t result = 0U;
    for (const ItemSlot slot : slots)
        result = static_cast<std::uint8_t>(result | slot_bit(slot));
    return result;
}

constexpr std::uint8_t kAllSlots = mask({ItemSlot::weapon, ItemSlot::helmet,
    ItemSlot::chest, ItemSlot::gloves, ItemSlot::boots, ItemSlot::accessory});
constexpr std::array<std::int32_t, 8> kElementFlat{{1, 2, 3, 4, 5, 6, 8, 10}};
constexpr std::array<std::int32_t, 8> kDamageIncreased{{500, 800, 1200,
    1600, 2100, 2700, 3400, 4200}};
constexpr std::array<std::int32_t, 8> kRating{{25, 60, 150, 400, 1000,
    3000, 10000, 30000}};
constexpr std::array<std::int32_t, 8> kReduction{{300, 500, 700, 900,
    1100, 1400, 1700, 2000}};

constexpr std::array<BaseDefinition, 6> kBases{{
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

constexpr std::array<AffixDefinition, 24> kAffixes{{
    {1U, 1U, AffixKind::prefix, mask({ItemSlot::weapon}),
        ItemEffectKind::local_weapon_physical_flat, StatId::count,
        ModifierOperation::flat, {{1, 2, 3, 4, 5, 7, 9, 12}}},
    {2U, 2U, AffixKind::prefix, mask({ItemSlot::weapon}),
        ItemEffectKind::local_weapon_physical_increased, StatId::count,
        ModifierOperation::flat, kDamageIncreased},
    {3U, 3U, AffixKind::prefix, mask({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::fire_flat_damage, ModifierOperation::flat, kElementFlat},
    {4U, 4U, AffixKind::prefix, mask({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::water_flat_damage, ModifierOperation::flat, kElementFlat},
    {5U, 5U, AffixKind::prefix, mask({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::lightning_flat_damage, ModifierOperation::flat, kElementFlat},
    {6U, 6U, AffixKind::prefix, mask({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::chaos_flat_damage, ModifierOperation::flat, kElementFlat},
    {7U, 7U, AffixKind::prefix, kAllSlots, ItemEffectKind::global_modifier,
        StatId::fire_damage, ModifierOperation::increased, kDamageIncreased},
    {8U, 8U, AffixKind::prefix, kAllSlots, ItemEffectKind::global_modifier,
        StatId::water_damage, ModifierOperation::increased, kDamageIncreased},
    {9U, 9U, AffixKind::prefix, kAllSlots, ItemEffectKind::global_modifier,
        StatId::lightning_damage, ModifierOperation::increased, kDamageIncreased},
    {10U, 10U, AffixKind::prefix, kAllSlots, ItemEffectKind::global_modifier,
        StatId::chaos_damage, ModifierOperation::increased, kDamageIncreased},
    {11U, 11U, AffixKind::prefix, mask({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::gloves, ItemSlot::boots, ItemSlot::accessory}),
        ItemEffectKind::global_modifier, StatId::max_health,
        ModifierOperation::flat, {{4, 7, 11, 16, 22, 29, 37, 46}}},
    {12U, 12U, AffixKind::prefix, mask({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::gloves, ItemSlot::boots, ItemSlot::accessory}),
        ItemEffectKind::global_modifier, StatId::max_barrier,
        ModifierOperation::flat, {{3, 5, 8, 12, 17, 23, 30, 38}}},
    {101U, 101U, AffixKind::suffix, mask({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::slot_dependent_attack_speed,
        StatId::count, ModifierOperation::flat,
        {{200, 300, 400, 500, 600, 800, 1000, 1200}}},
    {102U, 102U, AffixKind::suffix, mask({ItemSlot::boots, ItemSlot::accessory}),
        ItemEffectKind::global_modifier, StatId::move_speed,
        ModifierOperation::increased,
        {{200, 300, 400, 500, 600, 700, 800, 1000}}},
    {103U, 103U, AffixKind::suffix, mask({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::gloves, ItemSlot::boots, ItemSlot::accessory}),
        ItemEffectKind::global_modifier, StatId::evasion,
        ModifierOperation::flat, kRating},
    {104U, 104U, AffixKind::suffix, mask({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::gloves, ItemSlot::boots}), ItemEffectKind::global_modifier,
        StatId::armor, ModifierOperation::flat, kRating},
    {105U, 105U, AffixKind::suffix, mask({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::impulse_scale, ModifierOperation::increased,
        {{400, 600, 800, 1000, 1300, 1600, 2000, 2500}}},
    {106U, 106U, AffixKind::suffix, mask({ItemSlot::weapon, ItemSlot::gloves,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::melee_damage, ModifierOperation::increased,
        {{400, 600, 900, 1200, 1500, 1900, 2400, 3000}}},
    {107U, 107U, AffixKind::suffix, mask({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::boots, ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::fire_damage_reduction, ModifierOperation::flat, kReduction},
    {108U, 108U, AffixKind::suffix, mask({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::boots, ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::water_damage_reduction, ModifierOperation::flat, kReduction},
    {109U, 109U, AffixKind::suffix, mask({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::boots, ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::lightning_damage_reduction, ModifierOperation::flat, kReduction},
    {110U, 110U, AffixKind::suffix, mask({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::boots, ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::chaos_damage_reduction, ModifierOperation::flat, kReduction},
    {111U, 111U, AffixKind::suffix, mask({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::accessory}), ItemEffectKind::global_modifier,
        StatId::max_health, ModifierOperation::increased,
        {{300, 500, 700, 900, 1200, 1500, 1900, 2400}}},
    {112U, 112U, AffixKind::suffix, mask({ItemSlot::helmet, ItemSlot::chest,
        ItemSlot::accessory}),
        ItemEffectKind::variant_element_damage_reduction_cap, StatId::count,
        ModifierOperation::flat, {{100, 100, 200, 200, 300, 400, 500, 600}}},
}};

constexpr bool effect_sentinel_is_valid(ItemEffectKind effect, StatId stat,
    ModifierOperation operation) noexcept {
    if (effect == ItemEffectKind::global_modifier)
        return stat < StatId::count;
    return stat == StatId::count && operation == ModifierOperation::flat;
}

constexpr bool roll_is_zero(const AffixRoll& roll) noexcept {
    return roll.affix_id == 0U && roll.tier == 0U && roll.variant == 0U;
}

}  // namespace

const BaseDefinition* base_definition(std::uint8_t id) noexcept {
    if (id == 0U || id > kBases.size())
        return nullptr;
    return &kBases[static_cast<std::size_t>(id - 1U)];
}

const AffixDefinition* affix_definition(std::uint16_t id) noexcept {
    if (id >= 1U && id <= 12U)
        return &kAffixes[static_cast<std::size_t>(id - 1U)];
    if (id >= 101U && id <= 112U)
        return &kAffixes[static_cast<std::size_t>(id - 89U)];
    return nullptr;
}

std::uint8_t tier_minimum_level(std::uint8_t tier) noexcept {
    return tier >= 1U && tier <= 8U
        ? kTierMinimumLevels[static_cast<std::size_t>(8U - tier)] : 0U;
}

std::uint32_t tier_base_weight(std::uint8_t tier) noexcept {
    return tier >= 1U && tier <= 8U
        ? kTierBaseWeights[static_cast<std::size_t>(8U - tier)] : 0U;
}

bool validate_catalog() noexcept {
    std::uint8_t seen_base_slots = 0U;
    for (std::size_t index = 0U; index < kBases.size(); ++index) {
        const BaseDefinition& base = kBases[index];
        if (base.id != index + 1U || base.name.empty()
            || slot_bit(base.slot) == 0U
            || (seen_base_slots & slot_bit(base.slot)) != 0U
            || !effect_sentinel_is_valid(base.effect, base.stat, base.operation))
            return false;
        seen_base_slots = static_cast<std::uint8_t>(
            seen_base_slots | slot_bit(base.slot));
    }
    if (seen_base_slots != kAllSlots)
        return false;

    std::array<std::uint8_t, 6> prefix_counts{};
    std::array<std::uint8_t, 6> suffix_counts{};
    for (std::size_t index = 0U; index < kAffixes.size(); ++index) {
        const AffixDefinition& affix = kAffixes[index];
        const std::uint16_t expected_id = index < 12U
            ? static_cast<std::uint16_t>(index + 1U)
            : static_cast<std::uint16_t>(index + 89U);
        if (affix.id != expected_id || affix.group_id != affix.id
            || (affix.kind != AffixKind::prefix
                && affix.kind != AffixKind::suffix)
            || affix.slot_mask == 0U || (affix.slot_mask & ~kAllSlots) != 0U
            || !effect_sentinel_is_valid(
                affix.effect, affix.stat, affix.operation))
            return false;
        for (std::size_t slot = 0U; slot < prefix_counts.size(); ++slot) {
            if ((affix.slot_mask & slot_bit(static_cast<ItemSlot>(slot))) == 0U)
                continue;
            auto& count = affix.kind == AffixKind::prefix
                ? prefix_counts[slot] : suffix_counts[slot];
            ++count;
        }
    }
    for (std::size_t slot = 0U; slot < prefix_counts.size(); ++slot) {
        if (prefix_counts[slot] < 3U || suffix_counts[slot] < 3U)
            return false;
    }
    return true;
}

bool validate_item(const ItemInstance& item) noexcept {
    const BaseDefinition* base = base_definition(item.base_id);
    if (base == nullptr || item.affix_count > item.affixes.size())
        return false;
    for (const std::uint8_t byte : item.reserved)
        if (byte != 0U) return false;

    std::uint8_t minimum_affixes = 0U;
    std::uint8_t maximum_affixes = 0U;
    switch (item.rarity) {
    case ItemRarity::normal:
        maximum_affixes = 0U;
        break;
    case ItemRarity::magic:
        minimum_affixes = 1U;
        maximum_affixes = 2U;
        break;
    case ItemRarity::rare:
        minimum_affixes = 3U;
        maximum_affixes = 6U;
        break;
    default:
        return false;
    }
    if (item.affix_count < minimum_affixes || item.affix_count > maximum_affixes)
        return false;

    std::array<std::uint16_t, 6> groups{};
    std::uint8_t prefix_count = 0U;
    std::uint8_t suffix_count = 0U;
    std::uint8_t required_level = 1U;
    for (std::size_t index = 0U; index < item.affixes.size(); ++index) {
        const AffixRoll& roll = item.affixes[index];
        if (index >= item.affix_count) {
            if (!roll_is_zero(roll))
                return false;
            continue;
        }

        const AffixDefinition* affix = affix_definition(roll.affix_id);
        const std::uint8_t tier_level = tier_minimum_level(roll.tier);
        if (affix == nullptr || tier_level == 0U
            || (affix->slot_mask & slot_bit(base->slot)) == 0U
            || item.item_level < tier_level)
            return false;
        if (affix->id == 112U) {
            if (roll.variant > 3U)
                return false;
        } else if (roll.variant != 0xFFU) {
            return false;
        }
        for (std::size_t prior = 0U; prior < index; ++prior) {
            if (groups[prior] == affix->group_id)
                return false;
        }
        groups[index] = affix->group_id;
        prefix_count += affix->kind == AffixKind::prefix ? 1U : 0U;
        suffix_count += affix->kind == AffixKind::suffix ? 1U : 0U;
        if (prefix_count > 3U || suffix_count > 3U)
            return false;
        if (tier_level > required_level)
            required_level = tier_level;
    }
    return item.required_level == required_level
        && item.item_level >= required_level;
}

OwnershipValidationResult validate_ownership_detailed(
    const ItemOwnershipState& state) noexcept {
    if (state.next_item_sequence == 0U || state.items.size() > 65535U)
        return OwnershipValidationResult::invalid_state;

    const std::size_t item_count = state.items.size();
    const std::unique_ptr<std::uint64_t[]> ids{item_count == 0U
        ? nullptr : new (std::nothrow) std::uint64_t[item_count]};
    if (item_count != 0U && !ids)
        return OwnershipValidationResult::allocation_failure;
    for (std::size_t index = 0U; index < item_count; ++index) {
        const ItemInstance& item = state.items[index];
        if (item.id == 0U || !validate_item(item))
            return OwnershipValidationResult::invalid_state;
        ids[index] = item.id;
    }
    if (item_count > 1U)
        std::sort(ids.get(), ids.get() + item_count);
    for (std::size_t index = 1U; index < item_count; ++index) {
        if (ids[index - 1U] == ids[index])
            return OwnershipValidationResult::invalid_state;
    }

    for (std::size_t slot = 0U; slot < state.equipment.equipped_ids.size(); ++slot) {
        const std::uint64_t equipped_id = state.equipment.equipped_ids[slot];
        if (equipped_id == 0U)
            continue;
        for (std::size_t prior = 0U; prior < slot; ++prior) {
            if (state.equipment.equipped_ids[prior] == equipped_id)
                return OwnershipValidationResult::invalid_state;
        }
        const ItemInstance* equipped_item = nullptr;
        for (const ItemInstance& item : state.items) {
            if (item.id == equipped_id) {
                equipped_item = &item;
                break;
            }
        }
        if (equipped_item == nullptr)
            return OwnershipValidationResult::invalid_state;
        const BaseDefinition* base = base_definition(equipped_item->base_id);
        if (base == nullptr || base->slot != static_cast<ItemSlot>(slot))
            return OwnershipValidationResult::invalid_state;
    }
    return OwnershipValidationResult::valid;
}

bool validate_ownership(const ItemOwnershipState& state) noexcept {
    return validate_ownership_detailed(state) == OwnershipValidationResult::valid;
}

}  // namespace arpg::items
