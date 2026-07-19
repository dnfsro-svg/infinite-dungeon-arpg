#include "combat_renderer.hpp"

#include "combat/room_bounds.hpp"
#include "dungeon_view_math.hpp"
#include "environment_render_plan.hpp"
#include "material_animation.hpp"
#include "render_layout.hpp"

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
    dungeon::DungeonElement ecology) noexcept {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const float scale = (std::max)(width / 1024.0F, height / 704.0F);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
        Color{9, 12, 20, 255});
    return material_pack.draw(select_floor_sprite(ecology),
        {width * 0.5F, height}, false, scale);
}

bool can_draw_room_environment(const dungeon::DungeonSnapshot& snapshot,
    const MaterialPack& material_pack) noexcept {
    const DoorVisualMode door_mode = door_visual_mode(snapshot.phase,
        snapshot.has_active_room, snapshot.exits_open[0]);
    const HoleVisualMode hole_mode = hole_visual_mode(snapshot);
    return should_draw_material_environment({
        material_pack.can_draw(select_floor_sprite(snapshot.ecology)),
        material_pack.can_draw(select_door_sprite(snapshot.ecology)),
        material_pack.can_draw(MaterialSpriteId::environment_hole),
        door_mode != DoorVisualMode::hidden,
        hole_mode != HoleVisualMode::hidden,
    });
}

void draw_doors(const dungeon::DungeonSnapshot& snapshot,
    float width, float height, const MaterialPack& material_pack,
    bool draw_material_environment) noexcept {
    const DoorVisualMode mode = door_visual_mode(snapshot.phase,
        snapshot.has_active_room, snapshot.exits_open[0]);
    if (mode == DoorVisualMode::hidden) {
        return;
    }
    constexpr std::array<combat::Vec3, 4> kDoorCenters{{
        {0.0F, combat::room_bounds::min_y, 0.0F},
        {0.0F, combat::room_bounds::max_y, 0.0F},
        {combat::room_bounds::min_x, 0.0F, 0.0F},
        {combat::room_bounds::max_x, 0.0F, 0.0F},
    }};
    constexpr std::array<dungeon::ExitDirection, 4> kDirections{{
        dungeon::ExitDirection::up, dungeon::ExitDirection::down,
        dungeon::ExitDirection::left, dungeon::ExitDirection::right}};
    for (std::size_t index = 0; index < kDoorCenters.size(); ++index) {
        const RenderProjection projected = project_render_world(
            kDoorCenters[index].x, kDoorCenters[index].y, kDoorCenters[index].z,
            width, height);
        const DoorRenderDecision visual = door_render_decision(mode, kDirections[index]);
        const Color frame_color{visual.frame.r, visual.frame.g, visual.frame.b, visual.frame.a};
        const Color text_color{visual.text.r, visual.text.g, visual.text.b, visual.text.a};
        const float door_width = 82.0F * projected.scale;
        const float door_height = 70.0F * projected.scale;
        const Rectangle frame{projected.x - door_width * 0.5F,
            projected.ground_y - door_height, door_width, door_height};
        if (draw_material_environment) {
            static_cast<void>(material_pack.draw(select_door_sprite(snapshot.ecology),
                {projected.x, projected.ground_y}, false,
                0.72F * projected.scale));
        } else {
            DrawRectangleLinesEx(frame, 5.0F * projected.scale, frame_color);
        }
        if (visual.draw_locked_interior) {
            DrawRectangleRec({frame.x + 7.0F * projected.scale,
                frame.y + 7.0F * projected.scale,
                frame.width - 14.0F * projected.scale,
                frame.height - 7.0F * projected.scale},
                Color{visual.locked_interior.r, visual.locked_interior.g,
                    visual.locked_interior.b, visual.locked_interior.a});
        }
        const int font_size = static_cast<int>(22.0F * projected.scale);
        const int arrow_width = MeasureText(visual.arrow, font_size);
        DrawText(visual.arrow, static_cast<int>(projected.x) - arrow_width / 2,
            static_cast<int>(frame.y + 17.0F * projected.scale), font_size, text_color);
        if (abyss_door_marker(snapshot, kDirections[index])) {
            const Vector2 marker{
                frame.x + frame.width - 12.0F * projected.scale,
                frame.y + 13.0F * projected.scale,
            };
            DrawPoly(marker, 4, 9.0F * projected.scale, 45.0F,
                Color{231, 46, 157, 245});
            DrawPolyLinesEx(marker, 4, 9.0F * projected.scale, 45.0F,
                2.0F * projected.scale, Color{255, 155, 221, 255});
        }
        DrawText(visual.label, static_cast<int>(frame.x),
            static_cast<int>(frame.y - 15.0F * projected.scale),
            static_cast<int>(11.0F * projected.scale), text_color);
    }
}

