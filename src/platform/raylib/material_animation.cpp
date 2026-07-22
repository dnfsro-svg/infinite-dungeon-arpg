#include "material_animation.hpp"

#include "material_manifest.hpp"

#include <array>
#include <cstddef>

namespace arpg::platform {
namespace {

constexpr float kPlayerTrimmedHeight = 172.0F;
constexpr float kPlayerTargetHeight = 82.0F;
constexpr float kMonsterTrimmedHeight = 176.0F;
constexpr float kMonsterTargetHeight = 78.0F;
constexpr float kPlayerAnimationCell = 128.0F;
constexpr std::uint8_t kPlayerAnimationColumns = 8U;

constexpr std::array<PlayerAnimationClipDefinition,
    static_cast<std::size_t>(PlayerAnimationClipId::count)> kPlayerClips{{
    {PlayerAnimationClipId::idle, MaterialAtlasId::player_locomotion, 0U, 16U},
    {PlayerAnimationClipId::move, MaterialAtlasId::player_locomotion, 16U, 20U},
    {PlayerAnimationClipId::jump, MaterialAtlasId::player_locomotion, 36U, 24U},
    {PlayerAnimationClipId::j1, MaterialAtlasId::player_combo_a, 0U, 18U},
    {PlayerAnimationClipId::j2, MaterialAtlasId::player_combo_a, 18U, 22U},
    {PlayerAnimationClipId::j3, MaterialAtlasId::player_combo_b, 0U, 26U},
    {PlayerAnimationClipId::launcher, MaterialAtlasId::player_combo_b, 26U, 24U},
    {PlayerAnimationClipId::air_j, MaterialAtlasId::player_air, 0U, 18U},
    {PlayerAnimationClipId::landing, MaterialAtlasId::player_air, 18U, 14U},
    {PlayerAnimationClipId::hurt, MaterialAtlasId::player_reaction, 0U, 10U},
    {PlayerAnimationClipId::down, MaterialAtlasId::player_reaction, 10U, 16U},
    {PlayerAnimationClipId::get_up, MaterialAtlasId::player_reaction, 26U, 14U},
    {PlayerAnimationClipId::death, MaterialAtlasId::player_reaction, 40U, 24U},
}};

[[nodiscard]] MaterialSpriteId select_attack_sprite(
    combat::AttackId attack) noexcept {
    switch (attack) {
    case combat::AttackId::j1:
        return MaterialSpriteId::player_j1;
    case combat::AttackId::j2:
        return MaterialSpriteId::player_j2;
    case combat::AttackId::j3:
        return MaterialSpriteId::player_j3;
    case combat::AttackId::launcher:
        return MaterialSpriteId::player_launcher;
    case combat::AttackId::air_j:
        return MaterialSpriteId::player_air_j;
    case combat::AttackId::none:
        return MaterialSpriteId::player_idle;
    }
    return MaterialSpriteId::missing;
}

[[nodiscard]] PlayerAnimationClipId select_attack_clip(
    combat::AttackId attack) noexcept {
    switch (attack) {
    case combat::AttackId::j1: return PlayerAnimationClipId::j1;
    case combat::AttackId::j2: return PlayerAnimationClipId::j2;
    case combat::AttackId::j3: return PlayerAnimationClipId::j3;
    case combat::AttackId::launcher: return PlayerAnimationClipId::launcher;
    case combat::AttackId::air_j: return PlayerAnimationClipId::air_j;
    case combat::AttackId::none: return PlayerAnimationClipId::idle;
    }
    return PlayerAnimationClipId::idle;
}

}  // namespace

MaterialSpriteId select_player_sprite(
    combat::PlayerState state,
    combat::AttackId attack) noexcept {
    switch (state) {
    case combat::PlayerState::idle:
        return MaterialSpriteId::player_idle;
    case combat::PlayerState::move:
        return MaterialSpriteId::player_move;
    case combat::PlayerState::attack_startup:
    case combat::PlayerState::attack_active:
    case combat::PlayerState::attack_recovery:
        return select_attack_sprite(attack);
    case combat::PlayerState::jump_rise:
        return MaterialSpriteId::player_jump_rise;
    case combat::PlayerState::jump_fall:
        return MaterialSpriteId::player_jump_fall;
    case combat::PlayerState::landing:
        return MaterialSpriteId::player_landing;
    }
    return MaterialSpriteId::missing;
}

PlayerAnimationClipId select_player_animation_clip(
    combat::PlayerState state, combat::AttackId attack) noexcept {
    switch (state) {
    case combat::PlayerState::idle: return PlayerAnimationClipId::idle;
    case combat::PlayerState::move: return PlayerAnimationClipId::move;
    case combat::PlayerState::attack_startup:
    case combat::PlayerState::attack_active:
    case combat::PlayerState::attack_recovery: return select_attack_clip(attack);
    case combat::PlayerState::jump_rise:
    case combat::PlayerState::jump_fall: return PlayerAnimationClipId::jump;
    case combat::PlayerState::landing: return PlayerAnimationClipId::landing;
    }
    return PlayerAnimationClipId::idle;
}

const PlayerAnimationClipDefinition* player_animation_clip(
    PlayerAnimationClipId id) noexcept {
    const std::size_t index = static_cast<std::size_t>(id);
    return index < kPlayerClips.size() ? &kPlayerClips[index] : nullptr;
}

std::optional<PlayerAnimationFrame> player_animation_frame(
    const PlayerAnimationClipDefinition& clip, std::uint16_t frame) noexcept {
    if (frame >= clip.frame_count) return std::nullopt;
    const std::uint16_t cell = static_cast<std::uint16_t>(clip.first_cell) + frame;
    const std::uint16_t column = cell % kPlayerAnimationColumns;
    const std::uint16_t row = cell / kPlayerAnimationColumns;
    return PlayerAnimationFrame{clip.atlas,
        {static_cast<float>(column) * kPlayerAnimationCell,
         static_cast<float>(row) * kPlayerAnimationCell,
         kPlayerAnimationCell, kPlayerAnimationCell},
        {64.0F, 124.0F}, {88.0F, 64.0F}};
}

MaterialSpriteId select_monster_sprite(
    combat::MonsterId monster,
    combat::MonsterAiPhase phase) noexcept {
    switch (monster) {
    case combat::MonsterId::fire_bomber:
        switch (phase) {
        case combat::MonsterAiPhase::idle: return MaterialSpriteId::fire_bomber_idle;
        case combat::MonsterAiPhase::move: return MaterialSpriteId::fire_bomber_move;
        case combat::MonsterAiPhase::telegraph: return MaterialSpriteId::fire_bomber_telegraph;
        case combat::MonsterAiPhase::active: return MaterialSpriteId::fire_bomber_active;
        case combat::MonsterAiPhase::recovery: return MaterialSpriteId::fire_bomber_recovery;
        case combat::MonsterAiPhase::cooldown: return MaterialSpriteId::fire_bomber_cooldown;
        case combat::MonsterAiPhase::defeated: return MaterialSpriteId::fire_bomber_defeated;
        }
        break;
    case combat::MonsterId::fire_charger:
        switch (phase) {
        case combat::MonsterAiPhase::idle: return MaterialSpriteId::fire_charger_idle;
        case combat::MonsterAiPhase::move: return MaterialSpriteId::fire_charger_move;
        case combat::MonsterAiPhase::telegraph: return MaterialSpriteId::fire_charger_telegraph;
        case combat::MonsterAiPhase::active: return MaterialSpriteId::fire_charger_active;
        case combat::MonsterAiPhase::recovery: return MaterialSpriteId::fire_charger_recovery;
        case combat::MonsterAiPhase::cooldown: return MaterialSpriteId::fire_charger_cooldown;
        case combat::MonsterAiPhase::defeated: return MaterialSpriteId::fire_charger_defeated;
        }
        break;
    case combat::MonsterId::water_bulwark:
        switch (phase) {
        case combat::MonsterAiPhase::idle: return MaterialSpriteId::water_bulwark_idle;
        case combat::MonsterAiPhase::move: return MaterialSpriteId::water_bulwark_move;
        case combat::MonsterAiPhase::telegraph: return MaterialSpriteId::water_bulwark_telegraph;
        case combat::MonsterAiPhase::active: return MaterialSpriteId::water_bulwark_active;
        case combat::MonsterAiPhase::recovery: return MaterialSpriteId::water_bulwark_recovery;
        case combat::MonsterAiPhase::cooldown: return MaterialSpriteId::water_bulwark_cooldown;
        case combat::MonsterAiPhase::defeated: return MaterialSpriteId::water_bulwark_defeated;
        }
        break;
    case combat::MonsterId::water_support:
        switch (phase) {
        case combat::MonsterAiPhase::idle: return MaterialSpriteId::water_support_idle;
        case combat::MonsterAiPhase::move: return MaterialSpriteId::water_support_move;
        case combat::MonsterAiPhase::telegraph: return MaterialSpriteId::water_support_telegraph;
        case combat::MonsterAiPhase::active: return MaterialSpriteId::water_support_active;
        case combat::MonsterAiPhase::recovery: return MaterialSpriteId::water_support_recovery;
        case combat::MonsterAiPhase::cooldown: return MaterialSpriteId::water_support_cooldown;
        case combat::MonsterAiPhase::defeated: return MaterialSpriteId::water_support_defeated;
        }
        break;
    case combat::MonsterId::lightning_shooter:
        switch (phase) {
        case combat::MonsterAiPhase::idle: return MaterialSpriteId::lightning_shooter_idle;
        case combat::MonsterAiPhase::move: return MaterialSpriteId::lightning_shooter_move;
        case combat::MonsterAiPhase::telegraph: return MaterialSpriteId::lightning_shooter_telegraph;
        case combat::MonsterAiPhase::active: return MaterialSpriteId::lightning_shooter_active;
        case combat::MonsterAiPhase::recovery: return MaterialSpriteId::lightning_shooter_recovery;
        case combat::MonsterAiPhase::cooldown: return MaterialSpriteId::lightning_shooter_cooldown;
        case combat::MonsterAiPhase::defeated: return MaterialSpriteId::lightning_shooter_defeated;
        }
        break;
    case combat::MonsterId::lightning_dasher:
        switch (phase) {
        case combat::MonsterAiPhase::idle: return MaterialSpriteId::lightning_dasher_idle;
        case combat::MonsterAiPhase::move: return MaterialSpriteId::lightning_dasher_move;
        case combat::MonsterAiPhase::telegraph: return MaterialSpriteId::lightning_dasher_telegraph;
        case combat::MonsterAiPhase::active: return MaterialSpriteId::lightning_dasher_active;
        case combat::MonsterAiPhase::recovery: return MaterialSpriteId::lightning_dasher_recovery;
        case combat::MonsterAiPhase::cooldown: return MaterialSpriteId::lightning_dasher_cooldown;
        case combat::MonsterAiPhase::defeated: return MaterialSpriteId::lightning_dasher_defeated;
        }
        break;
    case combat::MonsterId::chaos_chaser:
        switch (phase) {
        case combat::MonsterAiPhase::idle: return MaterialSpriteId::chaos_chaser_idle;
        case combat::MonsterAiPhase::move: return MaterialSpriteId::chaos_chaser_move;
        case combat::MonsterAiPhase::telegraph: return MaterialSpriteId::chaos_chaser_telegraph;
        case combat::MonsterAiPhase::active: return MaterialSpriteId::chaos_chaser_active;
        case combat::MonsterAiPhase::recovery: return MaterialSpriteId::chaos_chaser_recovery;
        case combat::MonsterAiPhase::cooldown: return MaterialSpriteId::chaos_chaser_cooldown;
        case combat::MonsterAiPhase::defeated: return MaterialSpriteId::chaos_chaser_defeated;
        }
        break;
    case combat::MonsterId::chaos_hazard:
        switch (phase) {
        case combat::MonsterAiPhase::idle: return MaterialSpriteId::chaos_hazard_idle;
        case combat::MonsterAiPhase::move: return MaterialSpriteId::chaos_hazard_move;
        case combat::MonsterAiPhase::telegraph: return MaterialSpriteId::chaos_hazard_telegraph;
        case combat::MonsterAiPhase::active: return MaterialSpriteId::chaos_hazard_active;
        case combat::MonsterAiPhase::recovery: return MaterialSpriteId::chaos_hazard_recovery;
        case combat::MonsterAiPhase::cooldown: return MaterialSpriteId::chaos_hazard_cooldown;
        case combat::MonsterAiPhase::defeated: return MaterialSpriteId::chaos_hazard_defeated;
        }
        break;
    case combat::MonsterId::count:
        return MaterialSpriteId::missing;
    }
    return MaterialSpriteId::missing;
}

MaterialSpriteId select_floor_sprite(dungeon::DungeonElement element) noexcept {
    switch (element) {
    case dungeon::DungeonElement::fire:
        return MaterialSpriteId::environment_floor_fire;
    case dungeon::DungeonElement::water:
        return MaterialSpriteId::environment_floor_water;
    case dungeon::DungeonElement::lightning:
        return MaterialSpriteId::environment_floor_lightning;
    case dungeon::DungeonElement::chaos:
        return MaterialSpriteId::environment_floor_chaos;
    }
    return MaterialSpriteId::missing;
}

MaterialSpriteId select_door_sprite(dungeon::DungeonElement element) noexcept {
    switch (element) {
    case dungeon::DungeonElement::fire:
        return MaterialSpriteId::environment_door_fire;
    case dungeon::DungeonElement::water:
        return MaterialSpriteId::environment_door_water;
    case dungeon::DungeonElement::lightning:
        return MaterialSpriteId::environment_door_lightning;
    case dungeon::DungeonElement::chaos:
        return MaterialSpriteId::environment_door_chaos;
    }
    return MaterialSpriteId::missing;
}

MaterialSpriteId select_element_effect(dungeon::DungeonElement element) noexcept {
    switch (element) {
    case dungeon::DungeonElement::fire: return MaterialSpriteId::effect_fire;
    case dungeon::DungeonElement::water: return MaterialSpriteId::effect_water;
    case dungeon::DungeonElement::lightning: return MaterialSpriteId::effect_lightning;
    case dungeon::DungeonElement::chaos: return MaterialSpriteId::effect_chaos;
    }
    return MaterialSpriteId::missing;
}

float material_actor_draw_scale(bool player, float projection_scale) noexcept {
    if (projection_scale <= 0.0F) return 0.0F;
    const float trimmed_height = player ? kPlayerTrimmedHeight
                                        : kMonsterTrimmedHeight;
    const float target_height = player ? kPlayerTargetHeight
                                       : kMonsterTargetHeight;
    return projection_scale * target_height / trimmed_height;
}

const AnimationClipDefinition* material_animation_clip(AnimationClipId id) noexcept {
    const MaterialManifestDefinition manifest = default_material_manifest();
    for (std::size_t index = 0U; index < manifest.clip_count; ++index) {
        if (manifest.clips[index].id == id) return &manifest.clips[index];
    }
    return nullptr;
}

MaterialSpriteId material_animation_frame_sprite(
    const AnimationClipDefinition& clip, std::uint16_t frame) noexcept {
    const MaterialManifestDefinition manifest = default_material_manifest();
    if (clip.frame_count == 0U || frame >= clip.frame_count
        || static_cast<std::size_t>(clip.first_frame) + frame >= manifest.frame_count) {
        return MaterialSpriteId::missing;
    }
    return manifest.frames[clip.first_frame + frame].id;
}

}  // namespace arpg::platform
