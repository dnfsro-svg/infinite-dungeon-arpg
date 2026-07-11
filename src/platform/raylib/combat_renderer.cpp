#include "combat_renderer.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/combat_collision.hpp"
#include "combat_view_math.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>

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
    case AttackId::heavy: return "L";
    case AttackId::launcher: return "U";
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

const char* reaction_name(ReactionState state) noexcept {
    switch (state) {
    case ReactionState::idle: return "Idle";
    case ReactionState::hitstun: return "Hitstun";
    case ReactionState::airborne: return "Airborne";
    case ReactionState::knockdown: return "Knockdown";
    case ReactionState::rising: return "Rising";
    case ReactionState::defeated: return "Defeated";
    case ReactionState::respawning: return "Respawning";
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
    }
    return "?";
}

Color dummy_color(DummyKind kind) noexcept {
    switch (kind) {
    case DummyKind::light: return Color{76, 205, 122, 255};
    case DummyKind::normal: return Color{232, 190, 75, 255};
    case DummyKind::heavy: return Color{218, 80, 76, 255};
    }
    return MAGENTA;
}

void draw_graybox_room() noexcept {
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
    const Color floor{45, 51, 63, 255};
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

    for (const DummySnapshot& dummy : snapshot.dummies) {
        draw_projected_aabb(
            make_dummy_hurtbox(dummy.kind, dummy.position),
            width,
            height,
            Color{94, 255, 173, 210},
            "Hurt");
    }
}

}  // namespace

void CombatRenderer::consume_event(const CombatEvent& event) noexcept {
    if (event.kind == CombatEventKind::reset) {
        flash_until_tick_.fill(0);
    }
    if (event.kind == CombatEventKind::hit
        && event.target_index < flash_until_tick_.size()) {
        flash_until_tick_[event.target_index] = event.tick + 2;
    }
    last_event_ = event;
    has_last_event_ = true;
}

void CombatRenderer::draw(
    const CombatSnapshot& previous,
    const CombatSnapshot& current,
    float interpolation_alpha,
    bool draw_debug) const noexcept {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const float alpha = std::clamp(interpolation_alpha, 0.0F, 1.0F);
    draw_graybox_room();

    std::array<Vec3, 4> positions{};
    positions[0] = interpolate(
        previous.player.position, current.player.position, alpha);
    for (std::size_t index = 0; index < kDummyCount; ++index) {
        positions[index + 1] = interpolate(
            previous.dummies[index].position,
            current.dummies[index].position,
            alpha);
    }

    std::array<ActorDrawItem, 4> draw_items{{
        {positions[0], 0},
        {positions[1], 1},
        {positions[2], 2},
        {positions[3], 3},
    }};
    sort_actor_draw_items(draw_items);

    for (const ActorDrawItem& item : draw_items) {
        const Vec3 ground_position{item.position.x, item.position.y, 0.0F};
        const ScreenProjection ground = project_combat_position(
            ground_position, width, height);
        const bool player = item.index == 0;
        const float shadow_width = (player ? 48.0F : 54.0F) * ground.scale;
        DrawEllipse(
            static_cast<int>(ground.x),
            static_cast<int>(ground.ground_y + 3.0F),
            shadow_width,
            10.0F * ground.scale,
            Color{3, 5, 8, 125});
    }

    for (const ActorDrawItem& item : draw_items) {
        const ScreenProjection projected = project_combat_position(
            item.position, width, height);
        if (item.index == 0) {
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

        const std::size_t dummy_index = item.index - 1;
        const DummySnapshot& dummy = current.dummies[dummy_index];
        const float body_width = (dummy.kind == DummyKind::heavy ? 58.0F
                                 : dummy.kind == DummyKind::normal ? 48.0F
                                                                  : 40.0F)
            * projected.scale;
        const float body_height = (dummy.kind == DummyKind::heavy ? 96.0F
                                  : dummy.kind == DummyKind::normal ? 84.0F
                                                                   : 74.0F)
            * projected.scale;
        Color color = dummy_color(dummy.kind);
        if (flash_until_tick_[dummy_index] != 0
            && current.tick <= flash_until_tick_[dummy_index]) {
            color = Color{255, 250, 220, 255};
        }
        DrawRectangleRounded(
            {projected.x - body_width * 0.5F,
             projected.y - body_height,
             body_width,
             body_height},
            0.16F,
            6,
            color);
        const float bar_width = std::max(52.0F, body_width);
        draw_bar(
            projected.x - bar_width * 0.5F,
            projected.y - body_height - 13.0F,
            bar_width,
            dummy.max_hp == 0
                ? 0.0F
                : static_cast<float>(dummy.hp)
                    / static_cast<float>(dummy.max_hp),
            Color{70, 221, 113, 255});
        if (dummy.kind == DummyKind::heavy) {
            draw_bar(
                projected.x - bar_width * 0.5F,
                projected.y - body_height - 6.0F,
                bar_width,
                dummy.max_break == 0
                    ? 0.0F
                    : static_cast<float>(dummy.break_value)
                        / static_cast<float>(dummy.max_break),
                dummy.armor == ArmorState::broken
                    ? Color{255, 105, 190, 255}
                    : Color{255, 167, 72, 255});
        }
        DrawText(
            reaction_name(dummy.reaction),
            static_cast<int>(projected.x - body_width * 0.5F),
            static_cast<int>(projected.y + 7.0F),
            12,
            Color{225, 230, 239, 230});
    }

    if (draw_debug) {
        draw_debug_volumes(current, width, height);
    }

    DrawRectangleRounded(
        {16.0F, 14.0F, 430.0F, 218.0F},
        0.06F,
        6,
        Color{7, 10, 17, 220});
    const Color text{218, 226, 239, 255};
    const Color accent{110, 207, 255, 255};
    int y = 28;
    DrawText("WASD Move  J Light  K Jump  L Heavy  U Launcher", 30, y, 16, accent);
    y += 25;
    DrawText("R Reset  F1 Debug  F12 Screenshot  Esc Exit", 30, y, 16, accent);
    y += 28;
    DrawText(
        TextFormat("Tick %llu  State %s", static_cast<unsigned long long>(current.tick), player_state_name(current.player.state)),
        30, y, 16, text);
    y += 23;
    DrawText(
        TextFormat("Action %s  %s  Combo %u", attack_name(current.player.active_attack), phase_name(current.player.attack_phase), current.player.combo_stage),
        30, y, 16, text);
    y += 23;
    DrawText(
        TextFormat("Input %u  Expired %u  Overflow %u", static_cast<unsigned>(current.diagnostics.input_size), current.diagnostics.input_expired_count, current.diagnostics.input_overflow_count),
        30, y, 16, text);
    y += 23;
    DrawText(
        TextFormat("Z %.2f  Hit Stop %u  Event overflow %u", current.player.position.z, current.player.hit_stop_ticks, current.diagnostics.event_overflow_count),
        30, y, 16, text);
    y += 23;
    DrawText(
        has_last_event_
            ? TextFormat("Last event %s  target %u", event_name(last_event_.kind), last_event_.target_index)
            : "Last event None",
        30, y, 16, text);

    DrawText(
        draw_debug ? "F1 DEBUG ON" : "F1 DEBUG OFF",
        GetScreenWidth() - 150,
        20,
        16,
        draw_debug ? Color{255, 126, 197, 255} : Color{142, 153, 170, 255});
}

}  // namespace arpg::platform
