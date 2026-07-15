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

enum class EquipmentProjectionStatus : std::uint8_t {
    valid,
    invalid_state,
    allocation_failure,
};

struct EquipmentProjectionResult final {
    EquipmentProjection projection{};
    EquipmentProjectionStatus status{
        EquipmentProjectionStatus::invalid_state};
};

[[nodiscard]] EquipmentProjectionResult project_equipment_detailed(
    const ItemOwnershipState& state) noexcept;
[[nodiscard]] EquipmentProjectionResult project_equipment_detailed(
    const ItemOwnershipState& state,
    const EquipmentState& equipment_override) noexcept;

[[nodiscard]] EquipmentProjection project_equipment(
    const ItemOwnershipState& state) noexcept;

}  // namespace arpg::items
