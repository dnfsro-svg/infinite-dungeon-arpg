#include "combat_renderer.hpp"

#include "combat/attack_catalog.hpp"
#include "combat_view_math.hpp"
#include "hud_renderer.hpp"
#include "dungeon_view_math.hpp"
#include "material_animation.hpp"
#include "ui_text_renderer.hpp"
#include "ui_typography.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>

namespace arpg::platform {
namespace {

using namespace combat;

Vec3 interpolate(Vec3 from, Vec3 to, float amount) noexcept {
    return {from.x + (to.x - from.x) * amount, from.y + (to.y - from.y) * amount,
        from.z + (to.z - from.z) * amount};
}

Color to_color(Rgba8 color) noexcept { return {color.r, color.g, color.b, color.a}; }

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

struct RenderActor final {
    Vec3 position{};
    std::uint8_t monster_index{};
    bool player{};
    MonsterMaterialDrawPlan material_plan{};
};

bool render_actor_precedes(const RenderActor& lhs, const RenderActor& rhs) noexcept {
    if (lhs.position.y != rhs.position.y) return lhs.position.y < rhs.position.y;
    if (lhs.position.z != rhs.position.z) return lhs.position.z < rhs.position.z;
    if (lhs.position.x != rhs.position.x) return lhs.position.x < rhs.position.x;
    return lhs.player != rhs.player ? lhs.player : lhs.monster_index < rhs.monster_index;
}

void sort_render_actors(std::array<RenderActor, kMonsterCapacity + 1>& actors,
    std::size_t count) noexcept {
    for (std::size_t index = 1; index < count; ++index) {
        const RenderActor value = actors[index];
        std::size_t insertion = index;
        while (insertion > 0 && render_actor_precedes(value, actors[insertion - 1])) {
            actors[insertion] = actors[insertion - 1];
            --insertion;
        }
        actors[insertion] = value;
    }
}

void draw_bar(float x, float y, float width, float ratio, Color color) noexcept {
    ratio = std::clamp(ratio, 0.0F, 1.0F);
    DrawRectangleRec({x, y, width, 5.0F}, Color{20, 23, 31, 230});
    DrawRectangleRec({x + 1.0F, y + 1.0F, (width - 2.0F) * ratio, 3.0F}, color);
}

[[nodiscard]] float monster_bar_ratio(int value, int maximum) noexcept {
    if (maximum <= 0) return 0.0F;
    return std::clamp(static_cast<float>(value) / static_cast<float>(maximum),
        0.0F, 1.0F);
}

MaterialSpriteId effect_sprite(VisualEffectKind kind) noexcept {
    switch (kind) {
    case VisualEffectKind::weapon_trail: return MaterialSpriteId::effect_launcher_trail;
    case VisualEffectKind::dust: return MaterialSpriteId::effect_landing_dust;
    case VisualEffectKind::spark: return MaterialSpriteId::effect_hit_spark;
    case VisualEffectKind::damage_number: return MaterialSpriteId::missing;
    case VisualEffectKind::defeat_marker: return MaterialSpriteId::missing;
    }
    return MaterialSpriteId::missing;
}

void draw_outlined_text(const char* text, int x, int y, int font_size,
    Color color, int outline_pixels) noexcept {
    const Color outline{5, 8, 14, 235};
    for (int offset_y = -outline_pixels; offset_y <= outline_pixels; ++offset_y) {
        for (int offset_x = -outline_pixels; offset_x <= outline_pixels; ++offset_x) {
            if (offset_x == 0 && offset_y == 0) continue;
            DrawText(text, x + offset_x, y + offset_y, font_size, outline);
        }
    }
    DrawText(text, x, y, font_size, color);
}

void draw_scene_label(Font font, bool font_ready, const char* text,
    float x, float y, int font_size, Color color) noexcept {
    if (font_ready && IsFontValid(font)) {
        draw_crisp_ui_text(font, text, {x, y},
            static_cast<float>(font_size), 1.0F, color);
        return;
    }
    DrawText(text, static_cast<int>(std::round(x)),
        static_cast<int>(std::round(y)), font_size, color);
}

void draw_effects(const CombatFeedback& feedback, const MaterialPack& material_pack,
    float width, float height, bool foreground) noexcept {
    for (const VisualEffect& effect : feedback.effects()) {
        if (!effect.active) continue;
        const bool is_foreground = effect.kind == VisualEffectKind::spark
            || effect.kind == VisualEffectKind::damage_number
            || effect.kind == VisualEffectKind::defeat_marker;
        if (is_foreground != foreground) continue;
        const float progress = effect.lifetime_seconds <= 0.0F ? 1.0F
            : std::clamp(effect.age_seconds / effect.lifetime_seconds, 0.0F, 1.0F);
        const float opacity = 1.0F - progress;
        const ScreenProjection projected = project_combat_position(effect.position, width, height);
        const MaterialSpriteId sprite = effect_sprite(effect.kind);
        if (sprite != MaterialSpriteId::missing
            && material_pack.draw(sprite, {projected.x, projected.y}, false,
                0.62F * projected.scale, Fade(WHITE, opacity))) {
            continue;
        }
        switch (effect.kind) {
        case VisualEffectKind::weapon_trail:
            DrawLineEx({projected.x - 36.0F * projected.scale, projected.y - 42.0F * projected.scale},
                {projected.x + 44.0F * projected.scale, projected.y - 68.0F * projected.scale},
                8.0F * projected.scale * opacity, Fade(Color{105, 224, 255, 255}, opacity)); break;
        case VisualEffectKind::dust:
            DrawEllipse(static_cast<int>(projected.x), static_cast<int>(projected.ground_y),
                (18.0F + progress * 24.0F) * projected.scale, 7.0F * projected.scale,
                Fade(Color{185, 193, 207, 255}, opacity)); break;
        case VisualEffectKind::spark:
            DrawCircleLines(static_cast<int>(projected.x),
                static_cast<int>(projected.y - 42.0F * projected.scale),
                (8.0F + progress * 18.0F) * projected.scale, Fade(Color{255, 218, 96, 255}, opacity));
            DrawLineEx({projected.x - 22.0F, projected.y - 54.0F},
                {projected.x + 25.0F, projected.y - 30.0F}, 3.0F,
                Fade(Color{255, 248, 210, 255}, opacity)); break;
        case VisualEffectKind::damage_number:
            DrawText(TextFormat("%d", effect.value), static_cast<int>(projected.x + 8.0F),
                static_cast<int>(projected.y - 90.0F - progress * 32.0F), 20,
                Fade(Color{255, 238, 156, 255}, opacity)); break;
        case VisualEffectKind::defeat_marker: {
            constexpr const char* kLabel = "DEFEATED";
            const int font_size = 24;
            const int label_x = static_cast<int>(projected.x)
                - MeasureText(kLabel, font_size) / 2;
            const int label_y = static_cast<int>(projected.y
                - 112.0F - progress * 24.0F);
            draw_outlined_text(kLabel, label_x, label_y, font_size,
                Fade(Color{255, 196, 92, 255}, opacity), 2);
            DrawCircleLines(static_cast<int>(projected.x),
                static_cast<int>(projected.y - 46.0F * projected.scale),
                (30.0F + progress * 18.0F) * projected.scale,
                Fade(Color{255, 196, 92, 255}, opacity));
            break;
        }
        }
    }
}

void draw_hazards(const CombatSnapshot& snapshot, float width, float height) noexcept {
    for (const HazardSnapshot& hazard : snapshot.hazards) {
        if (!uses_generic_hazard_pass(hazard)) continue;
        const HazardVisualMode mode = hazard_visual_mode(hazard);
        if (mode == HazardVisualMode::hidden) continue;
        const ScreenProjection projected = project_hazard_center(hazard, width, height);
        const float radius = std::max(9.0F, hazard.radius * 58.0F) * projected.scale;
        const Color color = to_color(hazard_color(hazard.kind));
        const Color draw_color = mode == HazardVisualMode::telegraph
            ? Fade(color, 0.82F) : color;
        if (mode == HazardVisualMode::active) {
            DrawEllipse(static_cast<int>(projected.x), static_cast<int>(projected.ground_y),
                radius, radius * 0.36F, draw_color);
        }
        DrawEllipseLines(static_cast<int>(projected.x), static_cast<int>(projected.ground_y),
            radius, radius * 0.36F, draw_color);
    }
}

void draw_projectiles(const CombatSnapshot& snapshot, const MaterialPack& material_pack,
    float width, float height, dungeon::DungeonElement ecology) noexcept {
    const Color color = to_color(monster_ecology_color(ecology));
    for (const ProjectileSnapshot& projectile : snapshot.projectiles) {
        if (!projectile.active) continue;
        const ScreenProjection projected = project_projectile_position(projectile, width, height);
        const float radius = std::max(3.0F, projectile.radius * 22.0F) * projected.scale;
        if (material_pack.draw(select_element_effect(ecology),
                {projected.x, projected.y}, false, 0.30F * projected.scale)) {
            continue;
        }
        DrawCircle(static_cast<int>(projected.x), static_cast<int>(projected.y), radius, color);
        DrawCircleLines(static_cast<int>(projected.x), static_cast<int>(projected.y),
            radius + 2.0F, Color{244, 248, 255, 235});
    }
}

void draw_monster_warning(const MonsterSnapshot& monster, Vec3 position,
    dungeon::DungeonElement ecology, float width, float height) noexcept {
    const MonsterVisual visual = monster_visual(monster.id, monster.ai_phase, ecology);
    if (visual.warning_mode == MonsterWarningMode::none) return;
    const ScreenProjection projected = project_combat_position(position, width, height);
    const Color warning = to_color(visual.warning);
    const float size = 34.0F * projected.scale;
    const float opacity = visual.warning_mode == MonsterWarningMode::active
        ? 0.38F : 0.18F;
    DrawEllipse(static_cast<int>(projected.x),
        static_cast<int>(projected.ground_y), size * 1.25F, size * 0.42F,
        Fade(warning, opacity));
    DrawEllipseLines(static_cast<int>(projected.x),
        static_cast<int>(projected.ground_y), size * 1.25F, size * 0.42F,
        warning);
    if (visual.priority_warning) {
        draw_outlined_text("!", static_cast<int>(projected.x - 7.0F),
            static_cast<int>(projected.y - 126.0F * projected.scale),
            28, warning, 2);
    }
    if (visual.shape == MonsterShapeId::charger || visual.shape == MonsterShapeId::dasher) {
        const ScreenProjection target = project_combat_position(monster.attack_target_position, width, height);
        DrawLineEx({projected.x, projected.ground_y - 18.0F * projected.scale},
            {target.x, target.ground_y - 18.0F * target.scale},
            visual.warning_mode == MonsterWarningMode::active ? 5.0F : 2.0F, warning); return;
    }
    if (visual.shape == MonsterShapeId::hazard_caster) {
        const ScreenProjection target = project_combat_position(monster.attack_target_position, width, height);
        DrawEllipseLines(static_cast<int>(target.x), static_cast<int>(target.ground_y),
            size * 1.5F, size * 0.55F, warning); return;
    }
    DrawCircleLines(static_cast<int>(projected.x), static_cast<int>(projected.ground_y - size), size, warning);
}

void draw_blink_affix_warning(const MonsterSnapshot& monster, Vec3 position,
    float width, float height) noexcept {
    if (!blink_affix_warning_visible(monster)) return;
    const ScreenProjection projected = project_combat_position(position, width, height);
    const Color warning{255, 86, 214, 235};
    const float actor_radius = blink_affix_warning_actor_radius(monster) * projected.scale;
    const float ground_radius = blink_affix_warning_ground_radius(monster) * projected.scale;
    DrawCircleLines(static_cast<int>(projected.x),
        static_cast<int>(projected.y - 44.0F * projected.scale), actor_radius, warning);
    DrawEllipseLines(static_cast<int>(projected.x), static_cast<int>(projected.ground_y),
        ground_radius, ground_radius * 0.38F, warning);
}

bool draw_material_actor(const MaterialPack& material_pack,
    MaterialSpriteId sprite, Facing facing, bool player,
    const ScreenProjection& projected, float hit_flash_seconds = 0.0F) noexcept {
    const float scale = material_actor_draw_scale(player, projected.scale);
    if (!material_pack.draw(sprite, {projected.x, projected.y},
            facing == Facing::left, scale)) {
        return false;
    }
    if (hit_flash_seconds > 0.0F) {
        BeginBlendMode(BLEND_ADDITIVE);
        static_cast<void>(material_pack.draw(sprite, {projected.x, projected.y},
            facing == Facing::left, scale, Color{255, 249, 220, 185}));
        EndBlendMode();
    }
    return true;
}

std::uint16_t player_loop_duration_ticks(
    const PlayerAnimationClipDefinition& clip) noexcept {
    return static_cast<std::uint16_t>((static_cast<std::uint32_t>(clip.frame_count)
        * 60U + clip.frames_per_second - 1U) / clip.frames_per_second);
}

bool draw_player_animation(const MaterialPack& material_pack,
    const PlayerSnapshot& player, std::uint64_t world_tick,
    const ScreenProjection& projected) noexcept {
    PlayerAnimationClipId clip_id = select_player_animation_clip(
        player.state, player.active_attack);
    std::uint64_t elapsed_ticks = world_tick;
    std::uint16_t duration_ticks{};
    bool loop = true;
    if (player.hp <= 0) {
        clip_id = PlayerAnimationClipId::death;
        elapsed_ticks = 1U;
        duration_ticks = 1U;
        loop = false;
    } else if (player.hurt_ticks > 0U) {
        clip_id = PlayerAnimationClipId::hurt;
    } else if (player.active_attack != AttackId::none
        && (player.state == PlayerState::attack_startup
            || player.state == PlayerState::attack_active
            || player.state == PlayerState::attack_recovery)) {
        const AttackDefinition* const attack = find_attack_definition(
            player.active_attack);
        if (attack != nullptr) {
            elapsed_ticks = player.attack_elapsed_ticks;
            duration_ticks = static_cast<std::uint16_t>(attack->startup_ticks
                + attack->active_ticks + attack->recovery_ticks);
            loop = false;
        }
    }
    const PlayerAnimationClipDefinition* const clip = player_animation_clip(clip_id);
    if (clip == nullptr) return false;
    if (duration_ticks == 0U) duration_ticks = player_loop_duration_ticks(*clip);
    const std::uint16_t frame_index = player_animation_frame_index(*clip,
        elapsed_ticks, duration_ticks, loop);
    const auto frame = player_animation_frame(*clip, frame_index);
    if (!frame.has_value()) return false;
    return material_pack.draw_frame(frame->atlas, frame->source, frame->foot_anchor,
        {projected.x, projected.y}, player.facing == Facing::left,
        material_actor_draw_scale(true, projected.scale));
}

bool draw_monster_animation(const MaterialPack& material_pack,
    const MonsterSnapshot& monster, const MonsterMaterialDrawPlan& plan,
    const ScreenProjection& projected, float hit_flash_seconds) noexcept {
    if (!plan.use_material_frame || !plan.frame.has_value()) return false;
    const MonsterAnimationFrame& frame = *plan.frame;
    const float scale = monster_material_draw_scale(frame.atlas, projected.scale);
    const bool drawn = material_pack.draw_frame(frame.atlas, frame.source,
        frame.foot_anchor, {projected.x, projected.y},
        monster.facing == Facing::left, scale);
    if (drawn && hit_flash_seconds > 0.0F) {
        BeginBlendMode(BLEND_ADDITIVE);
        static_cast<void>(material_pack.draw_frame(frame.atlas, frame.source,
            frame.foot_anchor, {projected.x, projected.y},
            monster.facing == Facing::left, scale,
            Color{230, 248, 255, 175}));
        EndBlendMode();
    }
    return drawn;
}

void draw_player_geometry(const ScreenProjection& projected) noexcept {
    const float body_width = 42.0F * projected.scale;
    const float body_height = 82.0F * projected.scale;
    DrawRectangleRounded({projected.x - body_width * .5F,
        projected.y - body_height, body_width, body_height}, .20F, 6,
        Color{65, 202, 223, 255});
    DrawTriangle({projected.x, projected.y - body_height - 16.0F * projected.scale},
        {projected.x - 12.0F * projected.scale,
            projected.y - body_height + 5.0F * projected.scale},
        {projected.x + 12.0F * projected.scale,
            projected.y - body_height + 5.0F * projected.scale},
        Color{134, 237, 255, 255});
}

void draw_player_hit_direction(const CombatFeedback& feedback,
    Vec3 player_position, float width, float height) noexcept {
    const float seconds = feedback.player_hit_indicator_seconds();
    if (seconds <= 0.0F) return;
    const ScreenProjection player = project_combat_position(
        player_position, width, height);
    const ScreenProjection source = project_combat_position(
        feedback.player_hit_source(), width, height);
    float direction_x = source.x - player.x;
    float direction_y = source.ground_y - player.ground_y;
    const float length = std::sqrt(direction_x * direction_x
        + direction_y * direction_y);
    if (length <= 0.001F) return;
    direction_x /= length;
    direction_y /= length;
    const float radius = 46.0F * player.scale;
    const Vector2 tip{player.x + direction_x * radius,
        player.y - 42.0F * player.scale + direction_y * radius};
    const Vector2 base{tip.x - direction_x * 16.0F * player.scale,
        tip.y - direction_y * 16.0F * player.scale};
    const Vector2 perpendicular{-direction_y * 9.0F * player.scale,
        direction_x * 9.0F * player.scale};
    const float opacity = std::clamp(seconds / 0.55F, 0.0F, 1.0F);
    DrawTriangle(tip, {base.x + perpendicular.x, base.y + perpendicular.y},
        {base.x - perpendicular.x, base.y - perpendicular.y},
        Fade(Color{255, 72, 80, 255}, opacity));
}

void draw_monster_presentation(const MonsterSnapshot& monster, Vec3 position,
    dungeon::DungeonElement ecology, float width, float height,
    std::uint64_t tick, std::size_t label_lane,
    Font hud_font, bool hud_font_ready) noexcept {
    const ScreenProjection projected = project_combat_position(position, width, height);
    const MonsterVisual visual = monster_visual(monster.id, monster.ai_phase, ecology);
    const float viewport_scale = ui_viewport_scale(
        static_cast<int>(width), static_cast<int>(height));
    const MonsterLabelTextStyle text_style =
        monster_label_text_style(viewport_scale);
    const float scale = projected.scale;
    const float x = projected.x;
    const float y = projected.y;
    // Keep each monster's complete information block in a stable screen lane.
    // Close combat naturally stacks actors; placing every label at the actor's
    // feet made role and phase names unreadable precisely when they mattered.
    const float label_offset = static_cast<float>(label_lane % 4U)
        * 54.0F * viewport_scale;
    const float role_y = y - 120.0F * scale - label_offset;
    const float phase_y = role_y
        + static_cast<float>(text_style.role_font_size) + 2.0F;
    const std::size_t affix_count = std::min<std::size_t>(
        monster.affixes.count, monster.affixes.values.size());
    for (std::size_t index = 0U; index < affix_count; ++index) {
        const AffixOutline outline = monster_affix_outline(
            monster.affixes.values[index], tick);
        const float radius = (35.0F + static_cast<float>(index) * 4.0F) * scale;
        DrawEllipseLines(static_cast<int>(x), static_cast<int>(y - 42.0F * scale),
            radius, radius * 1.18F,
            Fade(to_color(outline.color), static_cast<float>(outline.alpha) / 255.0F));
    }
    const MonsterBarVisualPlan bar_visual = make_monster_bar_visual_plan(monster);
    const float bar_width = 54.0F * scale;
    for (std::size_t index = 0U; index < bar_visual.bars.size(); ++index) {
        const MonsterBarPlan& bar = bar_visual.bars[index];
        if (!bar.visible) continue;
        draw_bar(x - bar_width * .5F,
            role_y - 8.0F - static_cast<float>(index) * 7.0F,
            bar_width, bar.ratio, hud_palette_color(bar.palette_id));
    }
    for (std::size_t index = 0U; index < affix_count; ++index) {
        const AffixBadge badge = monster_affix_badge(monster.affixes.values[index]);
        draw_scene_label(hud_font, hud_font_ready,
            TextFormat("%s %s", badge.short_name, badge.tier_text),
            x - bar_width * .5F,
            phase_y + static_cast<float>(text_style.phase_font_size + 2)
                + static_cast<float>(index)
                    * static_cast<float>(text_style.affix_font_size + 2),
            text_style.affix_font_size, to_color(badge.color));
    }
    draw_scene_label(hud_font, hud_font_ready, visual.role_label,
        x - bar_width * .5F, role_y, text_style.role_font_size,
        Color{248, 246, 238, 255});
    draw_scene_label(hud_font, hud_font_ready,
        monster_phase_name(monster.ai_phase), x - bar_width * .5F,
        phase_y, text_style.phase_font_size, Color{205, 218, 237, 255});
}

void draw_monster_silhouette(const MonsterSnapshot& monster, Vec3 position,
    dungeon::DungeonElement ecology, float width, float height,
    const CombatFeedback& feedback, std::size_t monster_index,
    std::uint64_t tick, std::size_t label_lane,
    Font hud_font, bool hud_font_ready) noexcept {
    const ScreenProjection projected = project_combat_position(position, width, height);
    const MonsterVisual visual = monster_visual(monster.id, monster.ai_phase, ecology);
    Color body = to_color(visual.body);
    if (feedback.target_flash_seconds(monster_index) > 0.0F) body = Color{255, 249, 220, 255};
    const Color accent = to_color(visual.accent);
    const float scale = projected.scale; const float x = projected.x; const float y = projected.y;
    switch (visual.shape) {
    case MonsterShapeId::bomber:
        DrawCircle(static_cast<int>(x), static_cast<int>(y - 43.0F * scale), 27.0F * scale, body);
        DrawCircleLines(static_cast<int>(x), static_cast<int>(y - 43.0F * scale), 31.0F * scale, accent);
        DrawLineEx({x - 14.0F * scale, y - 70.0F * scale}, {x + 16.0F * scale, y - 82.0F * scale}, 3.0F * scale, accent); break;
    case MonsterShapeId::charger:
        DrawRectangleRounded({x - 22.0F * scale, y - 78.0F * scale, 44.0F * scale, 78.0F * scale}, .12F, 5, body);
        DrawLineEx({x - 8.0F * scale, y - 54.0F * scale}, {x + 45.0F * scale, y - 76.0F * scale}, 6.0F * scale, accent); break;
    case MonsterShapeId::bulwark:
        DrawRectangleRounded({x - 34.0F * scale, y - 86.0F * scale, 68.0F * scale, 86.0F * scale}, .12F, 5, body);
        DrawRectangleLinesEx({x + 18.0F * scale, y - 66.0F * scale, 18.0F * scale, 52.0F * scale}, 3.0F * scale, accent); break;
    case MonsterShapeId::support:
        DrawCircle(static_cast<int>(x), static_cast<int>(y - 44.0F * scale), 25.0F * scale, body);
        DrawLineEx({x - 22.0F * scale, y - 44.0F * scale}, {x + 22.0F * scale, y - 44.0F * scale}, 5.0F * scale, accent);
        DrawLineEx({x, y - 66.0F * scale}, {x, y - 22.0F * scale}, 5.0F * scale, accent);
        if (monster.shield > 0) DrawCircleLines(static_cast<int>(x), static_cast<int>(y - 44.0F * scale), 33.0F * scale, accent); break;
    case MonsterShapeId::shooter:
        DrawRectangleRounded({x - 21.0F * scale, y - 70.0F * scale, 42.0F * scale, 70.0F * scale}, .16F, 5, body);
        DrawLineEx({x, y - 55.0F * scale}, {x + 38.0F * scale, y - 55.0F * scale}, 6.0F * scale, accent); break;
    case MonsterShapeId::dasher:
        DrawTriangle({x + 30.0F * scale, y - 39.0F * scale}, {x - 27.0F * scale, y - 72.0F * scale}, {x - 27.0F * scale, y - 8.0F * scale}, body);
        DrawLineEx({x - 26.0F * scale, y - 39.0F * scale}, {x + 24.0F * scale, y - 39.0F * scale}, 4.0F * scale, accent); break;
    case MonsterShapeId::chaser:
        DrawTriangle({x, y - 82.0F * scale}, {x - 27.0F * scale, y}, {x + 27.0F * scale, y}, body);
        DrawLineEx({x - 25.0F * scale, y - 33.0F * scale}, {x - 43.0F * scale, y - 10.0F * scale}, 4.0F * scale, accent);
        DrawLineEx({x + 25.0F * scale, y - 33.0F * scale}, {x + 43.0F * scale, y - 10.0F * scale}, 4.0F * scale, accent); break;
    case MonsterShapeId::hazard_caster:
        DrawRectangleRounded({x - 20.0F * scale, y - 73.0F * scale, 40.0F * scale, 73.0F * scale}, .20F, 5, body);
        DrawLineEx({x + 12.0F * scale, y - 15.0F * scale}, {x + 25.0F * scale, y - 84.0F * scale}, 4.0F * scale, accent);
        DrawCircleLines(static_cast<int>(x + 25.0F * scale), static_cast<int>(y - 88.0F * scale), 8.0F * scale, accent); break;
    }

    draw_monster_presentation(monster, position, ecology, width, height,
        tick, label_lane, hud_font, hud_font_ready);
}

}  // namespace

MonsterBarVisualPlan make_monster_bar_visual_plan(
    const combat::MonsterSnapshot& monster) noexcept {
    MonsterBarVisualPlan plan{};
    plan.bars[0] = {true, monster_bar_ratio(monster.hp, monster.max_hp),
        HudPaletteId::health};
    plan.bars[1] = {monster.max_shield > 0,
        monster_bar_ratio(monster.shield, monster.max_shield),
        HudPaletteId::barrier};
    plan.bars[2] = {monster.max_break > 0,
        monster_bar_ratio(monster.break_value, monster.max_break),
        HudPaletteId::experience};
    return plan;
}

void CombatRenderer::draw_actors(const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current,
    const ActiveSkillEffectPlan& active_skill_plan,
    float alpha, bool draw_debug,
    const CombatFeedback& feedback) noexcept {
    monster_material_draw_statuses_.fill({});
    if (!current.combat.has_value()) return;
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const CombatSnapshot& current_combat = *current.combat;
    const CombatSnapshot& previous_combat = can_interpolate_room(previous, current)
        ? *previous.combat : current_combat;
    std::array<RenderActor, kMonsterCapacity + 1> draw_items{};
    std::size_t draw_count = 0;
    if (!active_skill_plan.suppress_base_player) {
        draw_items[draw_count++] = {
            current_combat.player.position, 0U, true, {}};
    }
    for (std::size_t index = 0; index < current_combat.monsters.size(); ++index) {
        const MonsterSnapshot& monster = current_combat.monsters[index];
        const MonsterMaterialDrawPlan material_plan =
            monster_presenter_.collect_draw_plan(index, monster,
                current_combat.tick,
                feedback.target_flash_seconds(index) > 0.0F);
        if (!material_plan.visible) continue;
        MonsterMaterialDrawRuntimeStatus& material_status =
            monster_material_draw_statuses_[static_cast<std::size_t>(monster.id)];
        material_status.presenter_visible = true;
        material_status.use_material_frame = material_plan.use_material_frame;
        material_status.frame_index = material_plan.frame_index;
        if (material_plan.frame.has_value()) {
            material_status.atlas = material_plan.frame->atlas;
        }
        Vec3 position = monster.position;
        const MonsterSnapshot& previous_monster = previous_combat.monsters[index];
        if (monster.id == previous_monster.id && monster.generation == previous_monster.generation
            && previous_monster.active) position = interpolate(
                previous_monster.position, monster.position, alpha);
        draw_items[draw_count++] = {position, static_cast<std::uint8_t>(index),
            false, material_plan};
        draw_monster_warning(monster, position, current.ecology, width, height);
        draw_blink_affix_warning(monster, position, width, height);
    }
    sort_render_actors(draw_items, draw_count);
    draw_hazards(current_combat, width, height);
    draw_effects(feedback, material_pack_, width, height, false);
    draw_projectiles(current_combat, material_pack_, width, height, current.ecology);
    for (std::size_t index = 0; index < draw_count; ++index) {
        const RenderActor& item = draw_items[index];
        const ScreenProjection ground = project_combat_position({item.position.x, item.position.y, 0.0F}, width, height);
        const float shadow_width = (item.player ? 48.0F : 54.0F) * ground.scale;
        DrawEllipse(static_cast<int>(ground.x), static_cast<int>(ground.ground_y + 3.0F), shadow_width, 10.0F * ground.scale, Color{3, 5, 8, 125});
    }
    for (std::size_t index = 0; index < draw_count; ++index) {
        const RenderActor& item = draw_items[index];
        const ScreenProjection projected = project_combat_position(item.position, width, height);
        if (item.player) {
            MaterialSpriteId sprite = select_player_sprite(
                current_combat.player.state, current_combat.player.active_attack);
            if (current_combat.player.hp <= 0) {
                sprite = MaterialSpriteId::player_dead;
            } else if (current_combat.player.hurt_ticks > 0) {
                sprite = MaterialSpriteId::player_hurt;
            }
            if (!draw_player_animation(material_pack_, current_combat.player,
                    current_combat.tick, projected)
                && !draw_material_actor(material_pack_, sprite,
                    current_combat.player.facing, true, projected)) {
                draw_player_geometry(projected);
            }
            draw_player_hit_direction(feedback, item.position, width, height);
        } else {
            const MonsterSnapshot& monster = current_combat.monsters[item.monster_index];
            const MaterialSpriteId sprite = select_monster_sprite(monster.id,
                monster.ai_phase);
            if (monster.affixes.count > 0U) {
                static_cast<void>(material_pack_.draw(MaterialSpriteId::effect_affix_aura,
                    {projected.x, projected.ground_y}, false, 0.72F * projected.scale,
                    Color{230, 142, 255, 155}));
            }
            const float hit_flash_seconds = feedback.target_flash_seconds(
                item.monster_index);
            const bool material_frame_drawn = draw_monster_animation(
                material_pack_, monster, item.material_plan, projected,
                hit_flash_seconds);
            monster_material_draw_statuses_[static_cast<std::size_t>(monster.id)]
                .drawn |= material_frame_drawn;
            const bool legacy_frame_drawn = !item.material_plan.use_material_frame
                && draw_material_actor(material_pack_, sprite, monster.facing,
                    false, projected, hit_flash_seconds);
            const MonsterRenderPath render_path = select_monster_render_path(
                item.material_plan.use_material_frame, material_frame_drawn,
                legacy_frame_drawn);
            if (render_path != MonsterRenderPath::silhouette) {
                draw_monster_presentation(monster, item.position, current.ecology,
                    width, height, current_combat.tick, item.monster_index,
                    hud_renderer_.hud_font(), hud_renderer_.font_ready());
            } else {
                draw_monster_silhouette(monster, item.position, current.ecology,
                    width, height, feedback, item.monster_index, current_combat.tick,
                    item.monster_index, hud_renderer_.hud_font(),
                    hud_renderer_.font_ready());
            }
        }
    }
    draw_effects(feedback, material_pack_, width, height, true);
    if (draw_debug) draw_debug_world_volumes(current_combat, width, height);
}

}  // namespace arpg::platform
