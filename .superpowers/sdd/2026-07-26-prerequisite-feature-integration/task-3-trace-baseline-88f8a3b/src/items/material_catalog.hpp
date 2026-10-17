#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace arpg::items {

enum class MaterialId : std::uint8_t {
    transmute,
    augment,
    regal,
    chaos,
    exalt,
    annul,
    divine,
    scour,
    directed,
    reinforcement_stone,
    coupon_6,
    coupon_9,
    coupon_12,
    coupon_15,
    count,
};

inline constexpr std::size_t kMaterialCount =
    static_cast<std::size_t>(MaterialId::count);
inline constexpr std::uint16_t kMaterialDiscoveryMask =
    static_cast<std::uint16_t>((std::uint32_t{1U} << kMaterialCount) - 1U);

struct MaterialDefinition final {
    MaterialId id{MaterialId::count};
    std::uint8_t stable_id{};
    std::string_view name{};
};

[[nodiscard]] constexpr std::size_t material_index(MaterialId id) noexcept {
    const std::size_t index = static_cast<std::size_t>(id);
    return index < kMaterialCount ? index : kMaterialCount;
}

[[nodiscard]] const MaterialDefinition* material_definition(
    MaterialId id) noexcept;
[[nodiscard]] bool material_is_crafting_currency(MaterialId id) noexcept;
[[nodiscard]] bool material_is_coupon(MaterialId id) noexcept;
[[nodiscard]] std::uint32_t coupon_reinforcement_level(MaterialId id) noexcept;
[[nodiscard]] bool validate_material_catalog() noexcept;

}  // namespace arpg::items
