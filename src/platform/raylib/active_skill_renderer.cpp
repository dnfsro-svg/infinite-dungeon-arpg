#include "active_skill_renderer.hpp"

#include "ui_text_contrast.hpp"
#include "ui_text_bounds_audit.hpp"
#include "ui_text_renderer.hpp"
#include "ui_typography.hpp"

#include "combat/active_skill_runtime.hpp"
#include "combat/room_bounds.hpp"
#include "combat_view_math.hpp"
#include "skills/active_skill_catalog.hpp"
#include "ui_material.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace arpg::platform {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr std::uint64_t kFlashTicks = 7U;
constexpr std::uint16_t kDrawSlashImpactTick = 46U;
constexpr std::uint16_t kStormFinisherTick = 324U;
constexpr std::size_t kStormGroundSwordCount = 12U;

[[nodiscard]] bool recent_finisher_event(
    const combat::CombatSnapshot& snapshot,
    const combat::CombatEvent* event,
    std::uint64_t& age) noexcept {
    if (event == nullptr
        || event->skill != skills::ActiveSkillId::storm_swords
        || !event->finisher || snapshot.tick < event->tick) {
        return false;
    }
    age = snapshot.tick - event->tick;
    return age <= kFlashTicks;
}

void draw_skill_text(Font font, const char* text,
    float x, float y, float size, Color color) noexcept {
    if (text == nullptr || text[0] == '\0') return;
    const UiTextContrastStyle style = ui_text_contrast_style();
    color.a = 255U;
    if (ui_luma_contrast_ratio(color, style.backing) < 4.5F) {
        color = style.muted;
    }
    draw_crisp_ui_text(font, text, {x, y}, size, 0.5F, color);
}

[[nodiscard]] bool draw_draw_slash(const DrawSlashVisualPlan& plan,
    const CombatCameraView& camera,
    float width, float height) noexcept {
    if (!plan.visible) return false;
    const ScreenProjection projected = project_combat_position(
        plan.center, camera, width, height);
    constexpr std::size_t kArcSegments = 14U;
    std::array<Vector2, kArcSegments + 2U> fan{};
    const float direction = plan.facing == combat::Facing::left ? -1.0F : 1.0F;
    const Vector2 origin{projected.x,
        projected.ground_y - 42.0F * projected.scale};
    fan[0U] = origin;
    const float radius = 250.0F * projected.scale;
    for (std::size_t index = 0U; index <= kArcSegments; ++index) {
        const float t = static_cast<float>(index)
            / static_cast<float>(kArcSegments);
        const float angle = (-0.62F + t * 1.24F)
            + (direction < 0.0F ? kPi : 0.0F);
        fan[index + 1U] = {
            origin.x + std::cos(angle) * radius,
            origin.y + std::sin(angle) * radius * 0.58F,
        };
    }
    DrawTriangleFan(fan.data(), static_cast<int>(fan.size()),
        Fade(Color{150, 224, 255, 255}, 0.24F * plan.opacity));
    for (std::size_t index = 1U; index + 1U < fan.size(); ++index) {
        DrawLineEx(fan[index], fan[index + 1U],
            3.0F * projected.scale,
            Fade(Color{236, 251, 255, 255}, plan.opacity));
    }
    return true;
}

void fit_skill_label(Font font, const char* source, char* output,
    std::size_t capacity, float width, float size) noexcept {
    if (output == nullptr || capacity == 0U) return;
    static_cast<void>(std::snprintf(output, capacity, "%s",
        source == nullptr ? "" : source));
    output[capacity - 1U] = '\0';
    if (MeasureTextEx(font, output, size, 0.5F).x <= width) return;
    std::size_t length = std::char_traits<char>::length(output);
    while (length > 0U) {
        do {
            --length;
        } while (length > 0U
            && (static_cast<unsigned char>(output[length]) & 0xC0U) == 0x80U);
        output[length] = '\0';
        if (length + 4U >= capacity) continue;
        static_cast<void>(std::snprintf(output + length,
            capacity - length, "..."));
        if (MeasureTextEx(font, output, size, 0.5F).x <= width) return;
        output[length] = '\0';
    }
}

