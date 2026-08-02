#include "combat_renderer.hpp"

#include "combat/room_bounds.hpp"
#include "chaos_room_material_slice.hpp"
#include "dungeon_view_math.hpp"
#include "environment_prop_layout.hpp"
#include "environment_render_plan.hpp"
#include "fire_room_material_slice.hpp"
#include "material_animation.hpp"
#include "material_loot_view.hpp"
#include "lightning_room_material_slice.hpp"
#include "render_layout.hpp"
#include "room_background_render_plan.hpp"
#include "ui_text_renderer.hpp"
#include "ui_typography.hpp"
#include "water_room_material_slice.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace arpg::platform {
namespace {

Vector2 lerp(Vector2 from, Vector2 to, float amount) noexcept {
    return {
        from.x + (to.x - from.x) * amount,
        from.y + (to.y - from.y) * amount,
    };
}

void draw_graybox_room(dungeon::DungeonElement ecology) noexcept {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const Vector2 back_left{width * 0.20F, height * 0.22F};
    const Vector2 back_right{width * 0.80F, height * 0.22F};
    const Vector2 floor_left{width * 0.04F, height * 0.92F};
    const Vector2 floor_right{width * 0.96F, height * 0.92F};
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
        Color{13, 17, 27, 255}, Color{28, 32, 43, 255});
    DrawRectangle(static_cast<int>(back_left.x), 0,
        static_cast<int>(back_right.x - back_left.x),
        static_cast<int>(back_left.y), Color{31, 37, 51, 255});
    DrawTriangle({0.0F, 0.0F}, floor_left, back_left, Color{22, 27, 39, 255});
    DrawTriangle({0.0F, 0.0F}, {0.0F, height}, floor_left, Color{22, 27, 39, 255});
    DrawTriangle({width, 0.0F}, back_right, floor_right, Color{22, 27, 39, 255});
    DrawTriangle({width, 0.0F}, floor_right, {width, height}, Color{22, 27, 39, 255});
    const Rgba8 tint = ecosystem_tint(ecology);
    const Color floor{tint.r, tint.g, tint.b, 255};
    DrawTriangle(back_left, floor_left, floor_right, floor);
    DrawTriangle(back_left, floor_right, back_right, floor);
    const Color grid{87, 99, 119, 110};
    for (int column = 0; column <= 10; ++column) {
        const float amount = static_cast<float>(column) / 10.0F;
        DrawLineEx(lerp(back_left, back_right, amount),
            lerp(floor_left, floor_right, amount), 1.0F, grid);
    }
    for (int row = 0; row <= 8; ++row) {
        const float linear = static_cast<float>(row) / 8.0F;
        const float perspective = linear * linear;
        DrawLineEx(lerp(back_left, floor_left, perspective),
            lerp(back_right, floor_right, perspective), 1.0F, grid);
    }
}

bool draw_environment_room(const MaterialPack& material_pack,
    dungeon::DungeonElement ecology,
    const CombatCameraView& camera) noexcept {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
        Color{9, 12, 20, 255});
    const RoomBackgroundWorldTilePlan plan =
        room_background_world_tile_plan(ecology, camera);
    if (!plan.valid || plan.count == 0U
            || !material_pack.available(plan.atlas)) return false;
    std::array<ProjectedRoomBackgroundWorldTile,
        kRoomBackgroundWorldTileCapacity> projected_tiles{};
    for (std::size_t index{}; index < plan.count; ++index) {
        projected_tiles[index] = project_room_background_world_tile(
            plan.tiles[index], camera, width, height);
        if (!projected_tiles[index].valid) return false;
    }
    for (std::size_t index{}; index < plan.count; ++index) {
        if (!material_pack.draw_frame_quad(plan.atlas,
                plan.tiles[index].source,
                projected_tiles[index].destination)) return false;
    }
    return true;
}

