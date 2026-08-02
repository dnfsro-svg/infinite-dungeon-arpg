#include "environment_prop_layout.hpp"

#include "chaos_room_material_slice.hpp"
#include "dungeon_view_math.hpp"
#include "fire_room_material_slice.hpp"
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
constexpr std::array<combat::Vec3, 5> kLegacyWorldFootPositions{{
    {-8.5F, -3.4F, 0.0F}, {8.5F, -3.4F, 0.0F},
    {-8.1F, 3.0F, 0.0F}, {7.0F, 3.0F, 0.0F},
    {-5.0F, 4.2F, 0.0F},
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

MaterialSpriteId room_prop_sprite(dungeon::DungeonElement ecology,
    combat::RoomPropKind prop) noexcept {
    using combat::RoomPropKind;
    if (ecology == dungeon::DungeonElement::fire) {
        const FireRoomMaterialSlice& slice = fire_room_material_slice();
        switch (prop) {
        case RoomPropKind::torch:
            return slice.props[static_cast<std::size_t>(
                FireRoomPropId::torch)].sprite;
        case RoomPropKind::banner:
            return slice.props[static_cast<std::size_t>(
                FireRoomPropId::banner)].sprite;
        case RoomPropKind::weapon_rack:
            return slice.props[static_cast<std::size_t>(
                FireRoomPropId::weapon_rack)].sprite;
        case RoomPropKind::bone_pile:
            return slice.props[static_cast<std::size_t>(
                FireRoomPropId::bone_pile)].sprite;
        case RoomPropKind::crate:
            return slice.props[static_cast<std::size_t>(
                FireRoomPropId::breakable_crate)].sprite;
        case RoomPropKind::brazier:
            return slice.props[static_cast<std::size_t>(
                FireRoomPropId::solid_brazier)].sprite;
        default: return MaterialSpriteId::missing;
        }
    }
    if (ecology == dungeon::DungeonElement::water) {
        const auto& props = water_room_material_slice().props;
        switch (prop) {
        case RoomPropKind::lantern:
            return props[static_cast<std::size_t>(
                WaterRoomPropId::lantern)].sprite;
        case RoomPropKind::coral:
        case RoomPropKind::crate:
            return props[static_cast<std::size_t>(
                WaterRoomPropId::coral)].sprite;
        case RoomPropKind::grate:
            return props[static_cast<std::size_t>(
                WaterRoomPropId::grate)].sprite;
        default: return MaterialSpriteId::missing;
        }
    }
    if (ecology == dungeon::DungeonElement::lightning) {
        const auto& props = lightning_room_material_slice().props;
        switch (prop) {
        case RoomPropKind::arc_lamp:
            return props[static_cast<std::size_t>(
                LightningRoomPropId::arc_lamp)].sprite;
        case RoomPropKind::capacitor_bank:
        case RoomPropKind::crate:
            return props[static_cast<std::size_t>(
                LightningRoomPropId::capacitor_bank)].sprite;
        case RoomPropKind::grounding_rod:
            return props[static_cast<std::size_t>(
                LightningRoomPropId::grounding_rod)].sprite;
        default: return MaterialSpriteId::missing;
        }
    }
    if (ecology == dungeon::DungeonElement::chaos) {
        const auto& props = chaos_room_material_slice().props;
        switch (prop) {
        case RoomPropKind::rift_lantern:
            return props[static_cast<std::size_t>(
                ChaosRoomPropId::rift_lantern)].sprite;
        case RoomPropKind::anomaly_condenser:
        case RoomPropKind::crate:
            return props[static_cast<std::size_t>(
                ChaosRoomPropId::anomaly_condenser)].sprite;
        case RoomPropKind::warning_obelisk:
            return props[static_cast<std::size_t>(
                ChaosRoomPropId::warning_obelisk)].sprite;
        default: return MaterialSpriteId::missing;
        }
    }
    return MaterialSpriteId::missing;
}

float authored_prop_scale(MaterialSpriteId sprite) noexcept {
    if (sprite == MaterialSpriteId::missing) return 0.0F;
    if (const EnvironmentPropDefinition* const definition =
            environment_prop_definition(sprite)) {
        return definition->recommended_scale;
    }
    return 0.72F;
}

bool append_environment_prop(EnvironmentPropLayout& layout,
    dungeon::DungeonElement ecology,
    const combat::RoomEnvironmentRecord& record,
    EnvironmentPropVisualState visual_state) noexcept {
    const MaterialSpriteId sprite = room_prop_sprite(ecology, record.prop);
    if (sprite == MaterialSpriteId::missing) return true;
    if (layout.count >= layout.props.size()) return false;
    const float authored_scale = authored_prop_scale(sprite);
    const float blueprint_scale = static_cast<float>(record.scale_bp)
        / 10000.0F;
    layout.props[layout.count++] = {
        sprite,
        record.anchor,
        record.mirror_x,
        authored_scale * blueprint_scale,
        record.ordinal,
        record.quarter_turns,
        record.obstacle.kind,
        record.obstacle.bounds,
        visual_state,
    };
    return true;
}

}  // namespace

EnvironmentPropDrawStyle environment_prop_draw_style(
    const EnvironmentPropPlacement& placement) noexcept {
    EnvironmentPropDrawStyle style{};
    // quarter_turns is a sealed world-space orientation used by placement and
    // collision. Environment art is authored as an upright 2.5D billboard, so
    // rotating the screen quad would lay torches, banners, and obelisks flat.
    if (placement.visual_state
            == EnvironmentPropVisualState::broken_obstacle) {
        style.tint = Color{113U, 97U, 83U, 211U};
        style.draw_break_marker = true;
    }
    return style;
}

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
    const CombatCameraView camera = make_combat_camera_view(
        {}, width, height);
    return project_environment_prop_bounds(
        definition, placement, camera, width, height);
}

