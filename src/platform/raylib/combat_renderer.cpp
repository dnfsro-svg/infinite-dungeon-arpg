#include "combat_renderer.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/combat_collision.hpp"
#include "combat_view_math.hpp"
#include "dungeon_view_math.hpp"

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

void draw_doors(
    DoorVisualMode mode,
    float width,
    float height) noexcept {
    if (mode == DoorVisualMode::hidden) {
        return;
    }

    constexpr std::array<Vec3, 4> kDoorCenters{{
        {0.0F, -3.5F, 0.0F},
        {0.0F, 3.5F, 0.0F},
        {-8.0F, 0.0F, 0.0F},
        {8.0F, 0.0F, 0.0F},
    }};
    constexpr std::array<const char*, 4> kDoorArrows{{"^", "v", "<", ">"}};
    const bool open = mode == DoorVisualMode::open;
    const Color frame_color = open
        ? Color{211, 244, 248, 255}
        : Color{65, 70, 80, 255};

    for (std::size_t index = 0; index < kDoorCenters.size(); ++index) {
        const ScreenProjection projected = project_combat_position(
            kDoorCenters[index], width, height);
        const float door_width = 82.0F * projected.scale;
        const float door_height = 70.0F * projected.scale;
        const Rectangle frame{
            projected.x - door_width * 0.5F,
            projected.ground_y - door_height,
            door_width,
            door_height,
        };
        DrawRectangleLinesEx(frame, 5.0F * projected.scale, frame_color);
        if (!open) {
            DrawRectangleRec(
                {frame.x + 7.0F * projected.scale,
                 frame.y + 7.0F * projected.scale,
                 frame.width - 14.0F * projected.scale,
                 frame.height - 7.0F * projected.scale},
                Color{177, 31, 46, 235});
            continue;
        }

        const int font_size = static_cast<int>(28.0F * projected.scale);
        const int arrow_width = MeasureText(kDoorArrows[index], font_size);
        DrawText(
            kDoorArrows[index],
            static_cast<int>(projected.x) - arrow_width / 2,
            static_cast<int>(frame.y + 17.0F * projected.scale),
            font_size,
            frame_color);
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
    last_event_ = event;
    has_last_event_ = true;
}

void CombatRenderer::consume_dungeon_event(
    const dungeon::DungeonEvent& event) noexcept {
    if (event.kind == dungeon::DungeonEventKind::room_destroyed) {
        transition_seconds_left_ = 0.12F;
    }
}

void CombatRenderer::clear_combat_transients() noexcept {
    last_event_ = combat::CombatEvent{};
    has_last_event_ = false;
}

void CombatRenderer::update(float frame_seconds) noexcept {
    transition_seconds_left_ = std::max(
        0.0F,
        transition_seconds_left_ - std::clamp(frame_seconds, 0.0F, 0.1F));
}

void CombatRenderer::draw(
    const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current,
    float interpolation_alpha,
    bool draw_debug,
    const CombatFeedback& feedback,
    bool audio_ready) noexcept {
    const bool transitioning = current.phase == dungeon::RoomPhase::transitioning;
    if (transitioning && !transition_phase_seen_) {
        transition_seconds_left_ = 0.12F;
    }
    transition_phase_seen_ = transitioning;

    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const float alpha = std::clamp(interpolation_alpha, 0.0F, 1.0F);
    const CameraOffset camera_offset = feedback.camera_offset();
    Camera2D world_camera{};
    world_camera.offset = {camera_offset.x, camera_offset.y};
    world_camera.zoom = 1.0F;
    BeginMode2D(world_camera);
    draw_graybox_room();
    draw_doors(door_visual_mode(current.phase, current.has_active_room), width, height);

    if (current.combat.has_value()) {
        const CombatSnapshot& current_combat = *current.combat;
        const CombatSnapshot& previous_combat =
            can_interpolate_room(previous, current)
            ? *previous.combat
            : current_combat;

        std::array<Vec3, 4> positions{};
        positions[0] = interpolate(
            previous_combat.player.position,
            current_combat.player.position,
            alpha);
        for (std::size_t index = 0; index < kDummyCount; ++index) {
            positions[index + 1] = interpolate(
                previous_combat.dummies[index].position,
                current_combat.dummies[index].position,
                alpha);
        }

        std::array<ActorDrawItem, 4> draw_items{{
            {positions[0], 0},
            {positions[1], 1},
            {positions[2], 2},
            {positions[3], 3},
        }};
        sort_actor_draw_items(draw_items);

        draw_effects(feedback, width, height, false);

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
            const DummySnapshot& dummy = current_combat.dummies[dummy_index];
            const float body_width = (dummy.kind == DummyKind::heavy ? 58.0F
                                     : dummy.kind == DummyKind::normal ? 48.0F
                                                                      : 40.0F)
                * projected.scale;
            const float body_height = (dummy.kind == DummyKind::heavy ? 96.0F
                                      : dummy.kind == DummyKind::normal ? 84.0F
                                                                       : 74.0F)
                * projected.scale;
            Color color = dummy_color(dummy.kind);
            if (feedback.target_flash_seconds(dummy_index) > 0.0F) {
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
        {16.0F, 14.0F, 500.0F, draw_debug ? 378.0F : 150.0F},
        0.06F,
        6,
        Color{7, 10, 17, 220});
    const Color text{218, 226, 239, 255};
    const Color accent{110, 207, 255, 255};
    int y = 28;
    DrawText("WASD Move  J Light  K Jump  L Launcher", 30, y, 16, accent);
    y += 25;
    DrawText("R Reset  F1 Debug  F12 Screenshot  Esc Exit", 30, y, 16, accent);
    y += 28;
    DrawText(
        TextFormat(
            "Room %llu  Phase %s  Remaining %u",
            static_cast<unsigned long long>(room_ordinal),
            room_phase_label(current.phase),
            static_cast<unsigned>(current.remaining_targets)),
        30, y, 16, text);
    y += 23;
    DrawText(
        TextFormat(
            "Doors: %s  Last exit: %s",
            doors_open ? "OPEN" : "LOCKED",
            exit_direction_label(current.last_exit)),
        30, y, 16, text);

    if (draw_debug) {
        y += 25;
        DrawText(
            TextFormat(
                "Seed %016llX  Session tick %llu",
                static_cast<unsigned long long>(current.room_seed),
                static_cast<unsigned long long>(current.session_tick)),
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
        transition_seconds_left_);
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
