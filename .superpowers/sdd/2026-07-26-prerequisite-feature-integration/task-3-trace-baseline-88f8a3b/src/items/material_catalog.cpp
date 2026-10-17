#include "items/material_catalog.hpp"

#include <array>
#include <cstddef>

namespace arpg::items {
namespace {

constexpr std::array<MaterialDefinition, kMaterialCount> kMaterials{{
    {MaterialId::transmute, 1U, "Transmute"},
    {MaterialId::augment, 2U, "Augment"},
    {MaterialId::regal, 3U, "Regal"},
    {MaterialId::chaos, 4U, "Chaos"},
    {MaterialId::exalt, 5U, "Exalt"},
    {MaterialId::annul, 6U, "Annul"},
    {MaterialId::divine, 7U, "Divine"},
    {MaterialId::scour, 8U, "Scour"},
    {MaterialId::directed, 9U, "Directed Core"},
    {MaterialId::reinforcement_stone, 10U, "Reinforcement Stone"},
    {MaterialId::coupon_6, 11U, "+6 Coupon"},
    {MaterialId::coupon_9, 12U, "+9 Coupon"},
    {MaterialId::coupon_12, 13U, "+12 Coupon"},
    {MaterialId::coupon_15, 14U, "+15 Coupon"},
}};

}  // namespace

const MaterialDefinition* material_definition(MaterialId id) noexcept {
    const std::size_t index = material_index(id);
    return index < kMaterials.size() ? &kMaterials[index] : nullptr;
}

bool material_is_crafting_currency(MaterialId id) noexcept {
    return material_index(id) <= material_index(MaterialId::directed);
}

bool material_is_coupon(MaterialId id) noexcept {
    const std::size_t index = material_index(id);
    return index >= material_index(MaterialId::coupon_6)
        && index <= material_index(MaterialId::coupon_15);
}

std::uint32_t coupon_reinforcement_level(MaterialId id) noexcept {
    switch (id) {
    case MaterialId::coupon_6: return 6U;
    case MaterialId::coupon_9: return 9U;
    case MaterialId::coupon_12: return 12U;
    case MaterialId::coupon_15: return 15U;
    default: return 0U;
    }
}

bool validate_material_catalog() noexcept {
    for (std::size_t index = 0U; index < kMaterials.size(); ++index) {
        const MaterialDefinition& definition = kMaterials[index];
        if (material_index(definition.id) != index
            || definition.stable_id != index + 1U
            || definition.name.empty()) {
            return false;
        }
    }
    return true;
}

}  // namespace arpg::items
