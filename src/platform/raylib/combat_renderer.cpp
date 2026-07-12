#include "combat_renderer.hpp"

#include "combat/room_bounds.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/combat_collision.hpp"
#include "combat_view_math.hpp"
#include "dungeon_view_math.hpp"
#include "dungeon_runtime.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::platform {
namespace {

using namespace combat;

Vec3 interpolate(Vec3 from, Vec3 to, float amount) noexcept {
    return {
        from.x + (to.x - from.x) * amount,
        from.y + (to.y - from.y) * amount,
        from.z + (to.z - from.z) * amount,
    };
}

Vector2 lerp(Vector2 from, Vector2 to, float amount) noexcept {
    return {
        from.x + (to.x - from.x) * amount,
        from.y + (to.y - from.y) * amount,
    };
}

const char* attack_name(AttackId id) noexcept {
    switch (id) {
    case AttackId::j1: return "J1";
    case AttackId::j2: return "J2";
    case AttackId::j3: return "J3";
    case AttackId::launcher: return "L";
    case AttackId::air_j: return "Air J";
    case AttackId::none: return "None";
    }
    return "?";
}

const char* phase_name(AttackPhase phase) noexcept {
    switch (phase) {
    case AttackPhase::startup: return "Startup";
    case AttackPhase::active: return "Active";
    case AttackPhase::recovery: return "Recovery";
    case AttackPhase::finished: return "Finished";
    }
    return "?";
}

const char* player_state_name(PlayerState state) noexcept {
    switch (state) {
    case PlayerState::idle: return "Idle";
    case PlayerState::move: return "Move";
    case PlayerState::attack_startup: return "Attack startup";
    case PlayerState::attack_active: return "Attack active";
    case PlayerState::attack_recovery: return "Attack recovery";
    case PlayerState::jump_rise: return "Jump rise";
    case PlayerState::jump_fall: return "Jump fall";
    case PlayerState::landing: return "Landing";
    }
    return "?";
}

const char* monster_phase_name(MonsterAiPhase phase) noexcept {
    switch (phase) {
    case MonsterAiPhase::idle: return "Idle";
    case MonsterAiPhase::move: return "Move";
    case MonsterAiPhase::telegraph: return "Telegraph";
    case MonsterAiPhase::active: return "Active";
    case MonsterAiPhase::recovery: return "Recovery";
    case MonsterAiPhase::cooldown: return "Cooldown";
    case MonsterAiPhase::defeated: return "Defeated";
    }
    return "?";
}

const char* event_name(CombatEventKind kind) noexcept {
    switch (kind) {
    case CombatEventKind::swing: return "Swing";
    case CombatEventKind::hit: return "Hit";
    case CombatEventKind::impact_summary: return "Impact";
    case CombatEventKind::landing: return "Landing";
    case CombatEventKind::break_started: return "Break";
    case CombatEventKind::defeated: return "Defeated";
    case CombatEventKind::respawned: return "Respawned";
    case CombatEventKind::reset: return "Reset";
    case CombatEventKind::player_hit: return "Player hit";
    case CombatEventKind::player_hurt_started: return "Player hurt";
    case CombatEventKind::player_health_reset: return "Player heal";
    }
    return "?";
}

const char* ecology_name(dungeon::DungeonElement element) noexcept {
    switch (element) {
    case dungeon::DungeonElement::fire: return "FIRE";
    case dungeon::DungeonElement::water: return "WATER";
    case dungeon::DungeonElement::lightning: return "LIGHTNING";
    case dungeon::DungeonElement::chaos: return "CHAOS";
    }
    return "UNKNOWN";
}

Color to_color(Rgba8 color) noexcept {
    return {color.r, color.g, color.b, color.a};
}

struct RenderActor final {
    Vec3 position{};
    std::uint8_t monster_index{};
    bool player{};
};

bool render_actor_precedes(
    const RenderActor& lhs,
    const RenderActor& rhs) noexcept {
    if (lhs.position.y != rhs.position.y) {
        return lhs.position.y < rhs.position.y;
    }
    if (lhs.position.z != rhs.position.z) {
        return lhs.position.z < rhs.position.z;
    }
    if (lhs.position.x != rhs.position.x) {
        return lhs.position.x < rhs.position.x;
    }
    if (lhs.player != rhs.player) {
        return lhs.player;
    }
    return lhs.monster_index < rhs.monster_index;
}

void sort_render_actors(
    std::array<RenderActor, kMonsterCapacity + 1>& actors,
    std::size_t count) noexcept {
    for (std::size_t index = 1; index < count; ++index) {
        const RenderActor value = actors[index];
        std::size_t insertion = index;
        while (insertion > 0
            && render_actor_precedes(value, actors[insertion - 1])) {
            actors[insertion] = actors[insertion - 1];
            --insertion;
        }
        actors[insertion] = value;
    }
}

void draw_graybox_room(dungeon::DungeonElement ecology) noexcept {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const Vector2 back_left{width * 0.20F, height * 0.22F};
    const Vector2 back_right{width * 0.80F, height * 0.22F};
    const Vector2 floor_left{width * 0.04F, height * 0.92F};
    const Vector2 floor_right{width * 0.96F, height * 0.92F};

    DrawRectangleGradientV(
        0,
        0,
        GetScreenWidth(),
        GetScreenHeight(),
        Color{13, 17, 27, 255},
        Color{28, 32, 43, 255});
    DrawRectangle(
        static_cast<int>(back_left.x),
        0,
        static_cast<int>(back_right.x - back_left.x),
        static_cast<int>(back_left.y),
        Color{31, 37, 51, 255});
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
        DrawLineEx(
            lerp(back_left, back_right, amount),
            lerp(floor_left, floor_right, amount),
            1.0F,
            grid);
    }
    for (int row = 0; row <= 8; ++row) {
        const float linear = static_cast<float>(row) / 8.0F;
        const float perspective = linear * linear;
        DrawLineEx(
            lerp(back_left, floor_left, perspective),
            lerp(back_right, floor_right, perspective),
            1.0F,
            grid);
    }
}

