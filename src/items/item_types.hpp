#pragma once

#include <array>
#include <cstdint>
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
};

struct EquipmentState final {
    std::array<std::uint64_t, 6> equipped_ids{};
};

struct ItemOwnershipState final {
    std::vector<ItemInstance> items{};
    EquipmentState equipment{};
    std::array<std::uint64_t, 3> claimed_drop_bits{};
    std::uint64_t next_item_sequence{1};
};

}  // namespace arpg::items
