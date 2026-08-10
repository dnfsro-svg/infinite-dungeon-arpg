#include "combat_renderer.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/combat_collision.hpp"
#include "combat_view_math.hpp"
#include "ui_text_renderer.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace arpg::platform {
namespace {

void draw_projected_aabb(const combat::Aabb& box,
    const CombatCameraView& camera, float width, float height,
    Color color, Font shared_ui_font, bool shared_ui_font_ready,
    const char* label) noexcept {
    std::array<ScreenProjection, 8> corners{};
    for (std::size_t index = 0; index < corners.size(); ++index) {
        const combat::Vec3 corner{(index & 1U) != 0 ? box.maximum.x : box.minimum.x,
            (index & 2U) != 0 ? box.maximum.y : box.minimum.y,
            (index & 4U) != 0 ? box.maximum.z : box.minimum.z};
        corners[index] = project_combat_position(
            corner, camera, width, height);
    }
    for (std::size_t index = 0; index < corners.size(); ++index) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const std::size_t bit = std::size_t{1} << axis;
            if ((index & bit) == 0) {
                const ScreenProjection& from = corners[index];
                const ScreenProjection& to = corners[index | bit];
                DrawLineEx({from.x, from.y}, {to.x, to.y}, 1.5F, color);
            }
        }
    }
    if (shared_ui_font_ready && IsFontValid(shared_ui_font)) {
        draw_crisp_ui_text(shared_ui_font, label,
            {corners[4].x + 4.0F, corners[4].y - 14.0F},
            12.0F, 0.5F, color, 1);
    }
}

}  // namespace

void CombatRenderer::draw_debug_world_volumes(
    const combat::CombatSnapshot& snapshot,
    const CombatCameraView& camera,
    float width, float height) const noexcept {
    if (const combat::AttackDefinition* definition =
            combat::find_attack_definition(snapshot.player.active_attack)) {
        const combat::Aabb attack_box = combat::make_world_aabb(
            definition->local_hitbox, snapshot.player.position, snapshot.player.facing);
        const combat::Aabb assist_box = combat::make_attack_assist_volume(
            *definition, snapshot.player.position, snapshot.player.facing);
        draw_projected_aabb(assist_box, camera, width, height,
            Color{90, 190, 255, 190}, world_ui_font_, world_ui_font_ready_,
            debug_world_volume_label(DebugWorldVolumeKind::assist));
        draw_projected_aabb(attack_box, camera, width, height,
            Color{255, 88, 184, 230}, world_ui_font_, world_ui_font_ready_,
            debug_world_volume_label(DebugWorldVolumeKind::attack));
    }
    for (const combat::MonsterSnapshot& monster : snapshot.monsters) {
        if (monster_visible(monster)) {
            draw_projected_aabb(combat::make_dummy_hurtbox(monster.kind, monster.position),
                camera, width, height,
                Color{94, 255, 173, 210}, world_ui_font_, world_ui_font_ready_,
                debug_world_volume_label(DebugWorldVolumeKind::hurt));
        }
    }
}

}  // namespace arpg::platform
