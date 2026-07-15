#pragma once

#include "items/item_types.hpp"
#include "modifiers/modifier_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::items {

struct EquipmentProjection final {
    std::array<modifiers::Modifier, 128> modifiers{};
    std::size_t modifier_count{};
    std::int64_t weapon_physical{};
    std::int32_t local_attack_speed_bp{};
    bool valid{};
};

[[nodiscard]] EquipmentProjection project_equipment(
    const ItemOwnershipState& state) noexcept;

}  // namespace arpg::items
