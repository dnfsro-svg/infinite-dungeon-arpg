#pragma once

#include "material_asset_types.hpp"

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_types.hpp"

#include <optional>

namespace arpg::platform {

enum class PlayerAnimationClipId : std::uint8_t {
    idle,
    move,
    jump,
    j1,
    j2,
    j3,
    launcher,
    air_j,
    landing,
    hurt,
    down,
    get_up,
    death,
    count,
};

struct PlayerAnimationClipDefinition final {
    PlayerAnimationClipId id{PlayerAnimationClipId::idle};
    MaterialAtlasId atlas{MaterialAtlasId::player_locomotion};
    std::uint8_t first_cell{};
    std::uint8_t frame_count{};
    std::uint8_t frames_per_second{24U};
};

struct PlayerAnimationFrame final {
    MaterialAtlasId atlas{MaterialAtlasId::player_locomotion};
    Rectangle source{};
    Vector2 foot_anchor{};
    Vector2 weapon_anchor{};
};

[[nodiscard]] MaterialSpriteId select_player_sprite(
    combat::PlayerState state,
    combat::AttackId attack) noexcept;

[[nodiscard]] PlayerAnimationClipId select_player_animation_clip(
    combat::PlayerState state, combat::AttackId attack) noexcept;
[[nodiscard]] const PlayerAnimationClipDefinition* player_animation_clip(
    PlayerAnimationClipId id) noexcept;
[[nodiscard]] std::optional<PlayerAnimationFrame> player_animation_frame(
    const PlayerAnimationClipDefinition& clip, std::uint16_t frame) noexcept;

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