void draw_doors(const dungeon::DungeonSnapshot& snapshot,
    float width,
    float height) noexcept {
    const DoorVisualMode mode = door_visual_mode(
        snapshot.phase, snapshot.has_active_room);
    if (mode == DoorVisualMode::hidden) {
        return;
    }

    constexpr std::array<Vec3, 4> kDoorCenters{{
        {0.0F, combat::room_bounds::min_y, 0.0F},
        {0.0F, combat::room_bounds::max_y, 0.0F},
        {combat::room_bounds::min_x, 0.0F, 0.0F},
        {combat::room_bounds::max_x, 0.0F, 0.0F},
    }};
    constexpr std::array<dungeon::ExitDirection, 4> kDirections{{
        dungeon::ExitDirection::up, dungeon::ExitDirection::down,
        dungeon::ExitDirection::left, dungeon::ExitDirection::right}};
    for (std::size_t index = 0; index < kDoorCenters.size(); ++index) {
        const ScreenProjection projected = project_combat_position(
            kDoorCenters[index], width, height);
        const DoorRenderDecision visual = door_render_decision(
            mode, kDirections[index]);
        const Color frame_color{visual.frame.r, visual.frame.g,
            visual.frame.b, visual.frame.a};
        const Color text_color{visual.text.r, visual.text.g, visual.text.b,
            visual.text.a};
        const float door_width = 82.0F * projected.scale;
        const float door_height = 70.0F * projected.scale;
        const Rectangle frame{
            projected.x - door_width * 0.5F,
            projected.ground_y - door_height,
            door_width,
            door_height,
        };
        DrawRectangleLinesEx(frame, 5.0F * projected.scale, frame_color);
        if (visual.draw_locked_interior) {
            DrawRectangleRec(
                {frame.x + 7.0F * projected.scale,
                 frame.y + 7.0F * projected.scale,
                 frame.width - 14.0F * projected.scale,
                 frame.height - 7.0F * projected.scale},
                Color{visual.locked_interior.r, visual.locked_interior.g,
                    visual.locked_interior.b, visual.locked_interior.a});
        }

        const int font_size = static_cast<int>(22.0F * projected.scale);
        const int arrow_width = MeasureText(visual.arrow, font_size);
        DrawText(
            visual.arrow,
            static_cast<int>(projected.x) - arrow_width / 2,
            static_cast<int>(frame.y + 17.0F * projected.scale),
            font_size,
            text_color);
        DrawText(visual.label, static_cast<int>(frame.x),
            static_cast<int>(frame.y - 15.0F * projected.scale),
            static_cast<int>(11.0F * projected.scale), text_color);
    }
}

void draw_abyss(const dungeon::DungeonSnapshot& snapshot,
    float elapsed_seconds) noexcept {
    if (snapshot.is_abyss) {
        const float pulse = abyss_pulse_alpha(elapsed_seconds);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
            Fade(Color{125, 19, 92, 255}, 0.12F + pulse * 0.16F));
        DrawRectangleLinesEx({8.0F, 8.0F,
            static_cast<float>(GetScreenWidth() - 16),
            static_cast<float>(GetScreenHeight() - 16)}, 8.0F,
            Fade(Color{225, 47, 160, 255}, 0.20F + pulse * 0.45F));
    }
}

