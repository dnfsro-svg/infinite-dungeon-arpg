#include "items/item_modifiers.hpp"

#include "items/item_catalog.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::items {
namespace {

inline constexpr modifiers::ModifierId kEquipmentModifierDomain =
    0x80000000U;
inline constexpr std::uint16_t kBaseSourceStart = 1U;
inline constexpr std::uint16_t kAffixSourceStart = 0x100U;
inline constexpr std::uint16_t kSourceStride = 4U;

bool checked_add(std::int64_t left,
    std::int64_t right,
    std::int64_t& output) noexcept {
    const auto maximum = (std::numeric_limits<std::int64_t>::max)();
    const auto minimum = (std::numeric_limits<std::int64_t>::min)();
    if ((right > 0 && left > maximum - right)
        || (right < 0 && left < minimum - right))
        return false;
    output = left + right;
    return true;
}

bool checked_multiply(std::int64_t left,
    std::int64_t right,
    std::int64_t& output) noexcept {
    const auto maximum = (std::numeric_limits<std::int64_t>::max)();
    const auto minimum = (std::numeric_limits<std::int64_t>::min)();
    if (left == 0 || right == 0) {
        output = 0;
        return true;
    }
    if ((left == -1 && right == minimum)
        || (right == -1 && left == minimum))
        return false;
    if (left > 0) {
        if ((right > 0 && left > maximum / right)
            || (right < 0 && right < minimum / left))
            return false;
    } else if ((right > 0 && left < minimum / right)
        || (right < 0 && left < maximum / right)) {
        return false;
    }
    output = left * right;
    return true;
}

bool checked_add_int32(std::int32_t left,
    std::int32_t right,
    std::int32_t& output) noexcept {
    const std::int64_t sum = static_cast<std::int64_t>(left) + right;
    if (sum < (std::numeric_limits<std::int32_t>::min)()
        || sum > (std::numeric_limits<std::int32_t>::max)())
        return false;
    output = static_cast<std::int32_t>(sum);
    return true;
}

std::size_t value_index(std::uint8_t item_level) noexcept {
    for (std::uint8_t tier = 1U; tier <= 8U; ++tier) {
        if (item_level >= tier_minimum_level(tier))
            return static_cast<std::size_t>(8U - tier);
    }
    return 0U;
}

bool uses_fixed_units(modifiers::StatId stat,
    modifiers::ModifierOperation operation) noexcept {
    if (operation != modifiers::ModifierOperation::flat) return false;
    return stat == modifiers::StatId::physical_flat_damage
        || stat == modifiers::StatId::fire_flat_damage
        || stat == modifiers::StatId::water_flat_damage
        || stat == modifiers::StatId::lightning_flat_damage
        || stat == modifiers::StatId::chaos_flat_damage
        || stat == modifiers::StatId::max_health
        || stat == modifiers::StatId::max_barrier;
}

modifiers::ModifierId modifier_id(ItemSlot slot,
    std::uint16_t source) noexcept {
    return kEquipmentModifierDomain
        | (static_cast<modifiers::ModifierId>(slot) << 16U)
        | source;
}

bool append_modifier(EquipmentProjection& projection,
    ItemSlot slot,
    std::uint16_t source,
    modifiers::StatId stat,
    modifiers::ModifierOperation operation,
    std::int32_t raw_value) noexcept {
    if (projection.modifier_count >= projection.modifiers.size()) return false;
    std::int64_t value = raw_value;
    if (uses_fixed_units(stat, operation)
        && !checked_multiply(value, modifiers::kFixedOne, value))
        return false;
    projection.modifiers[projection.modifier_count++] = modifiers::Modifier{
        modifier_id(slot, source), stat, operation, value};
    return true;
}

bool apply_effect(EquipmentProjection& projection,
    ItemSlot slot,
    ItemEffectKind effect,
    modifiers::StatId stat,
    modifiers::ModifierOperation operation,
    std::int32_t value,
    std::uint8_t variant,
    std::uint16_t source,
    std::int64_t& local_flat,
    std::int64_t& local_increased) noexcept {
    switch (effect) {
    case ItemEffectKind::global_modifier:
        return append_modifier(projection, slot, source,
            stat, operation, value);
    case ItemEffectKind::local_weapon_physical_flat:
        return slot == ItemSlot::weapon
            && checked_add(local_flat, value, local_flat);
    case ItemEffectKind::local_weapon_physical_increased:
        return slot == ItemSlot::weapon
            && checked_add(local_increased, value, local_increased);
    case ItemEffectKind::slot_dependent_attack_speed:
        if (slot == ItemSlot::weapon) {
            return checked_add_int32(projection.local_attack_speed_bp,
                value, projection.local_attack_speed_bp);
        }
        return append_modifier(projection, slot, source,
            modifiers::StatId::attack_speed,
            modifiers::ModifierOperation::increased, value);
    case ItemEffectKind::all_element_damage_reduction:
        for (std::uint16_t element = 0U; element < 4U; ++element) {
            const auto element_stat = static_cast<modifiers::StatId>(
                static_cast<std::uint16_t>(
                    modifiers::StatId::fire_damage_reduction) + element);
            if (!append_modifier(projection, slot,
                    static_cast<std::uint16_t>(source + element),
                    element_stat, modifiers::ModifierOperation::flat, value))
                return false;
        }
        return true;
    case ItemEffectKind::variant_element_damage_reduction_cap:
        if (variant >= 4U) return false;
        return append_modifier(projection, slot, source,
            static_cast<modifiers::StatId>(static_cast<std::uint16_t>(
                modifiers::StatId::fire_damage_reduction_cap) + variant),
            modifiers::ModifierOperation::flat, value);
    }
    return false;
}

const ItemInstance* equipped_item(const ItemOwnershipState& state,
    std::uint64_t id) noexcept {
    for (const ItemInstance& item : state.items)
        if (item.id == id) return &item;
    return nullptr;
}

bool checked_weapon_formula(std::int64_t base_and_flat,
    std::int64_t local_increased,
    std::int64_t& output) noexcept {
    std::int64_t factor{};
    if (!checked_add(modifiers::kFixedOne, local_increased, factor))
        return false;
    std::int64_t whole{};
    std::int64_t remainder{};
    if (!checked_multiply(base_and_flat / modifiers::kFixedOne,
            factor, whole)
        || !checked_multiply(base_and_flat % modifiers::kFixedOne,
            factor, remainder))
        return false;
    remainder /= modifiers::kFixedOne;
    return checked_add(whole, remainder, output);
}

}  // namespace

