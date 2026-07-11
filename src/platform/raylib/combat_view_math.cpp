#include "combat_view_math.hpp"

#include <algorithm>
#include <cstddef>

namespace arpg::platform {
namespace {

bool actor_precedes(
    const ActorDrawItem& lhs,
    const ActorDrawItem& rhs) noexcept {
    if (lhs.position.y != rhs.position.y) {
        return lhs.position.y < rhs.position.y;
    }
    if (lhs.position.z != rhs.position.z) {
        return lhs.position.z < rhs.position.z;
    }
    if (lhs.position.x != rhs.position.x) {
        return lhs.position.x < rhs.position.x;
    }
    return lhs.index < rhs.index;
}

}  // namespace

ScreenProjection project_combat_position(
    combat::Vec3 position,
    float width,
    float height) noexcept {
    const float depth = std::clamp(
        (position.y + 3.5F) / 7.0F, 0.0F, 1.0F);
    const float scale = 0.70F + 0.30F * depth;
    const float ground_y = height * (0.38F + 0.50F * depth);
    return {
        width * 0.50F + position.x * (width / 18.0F) * scale,
        ground_y - position.z * 70.0F * scale,
        ground_y,
        scale,
    };
}

void sort_actor_draw_items(
    std::array<ActorDrawItem, 4>& items) noexcept {
    for (std::size_t index = 1; index < items.size(); ++index) {
        const ActorDrawItem value = items[index];
        std::size_t insertion = index;
        while (insertion > 0
               && actor_precedes(value, items[insertion - 1])) {
            items[insertion] = items[insertion - 1];
            --insertion;
        }
        items[insertion] = value;
    }
}

}  // namespace arpg::platform