void draw_hole(const dungeon::DungeonSnapshot& snapshot) noexcept {
    const HoleVisualMode hole = hole_visual_mode(snapshot);
    if (hole == HoleVisualMode::hidden) {
        return;
    }
    const ScreenProjection projected = project_combat_position(
        kHoleCenter, static_cast<float>(GetScreenWidth()),
        static_cast<float>(GetScreenHeight()));
    const int x = static_cast<int>(projected.x);
    const int y = static_cast<int>(projected.ground_y);
    Color color{43, 25, 55, 255};
    if (hole == HoleVisualMode::ready) {
        color = Color{230, 79, 186, 255};
    } else if (hole == HoleVisualMode::busy) {
        color = Color{255, 194, 74, 255};
    }
    DrawEllipse(x, y, 74.0F, 25.0F, Color{5, 2, 9, 235});
    DrawEllipseLines(x, y, 74.0F, 25.0F, color);
    const char* label = hole == HoleVisualMode::sealed ? "SEALED"
        : hole == HoleVisualMode::ready ? "READY"
        : hole == HoleVisualMode::busy ? "SAVING" : "FAULTED";
    DrawText(label, x - MeasureText(label, 16) / 2, y - 8, 16, color);
    const bool prompt = snapshot.combat.has_value()
        && can_prompt_descent(snapshot, snapshot.combat->player.position);
    if (prompt) {
        constexpr const char* kPrompt = "Press E to descend";
        DrawText(kPrompt, x - MeasureText(kPrompt, 18) / 2,
            y + 34, 18, RAYWHITE);
    }
}

void draw_bar(
    float x,
    float y,
    float width,
    float ratio,
    Color color) noexcept {
    const float clamped = std::clamp(ratio, 0.0F, 1.0F);
    DrawRectangleRec({x, y, width, 5.0F}, Color{20, 23, 31, 230});
    DrawRectangleRec({x + 1.0F, y + 1.0F, (width - 2.0F) * clamped, 3.0F}, color);
}

void draw_projected_aabb(
    const Aabb& box,
    float width,
    float height,
    Color color,
    const char* label) noexcept {
    std::array<ScreenProjection, 8> corners{};
    for (std::size_t index = 0; index < corners.size(); ++index) {
        const Vec3 corner{
            (index & 1U) != 0 ? box.maximum.x : box.minimum.x,
            (index & 2U) != 0 ? box.maximum.y : box.minimum.y,
            (index & 4U) != 0 ? box.maximum.z : box.minimum.z,
        };
        corners[index] = project_combat_position(corner, width, height);
    }

    for (std::size_t index = 0; index < corners.size(); ++index) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const std::size_t bit = std::size_t{1} << axis;
            if ((index & bit) != 0) {
                continue;
            }
            const ScreenProjection& from = corners[index];
            const ScreenProjection& to = corners[index | bit];
            DrawLineEx({from.x, from.y}, {to.x, to.y}, 1.5F, color);
        }
    }

    DrawText(
        label,
        static_cast<int>(corners[4].x + 4.0F),
        static_cast<int>(corners[4].y - 14.0F),
        12,
        color);
}

void draw_effects(
    const CombatFeedback& feedback,
    float width,
    float height,
    bool foreground) noexcept {
    for (const VisualEffect& effect : feedback.effects()) {
        if (!effect.active) {
            continue;
        }
        const bool is_foreground =
            effect.kind == VisualEffectKind::spark
            || effect.kind == VisualEffectKind::damage_number;
        if (is_foreground != foreground) {
            continue;
        }

        const float progress = effect.lifetime_seconds <= 0.0F
            ? 1.0F
            : std::clamp(
                  effect.age_seconds / effect.lifetime_seconds,
                  0.0F,
                  1.0F);
        const float opacity = 1.0F - progress;
        const ScreenProjection projected = project_combat_position(
            effect.position, width, height);
        switch (effect.kind) {
        case VisualEffectKind::weapon_trail:
            DrawLineEx(
                {projected.x - 36.0F * projected.scale,
                 projected.y - 42.0F * projected.scale},
                {projected.x + 44.0F * projected.scale,
                 projected.y - 68.0F * projected.scale},
                8.0F * projected.scale * opacity,
                Fade(Color{105, 224, 255, 255}, opacity));
            break;
        case VisualEffectKind::dust:
            DrawEllipse(
                static_cast<int>(projected.x),
                static_cast<int>(projected.ground_y),
                (18.0F + progress * 24.0F) * projected.scale,
                7.0F * projected.scale,
                Fade(Color{185, 193, 207, 255}, opacity));
            break;
        case VisualEffectKind::spark:
            DrawCircleLines(
                static_cast<int>(projected.x),
                static_cast<int>(projected.y - 42.0F * projected.scale),
                (8.0F + progress * 18.0F) * projected.scale,
                Fade(Color{255, 218, 96, 255}, opacity));
            DrawLineEx(
                {projected.x - 22.0F, projected.y - 54.0F},
                {projected.x + 25.0F, projected.y - 30.0F},
                3.0F,
                Fade(Color{255, 248, 210, 255}, opacity));
            break;
        case VisualEffectKind::damage_number:
            DrawText(
                TextFormat("%d", effect.value),
                static_cast<int>(projected.x + 8.0F),
                static_cast<int>(
                    projected.y - 90.0F - progress * 32.0F),
                20,
                Fade(Color{255, 238, 156, 255}, opacity));
            break;
        }
    }
}

