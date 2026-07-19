#pragma once

#include "material_asset_types.hpp"

#include "combat/combat_types.hpp"

namespace arpg::platform {

[[nodiscard]] MaterialSpriteId select_player_sprite(
    combat::PlayerState state,
    combat::AttackId attack) noexcept;

[[nodiscard]] MaterialSpriteId select_monster_sprite(
    combat::MonsterId monster,
    combat::MonsterAiPhase phase) noexcept;

}  // namespace arpg::platform
