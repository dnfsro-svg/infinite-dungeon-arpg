#pragma once

#include "items/material_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace arpg::items {

enum class ItemSlot : std::uint8_t {
    weapon,
    helmet,
    chest,
    gloves,
    boots,
    accessory,
    count,
};

enum class ItemRarity : std::uint8_t {
    normal,
    magic,
    rare,
};

enum class AffixKind : std::uint8_t {
    prefix,
    suffix,
};

[[nodiscard]] constexpr std::uint8_t slot_bit(ItemSlot slot) noexcept {
    const auto value = static_cast<std::uint8_t>(slot);
    return value < static_cast<std::uint8_t>(ItemSlot::count)
        ? static_cast<std::uint8_t>(1U << value)
        : 0U;
}

struct AffixRoll final {
    std::uint16_t affix_id{};
    std::uint8_t tier{};
    std::uint8_t variant{};
};

struct ItemInstance final {
    std::uint64_t id{};
    std::uint8_t base_id{};
    ItemRarity rarity{};
    std::uint8_t item_level{};
    std::uint8_t required_level{};
    std::array<AffixRoll, 6> affixes{};
    std::uint8_t affix_count{};
    std::array<std::uint8_t, 3> reserved{};
    std::uint32_t reinforcement{};
    std::array<std::uint8_t, 4> extension_reserved{};
};

static_assert(std::is_standard_layout_v<ItemInstance>);
static_assert(std::is_trivially_copyable_v<ItemInstance>);
static_assert(sizeof(ItemInstance) == 48U);
static_assert(offsetof(ItemInstance, affixes) == 12U);
static_assert(offsetof(ItemInstance, affix_count) == 36U);
static_assert(offsetof(ItemInstance, reserved) == 37U);
static_assert(offsetof(ItemInstance, reinforcement) == 40U);
static_assert(offsetof(ItemInstance, extension_reserved) == 44U);

struct EquipmentState final {
    std::array<std::uint64_t, 6> equipped_ids{};
};

struct ItemOwnershipState final {
    std::vector<ItemInstance> items{};
    EquipmentState equipment{};
    std::array<std::uint32_t, kMaterialCount> materials{};
    std::array<std::uint64_t, 3> claimed_drop_bits{};
    std::uint64_t next_item_sequence{1};
};

}  // namespace arpg::items
