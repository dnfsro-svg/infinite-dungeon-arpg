#include "environment_prop_layout.hpp"

#include "chaos_room_material_slice.hpp"
#include "dungeon_view_math.hpp"
#include "lightning_room_material_slice.hpp"
#include "render_layout.hpp"
#include "water_room_material_slice.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace arpg::platform {
namespace {

constexpr float kFrameWidth = 256.0F;
constexpr float kSafeInset = 8.25F;
constexpr float kExclusionGap = 0.25F;
constexpr std::size_t kCenterPlacementIndex = 4U;
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

float intersection_area(Rectangle left, Rectangle right) noexcept {
    const float width = std::max(0.0F,
        std::min(left.x + left.width, right.x + right.width)
            - std::max(left.x, right.x));
    const float height = std::max(0.0F,
        std::min(left.y + left.height, right.y + right.height)
            - std::max(left.y, right.y));
    return width * height;
}

bool placement_is_safe(const EnvironmentPropLayout& layout,
    std::size_t placement_index, const EnvironmentPropPlacement& candidate,
    Rectangle hole_bounds, float width, float height) noexcept {
    const EnvironmentPropDefinition* const definition =
        environment_prop_definition(candidate.sprite);
    if (definition == nullptr) return false;
    const Rectangle candidate_bounds = project_environment_prop_bounds(
        *definition, candidate, width, height);
    const float bottom = height - std::max(72.0F, 0.12F * height)
        - kSafeInset;
    if (candidate_bounds.x < kSafeInset
        || candidate_bounds.y < kSafeInset
        || candidate_bounds.x + candidate_bounds.width > width - kSafeInset
        || candidate_bounds.y + candidate_bounds.height > bottom
        || intersection_area(candidate_bounds, hole_bounds) > 0.0F) {
        return false;
    }
    for (std::size_t index{}; index < layout.count; ++index) {
        if (index == placement_index) continue;
        const EnvironmentPropDefinition* const other_definition =
            environment_prop_definition(layout.props[index].sprite);
        if (other_definition == nullptr) return false;
        const Rectangle other_bounds = project_environment_prop_bounds(
            *other_definition, layout.props[index], width, height);
        const float smaller_area = std::min(
            candidate_bounds.width * candidate_bounds.height,
            other_bounds.width * other_bounds.height);
        if (intersection_area(candidate_bounds, other_bounds)
            > smaller_area * 0.15F) {
            return false;
        }
    }
    return true;
}

void exclude_gameplay_hole(EnvironmentPropLayout& layout,
    dungeon::DungeonElement ecology, float width, float height) noexcept {
    if (layout.count <= kCenterPlacementIndex
        || width <= 0.0F || height <= 0.0F) {
        return;
    }
    MaterialSpriteId hole_sprite = MaterialSpriteId::missing;
    if (ecology == dungeon::DungeonElement::water) {
        hole_sprite = water_room_material_slice()
            .props[static_cast<std::size_t>(WaterRoomPropId::hole)].sprite;
    } else if (ecology == dungeon::DungeonElement::lightning) {
        hole_sprite = lightning_room_material_slice()
            .props[static_cast<std::size_t>(LightningRoomPropId::hole)].sprite;
    } else if (ecology == dungeon::DungeonElement::chaos) {
        hole_sprite = chaos_room_material_slice()
            .props[static_cast<std::size_t>(ChaosRoomPropId::hole)].sprite;
    }
    const EnvironmentPropDefinition* const hole_definition =
        environment_prop_definition(hole_sprite);
    const EnvironmentPropDefinition* const center_definition =
        environment_prop_definition(
            layout.props[kCenterPlacementIndex].sprite);
    if (hole_definition == nullptr || center_definition == nullptr) return;

    const RenderProjection hole_projection = project_render_world(
        kHoleCenter.x, kHoleCenter.y, kHoleCenter.z, width, height);
    const EnvironmentPropPlacement hole_placement{
        hole_sprite,
        {hole_projection.x / width, hole_projection.ground_y / height},
        false,
        kEnvironmentGameplayHoleScale * hole_projection.scale,
    };
    const Rectangle hole_bounds = project_environment_prop_bounds(
        *hole_definition, hole_placement, width, height);
    const Rectangle center_bounds = project_environment_prop_bounds(
        *center_definition, layout.props[kCenterPlacementIndex], width, height);
    if (intersection_area(center_bounds, hole_bounds) <= 0.0F) return;

    struct Translation final {
        float x{};
        float y{};
        float distance{};
    };
    std::array<Translation, 4> translations{{
        {hole_bounds.x - (center_bounds.x + center_bounds.width)
                - kExclusionGap,
            0.0F, 0.0F},
        {hole_bounds.x + hole_bounds.width - center_bounds.x
                + kExclusionGap,
            0.0F, 0.0F},
        {0.0F,
            hole_bounds.y - (center_bounds.y + center_bounds.height)
                - kExclusionGap,
            0.0F},
        {0.0F,
            hole_bounds.y + hole_bounds.height - center_bounds.y
                + kExclusionGap,
            0.0F},
    }};
    for (auto& translation : translations) {
        translation.distance = std::fabs(translation.x)
            + std::fabs(translation.y);
    }
    std::sort(translations.begin(), translations.end(),
        [](const Translation& left, const Translation& right) noexcept {
            return left.distance < right.distance;
        });
    for (const Translation translation : translations) {
        EnvironmentPropPlacement candidate =
            layout.props[kCenterPlacementIndex];
        candidate.normalized_foot_position.x += translation.x / width;
        candidate.normalized_foot_position.y += translation.y / height;
        if (placement_is_safe(layout, kCenterPlacementIndex, candidate,
                hole_bounds, width, height)) {
            layout.props[kCenterPlacementIndex] = candidate;
            return;
        }
    }
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
    exclude_gameplay_hole(layout, ecology, width, height);
    return layout;
}

}  // namespace arpg::platform
