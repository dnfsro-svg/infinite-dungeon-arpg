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

enum class MonsterAnimationState : std::uint8_t {
    idle,
    move,
    telegraph,
    active,
    recovery,
    cooldown,
    hurt,
    death,
    count,
};

struct ExplicitMonsterAnimationFrame final {
    Rectangle source{};
    Vector2 foot_anchor{};
    std::uint8_t duration_ticks{1U};
    std::uint8_t key_pose_index{};
};

struct MonsterAnimationClipDefinition final {
    combat::MonsterId monster{combat::MonsterId::water_bulwark};
    MonsterAnimationState state{MonsterAnimationState::idle};
    MaterialAtlasId atlas{MaterialAtlasId::water_bulwark};
    std::uint16_t first_cell{};
    std::uint16_t frame_count{};
    std::uint8_t frames_per_second{18U};
    std::uint8_t key_pose_count{4U};
    const ExplicitMonsterAnimationFrame* explicit_frames{};
};

struct MonsterAnimationFrame final {
    MaterialAtlasId atlas{MaterialAtlasId::water_bulwark};
    Rectangle source{};
    Vector2 foot_anchor{};
    std::uint8_t key_pose_index{};
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
[[nodiscard]] std::uint16_t player_animation_frame_index(
    const PlayerAnimationClipDefinition& clip, std::uint64_t elapsed_ticks,
    std::uint16_t duration_ticks, bool loop) noexcept;

[[nodiscard]] MaterialSpriteId select_monster_sprite(
    combat::MonsterId monster,
    combat::MonsterAiPhase phase) noexcept;
[[nodiscard]] MonsterAnimationState select_monster_animation_state(
    combat::MonsterAiPhase phase, bool hurt) noexcept;
[[nodiscard]] const MonsterAnimationClipDefinition* monster_animation_clip(
    combat::MonsterId monster, MonsterAnimationState state) noexcept;
[[nodiscard]] std::optional<MonsterAnimationFrame> monster_animation_frame(
    const MonsterAnimationClipDefinition& clip, std::uint16_t frame) noexcept;
[[nodiscard]] std::uint16_t monster_animation_frame_index(
    const MonsterAnimationClipDefinition& clip, std::uint64_t elapsed_ticks,
    bool loop = true) noexcept;

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