void draw_environment_room_props(const MaterialPack& material_pack,
    const dungeon::DungeonRenderSnapshot& world,
    const CombatCameraView& camera,
    float width, float height) noexcept {
    const EnvironmentPropLayout layout = environment_prop_layout(world);
    if (layout.status != EnvironmentPropLayoutStatus::ok) return;
    for (std::size_t index{}; index < layout.count; ++index) {
        const EnvironmentPropPlacement& placement = layout.props[index];
        const ProjectedEnvironmentProp projected = project_environment_prop(
            placement, camera, width, height);
        const EnvironmentPropDrawStyle style =
            environment_prop_draw_style(placement);
        if (!material_pack.draw_transformed(placement.sprite,
                projected.foot_position, placement.flip_x, projected.scale,
                style.rotation_degrees, style.tint)) {
            const EnvironmentPropDefinition* const definition =
                environment_prop_definition(placement.sprite);
            if (definition != nullptr) {
                const Rectangle bounds = project_environment_prop_bounds(
                    *definition, placement, camera, width, height);
                DrawRectangleLinesEx(bounds, 2.0F, Color{35, 48, 62, 72});
            }
        }
        if (style.draw_break_marker) {
            const float half = 11.0F * projected.scale;
            const float thickness = (std::max)(1.0F,
                2.0F * projected.scale);
            const Color marker{255U, 181U, 92U, 224U};
            DrawLineEx({projected.foot_position.x - half,
                    projected.foot_position.y - half},
                {projected.foot_position.x + half,
                    projected.foot_position.y + half},
                thickness, marker);
            DrawLineEx({projected.foot_position.x - half,
                    projected.foot_position.y + half},
                {projected.foot_position.x + half,
                    projected.foot_position.y - half},
                thickness, marker);
        }
    }
}

MaterialSpriteId hole_sprite(dungeon::DungeonElement ecology) noexcept {
    if (ecology == dungeon::DungeonElement::water) {
        return MaterialSpriteId::water_hole;
    }
    if (ecology == dungeon::DungeonElement::lightning) {
        return MaterialSpriteId::lightning_hole;
    }
    if (ecology == dungeon::DungeonElement::chaos) {
        return MaterialSpriteId::chaos_hole;
    }
    return MaterialSpriteId::environment_hole;
}