EquipmentProjection project_equipment(
    const ItemOwnershipState& state) noexcept {
    EquipmentProjection projection{};
    if (!validate_ownership(state)) return projection;

    std::int64_t local_flat = 0;
    std::int64_t local_increased = 0;
    for (std::size_t slot_index = 0U;
         slot_index < state.equipment.equipped_ids.size(); ++slot_index) {
        const std::uint64_t id = state.equipment.equipped_ids[slot_index];
        if (id == 0U) continue;
        const ItemInstance* item = equipped_item(state, id);
        if (item == nullptr) return EquipmentProjection{};
        const BaseDefinition* base = base_definition(item->base_id);
        if (base == nullptr) return EquipmentProjection{};
        const ItemSlot slot = static_cast<ItemSlot>(slot_index);
        const std::size_t base_value_index = value_index(item->item_level);
        if (!apply_effect(projection, slot, base->effect, base->stat,
                base->operation, base->values[base_value_index], 0xFFU,
                kBaseSourceStart, local_flat, local_increased))
            return EquipmentProjection{};

        for (std::size_t affix_index = 0U;
             affix_index < item->affix_count; ++affix_index) {
            const AffixRoll& roll = item->affixes[affix_index];
            const AffixDefinition* affix = affix_definition(roll.affix_id);
            if (affix == nullptr) return EquipmentProjection{};
            const std::size_t index = static_cast<std::size_t>(8U - roll.tier);
            const std::uint16_t source = static_cast<std::uint16_t>(
                kAffixSourceStart + affix_index * kSourceStride);
            if (!apply_effect(projection, slot, affix->effect, affix->stat,
                    affix->operation, affix->values[index], roll.variant,
                    source, local_flat, local_increased))
                return EquipmentProjection{};
        }
    }

    if (!checked_weapon_formula(
            local_flat, local_increased, projection.weapon_physical))
        return EquipmentProjection{};
    projection.valid = true;
    return projection;
}

}  // namespace arpg::items