void draw_sword(Vector2 center, float angle, float scale,
    bool highlighted, float opacity) noexcept {
    const Vector2 direction{std::cos(angle), std::sin(angle)};
    const Vector2 normal{-direction.y, direction.x};
    const float length = (highlighted ? 54.0F : 42.0F) * scale;
    const float half_width = (highlighted ? 6.5F : 5.2F) * scale;
    const Vector2 tip{center.x + direction.x * length * 0.58F,
        center.y + direction.y * length * 0.58F};
    const Vector2 base{center.x - direction.x * length * 0.42F,
        center.y - direction.y * length * 0.42F};
    const Vector2 left{base.x + normal.x * half_width,
        base.y + normal.y * half_width};
    const Vector2 right{base.x - normal.x * half_width,
        base.y - normal.y * half_width};
    const Color blade_edge = highlighted
        ? Fade(Color{248, 251, 255, 255}, opacity)
        : Fade(Color{126, 154, 182, 255}, opacity * 0.90F);
    const Color blade_core = highlighted
        ? Fade(Color{130, 207, 255, 255}, opacity)
        : Fade(Color{35, 58, 83, 255}, opacity);
    DrawTriangle(tip, left, right, blade_edge);
    const Vector2 core_tip{tip.x - direction.x * 4.0F * scale,
        tip.y - direction.y * 4.0F * scale};
    DrawTriangle(core_tip,
        {left.x + direction.x * 4.0F * scale, left.y + direction.y * 4.0F * scale},
        {right.x + direction.x * 4.0F * scale, right.y + direction.y * 4.0F * scale},
        blade_core);
    DrawLineEx({base.x + normal.x * half_width * 1.6F,
                   base.y + normal.y * half_width * 1.6F},
        {base.x - normal.x * half_width * 1.6F,
            base.y - normal.y * half_width * 1.6F},
        2.4F * scale, Fade(Color{202, 155, 67, 255}, opacity));
    DrawLineEx(base,
        {base.x - direction.x * 12.0F * scale,
         base.y - direction.y * 12.0F * scale},
        3.0F * scale, Fade(Color{38, 30, 29, 255}, opacity));
}

[[nodiscard]] bool draw_storm_swords(const StormSwordsVisualPlan& plan,
    const CombatCameraView& camera,
    float width, float height) noexcept {
    if (!plan.visible && !plan.finisher_visible) return false;
    const ScreenProjection projected = project_combat_position(
        plan.center, camera, width, height);
    bool drawn = false;
    if (plan.visible) {
        for (std::size_t index = 0U; index < plan.sword_count; ++index) {
            const StormSwordVisual& sword = plan.swords[index];
            if (!sword.visible) continue;
            const bool aerial = sword.band == StormSwordBand::aerial;
            const float radius = (aerial ? 112.0F : 155.0F) * projected.scale;
            const Vector2 position{
                projected.x + std::cos(sword.angle_radians) * radius,
                projected.ground_y
                    + std::sin(sword.angle_radians) * radius
                        * (aerial ? 0.26F : 0.38F)
                    - (aerial ? 138.0F : 54.0F) * projected.scale,
            };
            draw_sword(position, sword.angle_radians + kPi,
                projected.scale * (aerial ? 0.96F : 1.0F),
                sword.highlighted, aerial ? 1.0F : 0.92F);
            drawn = true;
        }
    }
    if (!plan.finisher_visible) return drawn;
    const float opacity = std::clamp(plan.finisher_opacity, 0.0F, 1.0F);
    const Vector2 sword_center{projected.x,
        projected.ground_y - 118.0F * projected.scale};
    draw_sword(sword_center, kPi * 0.5F, projected.scale * 1.8F,
        true, opacity);
    const float wave = (95.0F + (1.0F - opacity) * 95.0F) * projected.scale;
    DrawEllipseLines(static_cast<int>(projected.x),
        static_cast<int>(projected.ground_y), wave, wave * 0.34F,
        Fade(Color{225, 247, 255, 255}, opacity));
    return true;
}