void draw_doors(const dungeon::DungeonRenderSnapshot& snapshot,
    const CombatCameraView& camera,
    float width, float height, const MaterialPack& material_pack, Font hud_font,
    bool hud_font_ready) noexcept {
    const DoorVisualMode mode = door_visual_mode(snapshot.phase,
        snapshot.has_active_room, snapshot.exits_unlocked);
    if (mode == DoorVisualMode::hidden) {
        return;
    }
    for (std::size_t index = 0; index < snapshot.doors.size(); ++index) {
        const dungeon::DoorRenderSnapshot& door = snapshot.doors[index];
        const RenderProjection projected = project_render_world(
            door.position.x, door.position.y, door.position.z,
            camera, width, height);
        const DoorRenderDecision visual = door_render_decision(
            mode, door.direction, snapshot.full_clear);
        const Color body_tint{visual.body_tint.r, visual.body_tint.g,
            visual.body_tint.b, visual.body_tint.a};
        const Color text_color{visual.text.r, visual.text.g, visual.text.b, visual.text.a};
        const float door_width = 82.0F * projected.scale;
        const float door_height = 70.0F * projected.scale;
        const Rectangle frame{projected.x - door_width * 0.5F,
            projected.ground_y - door_height, door_width, door_height};
        if (!material_pack.draw(visual.sprite, {projected.x, projected.ground_y},
                false, 0.72F * projected.scale, body_tint)) {
            DrawRectangleLinesEx(frame, 5.0F * projected.scale, text_color);
        }
        if (visual.draw_lock_marker) {
            const Rectangle lock{frame.x + frame.width - 16.0F * projected.scale,
                frame.y + 10.0F * projected.scale,
                12.0F * projected.scale, 14.0F * projected.scale};
            const Color steel{35, 48, 62, 245};
            DrawRectangle(static_cast<int>(std::round(lock.x)),
                static_cast<int>(std::round(lock.y)),
                static_cast<int>(std::round(lock.width)),
                static_cast<int>(std::round(lock.height)), steel);
            DrawCircleSectorLines({lock.x + lock.width * 0.5F,
                lock.y + 8.0F * projected.scale}, 8.0F * projected.scale,
                180.0F, 360.0F, 8, steel);
        }
        const DoorArrowGeometry arrow = door_arrow_geometry(door.direction,
            projected.x, frame.y + 35.0F * projected.scale, projected.scale);
        DrawLineEx(arrow.tail, arrow.tip, arrow.thickness, text_color);
        DrawLineEx(arrow.tip, arrow.head_left, arrow.thickness, text_color);
        DrawLineEx(arrow.tip, arrow.head_right, arrow.thickness, text_color);
        if (mode == DoorVisualMode::open) {
            const Color edge = Fade(text_color, 0.32F);
            DrawLineEx({frame.x + 5.0F * projected.scale, frame.y},
                {frame.x + 5.0F * projected.scale, frame.y + frame.height},
                2.0F * projected.scale, edge);
            DrawLineEx({frame.x + frame.width - 5.0F * projected.scale, frame.y},
                {frame.x + frame.width - 5.0F * projected.scale,
                    frame.y + frame.height},
                2.0F * projected.scale, edge);
        }
        if (visual.draw_full_clear_decoration) {
            const Color clear_glow = Fade(text_color, 0.82F);
            DrawCircleLines(static_cast<int>(std::round(projected.x)),
                static_cast<int>(std::round(frame.y + frame.height * 0.46F)),
                13.0F * projected.scale, clear_glow);
            DrawCircleV({projected.x,
                frame.y + frame.height * 0.46F},
                3.0F * projected.scale, clear_glow);
        }
        if (door.abyss) {
            const Vector2 marker{
                frame.x + frame.width - 12.0F * projected.scale,
                frame.y + 13.0F * projected.scale,
            };
            DrawPoly(marker, 4, 9.0F * projected.scale, 45.0F,
                Color{231, 46, 157, 245});
            DrawPolyLinesEx(marker, 4, 9.0F * projected.scale, 45.0F,
                2.0F * projected.scale, Color{255, 155, 221, 255});
        }
        const MonsterLabelTextStyle scene_text = monster_label_text_style(
            ui_viewport_scale(static_cast<int>(width),
                static_cast<int>(height)));
        const int label_size = scene_text.role_font_size;
        const Font label_font = hud_font_ready ? hud_font : GetFontDefault();
        const Vector2 measured = MeasureTextEx(label_font, visual.label,
            static_cast<float>(label_size), 1.0F);
        const Vector2 label_position{
            projected.x - measured.x * 0.5F,
            frame.y - static_cast<float>(label_size) - 4.0F};
        if (hud_font_ready && IsFontValid(label_font)) {
            draw_crisp_ui_text(label_font, visual.label, label_position,
                static_cast<float>(label_size), 1.0F, text_color);
        } else {
            DrawText(visual.label, static_cast<int>(std::round(label_position.x)),
                static_cast<int>(std::round(label_position.y)),
                label_size, text_color);
        }
    }
}

void draw_environment_hazards(
    const dungeon::DungeonRenderSnapshot& snapshot,
    const CombatCameraView& camera, float width, float height) noexcept {
    if (!snapshot.has_combat) return;
    for (const combat::HazardSnapshot& hazard : snapshot.combat.hazards) {
        const EnvironmentHazardVisual visual =
            environment_hazard_visual(hazard);
        if (visual.mode == EnvironmentHazardVisualMode::hidden) continue;

        const RenderProjection center = project_render_world(
            visual.center.x, visual.center.y, 0.0F,
            camera, width, height);
        const RenderProjection x_edge = project_render_world(
            visual.center.x + visual.radius, visual.center.y, 0.0F,
            camera, width, height);
        const RenderProjection y_edge = project_render_world(
            visual.center.x, visual.center.y + visual.radius, 0.0F,
            camera, width, height);
        const float radius_x = std::max(
            1.0F, std::fabs(x_edge.x - center.x));
        const float radius_y = std::max(
            1.0F, std::fabs(y_edge.ground_y - center.ground_y));
        const Color fill{visual.fill.r, visual.fill.g,
            visual.fill.b, visual.fill.a};
        const Color outline{visual.outline.r, visual.outline.g,
            visual.outline.b, visual.outline.a};
        DrawEllipse(static_cast<int>(center.x),
            static_cast<int>(center.ground_y), radius_x, radius_y, fill);
        DrawEllipseLines(static_cast<int>(center.x),
            static_cast<int>(center.ground_y), radius_x, radius_y, outline);
        if (visual.mode == EnvironmentHazardVisualMode::warning) {
            DrawEllipseLines(static_cast<int>(center.x),
                static_cast<int>(center.ground_y),
                std::max(1.0F, radius_x - 4.0F),
                std::max(1.0F, radius_y - 2.0F), outline);
        }
    }
}