ProjectedEnvironmentProp project_environment_prop(
    const EnvironmentPropPlacement& placement,
    const CombatCameraView& camera,
    float width, float height) noexcept {
    const RenderProjection projected = project_render_world(
        placement.world_foot_position.x,
        placement.world_foot_position.y,
        placement.world_foot_position.z,
        camera, width, height);
    return {{projected.x, projected.ground_y},
        placement.scale * projected.scale};
}

Rectangle project_environment_prop_bounds(
    const EnvironmentPropDefinition& definition,
    const EnvironmentPropPlacement& placement,
    const CombatCameraView& camera,
    float width, float height) noexcept {
    const ProjectedEnvironmentProp projected = project_environment_prop(
        placement, camera, width, height);
    const float alpha_x = placement.flip_x
        ? kFrameWidth - definition.alpha_bounds.x
            - definition.alpha_bounds.width
        : definition.alpha_bounds.x;
    return {
        projected.foot_position.x
            + (alpha_x - definition.foot_anchor.x) * projected.scale,
        projected.foot_position.y
            + (definition.alpha_bounds.y - definition.foot_anchor.y)
                * projected.scale,
        definition.alpha_bounds.width * projected.scale,
        definition.alpha_bounds.height * projected.scale,
    };
}

EnvironmentPropLayout environment_prop_layout(
    const dungeon::DungeonRenderSnapshot& world) noexcept {
    EnvironmentPropLayout layout{};
    static_assert(dungeon::kVisibleEnvironmentCapacity
        <= std::tuple_size_v<decltype(layout.props)>);
    if (world.environment.count > world.environment.records.size()) {
        layout.status = EnvironmentPropLayoutStatus::capacity_fault;
        return layout;
    }
    const std::size_t count = world.environment.count;
    for (std::size_t index = 0U; index < count; ++index) {
        const combat::RoomEnvironmentRecord& record =
            world.environment.records[index];
        EnvironmentPropVisualState visual_state =
            EnvironmentPropVisualState::decoration;
        if (record.obstacle.kind != combat::RoomObstacleKind::none) {
            const dungeon::EnvironmentObstacleRenderSnapshot& obstacle =
                world.environment_obstacles[index];
            if (!obstacle.present || obstacle.ordinal != record.ordinal
                    || obstacle.kind != record.obstacle.kind
                    || obstacle.max_hp != record.obstacle.max_hp
                    || obstacle.hp > obstacle.max_hp
                    || (obstacle.kind == combat::RoomObstacleKind::solid
                        && !obstacle.intact)
                    || (obstacle.kind == combat::RoomObstacleKind::breakable
                        && obstacle.intact && obstacle.hp == 0U)) {
                return EnvironmentPropLayout{{}, 0U,
                    EnvironmentPropLayoutStatus::invalid_obstacle_state};
            }
            visual_state = obstacle.intact
                ? EnvironmentPropVisualState::intact_obstacle
                : EnvironmentPropVisualState::broken_obstacle;
        }
        if (!append_environment_prop(
                layout, world.ecology, record, visual_state)) {
            return EnvironmentPropLayout{{}, 0U,
                EnvironmentPropLayoutStatus::capacity_fault};
        }
    }
    return layout;
}

EnvironmentPropLayout environment_prop_layout(
    dungeon::DungeonElement ecology,
    const dungeon::VisibleEnvironmentSet& visible) noexcept {
    EnvironmentPropLayout layout{};
    if (visible.count > visible.records.size()) {
        layout.status = EnvironmentPropLayoutStatus::capacity_fault;
        return layout;
    }
    const std::size_t count = visible.count;
    for (std::size_t index = 0U; index < count; ++index) {
        const combat::RoomEnvironmentRecord& record = visible.records[index];
        const EnvironmentPropVisualState visual_state =
            record.obstacle.kind == combat::RoomObstacleKind::none
            ? EnvironmentPropVisualState::decoration
            : EnvironmentPropVisualState::intact_obstacle;
        if (!append_environment_prop(layout, ecology, record, visual_state)) {
            return EnvironmentPropLayout{{}, 0U,
                EnvironmentPropLayoutStatus::capacity_fault};
        }
    }
    return layout;
}

EnvironmentPropLayout environment_prop_layout(
    dungeon::DungeonElement ecology, float width, float height) noexcept {
    static_cast<void>(width);
    static_cast<void>(height);
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
    for (std::size_t index{}; index < sprites.size(); ++index) {
        const EnvironmentPropDefinition* const definition =
            environment_prop_definition(sprites[index]);
        if (definition == nullptr) return {};
        layout.props[layout.count++] = {
            sprites[index], kLegacyWorldFootPositions[index], flips[index],
            definition->recommended_scale};
    }
    return layout;
}

}  // namespace arpg::platform
