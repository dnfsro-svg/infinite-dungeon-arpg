#pragma once

#include "items/item_types.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace arpg::items {

struct RarityWeights final {
    std::uint32_t normal{};
    std::uint32_t magic{};
    std::uint32_t rare{};
};

struct ItemGenerationRequest final {
    std::uint64_t seed{};
    ItemSlot slot{};
    std::uint8_t item_level{};
    std::uint64_t item_id{};
    std::optional<ItemRarity> forced_rarity{};
};

[[nodiscard]] std::optional<RarityWeights> rarity_weights(
    std::uint8_t item_level) noexcept;
[[nodiscard]] std::optional<std::array<std::uint32_t, 8>> tier_weights(
    std::uint8_t item_level) noexcept;
[[nodiscard]] std::optional<ItemInstance> generate_item(
    const ItemGenerationRequest& request) noexcept;
[[nodiscard]] std::optional<ItemInstance> generate_recipe_item(
    std::uint64_t root_seed,
    std::uint64_t next_sequence,
    const ItemInstance& a,
    const ItemInstance& b,
    const ItemInstance& c) noexcept;

}  // namespace arpg::items