Color ground_item_color(items::ItemRarity rarity) noexcept {
    switch (rarity) {
    case items::ItemRarity::normal: return Color{222, 228, 236, 255};
    case items::ItemRarity::magic: return Color{70, 139, 255, 255};
    case items::ItemRarity::rare: return Color{255, 193, 52, 255};
    }
    return RAYWHITE;
}

void draw_ground_item_shape(items::ItemSlot slot, Vector2 center,
    float scale, Color color) noexcept {
    const float size = 11.0F * scale;
    switch (slot) {
    case items::ItemSlot::weapon:
        DrawLineEx({center.x - size, center.y + size},
            {center.x + size, center.y - size}, 4.0F * scale, color);
        DrawLineEx({center.x - size * 0.65F, center.y + size * 0.15F},
            {center.x - size * 0.1F, center.y + size * 0.7F},
            3.0F * scale, color);
        break;
    case items::ItemSlot::helmet:
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
            size, color);
        DrawLineEx({center.x - size, center.y + size * 0.25F},
            {center.x + size, center.y + size * 0.25F}, 3.0F * scale, color);
        break;
    case items::ItemSlot::chest:
        DrawRectangleLinesEx({center.x - size, center.y - size * 0.8F,
            size * 2.0F, size * 1.6F}, 3.0F * scale, color);
        DrawLineEx({center.x, center.y - size * 0.8F},
            {center.x, center.y + size * 0.8F}, 2.0F * scale, color);
        break;
    case items::ItemSlot::gloves:
        DrawCircleLines(static_cast<int>(center.x - size * 0.45F),
            static_cast<int>(center.y), size * 0.55F, color);
        DrawCircleLines(static_cast<int>(center.x + size * 0.45F),
            static_cast<int>(center.y), size * 0.55F, color);
        break;
    case items::ItemSlot::boots:
        DrawLineEx({center.x - size * 0.55F, center.y - size},
            {center.x - size * 0.55F, center.y + size * 0.55F},
            5.0F * scale, color);
        DrawLineEx({center.x - size * 0.55F, center.y + size * 0.55F},
            {center.x + size, center.y + size * 0.55F},
            5.0F * scale, color);
        break;
    case items::ItemSlot::accessory:
        DrawPolyLines(center, 4, size, 45.0F, color);
        DrawCircleV(center, 2.0F * scale, color);
        break;
    case items::ItemSlot::count:
        break;
    }
}

struct GroundItemRange final {
    const dungeon::GroundItemSnapshot* data{};
    std::size_t count{};
};

GroundItemRange ground_item_range(
    const dungeon::DungeonRenderSnapshot& snapshot) noexcept {
    return {snapshot.equipment.data(), (std::min)(
        static_cast<std::size_t>(snapshot.equipment_count),
        snapshot.equipment.size())};
}

// Compatibility adapter used only by material validation helpers.
GroundItemRange ground_item_range(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    return {snapshot.ground_items.data(), (std::min)(
        static_cast<std::size_t>(snapshot.ground_item_count),
        snapshot.ground_items.size())};
}

void draw_ground_items(GroundItemRange items,
    settings::LootFilterMode mode,
    const MaterialPack& material_pack, const CombatCameraView& camera,
    float width, float height) noexcept {
    for (std::size_t index = 0U; index < items.count; ++index) {
        const dungeon::GroundItemSnapshot& item = items.data[index];
        if (!ground_loot_visible(item, mode)) continue;
        const RenderProjection projected = project_render_world(
            item.position.x, item.position.y, item.position.z,
            camera, width, height);
        const Vector2 center{projected.x,
            projected.ground_y - 13.0F * projected.scale};
        const Color color = ground_item_color(item.rarity);
        DrawEllipse(static_cast<int>(projected.x),
            static_cast<int>(projected.ground_y + 2.0F),
            17.0F * projected.scale, 6.0F * projected.scale,
            Fade(color, 0.24F));
        const bool rarity_drawn = material_pack.draw(
            ground_loot_rarity_sprite(item.rarity,
                item.source == dungeon::GroundItemSource::abyss_chest),
            center, false, 0.34F * projected.scale);
        const bool item_drawn = material_pack.draw(
            ground_loot_item_sprite(item.slot), center,
            false, 0.25F * projected.scale);
        if (!rarity_drawn || !item_drawn) {
            draw_ground_item_shape(item.slot, center, projected.scale, color);
        }
    }
}