void draw_environment_hazards(const dungeon::DungeonSnapshot& snapshot,
    float width, float height) noexcept {
    if (!snapshot.combat.has_value()) return;
    for (const combat::HazardSnapshot& hazard : snapshot.combat->hazards) {
        const EnvironmentHazardVisual visual =
            environment_hazard_visual(hazard);
        if (visual.mode == EnvironmentHazardVisualMode::hidden) continue;

        const RenderProjection center = project_render_world(
            visual.center.x, visual.center.y, 0.0F, width, height);
        const RenderProjection x_edge = project_render_world(
            visual.center.x + visual.radius, visual.center.y, 0.0F,
            width, height);
        const RenderProjection y_edge = project_render_world(
            visual.center.x, visual.center.y + visual.radius, 0.0F,
            width, height);
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

const dungeon::GroundItemSnapshot* ground_item_with_ordinal(
    const dungeon::DungeonSnapshot& snapshot,
    std::uint16_t ordinal) noexcept {
    const std::size_t count = (std::min)(
        static_cast<std::size_t>(snapshot.ground_item_count),
        snapshot.ground_items.size());
    for (std::size_t index = 0U; index < count; ++index) {
        if (snapshot.ground_items[index].ordinal == ordinal) {
            return &snapshot.ground_items[index];
        }
    }
    return nullptr;
}

void draw_ground_items(const dungeon::DungeonSnapshot& snapshot,
    const GroundLootView& ground_loot,
    const MaterialPack& material_pack, float width, float height) noexcept {
    for (std::size_t index = 0U; index < ground_loot.count; ++index) {
        const dungeon::GroundItemSnapshot* const item =
            ground_item_with_ordinal(snapshot,
                ground_loot.labels[index].ordinal);
        if (item == nullptr) continue;
        const RenderProjection projected = project_render_world(
            item->position.x, item->position.y, item->position.z,
            width, height);
        const Vector2 center{projected.x,
            projected.ground_y - 13.0F * projected.scale};
        const Color color = ground_item_color(item->rarity);
        DrawEllipse(static_cast<int>(projected.x),
            static_cast<int>(projected.ground_y + 2.0F),
            17.0F * projected.scale, 6.0F * projected.scale,
            Fade(color, 0.24F));
        const bool abyss = item->source == dungeon::GroundItemSource::abyss_chest;
        if (!material_pack.draw(select_loot_sprite(item->rarity, abyss), center,
                false, 0.30F * projected.scale)) {
            draw_ground_item_shape(item->slot, center, projected.scale, color);
        }
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

void draw_hole(const dungeon::DungeonSnapshot& snapshot,
    const MaterialPack& material_pack, bool draw_material_environment) noexcept {
    const HoleVisualMode hole = hole_visual_mode(snapshot);
    if (hole == HoleVisualMode::hidden) {
        return;
    }
    const RenderProjection projected = project_render_world(kHoleCenter.x,
        kHoleCenter.y, kHoleCenter.z, static_cast<float>(GetScreenWidth()),
        static_cast<float>(GetScreenHeight()));
    const int x = static_cast<int>(projected.x);
    const int y = static_cast<int>(projected.ground_y);
    Color color{43, 25, 55, 255};
    if (hole == HoleVisualMode::ready) color = Color{230, 79, 186, 255};
    else if (hole == HoleVisualMode::busy) color = Color{255, 194, 74, 255};
    if (draw_material_environment) {
        static_cast<void>(material_pack.draw(MaterialSpriteId::environment_hole,
            {projected.x, projected.ground_y}, false, 0.77F * projected.scale));
    } else {
        DrawEllipse(x, y, 74.0F, 25.0F, Color{5, 2, 9, 235});
    }
    DrawEllipseLines(x, y, 74.0F, 25.0F, color);
    const char* label = hole == HoleVisualMode::sealed ? "SEALED"
        : hole == HoleVisualMode::ready ? "READY"
        : hole == HoleVisualMode::busy ? "SAVING" : "FAULTED";
    DrawText(label, x - MeasureText(label, 16) / 2, y - 8, 16, color);
    if (snapshot.combat.has_value()
        && can_prompt_descent(snapshot, snapshot.combat->player.position)) {
        constexpr const char* kPrompt = "Press E to descend";
        DrawText(kPrompt, x - MeasureText(kPrompt, 18) / 2, y + 34, 18, RAYWHITE);
    }
}

}  // namespace

void CombatRenderer::draw_room(
    const dungeon::DungeonSnapshot& current,
    const GroundLootView& ground_loot) const noexcept {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const bool draw_material_environment = can_draw_room_environment(current,
        material_pack_) && draw_environment_room(material_pack_, current.ecology);
    if (!draw_material_environment) {
        draw_graybox_room(current.ecology);
    }
    draw_abyss(current, static_cast<float>(GetTime()));
    draw_environment_hazards(current, width, height);
    draw_ground_items(current, ground_loot, material_pack_, width, height);
    draw_doors(current, width, height, material_pack_, draw_material_environment);
    draw_hole(current, material_pack_, draw_material_environment);
}

}  // namespace arpg::platform