void draw_hazards(
    const CombatSnapshot& snapshot,
    float width,
    float height) noexcept {
    for (const HazardSnapshot& hazard : snapshot.hazards) {
        const HazardVisualMode mode = hazard_visual_mode(hazard);
        if (mode == HazardVisualMode::hidden) {
            continue;
        }
        const ScreenProjection projected = project_hazard_center(
            hazard, width, height);
        const float radius = std::max(9.0F, hazard.radius * 58.0F)
            * projected.scale;
        const Color color = mode == HazardVisualMode::telegraph
            ? Color{255, 190, 76, 230} : Color{190, 73, 229, 150};
        if (mode == HazardVisualMode::active) {
            DrawEllipse(static_cast<int>(projected.x),
                static_cast<int>(projected.ground_y), radius, radius * 0.36F,
                color);
        }
        DrawEllipseLines(static_cast<int>(projected.x),
            static_cast<int>(projected.ground_y), radius, radius * 0.36F,
            color);
    }
}

void draw_projectiles(
    const CombatSnapshot& snapshot,
    float width,
    float height,
    dungeon::DungeonElement ecology) noexcept {
    const Color color = to_color(monster_ecology_color(ecology));
    for (const ProjectileSnapshot& projectile : snapshot.projectiles) {
        if (!projectile.active) {
            continue;
        }
        const ScreenProjection projected = project_projectile_position(
            projectile, width, height);
        const float radius = std::max(3.0F, projectile.radius * 22.0F)
            * projected.scale;
        DrawCircle(static_cast<int>(projected.x), static_cast<int>(projected.y),
            radius, color);
        DrawCircleLines(static_cast<int>(projected.x), static_cast<int>(projected.y),
            radius + 2.0F, Color{244, 248, 255, 235});
    }
}

void draw_monster_warning(
    const MonsterSnapshot& monster,
    Vec3 position,
    dungeon::DungeonElement ecology,
    float width,
    float height) noexcept {
    const MonsterVisual visual = monster_visual(
        monster.id, monster.ai_phase, ecology);
    if (visual.warning_mode == MonsterWarningMode::none) {
        return;
    }
    const ScreenProjection projected = project_combat_position(
        position, width, height);
    const Color warning = to_color(visual.warning);
    const float size = 34.0F * projected.scale;
    if (visual.shape == MonsterShapeId::charger
        || visual.shape == MonsterShapeId::dasher) {
        const ScreenProjection target = project_combat_position(
            monster.attack_target_position, width, height);
        DrawLineEx({projected.x, projected.ground_y - 18.0F * projected.scale},
            {target.x, target.ground_y - 18.0F * target.scale},
            visual.warning_mode == MonsterWarningMode::active ? 5.0F : 2.0F,
            warning);
        return;
    }
    if (visual.shape == MonsterShapeId::hazard_caster) {
        const ScreenProjection target = project_combat_position(
            monster.attack_target_position, width, height);
        DrawEllipseLines(static_cast<int>(target.x),
            static_cast<int>(target.ground_y), size * 1.5F, size * 0.55F,
            warning);
        return;
    }
    DrawCircleLines(static_cast<int>(projected.x),
        static_cast<int>(projected.ground_y - size), size, warning);
}