[[nodiscard]] bool draw_material_active_skill(
    const ActiveSkillEffectPlan& plan,
    const MaterialPack& material_pack,
    const CombatCameraView& camera,
    float width, float height) noexcept {
    skills::ActiveSkillId id = skills::ActiveSkillId::none;
    if (plan.atlas == MaterialAtlasId::skill_draw_slash) {
        id = skills::ActiveSkillId::draw_slash;
    } else if (plan.atlas == MaterialAtlasId::skill_storm_swords) {
        id = skills::ActiveSkillId::storm_swords;
    }
    const auto frame = active_skill_atlas_frame(id, plan.atlas_frame);
    if (!frame.has_value()) return false;
    const ScreenProjection player = project_combat_position(
        plan.player_position, camera, width, height);
    const bool flip_x = id == skills::ActiveSkillId::draw_slash
        && plan.draw_slash.facing == combat::Facing::left;
    const float scale = active_skill_material_draw_scale(id, player.scale);
    return material_pack.draw_frame(frame->atlas, frame->source,
        frame->foot_anchor, {player.x, player.ground_y}, flip_x, scale, WHITE);
}

}  // namespace

std::size_t active_skill_visual_frame_index(
    skills::ActiveSkillId id, std::uint16_t elapsed_ticks) noexcept {
    const std::size_t ticks = static_cast<std::size_t>(elapsed_ticks);
    if (id == skills::ActiveSkillId::draw_slash) {
        return (std::min<std::size_t>)(35U, ticks * 36U / 90U);
    }
    if (id != skills::ActiveSkillId::storm_swords) return 0U;
    std::size_t frame{};
    if (elapsed_ticks < 72U) {
        frame = ticks * 5U / 72U;
    } else if (elapsed_ticks < 324U) {
        frame = 5U + (ticks - 72U) * 15U / 252U;
    } else if (elapsed_ticks < 342U) {
        frame = 20U + (ticks - 324U) * 2U / 18U;
    } else {
        frame = 22U + (ticks - 342U) * 2U / 18U;
    }
    return (std::min<std::size_t>)(23U, frame);
}

ActiveSkillEffectPlan make_active_skill_effect_plan(
    const combat::CombatSnapshot& snapshot,
    const combat::CombatEvent* last_event,
    bool material_ready) noexcept {
    ActiveSkillEffectPlan result{};
    const combat::ActiveSkillSnapshot& skill = snapshot.active_skill;
    result.player_position = snapshot.player.position;
    result.effect_center = skill.locked_center;
    if (skill.id != skills::ActiveSkillId::draw_slash
            && skill.id != skills::ActiveSkillId::storm_swords) {
        return result;
    }
    result.atlas = active_skill_material_atlas(skill.id);
    result.atlas_frame = active_skill_visual_frame_index(
        skill.id, skill.elapsed_ticks);
    result.mode = material_ready ? ActiveSkillVisualMode::material
                                 : ActiveSkillVisualMode::procedural_fallback;
    result.suppress_base_player = material_ready;
    result.procedural_main_visual_count = material_ready ? 0U : 1U;

    result.draw_slash.facing = snapshot.player.facing;
    result.draw_slash.center = result.effect_center;
    result.storm_swords.center = result.effect_center;
    if (!material_ready && skill.id == skills::ActiveSkillId::draw_slash) {
        result.draw_slash.visible = true;
        result.draw_slash.opacity = 0.65F;
        if (skill.elapsed_ticks >= kDrawSlashImpactTick) {
        const std::uint16_t age = static_cast<std::uint16_t>(
            skill.elapsed_ticks - kDrawSlashImpactTick);
        if (age <= kFlashTicks) {
            result.draw_slash.opacity = 1.0F
                - static_cast<float>(age)
                    / static_cast<float>(kFlashTicks + 1U);
        }
        }
    }

    if (!material_ready && skill.id == skills::ActiveSkillId::storm_swords) {
        result.storm_swords.sword_count = std::min<std::size_t>(
            skill.spawned_sword_count, result.storm_swords.swords.size());
        result.storm_swords.visible = skill.transients_active
            && result.storm_swords.sword_count != 0U;
        for (std::size_t index = 0U;
             index < result.storm_swords.sword_count; ++index) {
            const bool aerial = index >= kStormGroundSwordCount;
            result.storm_swords.swords[index].angle_radians =
                static_cast<float>(index % kStormGroundSwordCount)
                    * (2.0F * kPi / static_cast<float>(kStormGroundSwordCount))
                    + (aerial ? 0.26F : 0.0F)
                    + static_cast<float>(skill.frame_index) * 0.035F;
            result.storm_swords.swords[index].visible = true;
            result.storm_swords.swords[index].band = aerial
                ? StormSwordBand::aerial : StormSwordBand::ground;
            result.storm_swords.swords[index].highlighted =
                skill.strike_index != 0U
                && ((skill.strike_index - 1U) % kStormGroundSwordCount)
                    == (index % kStormGroundSwordCount);
        }
    }

    std::uint64_t event_age = 0U;
    const bool recent_event = recent_finisher_event(
        snapshot, last_event, event_age);
    const bool snapshot_finisher_window =
        skill.id == skills::ActiveSkillId::storm_swords
        && (skill.phase == combat::ActiveSkillPhase::finisher
            || skill.phase == combat::ActiveSkillPhase::recovery)
        && skill.elapsed_ticks >= kStormFinisherTick
        && skill.elapsed_ticks <= kStormFinisherTick + kFlashTicks;
    const std::uint64_t snapshot_age = snapshot_finisher_window
        ? static_cast<std::uint64_t>(
            skill.elapsed_ticks - kStormFinisherTick)
        : 0U;
    if (snapshot_finisher_window || recent_event) {
        result.storm_swords.finisher_visible = !material_ready;
        const float age = static_cast<float>(snapshot_finisher_window
            ? snapshot_age : event_age);
        result.storm_swords.finisher_opacity = std::clamp(
            1.0F - age / static_cast<float>(kFlashTicks + 1U), 0.0F, 1.0F);
        result.screen_flash_alpha = 0.16F
            * result.storm_swords.finisher_opacity;
    }
    return result;
}

