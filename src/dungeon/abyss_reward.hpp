#pragma once

#include "abyss/abyss_types.hpp"
#include "dungeon/dungeon_types.hpp"
#include "items/item_types.hpp"

#include <cstdint>
#include <optional>

namespace arpg::dungeon {

struct AbyssRewardSlot final {
    std::uint8_t slot_index{};
    items::ItemSlot item_slot{items::ItemSlot::weapon};
    items::ItemRarity rarity{items::ItemRarity::normal};
    std::uint8_t item_level{1U};
    std::uint64_t item_seed{};
    std::uint64_t item_id{};
};

[[nodiscard]] std::optional<AbyssRewardSlot> derive_abyss_reward_slot(
    std::uint64_t room_seed,
    abyss::AbyssDanger danger,
    std::uint8_t base_item_level,
    std::uint8_t reward_ordinal) noexcept;
[[nodiscard]] std::optional<combat::Vec3> abyss_reward_position(
    std::uint8_t reward_ordinal) noexcept;
[[nodiscard]] std::optional<GroundItem> derive_abyss_ground_item(
    std::uint64_t room_seed,
    abyss::AbyssDanger danger,
    std::uint8_t base_item_level,
    std::uint8_t reward_ordinal,
    std::uint16_t ground_index) noexcept;
[[nodiscard]] bool same_ground_item(
    const GroundItem& left,
    const GroundItem& right) noexcept;

}  // namespace arpg::dungeon