void draw_secondary_loot_icon(combat::Vec3 position, Rgba8 rgba,
    MaterialSpriteId sprite, bool emphasized,
    const MaterialPack& material_pack, const CombatCameraView& camera,
    float width, float height) noexcept {
    const RenderProjection projected = project_render_world(
        position.x, position.y, position.z, camera, width, height);
    const Color color{rgba.r, rgba.g, rgba.b, rgba.a};
    const float radius = (emphasized ? 9.0F : 6.0F) * projected.scale;
    const Vector2 center{projected.x, projected.ground_y - radius};
    DrawEllipse(static_cast<int>(projected.x),
        static_cast<int>(projected.ground_y + 1.0F), radius * 1.6F,
        radius * 0.45F, Fade(color, 0.28F));
    if (emphasized) {
        DrawLineEx({center.x, center.y + radius},
            {center.x, center.y - 36.0F * projected.scale},
            2.0F * projected.scale, Fade(color, 0.65F));
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
            radius * 1.55F, Fade(color, 0.78F));
    }
    if (!material_pack.draw(sprite, center, false,
            (emphasized ? 0.24F : 0.20F) * projected.scale)) {
        DrawCircleV(center, radius, color);
    }
    DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
        radius, RAYWHITE);
}

void draw_ground_materials(const dungeon::DungeonRenderSnapshot& snapshot,
    const MaterialPack& material_pack, const CombatCameraView& camera,
    float width, float height) noexcept {
    const std::size_t material_count = (std::min)(
        static_cast<std::size_t>(snapshot.material_count),
        snapshot.materials.size());
    for (std::size_t index = 0U; index < material_count; ++index) {
        const dungeon::GroundMaterialSnapshot& material =
            snapshot.materials[index];
        if (items::material_definition(material.material) == nullptr) continue;
        draw_secondary_loot_icon(material.position,
            material_color(material.material),
            material_loot_sprite(material.material),
            material_is_emphasized(material.material),
            material_pack, camera, width, height);
    }
    const std::size_t potion_count = (std::min)(
        static_cast<std::size_t>(snapshot.health_potion_count),
        snapshot.health_potions.size());
    for (std::size_t index = 0U; index < potion_count; ++index) {
        draw_secondary_loot_icon(snapshot.health_potions[index].position,
            {255U, 48U, 48U, 255U}, MaterialSpriteId::health_potion, true,
            material_pack, camera, width, height);
    }
}

// Compatibility path used only by material validation helpers.
void draw_ground_materials(const dungeon::DungeonSnapshot& snapshot,
    const MaterialPack& material_pack, const CombatCameraView& camera,
    float width, float height) noexcept {
    const std::size_t material_count = (std::min)(
        static_cast<std::size_t>(snapshot.ground_material_count),
        snapshot.ground_materials.size());
    for (std::size_t index = 0U; index < material_count; ++index) {
        const dungeon::GroundMaterialSnapshot& material =
            snapshot.ground_materials[index];
        if (items::material_definition(material.material) == nullptr) continue;
        draw_secondary_loot_icon(material.position,
            material_color(material.material),
            material_loot_sprite(material.material),
            material_is_emphasized(material.material),
            material_pack, camera, width, height);
    }
    const std::size_t potion_count = (std::min)(
        static_cast<std::size_t>(snapshot.ground_health_potion_count),
        snapshot.ground_health_potions.size());
    for (std::size_t index = 0U; index < potion_count; ++index) {
        draw_secondary_loot_icon(snapshot.ground_health_potions[index].position,
            {255U, 48U, 48U, 255U}, MaterialSpriteId::health_potion, true,
            material_pack, camera, width, height);
    }
}

