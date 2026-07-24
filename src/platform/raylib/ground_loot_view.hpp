#pragma once

#include "dungeon/dungeon_types.hpp"
#include "dungeon_view_math.hpp"
#include "material_asset_types.hpp"
#include "platform/settings/settings_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

inline constexpr std::size_t kGroundLootTextCapacity = 64U;
inline constexpr float kGroundLootSafetyInset = 12.0F;

struct LootLabelRect final {
    float x{};
    float y{};
    float width{};
    float height{};
};

struct GroundLootLabel final {
    std::uint16_t ordinal{};
    float anchor_x{};
    float anchor_y{};
    LootLabelRect rect{};
    Rgba8 text_color{};
    Rgba8 border_color{};
    MaterialSpriteId item_sprite{MaterialSpriteId::missing};
    MaterialSpriteId rarity_sprite{MaterialSpriteId::missing};
    bool abyss{};
    std::array<char, kGroundLootTextCapacity> text{};
};

struct GroundLootViewDiagnostics final {
    std::uint32_t invalid_base_count{};
    std::uint32_t text_truncation_count{};
    std::uint32_t overlap_adjustment_count{};
    std::uint32_t capacity_saturation_count{};
};

struct GroundLootView final {
    std::array<GroundLootLabel, dungeon::kGroundDropCapacity> labels{};
    std::size_t count{};
    GroundLootViewDiagnostics diagnostics{};
};

[[nodiscard]] bool ground_loot_visible(
    const dungeon::GroundItemSnapshot& item,
    settings::LootFilterMode mode) noexcept;

[[nodiscard]] MaterialSpriteId ground_loot_item_sprite(
    items::ItemSlot slot) noexcept;
[[nodiscard]] MaterialSpriteId ground_loot_rarity_sprite(
    items::ItemRarity rarity, bool abyss) noexcept;

[[nodiscard]] GroundLootView build_ground_loot_view(
    const dungeon::DungeonSnapshot& snapshot,
    settings::LootFilterMode mode,
    float width,
    float height) noexcept;

}  // namespace arpg::platform