void draw_monster_silhouette(
    const MonsterSnapshot& monster,
    Vec3 position,
    dungeon::DungeonElement ecology,
    float width,
    float height,
    const CombatFeedback& feedback,
    std::size_t monster_index) noexcept {
    const ScreenProjection projected = project_combat_position(
        position, width, height);
    const MonsterVisual visual = monster_visual(
        monster.id, monster.ai_phase, ecology);
    Color body = to_color(visual.body);
    if (feedback.target_flash_seconds(monster_index) > 0.0F) {
        body = Color{255, 249, 220, 255};
    }
    const Color accent = to_color(visual.accent);
    const float scale = projected.scale;
    const float x = projected.x;
    const float y = projected.ground_y;
    switch (visual.shape) {
    case MonsterShapeId::bomber:
        DrawCircle(static_cast<int>(x), static_cast<int>(y - 43.0F * scale),
            27.0F * scale, body);
        DrawCircleLines(static_cast<int>(x), static_cast<int>(y - 43.0F * scale),
            31.0F * scale, accent);
        DrawLineEx({x - 14.0F * scale, y - 70.0F * scale},
            {x + 16.0F * scale, y - 82.0F * scale}, 3.0F * scale, accent);
        break;
    case MonsterShapeId::charger:
        DrawRectangleRounded({x - 22.0F * scale, y - 78.0F * scale,
            44.0F * scale, 78.0F * scale}, 0.12F, 5, body);
        DrawLineEx({x - 8.0F * scale, y - 54.0F * scale},
            {x + 45.0F * scale, y - 76.0F * scale}, 6.0F * scale, accent);
        break;
    case MonsterShapeId::bulwark:
        DrawRectangleRounded({x - 34.0F * scale, y - 86.0F * scale,
            68.0F * scale, 86.0F * scale}, 0.12F, 5, body);
        DrawRectangleLinesEx({x + 18.0F * scale, y - 66.0F * scale,
            18.0F * scale, 52.0F * scale}, 3.0F * scale, accent);
        break;
    case MonsterShapeId::support:
        DrawCircle(static_cast<int>(x), static_cast<int>(y - 44.0F * scale),
            25.0F * scale, body);
        DrawLineEx({x - 22.0F * scale, y - 44.0F * scale},
            {x + 22.0F * scale, y - 44.0F * scale}, 5.0F * scale, accent);
        DrawLineEx({x, y - 66.0F * scale}, {x, y - 22.0F * scale},
            5.0F * scale, accent);
        if (monster.shield > 0) {
            DrawCircleLines(static_cast<int>(x),
                static_cast<int>(y - 44.0F * scale), 33.0F * scale, accent);
        }
        break;
    case MonsterShapeId::shooter:
        DrawRectangleRounded({x - 21.0F * scale, y - 70.0F * scale,
            42.0F * scale, 70.0F * scale}, 0.16F, 5, body);
        DrawLineEx({x, y - 55.0F * scale}, {x + 38.0F * scale, y - 55.0F * scale},
            6.0F * scale, accent);
        break;
    case MonsterShapeId::dasher:
        DrawTriangle({x + 30.0F * scale, y - 39.0F * scale},
            {x - 27.0F * scale, y - 72.0F * scale},
            {x - 27.0F * scale, y - 8.0F * scale}, body);
        DrawLineEx({x - 26.0F * scale, y - 39.0F * scale},
            {x + 24.0F * scale, y - 39.0F * scale}, 4.0F * scale, accent);
        break;
    case MonsterShapeId::chaser:
        DrawTriangle({x, y - 82.0F * scale}, {x - 27.0F * scale, y},
            {x + 27.0F * scale, y}, body);
        DrawLineEx({x - 25.0F * scale, y - 33.0F * scale},
            {x - 43.0F * scale, y - 10.0F * scale}, 4.0F * scale, accent);
        DrawLineEx({x + 25.0F * scale, y - 33.0F * scale},
            {x + 43.0F * scale, y - 10.0F * scale}, 4.0F * scale, accent);
        break;
    case MonsterShapeId::hazard_caster:
        DrawRectangleRounded({x - 20.0F * scale, y - 73.0F * scale,
            40.0F * scale, 73.0F * scale}, 0.20F, 5, body);
        DrawLineEx({x + 12.0F * scale, y - 15.0F * scale},
            {x + 25.0F * scale, y - 84.0F * scale}, 4.0F * scale, accent);
        DrawCircleLines(static_cast<int>(x + 25.0F * scale),
            static_cast<int>(y - 88.0F * scale), 8.0F * scale, accent);
        break;
    }
    const float bar_width = 54.0F * scale;
    draw_bar(x - bar_width * 0.5F, y - 102.0F * scale, bar_width,
        monster.max_hp <= 0 ? 0.0F
            : static_cast<float>(monster.hp) / static_cast<float>(monster.max_hp),
        Color{78, 219, 120, 255});
    if (monster.max_shield > 0) {
        draw_bar(x - bar_width * 0.5F, y - 95.0F * scale, bar_width,
            static_cast<float>(monster.shield) / static_cast<float>(monster.max_shield),
            accent);
    }
    DrawText(visual.role_label, static_cast<int>(x - bar_width * 0.5F),
        static_cast<int>(y + 7.0F), 11, Color{225, 230, 239, 230});
    DrawText(monster_phase_name(monster.ai_phase),
        static_cast<int>(x - bar_width * 0.5F), static_cast<int>(y + 19.0F),
        10, Color{184, 196, 213, 220});
}

