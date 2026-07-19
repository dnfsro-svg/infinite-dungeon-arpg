#include "combat_renderer.hpp"

#include "combat_view_math.hpp"
#include "hud_renderer.hpp"
#include "dungeon_view_math.hpp"
#include "material_animation.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

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

struct RenderActor final { Vec3 position{}; std::uint8_t monster_index{}; bool player{}; };

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
    }
    return MaterialSpriteId::missing;
}

void draw_effects(const CombatFeedback& feedback, const MaterialPack& material_pack,
    float width, float height, bool foreground) noexcept {
    for (const VisualEffect& effect : feedback.effects()) {
        if (!effect.active) continue;
        const bool is_foreground = effect.kind == VisualEffectKind::spark
            || effect.kind == VisualEffectKind::damage_number;
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

void draw_monster_presentation(const MonsterSnapshot& monster, Vec3 position,
    dungeon::DungeonElement ecology, float width, float height,
    std::uint64_t tick) noexcept {
    const ScreenProjection projected = project_combat_position(position, width, height);
    const MonsterVisual visual = monster_visual(monster.id, monster.ai_phase, ecology);
    const float scale = projected.scale;
    const float x = projected.x;
    const float y = projected.y;
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
    constexpr float kBarOffsets[] = {102.0F, 95.0F, 88.0F};
    for (std::size_t index = 0U; index < bar_visual.bars.size(); ++index) {
        const MonsterBarPlan& bar = bar_visual.bars[index];
        if (!bar.visible) continue;
        draw_bar(x - bar_width * .5F, y - kBarOffsets[index] * scale,
            bar_width, bar.ratio, hud_palette_color(bar.palette_id));
    }
    for (std::size_t index = 0U; index < affix_count; ++index) {
        const AffixBadge badge = monster_affix_badge(monster.affixes.values[index]);
        DrawText(TextFormat("%s %s", badge.short_name, badge.tier_text),
            static_cast<int>(x - bar_width * .5F),
            static_cast<int>(y - (80.0F - static_cast<float>(index) * 10.0F) * scale),
            9, to_color(badge.color));
    }
    DrawText(visual.role_label, static_cast<int>(x - bar_width * .5F),
        static_cast<int>(y + 7.0F), 11, Color{225, 230, 239, 230});
    DrawText(monster_phase_name(monster.ai_phase),
        static_cast<int>(x - bar_width * .5F), static_cast<int>(y + 19.0F), 10,
        Color{184, 196, 213, 220});
}

void draw_monster_silhouette(const MonsterSnapshot& monster, Vec3 position,
    dungeon::DungeonElement ecology, float width, float height,
    const CombatFeedback& feedback, std::size_t monster_index,
    std::uint64_t tick) noexcept {
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

    draw_monster_presentation(monster, position, ecology, width, height, tick);
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
    const dungeon::DungeonSnapshot& current, float alpha, bool draw_debug,
    const CombatFeedback& feedback) const noexcept {
    if (!current.combat.has_value()) return;
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    const CombatSnapshot& current_combat = *current.combat;
    const CombatSnapshot& previous_combat = can_interpolate_room(previous, current)
        ? *previous.combat : current_combat;
    std::array<RenderActor, kMonsterCapacity + 1> draw_items{};
    std::size_t draw_count = 0;
    draw_items[draw_count++] = {current_combat.player.position, 0U, true};
    for (std::size_t index = 0; index < current_combat.monsters.size(); ++index) {
        const MonsterSnapshot& monster = current_combat.monsters[index];
        if (!monster_visible(monster)) continue;
        Vec3 position = monster.position;
        const MonsterSnapshot& previous_monster = previous_combat.monsters[index];
        if (monster.id == previous_monster.id && monster.generation == previous_monster.generation
            && monster_visible(previous_monster)) position = interpolate(previous_monster.position, monster.position, alpha);
        draw_items[draw_count++] = {position, static_cast<std::uint8_t>(index), false};
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
            if (!draw_material_actor(material_pack_, sprite,
                    current_combat.player.facing, true, projected)) {
                draw_player_geometry(projected);
            }
        } else {
            const MonsterSnapshot& monster = current_combat.monsters[item.monster_index];
            const MaterialSpriteId sprite = select_monster_sprite(monster.id,
                monster.ai_phase);
            if (monster.affixes.count > 0U) {
                static_cast<void>(material_pack_.draw(MaterialSpriteId::effect_affix_aura,
                    {projected.x, projected.ground_y}, false, 0.72F * projected.scale,
                    Color{230, 142, 255, 155}));
            }
            if (draw_material_actor(material_pack_, sprite, monster.facing, false,
                    projected, feedback.target_flash_seconds(item.monster_index))) {
                draw_monster_presentation(monster, item.position, current.ecology,
                    width, height, current_combat.tick);
            } else {
                draw_monster_silhouette(monster, item.position, current.ecology,
                    width, height, feedback, item.monster_index, current_combat.tick);
            }
        }
    }
    draw_effects(feedback, material_pack_, width, height, true);
    if (draw_debug) draw_debug_world_volumes(current_combat, width, height);
}

}  // namespace arpg::platform
