#include "combat_renderer.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/combat_collision.hpp"
#include "combat_view_math.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace arpg::platform {
namespace {

void draw_projected_aabb(const combat::Aabb& box, CombatCameraView view,
    float width, float height,
    Color color, const char* label) noexcept {
    std::array<ScreenProjection, 8> corners{};
    for (std::size_t index = 0; index < corners.size(); ++index) {
        const combat::Vec3 corner{(index & 1U) != 0 ? box.maximum.x : box.minimum.x,
            (index & 2U) != 0 ? box.maximum.y : box.minimum.y,
            (index & 4U) != 0 ? box.maximum.z : box.minimum.z};
        corners[index] = project_combat_position(
            corner, view, width, height);
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
    DrawText(label, static_cast<int>(corners[4].x + 4.0F),
        static_cast<int>(corners[4].y - 14.0F), 12, color);
}

}  // namespace

void CombatRenderer::draw_debug_world_volumes(
    const combat::CombatSnapshot& snapshot, CombatCameraView view,
    float width, float height) const noexcept {
    if (const combat::AttackDefinition* definition =
            combat::find_attack_definition(snapshot.player.active_attack)) {
        const combat::Aabb attack_box = combat::make_world_aabb(
            definition->local_hitbox, snapshot.player.position, snapshot.player.facing);
        const combat::Aabb assist_box = combat::make_attack_assist_volume(
            *definition, snapshot.player.position, snapshot.player.facing);
        draw_projected_aabb(assist_box, view, width, height,
            Color{90, 190, 255, 190}, "Assist");
        draw_projected_aabb(attack_box, view, width, height,
            Color{255, 88, 184, 230}, "Attack");
    }
    for (const combat::MonsterSnapshot& monster : snapshot.monsters) {
        if (monster_visible(monster)) {
            draw_projected_aabb(combat::make_dummy_hurtbox(monster.kind, monster.position),
                view, width, height, Color{94, 255, 173, 210}, "Hurt");
        }
    }
}

}  // namespace arpg::platform