void draw_debug_volumes(
    const CombatSnapshot& snapshot,
    float width,
    float height) noexcept {
    if (const AttackDefinition* definition =
            find_attack_definition(snapshot.player.active_attack)) {
        const Aabb attack_box = make_world_aabb(
            definition->local_hitbox,
            snapshot.player.position,
            snapshot.player.facing);
        const Aabb assist_box = make_attack_assist_volume(
            *definition,
            snapshot.player.position,
            snapshot.player.facing);
        draw_projected_aabb(
            assist_box,
            width,
            height,
            Color{90, 190, 255, 190},
            "Assist");
        draw_projected_aabb(
            attack_box,
            width,
            height,
            Color{255, 88, 184, 230},
            "Attack");
    }

    for (const MonsterSnapshot& monster : snapshot.monsters) {
        if (!monster_visible(monster)) {
            continue;
        }
        draw_projected_aabb(
            make_dummy_hurtbox(monster.kind, monster.position),
            width,
            height,
            Color{94, 255, 173, 210},
            "Hurt");
    }
}

}  // namespace

DoorRenderDecision door_render_decision(
    DoorVisualMode mode,
    dungeon::ExitDirection direction) noexcept {
    const DoorTheme theme = door_theme(direction);
    return {
        theme.label,
        theme.arrow,
        theme.frame,
        theme.frame,
        {177U, 31U, 46U, 235U},
        mode == DoorVisualMode::closed,
    };
}

void CombatRenderer::consume_event(const CombatEvent& event) noexcept {
    last_event_ = event;
    has_last_event_ = true;
}

void CombatRenderer::consume_dungeon_event(
    const dungeon::DungeonEvent& event) noexcept {
    transition_ = transition_after_dungeon_event(transition_, event.kind);
}

void CombatRenderer::clear_combat_transients() noexcept {
    last_event_ = combat::CombatEvent{};
    has_last_event_ = false;
}

void CombatRenderer::update(float frame_seconds) noexcept {
    transition_ = advance_transition(transition_, frame_seconds);
}

