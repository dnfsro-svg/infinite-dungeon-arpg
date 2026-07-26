#include "material_animation.hpp"

#include "material_manifest.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>

namespace arpg::platform {
namespace {

constexpr float kPlayerTrimmedHeight = 172.0F;
constexpr float kPlayerTargetHeight = 82.0F;
constexpr float kMonsterTrimmedHeight = 176.0F;
constexpr float kMonsterTargetHeight = 78.0F;
constexpr float kPlayerAnimationCell = 128.0F;
constexpr std::uint8_t kPlayerAnimationColumns = 8U;
constexpr float kWaterMonsterAnimationCell = 96.0F;
constexpr std::uint16_t kWaterMonsterAnimationColumns = 9U;

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

constexpr ExplicitMonsterAnimationFrame fire_frame(std::uint16_t cell,
    std::uint8_t duration_ticks, float anchor_x, float anchor_y,
    std::uint8_t key_pose_index = 0U) noexcept {
    return {{static_cast<float>(cell % 4U) * 256.0F,
             static_cast<float>(cell / 4U) * 256.0F, 256.0F, 256.0F},
        {anchor_x, anchor_y}, duration_ticks, key_pose_index};
}

constexpr ExplicitMonsterAnimationFrame kBomberIdle[]{fire_frame(0U, 6U, 125.0F, 234.0F)};
constexpr ExplicitMonsterAnimationFrame kBomberMove[]{fire_frame(1U, 4U, 111.0F, 235.0F)};
constexpr ExplicitMonsterAnimationFrame kBomberTelegraph[]{fire_frame(2U, 6U, 141.0F, 235.0F), fire_frame(7U, 6U, 113.0F, 229.0F, 1U)};
constexpr ExplicitMonsterAnimationFrame kBomberActive[]{fire_frame(8U, 3U, 121.0F, 222.0F), fire_frame(3U, 3U, 112.0F, 228.0F, 1U), fire_frame(9U, 4U, 109.0F, 224.0F, 2U)};
constexpr ExplicitMonsterAnimationFrame kBomberRecovery[]{fire_frame(4U, 4U, 121.0F, 221.0F), fire_frame(10U, 6U, 128.0F, 220.0F, 1U)};
constexpr ExplicitMonsterAnimationFrame kBomberCooldown[]{fire_frame(5U, 6U, 110.0F, 222.0F)};
constexpr ExplicitMonsterAnimationFrame kBomberHurt[]{fire_frame(4U, 5U, 121.0F, 221.0F)};
constexpr ExplicitMonsterAnimationFrame kBomberDeath[]{fire_frame(6U, 6U, 130.0F, 225.0F), fire_frame(11U, 12U, 116.0F, 220.0F, 1U)};

constexpr ExplicitMonsterAnimationFrame kChargerIdle[]{fire_frame(0U, 6U, 125.0F, 250.0F)};
constexpr ExplicitMonsterAnimationFrame kChargerMove[]{fire_frame(1U, 4U, 119.0F, 249.0F)};
constexpr ExplicitMonsterAnimationFrame kChargerTelegraph[]{fire_frame(2U, 5U, 132.0F, 253.0F), fire_frame(8U, 5U, 144.0F, 227.0F, 1U)};
constexpr ExplicitMonsterAnimationFrame kChargerActive[]{fire_frame(9U, 3U, 128.0F, 231.0F), fire_frame(3U, 3U, 123.0F, 250.0F, 1U), fire_frame(10U, 4U, 128.0F, 229.0F, 2U)};
constexpr ExplicitMonsterAnimationFrame kChargerRecovery[]{fire_frame(4U, 6U, 132.0F, 249.0F)};
constexpr ExplicitMonsterAnimationFrame kChargerCooldown[]{fire_frame(5U, 6U, 155.0F, 252.0F)};
constexpr ExplicitMonsterAnimationFrame kChargerHurt[]{fire_frame(4U, 5U, 132.0F, 249.0F)};
constexpr ExplicitMonsterAnimationFrame kChargerDeath[]{fire_frame(6U, 6U, 114.0F, 249.0F), fire_frame(11U, 12U, 109.0F, 229.0F, 1U)};

#define ARPG_ECOLOGY_CLIPS(monster, atlas) \
    {combat::MonsterId::monster, MonsterAnimationState::idle, MaterialAtlasId::atlas, 0U, 12U, 12U}, \
    {combat::MonsterId::monster, MonsterAnimationState::move, MaterialAtlasId::atlas, 12U, 16U, 18U}, \
    {combat::MonsterId::monster, MonsterAnimationState::telegraph, MaterialAtlasId::atlas, 28U, 20U, 20U}, \
    {combat::MonsterId::monster, MonsterAnimationState::active, MaterialAtlasId::atlas, 28U, 20U, 20U}, \
    {combat::MonsterId::monster, MonsterAnimationState::recovery, MaterialAtlasId::atlas, 28U, 20U, 20U}, \
    {combat::MonsterId::monster, MonsterAnimationState::cooldown, MaterialAtlasId::atlas, 28U, 20U, 20U}, \
    {combat::MonsterId::monster, MonsterAnimationState::hurt, MaterialAtlasId::atlas, 48U, 8U, 20U}, \
    {combat::MonsterId::monster, MonsterAnimationState::death, MaterialAtlasId::atlas, 56U, 16U, 16U}
#define ARPG_FIRE_CLIP(monster, state, frames) \
    {combat::MonsterId::monster, MonsterAnimationState::state, MaterialAtlasId::monster, \
        0U, static_cast<std::uint16_t>(std::size(frames)), 18U, 4U, frames}

constexpr std::array<MonsterAnimationClipDefinition, 64> kMonsterClips{{
    ARPG_ECOLOGY_CLIPS(water_bulwark, water_bulwark),
    ARPG_ECOLOGY_CLIPS(water_support, water_support),
    ARPG_ECOLOGY_CLIPS(lightning_shooter, lightning_shooter),
    ARPG_ECOLOGY_CLIPS(lightning_dasher, lightning_dasher),
    ARPG_ECOLOGY_CLIPS(chaos_chaser, chaos_chaser),
    ARPG_ECOLOGY_CLIPS(chaos_hazard, chaos_hazard),
    ARPG_FIRE_CLIP(fire_bomber, idle, kBomberIdle),
    ARPG_FIRE_CLIP(fire_bomber, move, kBomberMove),
    ARPG_FIRE_CLIP(fire_bomber, telegraph, kBomberTelegraph),
    ARPG_FIRE_CLIP(fire_bomber, active, kBomberActive),
    ARPG_FIRE_CLIP(fire_bomber, recovery, kBomberRecovery),
    ARPG_FIRE_CLIP(fire_bomber, cooldown, kBomberCooldown),
    ARPG_FIRE_CLIP(fire_bomber, hurt, kBomberHurt),
    ARPG_FIRE_CLIP(fire_bomber, death, kBomberDeath),
    ARPG_FIRE_CLIP(fire_charger, idle, kChargerIdle),
    ARPG_FIRE_CLIP(fire_charger, move, kChargerMove),
    ARPG_FIRE_CLIP(fire_charger, telegraph, kChargerTelegraph),
    ARPG_FIRE_CLIP(fire_charger, active, kChargerActive),
    ARPG_FIRE_CLIP(fire_charger, recovery, kChargerRecovery),
    ARPG_FIRE_CLIP(fire_charger, cooldown, kChargerCooldown),
    ARPG_FIRE_CLIP(fire_charger, hurt, kChargerHurt),
    ARPG_FIRE_CLIP(fire_charger, death, kChargerDeath),
}};

#undef ARPG_FIRE_CLIP
#undef ARPG_ECOLOGY_CLIPS

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

std::uint16_t player_animation_frame_index(
    const PlayerAnimationClipDefinition& clip, std::uint64_t elapsed_ticks,
    std::uint16_t duration_ticks, bool loop) noexcept {
    if (clip.frame_count == 0U || duration_ticks == 0U) return 0U;
    const std::uint64_t timeline_ticks = loop
        ? elapsed_ticks % duration_ticks : std::min<std::uint64_t>(
            elapsed_ticks, duration_ticks);
    const std::uint64_t frame = timeline_ticks * clip.frame_count / duration_ticks;
    return static_cast<std::uint16_t>(std::min<std::uint64_t>(
        frame, static_cast<std::uint64_t>(clip.frame_count - 1U)));
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

MonsterAnimationState select_monster_animation_state(
    combat::MonsterAiPhase phase, bool hurt) noexcept {
    if (phase == combat::MonsterAiPhase::defeated) {
        return MonsterAnimationState::death;
    }
    if (hurt) return MonsterAnimationState::hurt;
    switch (phase) {
    case combat::MonsterAiPhase::idle: return MonsterAnimationState::idle;
    case combat::MonsterAiPhase::move: return MonsterAnimationState::move;
    case combat::MonsterAiPhase::telegraph: return MonsterAnimationState::telegraph;
    case combat::MonsterAiPhase::active: return MonsterAnimationState::active;
    case combat::MonsterAiPhase::recovery: return MonsterAnimationState::recovery;
    case combat::MonsterAiPhase::cooldown: return MonsterAnimationState::cooldown;
    case combat::MonsterAiPhase::defeated: return MonsterAnimationState::death;
    }
    return MonsterAnimationState::idle;
}

const MonsterAnimationClipDefinition* monster_animation_clip(
    combat::MonsterId monster, MonsterAnimationState state) noexcept {
    for (const MonsterAnimationClipDefinition& clip : kMonsterClips) {
        if (clip.monster == monster && clip.state == state) return &clip;
    }
    return nullptr;
}

std::optional<MonsterAnimationFrame> monster_animation_frame(
    const MonsterAnimationClipDefinition& clip, std::uint16_t frame) noexcept {
    if (frame >= clip.frame_count) return std::nullopt;
    if (clip.explicit_frames != nullptr) {
        const ExplicitMonsterAnimationFrame& explicit_frame =
            clip.explicit_frames[frame];
        if (explicit_frame.duration_ticks == 0U) return std::nullopt;
        return MonsterAnimationFrame{clip.atlas, explicit_frame.source,
            explicit_frame.foot_anchor, explicit_frame.key_pose_index};
    }
    const std::uint16_t cell = clip.first_cell + frame;
    const std::uint16_t column = cell % kWaterMonsterAnimationColumns;
    const std::uint16_t row = cell / kWaterMonsterAnimationColumns;
    return MonsterAnimationFrame{clip.atlas,
        {static_cast<float>(column) * kWaterMonsterAnimationCell,
         static_cast<float>(row) * kWaterMonsterAnimationCell,
         kWaterMonsterAnimationCell, kWaterMonsterAnimationCell},
        {48.0F, 93.0F}, static_cast<std::uint8_t>((std::min)(3U,
            static_cast<unsigned int>(frame) * clip.key_pose_count
                / clip.frame_count))};
}

std::uint16_t monster_animation_frame_index(
    const MonsterAnimationClipDefinition& clip, std::uint64_t elapsed_ticks,
    bool loop) noexcept {
    if (clip.frame_count == 0U) return 0U;
    if (clip.explicit_frames != nullptr) {
        std::uint64_t duration{};
        for (std::uint16_t index{}; index < clip.frame_count; ++index) {
            duration += clip.explicit_frames[index].duration_ticks;
        }
        if (duration == 0U) return 0U;
        std::uint64_t timeline = loop
            ? elapsed_ticks % duration
            : (std::min)(elapsed_ticks, duration - 1U);
        for (std::uint16_t index{}; index < clip.frame_count; ++index) {
            const std::uint8_t frame_duration =
                clip.explicit_frames[index].duration_ticks;
            if (timeline < frame_duration) return index;
            timeline -= frame_duration;
        }
        return static_cast<std::uint16_t>(clip.frame_count - 1U);
    }
    if (clip.frames_per_second == 0U) return 0U;
    const std::uint64_t frame = elapsed_ticks * clip.frames_per_second / 60U;
    return static_cast<std::uint16_t>(loop ? frame % clip.frame_count
        : (std::min)(frame, static_cast<std::uint64_t>(clip.frame_count - 1U)));
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

float monster_material_draw_scale(
    MaterialAtlasId atlas, float projection_scale) noexcept {
    if (projection_scale <= 0.0F) return 0.0F;
    switch (atlas) {
    case MaterialAtlasId::water_bulwark:
    case MaterialAtlasId::water_support:
    case MaterialAtlasId::lightning_shooter:
    case MaterialAtlasId::lightning_dasher:
    case MaterialAtlasId::chaos_chaser:
    case MaterialAtlasId::chaos_hazard:
        return 0.92F * projection_scale;
    case MaterialAtlasId::fire_bomber:
    case MaterialAtlasId::fire_charger:
        return material_actor_draw_scale(false, projection_scale);
    default:
        return material_actor_draw_scale(false, projection_scale);
    }
}

MonsterRenderPath select_monster_render_path(bool use_material_frame,
    bool material_frame_drawn, bool legacy_frame_drawn) noexcept {
    if (use_material_frame && material_frame_drawn) {
        return MonsterRenderPath::material;
    }
    if (!use_material_frame && legacy_frame_drawn) {
        return MonsterRenderPath::legacy;
    }
    return MonsterRenderPath::silhouette;
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
