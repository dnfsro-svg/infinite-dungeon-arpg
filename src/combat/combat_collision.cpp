#include "combat/combat_collision.hpp"

namespace arpg::combat {

Aabb make_world_aabb(
    const Aabb& local_box,
    Vec3 origin,
    Facing facing) noexcept {
    Aabb result{};
    if (facing == Facing::right) {
        result.minimum.x = origin.x + local_box.minimum.x;
        result.maximum.x = origin.x + local_box.maximum.x;
    } else {
        result.minimum.x = origin.x - local_box.maximum.x;
        result.maximum.x = origin.x - local_box.minimum.x;
    }
    result.minimum.y = origin.y + local_box.minimum.y;
    result.maximum.y = origin.y + local_box.maximum.y;
    result.minimum.z = origin.z + local_box.minimum.z;
    result.maximum.z = origin.z + local_box.maximum.z;
    return result;
}

Aabb make_dummy_hurtbox(DummyKind kind, Vec3 position) noexcept {
    Vec3 half{};
    switch (kind) {
    case DummyKind::light:
        half = Vec3{0.45F, 0.35F, 1.40F};
        break;
    case DummyKind::normal:
        half = Vec3{0.55F, 0.40F, 1.60F};
        break;
    case DummyKind::heavy:
        half = Vec3{0.70F, 0.50F, 1.90F};
        break;
    default:
        return {position, position};
    }
    return {
        {position.x - half.x, position.y - half.y, position.z - half.z},
        {position.x + half.x, position.y + half.y, position.z + half.z},
    };
}

bool overlaps_inclusive(const Aabb& lhs, const Aabb& rhs) noexcept {
    return lhs.minimum.x <= rhs.maximum.x
        && lhs.maximum.x >= rhs.minimum.x
        && lhs.minimum.y <= rhs.maximum.y
        && lhs.maximum.y >= rhs.minimum.y
        && lhs.minimum.z <= rhs.maximum.z
        && lhs.maximum.z >= rhs.minimum.z;
}

}  // namespace arpg::combat
