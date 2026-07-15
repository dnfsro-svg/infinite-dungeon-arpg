#pragma once

#include "items/item_types.hpp"

#include "modifiers/modifier_types.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace arpg::items {

enum class ItemEffectKind : std::uint8_t {
    global_modifier,
    local_weapon_physical_flat,
    local_weapon_physical_increased,
    slot_dependent_attack_speed,
    all_element_damage_reduction,
    variant_element_damage_reduction_cap,
};

struct BaseDefinition final {
    std::uint8_t id{};
    std::string_view name{};
    ItemSlot slot{};
    ItemEffectKind effect{};
    modifiers::StatId stat{modifiers::StatId::count};
    modifiers::ModifierOperation operation{modifiers::ModifierOperation::flat};
    std::array<std::int32_t, 8> values{};
};

struct AffixDefinition final {
    std::uint16_t id{};
    std::uint16_t group_id{};
    AffixKind kind{};
    std::uint8_t slot_mask{};
    ItemEffectKind effect{};
    modifiers::StatId stat{modifiers::StatId::count};
    modifiers::ModifierOperation operation{modifiers::ModifierOperation::flat};
    std::array<std::int32_t, 8> values{};
};

enum class OwnershipValidationResult : std::uint8_t {
    valid,
    invalid_state,
    allocation_failure,
};

[[nodiscard]] const BaseDefinition* base_definition(std::uint8_t id) noexcept;
[[nodiscard]] const AffixDefinition* affix_definition(std::uint16_t id) noexcept;
[[nodiscard]] std::uint8_t tier_minimum_level(std::uint8_t tier) noexcept;
[[nodiscard]] std::uint32_t tier_base_weight(std::uint8_t tier) noexcept;
[[nodiscard]] bool validate_catalog() noexcept;
[[nodiscard]] bool validate_item(const ItemInstance& item) noexcept;
[[nodiscard]] OwnershipValidationResult validate_ownership_detailed(
    const ItemOwnershipState& state) noexcept;
[[nodiscard]] bool validate_ownership(const ItemOwnershipState& state) noexcept;

}  // namespace arpg::items