ActiveSkillDrawRuntimeStatus ActiveSkillRenderer::draw_world(
    const ActiveSkillEffectPlan& plan,
    const MaterialPack& material_pack,
    const CombatCameraView& camera,
    float width, float height) const noexcept {
    ActiveSkillDrawRuntimeStatus status{};
    status.mode = plan.mode;
    status.atlas = plan.atlas;
    status.atlas_frame = plan.atlas_frame;
    if (plan.mode == ActiveSkillVisualMode::material) {
        status.material_frame_drawn = draw_material_active_skill(
            plan, material_pack, camera, width, height);
    } else if (plan.mode == ActiveSkillVisualMode::procedural_fallback) {
        if (draw_draw_slash(plan.draw_slash, camera, width, height)) {
            ++status.procedural_main_visual_count;
        }
        if (draw_storm_swords(plan.storm_swords, camera, width, height)) {
            ++status.procedural_main_visual_count;
        }
    }
    if (plan.screen_flash_alpha > 0.0F) {
        DrawRectangle(0, 0, static_cast<int>(width), static_cast<int>(height),
            Fade(Color{220, 244, 255, 255}, plan.screen_flash_alpha));
    }
    return status;
}

ActiveSkillDrawRuntimeStatus ActiveSkillRenderer::draw_world(
    const ActiveSkillEffectPlan& plan,
    const MaterialPack& material_pack,
    float width, float height) const noexcept {
    const CombatCameraView full_room_camera{
        {}, combat::room_bounds::width, combat::room_bounds::depth};
    return draw_world(
        plan, material_pack, full_room_camera, width, height);
}

bool ActiveSkillRenderer::assets_ready() const noexcept {
    return active_skill_assets_ready();
}

ActiveSkillCooldownOverlayPlan make_active_skill_cooldown_overlay(
    Rectangle bounds, float cooldown_ratio, bool empty) noexcept {
    if (empty || cooldown_ratio <= 0.0F || bounds.width <= 0.0F
            || bounds.height <= 0.0F) {
        return {};
    }
    const float ratio = std::clamp(cooldown_ratio, 0.0F, 1.0F);
    const float overlay_height = bounds.height * ratio;
    return {true, {bounds.x, bounds.y + bounds.height - overlay_height,
        bounds.width, overlay_height}};
}

