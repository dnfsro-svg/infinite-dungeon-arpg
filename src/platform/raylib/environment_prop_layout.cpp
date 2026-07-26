#include "environment_prop_layout.hpp"

#include "chaos_room_material_slice.hpp"
#include "lightning_room_material_slice.hpp"
#include "water_room_material_slice.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace arpg::platform {
namespace {

constexpr float kFrameWidth = 256.0F;
constexpr float kSafeInset = 8.25F;
constexpr std::array<Vector2, 5> kNormalizedFootPositions{{
    {0.12F, 0.34F}, {0.88F, 0.34F}, {0.18F, 0.78F},
    {0.82F, 0.78F}, {0.50F, 0.82F},
}};

template <std::size_t Size>
const EnvironmentPropDefinition* find_definition(
    const std::array<EnvironmentPropDefinition, Size>& definitions,
    MaterialSpriteId sprite) noexcept {
    for (const auto& definition : definitions) {
        if (definition.sprite == sprite) return &definition;
    }
    return nullptr;
}

EnvironmentPropPlacement clamped_placement(MaterialSpriteId sprite,
    Vector2 normalized_position, bool flip_x, float width, float height) noexcept {
    const EnvironmentPropDefinition* const definition =
        environment_prop_definition(sprite);
    if (definition == nullptr || width <= 0.0F || height <= 0.0F) return {};
    EnvironmentPropPlacement placement{
        sprite, normalized_position, flip_x, definition->recommended_scale};
    float foot_x = normalized_position.x * width;
    float foot_y = normalized_position.y * height;
    const float hud_height = std::max(72.0F, 0.12F * height);
    const float right = width - kSafeInset;
    const float bottom = height - hud_height - kSafeInset;
    const float alpha_x = flip_x
        ? kFrameWidth - definition->alpha_bounds.x
            - definition->alpha_bounds.width
        : definition->alpha_bounds.x;
    const float left_offset =
        (alpha_x - definition->foot_anchor.x) * placement.scale;
    const float right_offset = left_offset
        + definition->alpha_bounds.width * placement.scale;
    const float top_offset = (definition->alpha_bounds.y
        - definition->foot_anchor.y) * placement.scale;
    const float bottom_offset = top_offset
        + definition->alpha_bounds.height * placement.scale;
    foot_x = std::clamp(foot_x, kSafeInset - left_offset,
        right - right_offset);
    foot_y = std::clamp(foot_y, kSafeInset - top_offset,
        bottom - bottom_offset);
    placement.normalized_foot_position = {foot_x / width, foot_y / height};
    return placement;
}

}  // namespace

const EnvironmentPropDefinition* environment_prop_definition(
    MaterialSpriteId sprite) noexcept {
    if (const auto* const definition = find_definition(
            water_room_material_slice().environment_props, sprite)) {
        return definition;
    }
    if (const auto* const definition = find_definition(
            lightning_room_material_slice().environment_props, sprite)) {
        return definition;
    }
    return find_definition(chaos_room_material_slice().environment_props, sprite);
}

Rectangle project_environment_prop_bounds(
    const EnvironmentPropDefinition& definition,
    const EnvironmentPropPlacement& placement,
    float width, float height) noexcept {
    const float alpha_x = placement.flip_x
        ? kFrameWidth - definition.alpha_bounds.x
            - definition.alpha_bounds.width
        : definition.alpha_bounds.x;
    return {
        placement.normalized_foot_position.x * width
            + (alpha_x - definition.foot_anchor.x) * placement.scale,
        placement.normalized_foot_position.y * height
            + (definition.alpha_bounds.y - definition.foot_anchor.y)
                * placement.scale,
        definition.alpha_bounds.width * placement.scale,
        definition.alpha_bounds.height * placement.scale,
    };
}

EnvironmentPropLayout environment_prop_layout(
    dungeon::DungeonElement ecology, float width, float height) noexcept {
    std::array<MaterialSpriteId, 5> sprites{};
    std::array<bool, 5> flips{};
    if (ecology == dungeon::DungeonElement::water) {
        const auto& props = water_room_material_slice().props;
        sprites = {{
            props[static_cast<std::size_t>(WaterRoomPropId::lantern)].sprite,
            props[static_cast<std::size_t>(WaterRoomPropId::wall)].sprite,
            props[static_cast<std::size_t>(WaterRoomPropId::coral)].sprite,
            props[static_cast<std::size_t>(WaterRoomPropId::coral)].sprite,
            props[static_cast<std::size_t>(WaterRoomPropId::grate)].sprite,
        }};
        flips[3] = true;
    } else if (ecology == dungeon::DungeonElement::lightning) {
        const auto& props = lightning_room_material_slice().props;
        sprites = {{
            props[static_cast<std::size_t>(LightningRoomPropId::arc_lamp)].sprite,
            props[static_cast<std::size_t>(LightningRoomPropId::wall)].sprite,
            props[static_cast<std::size_t>(LightningRoomPropId::capacitor_bank)].sprite,
            props[static_cast<std::size_t>(LightningRoomPropId::grounding_rod)].sprite,
            props[static_cast<std::size_t>(LightningRoomPropId::capacitor_bank)].sprite,
        }};
    } else if (ecology == dungeon::DungeonElement::chaos) {
        const auto& props = chaos_room_material_slice().props;
        sprites = {{
            props[static_cast<std::size_t>(ChaosRoomPropId::rift_lantern)].sprite,
            props[static_cast<std::size_t>(ChaosRoomPropId::wall)].sprite,
            props[static_cast<std::size_t>(ChaosRoomPropId::anomaly_condenser)].sprite,
            props[static_cast<std::size_t>(ChaosRoomPropId::warning_obelisk)].sprite,
            props[static_cast<std::size_t>(ChaosRoomPropId::anomaly_condenser)].sprite,
        }};
    } else {
        return {};
    }

    EnvironmentPropLayout layout{};
    layout.count = sprites.size();
    for (std::size_t index{}; index < layout.count; ++index) {
        layout.props[index] = clamped_placement(sprites[index],
            kNormalizedFootPositions[index], flips[index], width, height);
    }
    return layout;
}

}  // namespace arpg::platform
