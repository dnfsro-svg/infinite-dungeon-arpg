#include "combat_renderer.hpp"

#include "combat/room_bounds.hpp"
#include "dungeon_view_math.hpp"
#include "render_layout.hpp"

#include <raylib.h>

#include <array>
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

void draw_doors(const dungeon::DungeonSnapshot& snapshot,
    float width, float height) noexcept {
    const DoorVisualMode mode = door_visual_mode(snapshot.phase,
        snapshot.has_active_room);
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
        DrawRectangleLinesEx(frame, 5.0F * projected.scale, frame_color);
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
        DrawText(visual.label, static_cast<int>(frame.x),
            static_cast<int>(frame.y - 15.0F * projected.scale),
            static_cast<int>(11.0F * projected.scale), text_color);
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
}

void draw_hole(const dungeon::DungeonSnapshot& snapshot) noexcept {
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
    DrawEllipse(x, y, 74.0F, 25.0F, Color{5, 2, 9, 235});
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

void CombatRenderer::draw_room(const dungeon::DungeonSnapshot& current) const noexcept {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    draw_graybox_room(current.ecology);
    draw_abyss(current, static_cast<float>(GetTime()));
    draw_doors(current, width, height);
    draw_hole(current);
}

}  // namespace arpg::platform
