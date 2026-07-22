#pragma once

#include <raylib.h>

#include <cstdint>
#include <cstddef>

namespace arpg::platform {

enum class MaterialAtlasId : std::uint8_t {
    environment,
    actors,
    effects_ui,
    player_locomotion,
    player_combo_a,
    player_combo_b,
    player_reaction,
    player_air,
    fire_environment,
    fire_bomber,
    fire_charger,
    count,
};

enum class MaterialLayer : std::uint8_t {
    shadow,
    back_effect,
    body,
    weapon,
    front_effect,
    count,
};

enum class MaterialClass : std::uint8_t {
    environment,
    actor,
    effect,
    ui,
    loot,
    count,
};

enum class MaterialEcology : std::uint8_t {
    common,
    fire,
    water,
    lightning,
    chaos,
    count,
};

enum class AnimationEventKind : std::uint8_t {
    none,
    hit,
    footstep,
    spawn_effect,
    recovery,
};

enum class AnimationClipId : std::uint8_t {
    player_idle,
    player_move,
    player_j1,
    player_j2,
    player_j3,
    player_launcher,
    player_jump,
    monster_idle,
    monster_move,
    monster_attack,
    monster_hurt,
    monster_death,
    count,
};

enum class MaterialSpriteId : std::uint16_t {
    missing,
    player_idle,
    player_move,
    player_j1,
    player_j2,
    player_j3,
    player_launcher,
    player_jump_rise,
    player_jump_fall,
    player_air_j,
    player_landing,
    player_hurt,
    player_dead,
    fire_bomber_idle,
    fire_bomber_move,
    fire_bomber_telegraph,
    fire_bomber_active,
    fire_bomber_recovery,
    fire_bomber_cooldown,
    fire_bomber_defeated,
    fire_charger_idle,
    fire_charger_move,
    fire_charger_telegraph,
    fire_charger_active,
    fire_charger_recovery,
    fire_charger_cooldown,
    fire_charger_defeated,
    water_bulwark_idle,
    water_bulwark_move,
    water_bulwark_telegraph,
    water_bulwark_active,
    water_bulwark_recovery,
    water_bulwark_cooldown,
    water_bulwark_defeated,
    water_support_idle,
    water_support_move,
    water_support_telegraph,
    water_support_active,
    water_support_recovery,
    water_support_cooldown,
    water_support_defeated,
    lightning_shooter_idle,
    lightning_shooter_move,
    lightning_shooter_telegraph,
    lightning_shooter_active,
    lightning_shooter_recovery,
    lightning_shooter_cooldown,
    lightning_shooter_defeated,
    lightning_dasher_idle,
    lightning_dasher_move,
    lightning_dasher_telegraph,
    lightning_dasher_active,
    lightning_dasher_recovery,
    lightning_dasher_cooldown,
    lightning_dasher_defeated,
    chaos_chaser_idle,
    chaos_chaser_move,
    chaos_chaser_telegraph,
    chaos_chaser_active,
    chaos_chaser_recovery,
    chaos_chaser_cooldown,
    chaos_chaser_defeated,
    chaos_hazard_idle,
    chaos_hazard_move,
    chaos_hazard_telegraph,
    chaos_hazard_active,
    chaos_hazard_recovery,
    chaos_hazard_cooldown,
    chaos_hazard_defeated,
    environment_floor_fire,
    environment_floor_water,
    environment_floor_lightning,
    environment_floor_chaos,
    environment_door_fire,
    environment_door_water,
    environment_door_lightning,
    environment_door_chaos,
    environment_hole,
    fire_wall,
    fire_torch,
    fire_chain,
    fire_banner,
    fire_weapon_rack,
    fire_bone_pile,
    fire_breakable_crate,
    fire_solid_brazier,
    effect_fire,
    effect_water,
    effect_lightning,
    effect_chaos,
    effect_hit_spark,
    effect_launcher_trail,
    effect_landing_dust,
    effect_affix_aura,
    loot_icon_normal,
    loot_icon_magic,
    loot_icon_rare,
    loot_icon_abyss,
    count,
};

struct MaterialFrameDefinition final {
    MaterialSpriteId id{MaterialSpriteId::missing};
    MaterialAtlasId atlas{MaterialAtlasId::actors};
    Rectangle source{};
    Vector2 foot_anchor{};
    std::uint16_t duration_ms{};
    Vector2 weapon_anchor{};
    MaterialLayer layer{MaterialLayer::body};
    MaterialClass material_class{MaterialClass::actor};
    std::uint32_t perceptual_hash{};
};

struct AnimationEventDefinition final {
    std::uint16_t frame_index{};
    AnimationEventKind kind{AnimationEventKind::none};
};

struct AnimationClipDefinition final {
    AnimationClipId id{AnimationClipId::player_idle};
    MaterialSpriteId resource_id{MaterialSpriteId::missing};
    std::uint16_t first_frame{};
    std::uint16_t frame_count{};
    std::uint16_t minimum_frames{};
    std::uint8_t frames_per_second{24U};
    std::uint16_t first_event{};
    std::uint16_t event_count{};
    MaterialEcology ecology{MaterialEcology::common};
};

}  // namespace arpg::platform
