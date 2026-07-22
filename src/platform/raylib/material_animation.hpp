#pragma once

#include "material_asset_types.hpp"

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_types.hpp"

namespace arpg::platform {

[[nodiscard]] MaterialSpriteId select_player_sprite(
    combat::PlayerState state,
    combat::AttackId attack) noexcept;

[[nodiscard]] MaterialSpriteId select_monster_sprite(
    combat::MonsterId monster,
    combat::MonsterAiPhase phase) noexcept;

[[nodiscard]] MaterialSpriteId select_floor_sprite(
    dungeon::DungeonElement element) noexcept;

[[nodiscard]] MaterialSpriteId select_door_sprite(
    dungeon::DungeonElement element) noexcept;
[[nodiscard]] MaterialSpriteId select_element_effect(
    dungeon::DungeonElement element) noexcept;

[[nodiscard]] float material_actor_draw_scale(
    bool player, float projection_scale) noexcept;

[[nodiscard]] const AnimationClipDefinition* material_animation_clip(
    AnimationClipId id) noexcept;

[[nodiscard]] MaterialSpriteId material_animation_frame_sprite(
    const AnimationClipDefinition& clip, std::uint16_t frame) noexcept;

}  // namespace arpg::platform