void CombatRenderer::draw(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    float interpolation_alpha,
    bool draw_debug,
    const CombatFeedback& feedback,
    bool audio_ready) noexcept {
    transition_ = transition_after_room_phase(transition_, current.phase);

    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const float alpha = std::clamp(interpolation_alpha, 0.0F, 1.0F);
    const CameraOffset camera_offset = feedback.camera_offset();
    Camera2D world_camera{};
    world_camera.offset = {camera_offset.x, camera_offset.y};
    world_camera.zoom = 1.0F;
    BeginMode2D(world_camera);
    draw_graybox_room(current.ecology);
    draw_abyss(current, static_cast<float>(GetTime()));
    draw_doors(current, width, height);
    draw_hole(current);

    if (current.combat.has_value()) {
        const CombatSnapshot& current_combat = *current.combat;
        const CombatSnapshot& previous_combat =
            can_interpolate_room(previous, current)
            ? *previous.combat
            : current_combat;

        std::array<RenderActor, kMonsterCapacity + 1> draw_items{};
        std::size_t draw_count = 0;
        draw_items[draw_count++] = {interpolate(
            previous_combat.player.position, current_combat.player.position,
            alpha), 0U, true};
        for (std::size_t index = 0; index < current_combat.monsters.size(); ++index) {
            const MonsterSnapshot& monster = current_combat.monsters[index];
            if (!monster_visible(monster)) {
                continue;
            }
            Vec3 position = monster.position;
            const MonsterSnapshot& previous_monster = previous_combat.monsters[index];
            if (monster.id == previous_monster.id
                && monster.generation == previous_monster.generation
                && monster_visible(previous_monster)) {
                position = interpolate(previous_monster.position, monster.position, alpha);
            }
            draw_items[draw_count++] = {position,
                static_cast<std::uint8_t>(index), false};
            draw_monster_warning(monster, position, current.ecology, width, height);
        }
        sort_render_actors(draw_items, draw_count);

        draw_hazards(current_combat, width, height);
        draw_effects(feedback, width, height, false);
        draw_projectiles(current_combat, width, height, current.ecology);

        for (std::size_t index = 0; index < draw_count; ++index) {
            const RenderActor& item = draw_items[index];
            const Vec3 ground_position{item.position.x, item.position.y, 0.0F};
            const ScreenProjection ground = project_combat_position(
                ground_position, width, height);
            const bool player = item.player;
            const float shadow_width = (player ? 48.0F : 54.0F) * ground.scale;
            DrawEllipse(
                static_cast<int>(ground.x),
                static_cast<int>(ground.ground_y + 3.0F),
                shadow_width,
                10.0F * ground.scale,
                Color{3, 5, 8, 125});
        }

        for (std::size_t index = 0; index < draw_count; ++index) {
            const RenderActor& item = draw_items[index];
            const ScreenProjection projected = project_combat_position(
                item.position, width, height);
            if (item.player) {
                const float body_width = 42.0F * projected.scale;
                const float body_height = 82.0F * projected.scale;
                DrawRectangleRounded(
                    {projected.x - body_width * 0.5F,
                     projected.y - body_height,
                     body_width,
                     body_height},
                    0.20F,
                    6,
                    Color{65, 202, 223, 255});
                DrawTriangle(
                    {projected.x,
                     projected.y - body_height - 16.0F * projected.scale},
                    {projected.x - 12.0F * projected.scale,
                     projected.y - body_height + 5.0F * projected.scale},
                    {projected.x + 12.0F * projected.scale,
                     projected.y - body_height + 5.0F * projected.scale},
                    Color{134, 237, 255, 255});
                continue;
            }
            const std::size_t monster_index = item.monster_index;
            draw_monster_silhouette(current_combat.monsters[monster_index],
                item.position, current.ecology, width, height, feedback,
                monster_index);
        }

        draw_effects(feedback, width, height, true);

        if (draw_debug) {
            draw_debug_volumes(current_combat, width, height);
        }
    }

    EndMode2D();

    const std::uint64_t room_ordinal = current.room_index
            == (std::numeric_limits<std::uint64_t>::max)()
        ? current.room_index
        : current.room_index + 1U;
    const bool doors_open = current.phase == dungeon::RoomPhase::cleared
        || current.phase == dungeon::RoomPhase::awaiting_exit;
    DrawRectangleRounded(
        {16.0F, 14.0F, 570.0F, draw_debug ? 600.0F : 330.0F},
        0.06F,
        6,
        Color{7, 10, 17, 220});
    const Color text{218, 226, 239, 255};
    const Color accent{110, 207, 255, 255};
    int y = 28;
    DrawText("WASD Move  J Light  K Jump  L Launcher  E Descend", 30, y, 16, accent);
    y += 25;
    DrawText("R Reset  F1 Debug  F12 Screenshot  Esc Exit", 30, y, 16, accent);
    y += 28;
    DrawText(
        TextFormat("Depth %llu  Floor Room %llu  Global Room %llu",
            static_cast<unsigned long long>(current.depth),
            static_cast<unsigned long long>(current.floor_room_index),
            static_cast<unsigned long long>(room_ordinal)),
        30, y, 16, text);
    y += 23;
    DrawText(
        TextFormat("Ecology %s  F/W/L/C bias %u/%u/%u/%u",
            ecology_name(current.ecology),
            current.biases[0], current.biases[1], current.biases[2],
            current.biases[3]),
        30, y, 16, text);
    y += 23;
    if (current.combat.has_value()) {
        const CombatSnapshot& combat_state = *current.combat;
        DrawText(TextFormat("HP %d/%d", combat_state.player.hp,
            combat_state.player.max_hp), 30, y, 16, text);
        draw_bar(126.0F, static_cast<float>(y + 5), 150.0F,
            player_hp_ratio(combat_state.player), Color{77, 215, 127, 255});
        y += 23;
        const unsigned wave_current = current.wave_count == 0U ? 0U
            : static_cast<unsigned>(current.wave_index) + 1U;
        DrawText(TextFormat("Wave %u/%u  Budget %u/%u  Targets %u",
            wave_current, static_cast<unsigned>(current.wave_count),
            static_cast<unsigned>(current.encounter.current_wave_budget),
            static_cast<unsigned>(current.encounter.total_budget),
            static_cast<unsigned>(current.remaining_targets)),
            30, y, 16, text);
        y += 23;
        DrawText(TextFormat("Active M/P/H %u/%u/%u",
            static_cast<unsigned>(combat_state.monster_count),
            static_cast<unsigned>(combat_state.projectile_count),
            static_cast<unsigned>(combat_state.hazard_count)),
            30, y, 16, text);
        y += 23;
        DrawText(TextFormat("Saturation P/H %u/%u  Invalid owner P/H %u/%u",
            combat_state.diagnostics.projectile_saturation_count,
            combat_state.diagnostics.hazard_saturation_count,
            combat_state.diagnostics.projectile_invalid_owner_count,
            combat_state.diagnostics.hazard_invalid_owner_count),
            30, y, 16, text);
        y += 23;
    } else {
        DrawText("HP / wave / encounter diagnostics unavailable", 30, y, 16,
            Color{255, 151, 117, 255});
        y += 23;
    }
    DrawText(TextFormat("ABYSS %s  Hole %s  Autosave %s",
        current.is_abyss ? "YES" : "NO",
        current.has_hole ? (current.phase == dungeon::RoomPhase::committing
                ? "SAVING" : doors_open ? "READY" : "SEALED") : "NONE",
        save_indicator_label(runtime_status.indicator)), 30, y, 16,
        runtime_status.indicator == SaveIndicator::error
            ? Color{255, 120, 120, 255} : text);

    if (draw_debug) {
        y += 25;
        DrawText(
            TextFormat(
                "Root %016llX Room %016llX Generation %llu",
                static_cast<unsigned long long>(current.root_seed),
                static_cast<unsigned long long>(current.room_seed),
                static_cast<unsigned long long>(current.commit_generation)),
            30, y, 16, text);
        y += 23;
        DrawText(
            TextFormat("Saved ecology %s hole %s abyss %s slot %u error %u fault %u",
                ecology_name(current.ecology),
                current.has_hole ? "YES" : "NO", current.is_abyss ? "YES" : "NO",
                static_cast<unsigned>(runtime_status.active_slot),
                static_cast<unsigned>(runtime_status.error),
                static_cast<unsigned>(current.diagnostics.fault)),
            30, y, 16, text);
        y += 23;
        DrawText(
            TextFormat(
                "Dungeon event %u  relay %u  rejected %u  index fault %s",
                current.diagnostics.event_overflow_count,
                current.diagnostics.combat_relay_overflow_count,
                current.diagnostics.rejected_exit_count,
                current.diagnostics.room_index_overflow ? "YES" : "NO"),
            30, y, 16, text);

        if (current.combat.has_value()) {
            const CombatSnapshot& combat_state = *current.combat;
            y += 23;
            DrawText(
                TextFormat(
                    "Tick %llu  State %s  Z %.2f  Hit stop %u",
                    static_cast<unsigned long long>(combat_state.tick),
                    player_state_name(combat_state.player.state),
                    combat_state.player.position.z,
                    combat_state.player.hit_stop_ticks),
                30, y, 16, text);
            y += 23;
            DrawText(
                TextFormat(
                    "Action %s  %s  Combo %u",
                    attack_name(combat_state.player.active_attack),
                    phase_name(combat_state.player.attack_phase),
                    combat_state.player.combo_stage),
                30, y, 16, text);
            y += 23;
            DrawText(
                TextFormat(
                    "Input %llu  expired %u  overflow %u  event overflow %u",
                    static_cast<unsigned long long>(
                        combat_state.diagnostics.input_size),
                    combat_state.diagnostics.input_expired_count,
                    combat_state.diagnostics.input_overflow_count,
                    combat_state.diagnostics.event_overflow_count),
                30, y, 16, text);
        } else {
            y += 23;
            DrawText("Combat diagnostics unavailable during transition", 30, y, 16, text);
        }

        y += 23;
        DrawText(
            has_last_event_
                ? TextFormat(
                    "Last event %s  target %u",
                    event_name(last_event_.kind),
                    last_event_.target_index)
                : "Last event None",
            30, y, 16, text);
        y += 23;
        DrawText(
            TextFormat(
                "FX %u  Dropped %u  Shake %.1f  Audio %s",
                static_cast<unsigned>(feedback.active_count()),
                feedback.dropped_count(),
                feedback.shake_amplitude(),
                audio_ready ? "Ready" : "Unavailable"),
            30,
            y,
            16,
            audio_ready ? text : Color{255, 151, 117, 255});
    }

    DrawText(
        draw_debug ? "F1 DEBUG ON" : "F1 DEBUG OFF",
        GetScreenWidth() - 150,
        20,
        16,
        draw_debug ? Color{255, 126, 197, 255} : Color{142, 153, 170, 255});

    const float overlay_alpha = transition_overlay_alpha(
        transition_.seconds_left);
    if (overlay_alpha > 0.0F) {
        DrawRectangle(
            0,
            0,
            GetScreenWidth(),
            GetScreenHeight(),
            Fade(BLACK, overlay_alpha));
    }
}

}  // namespace arpg::platform
