#pragma once

#include "combat/combat_types.hpp"

namespace arpg::combat {

[[nodiscard]] Aabb make_world_aabb(
    const Aabb& local_box,
    Vec3 origin,
    Facing facing) noexcept;

[[nodiscard]] Aabb make_dummy_hurtbox(
    DummyKind kind,
    Vec3 position) noexcept;

[[nodiscard]] bool overlaps_inclusive(
    const Aabb& lhs,
    const Aabb& rhs) noexcept;

}  // namespace arpg::combat