void ActiveSkillRenderer::draw_hud(const ActiveSkillHudModel& model,
    const ActiveSkillHudLayout& layout,
    Font hud_font, bool hud_font_ready,
    const MaterialPack& material_pack) const noexcept {
    const float scale = ui_viewport_scale(GetScreenWidth(), GetScreenHeight());
    for (std::size_t index = 0U; index < model.slots.size(); ++index) {
        const ActiveSkillHudSlot& slot = model.slots[index];
        const Rectangle bounds = layout.slots[index];
        const UiMaterialElement slot_material = slot.cooldown_ratio > 0.0F
            ? UiMaterialElement::hud_skill_cooldown
            : slot.empty ? UiMaterialElement::hud_skill_empty
                         : UiMaterialElement::hud_skill_ready;
        const bool material_drawn = material_pack.draw_to(
            ui_material_sprite(slot_material), bounds);
        if (!material_drawn) {
            DrawRectangleRounded(bounds, 0.12F, 4, Color{12, 20, 31, 238});
            DrawRectangleRoundedLinesEx(bounds, 0.12F, 4, 2.0F,
                slot.empty ? Color{74, 91, 112, 235}
                           : Color{107, 199, 255, 255});
        }
        if (!material_drawn && slot.empty) {
            const Vector2 center{bounds.x + bounds.width * 0.5F,
                bounds.y + bounds.height * 0.5F + 4.0F};
            DrawCircleLines(static_cast<int>(center.x),
                static_cast<int>(center.y), 13.0F,
                Color{84, 103, 128, 220});
            DrawLineEx({center.x - 9.0F, center.y},
                {center.x + 9.0F, center.y}, 1.0F,
                Color{84, 103, 128, 220});
        }
        if (!slot.empty) {
            const float inset = 8.0F * scale;
            const Rectangle icon_bounds{bounds.x + inset, bounds.y + inset,
                bounds.width - inset * 2.0F, bounds.height - inset * 2.0F};
            static_cast<void>(material_pack.draw_to(slot.icon, icon_bounds));
        }

        const ActiveSkillCooldownOverlayPlan cooldown_overlay =
            make_active_skill_cooldown_overlay(
                bounds, slot.cooldown_ratio, slot.empty);
        if (cooldown_overlay.visible) {
            DrawRectangleRec(cooldown_overlay.bounds, Color{3, 7, 14, 190});
            DrawLineEx({cooldown_overlay.bounds.x,
                    cooldown_overlay.bounds.y},
                {cooldown_overlay.bounds.x + cooldown_overlay.bounds.width,
                    cooldown_overlay.bounds.y},
                1.5F, Color{112, 211, 255, 210});
        }
        if (!hud_font_ready) continue;
        char key[2]{static_cast<char>('0' + slot.key_number), '\0'};
        draw_skill_text(hud_font, key, bounds.x + 5.0F * scale,
            bounds.y + 3.0F * scale, 16.0F * scale,
            ui_text_contrast_style().primary);
        if (!slot.empty) {
            const float name_size =
                ui_typography().kHudSkillNameFontSize * scale;
            const Rectangle name_container{bounds.x + 3.0F * scale,
                bounds.y + bounds.height - 24.0F * scale,
                bounds.width - 6.0F * scale, 23.0F * scale};
            char fitted_name[48]{};
            fit_skill_label(hud_font, slot.name.data(), fitted_name,
                sizeof(fitted_name),
                name_container.width - 6.0F * scale, name_size);
            const Vector2 name_position{name_container.x + 3.0F * scale,
                name_container.y + 2.0F * scale};
            record_ui_text_bounds(UiTextAuditPage::hud,
                UiTextAuditRole::hud_skill_name, hud_font, fitted_name,
                name_position, name_size, 0.5F, name_container, name_size);
            draw_skill_text(hud_font, fitted_name, name_position.x,
                name_position.y, name_size,
                ui_text_contrast_style().primary);
        }
        if (slot.cooldown_ratio <= 0.0F || slot.empty) continue;
        const skills::ActiveSkillDefinition* const definition =
            skills::active_skill_definition(slot.id);
        if (definition == nullptr) continue;
        const float seconds = std::ceil(slot.cooldown_ratio
            * static_cast<float>(definition->cooldown_ticks) / 60.0F);
        char remaining[12]{};
        static_cast<void>(std::snprintf(remaining, sizeof(remaining),
            "%.0fs", seconds));
        draw_skill_text(hud_font, remaining, bounds.x + 18.0F * scale,
            bounds.y + 22.0F * scale, 16.0F * scale,
            ui_text_contrast_style().primary);
    }
}

}  // namespace arpg::platform