void draw_abyss(const dungeon::DungeonSnapshot& snapshot, float elapsed_seconds) noexcept {
    if (!snapshot.is_abyss) {
        return;
    }
    const float pulse = abyss_pulse_alpha(elapsed_seconds);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
        Fade(Color{125, 19, 92, 255}, 0.12F + pulse * 0.16F));
    DrawRectangleLinesEx({8.0F, 8.0F, static_cast<float>(GetScreenWidth() - 16),
        static_cast<float>(GetScreenHeight() - 16)}, 8.0F,
        Fade(Color{225, 47, 160, 255}, 0.20F + pulse * 0.45F));
    const AbyssHudValues values = abyss_hud_values(snapshot);
    if (values.visible) {
        const char* label = TextFormat("%s  |  %s",
            values.danger_label, values.rule_label);
        const int font_size = 20;
        DrawText(label, GetScreenWidth() / 2 - MeasureText(label, font_size) / 2,
            18, font_size, Color{255, 164, 221, 255});
    }
}

HoleVisualMode render_hole_visual_mode(
    const dungeon::DungeonRenderSnapshot& snapshot) noexcept {
    if (!snapshot.has_active_room || !snapshot.hole.present) {
        return HoleVisualMode::hidden;
    }
    switch (snapshot.phase) {
    case dungeon::RoomPhase::locked:
        return HoleVisualMode::sealed;
    case dungeon::RoomPhase::combat:
    case dungeon::RoomPhase::cleared:
    case dungeon::RoomPhase::awaiting_exit:
        return snapshot.exits_unlocked
            ? HoleVisualMode::ready : HoleVisualMode::sealed;
    case dungeon::RoomPhase::committing:
        return HoleVisualMode::busy;
    case dungeon::RoomPhase::faulted:
        return HoleVisualMode::faulted;
    case dungeon::RoomPhase::transitioning:
        return HoleVisualMode::hidden;
    }
    return HoleVisualMode::hidden;
}

void draw_hole(const dungeon::DungeonRenderSnapshot& snapshot,
    const MaterialPack& material_pack,
    const CombatCameraView& camera) noexcept {
    const HoleVisualMode hole = render_hole_visual_mode(snapshot);
    if (hole == HoleVisualMode::hidden) {
        return;
    }
    const HoleProjectedGeometry geometry = project_hole_geometry(
        snapshot.hole.position, camera,
        static_cast<float>(GetScreenWidth()),
        static_cast<float>(GetScreenHeight()));
    const int x = static_cast<int>(geometry.center.x);
    const int y = static_cast<int>(geometry.center.y);
    Color color{43, 25, 55, 255};
    if (hole == HoleVisualMode::ready) color = Color{230, 79, 186, 255};
    else if (hole == HoleVisualMode::busy) color = Color{255, 194, 74, 255};
    if (!material_pack.draw(hole_sprite(snapshot.ecology),
            geometry.center, false,
            kEnvironmentGameplayHoleScale
                * geometry.radius_x / 74.0F)) {
        DrawEllipse(static_cast<int>(geometry.center.x),
            static_cast<int>(geometry.center.y),
            geometry.radius_x, geometry.radius_y, Color{5, 2, 9, 235});
    }
    DrawEllipseLinesV(geometry.center,
        geometry.radius_x, geometry.radius_y, color);
    const char* label = hole == HoleVisualMode::sealed ? "SEALED"
        : hole == HoleVisualMode::ready ? "READY"
        : hole == HoleVisualMode::busy ? "SAVING" : "FAULTED";
    DrawText(label, x - MeasureText(label, 20) / 2, y - 12, 20, color);
    if (snapshot.has_combat && hole == HoleVisualMode::ready
        && player_in_hole_range(snapshot.combat.player.position,
            snapshot.hole.position, kHoleInteractionRadius)) {
        constexpr const char* kPrompt = "E: DESCEND";
        DrawText(kPrompt, x - MeasureText(kPrompt, 26) / 2, y + 36, 26, RAYWHITE);
    }
}

