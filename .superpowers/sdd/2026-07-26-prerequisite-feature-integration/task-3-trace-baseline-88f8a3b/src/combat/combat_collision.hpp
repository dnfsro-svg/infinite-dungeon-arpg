#pragma once

#include "combat/combat_types.hpp"

namespace arpg::combat {

inline constexpr float kAttackAssistMaxGap = 0.35F;
inline constexpr float kAttackAssistMaxDepth = 0.45F;
inline constexpr float kAttackAssistMaxCorrection = 0.18F;

[[nodiscard]] Aabb make_world_aabb(
    const Aabb& local_box,
    Vec3 origin,
    Facing facing) noexcept;

[[nodiscard]] Aabb make_dummy_hurtbox(
    DummyKind kind,
    Vec3 position) noexcept;

[[nodiscard]] Aabb make_attack_assist_volume(
    const AttackDefinition& definition,
    Vec3 player_position,
    Facing facing) noexcept;

[[nodiscard]] bool overlaps_inclusive(
    const Aabb& lhs,
    const Aabb& rhs) noexcept;

}  // namespace arpg::combat
