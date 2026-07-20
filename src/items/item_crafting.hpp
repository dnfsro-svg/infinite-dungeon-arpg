#pragma once

#include "items/item_types.hpp"
#include "items/material_catalog.hpp"

#include <cstdint>
#include <optional>

namespace arpg::items {

enum class DirectedCategory : std::uint8_t {
    damage,
    defense,
    speed,
    element,
    count,
};

enum class ReinforcementFailure : std::uint8_t {
    unchanged,
    reset_six,
    reset_zero,
    destroy,
};

struct ReinforcementPreview final {
    std::uint32_t current{};
    std::uint32_t target{};
    std::uint16_t success_chance_bp{};
    ReinforcementFailure failure{ReinforcementFailure::unchanged};
    bool can_attempt{};
};

struct CraftRequest final {
    std::uint64_t root_seed{};
    std::uint64_t operation_nonce{};
    ItemInstance item{};
    MaterialId material{MaterialId::count};
    std::optional<DirectedCategory> directed_category{};
};

struct CraftResult final {
    bool applied{};
    bool consumed{};
    ItemInstance item{};

    [[nodiscard]] explicit operator bool() const noexcept { return applied; }
};

[[nodiscard]] CraftResult craft_item(CraftRequest request) noexcept;
[[nodiscard]] std::uint16_t reinforcement_success_chance_bp(
    std::uint32_t target) noexcept;
[[nodiscard]] ReinforcementFailure reinforcement_failure(
    std::uint32_t current) noexcept;
[[nodiscard]] std::int64_t reinforced_base_value(
    std::int64_t base,
    std::uint32_t level) noexcept;
[[nodiscard]] std::int32_t reinforced_accessory_bonus_bp(
    std::uint32_t level) noexcept;
[[nodiscard]] ReinforcementPreview reinforcement_preview(
    std::uint32_t current) noexcept;
[[nodiscard]] ReinforcementPreview reinforcement_preview(
    const ItemInstance& item) noexcept;
[[nodiscard]] std::optional<ItemInstance> apply_coupon(
    ItemInstance item,
    MaterialId coupon) noexcept;

}  // namespace arpg::items