RoomBackgroundDrawRuntimeStatus room_background_status(
    const MaterialPack& material_pack,
    dungeon::DungeonElement ecology) noexcept {
    const RoomBackgroundRenderPlan plan = room_background_render_plan(ecology);
    return {
        ecology,
        plan.atlas,
        material_pack.available(plan.atlas),
        false,
        static_cast<std::uint16_t>(plan.source.width),
        static_cast<std::uint16_t>(plan.source.height),
        room_background_scale(static_cast<float>(GetScreenWidth()),
            static_cast<float>(GetScreenHeight())),
    };
}

MaterialResidencyRequest room_background_residency_request(
    dungeon::DungeonElement ecology) noexcept {
    MaterialResidencyRequest request = base_material_residency_request();
    switch (ecology) {
    case dungeon::DungeonElement::fire:
        request.require(MaterialAtlasId::fire_environment);
        request.require(MaterialAtlasId::fire_room_background);
        break;
    case dungeon::DungeonElement::water:
        request.require(MaterialAtlasId::water_environment);
        request.require(MaterialAtlasId::water_room_background);
        break;
    case dungeon::DungeonElement::lightning:
        request.require(MaterialAtlasId::lightning_environment);
        request.require(MaterialAtlasId::lightning_room_background);
        break;
    case dungeon::DungeonElement::chaos:
        request.require(MaterialAtlasId::chaos_environment);
        request.require(MaterialAtlasId::chaos_room_background);
        break;
    }
    return request;
}

}  // namespace

void CombatRenderer::draw_abyss_overlay(
    const dungeon::DungeonSnapshot& current) const noexcept {
    draw_abyss(current, static_cast<float>(GetTime()));
}

RoomBackgroundDrawRuntimeStatus CombatRenderer::draw_room_background_only(
    dungeon::DungeonElement ecology) noexcept {
    static_cast<void>(material_pack_.synchronize_residency(
        room_background_residency_request(ecology)));
    room_background_draw_status_ = room_background_status(material_pack_, ecology);
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const CombatCameraView camera = make_combat_camera_view(
        {}, width, height);
    room_background_draw_status_.drawn = room_background_draw_status_.resident
        && draw_environment_room(material_pack_, ecology, camera);
    if (!room_background_draw_status_.drawn) {
        draw_graybox_room(ecology);
    }
    return room_background_draw_status_;
}

GroundLootView CombatRenderer::draw_ground_loot_icons_only(
    const dungeon::DungeonSnapshot& snapshot,
    const CombatCameraView& camera) noexcept {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const GroundLootView ground_loot = build_ground_loot_view(
        snapshot, loot_filter_mode_, camera, width, height);

    Camera2D world_camera{};
    world_camera.zoom = 1.0F;
    BeginMode2D(world_camera);
    static_cast<void>(draw_room_background_only(snapshot.ecology));
    static_cast<void>(material_pack_.synchronize_residency(
        make_material_residency_request(snapshot)));
    draw_ground_materials(
        snapshot, material_pack_, camera, width, height);
    draw_ground_items(ground_item_range(snapshot), loot_filter_mode_, material_pack_,
        camera, width, height);
    EndMode2D();
    return ground_loot;
}

void CombatRenderer::draw_room(
    const dungeon::DungeonRenderSnapshot& current,
    const GroundLootView& ground_loot,
    const MaterialLootView& material_loot,
    const CombatCameraView& camera) noexcept {
    static_cast<void>(ground_loot);
    static_cast<void>(material_loot);
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    room_background_draw_status_ = room_background_status(
        material_pack_, current.ecology);
    const bool draw_material_background = room_background_draw_status_.resident
        && draw_environment_room(material_pack_, current.ecology, camera);
    room_background_draw_status_.drawn = draw_material_background;
    if (!draw_material_background) {
        draw_graybox_room(current.ecology);
    }
    draw_environment_hazards(current, camera, width, height);
    draw_environment_room_props(
        material_pack_, current, camera, width, height);
    draw_doors(current, camera, width, height, material_pack_, hud_renderer_.hud_font(),
        hud_renderer_.font_ready());
    draw_hole(current, material_pack_, camera);
    draw_ground_materials(
        current, material_pack_, camera, width, height);
    draw_ground_items(ground_item_range(current), loot_filter_mode_, material_pack_,
        camera, width, height);
}

}  // namespace arpg::platform
