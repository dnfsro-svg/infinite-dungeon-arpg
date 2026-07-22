#pragma once

#include "material_asset_types.hpp"

#include "items/item_types.hpp"

#include <cstddef>

namespace arpg::platform {

struct MaterialAtlasDefinition final {
    MaterialAtlasId id{MaterialAtlasId::actors};
    int width{};
    int height{};
    std::size_t rgba_bytes{};
    const char* color_path{};
    const char* material_path{};
    MaterialEcology ecology{MaterialEcology::common};
};

struct MaterialManifestDefinition final {
    const MaterialAtlasDefinition* atlases{};
    std::size_t atlas_count{};
    const MaterialFrameDefinition* frames{};
    std::size_t frame_count{};
    const AnimationClipDefinition* clips{};
    std::size_t clip_count{};
    const AnimationEventDefinition* events{};
    std::size_t event_count{};
    std::size_t memory_budget_bytes{64U * 1024U * 1024U};
};

namespace detail {

inline constexpr MaterialAtlasDefinition kDefaultMaterialAtlases[] = {
    {MaterialAtlasId::environment, 1024, 1024, 4U * 1024U * 1024U, "assets/stage12/environment.png", "assets/stage12/environment_material.png", MaterialEcology::common},
    {MaterialAtlasId::actors, 2048, 2048, 4U * 2048U * 2048U, "assets/stage12/actors.png", "assets/stage12/actors_material.png", MaterialEcology::common},
    {MaterialAtlasId::effects_ui, 1024, 1024, 4U * 1024U * 1024U, "assets/stage12/effects_ui.png", "assets/stage12/effects_ui_material.png", MaterialEcology::common},
    {MaterialAtlasId::player_locomotion, 1024, 1024, 4U * 1024U * 1024U, "assets/player/player_locomotion.png", "assets/player/player_locomotion_material.png", MaterialEcology::common},
    {MaterialAtlasId::player_combo_a, 1024, 1024, 4U * 1024U * 1024U, "assets/player/player_combo_a.png", "assets/player/player_combo_a_material.png", MaterialEcology::common},
    {MaterialAtlasId::player_combo_b, 1024, 1024, 4U * 1024U * 1024U, "assets/player/player_combo_b.png", "assets/player/player_combo_b_material.png", MaterialEcology::common},
    {MaterialAtlasId::player_reaction, 1024, 1024, 4U * 1024U * 1024U, "assets/player/player_reaction.png", "assets/player/player_reaction_material.png", MaterialEcology::common},
    {MaterialAtlasId::player_air, 1024, 1024, 4U * 1024U * 1024U, "assets/player/player_air.png", "assets/player/player_air_material.png", MaterialEcology::common},
    {MaterialAtlasId::fire_environment, 1024, 1024, 4U * 1024U * 1024U, "assets/stage12/fire_environment.png", "assets/stage12/fire_environment_material.png", MaterialEcology::fire},
    {MaterialAtlasId::fire_bomber, 1024, 1024, 4U * 1024U * 1024U, "assets/stage12/fire_bomber.png", "assets/stage12/fire_bomber_material.png", MaterialEcology::fire},
    {MaterialAtlasId::fire_charger, 1024, 1024, 4U * 1024U * 1024U, "assets/stage12/fire_charger.png", "assets/stage12/fire_charger_material.png", MaterialEcology::fire},
};

#define ARPG_ACTOR_FRAME(sprite, column, row) \
    {MaterialSpriteId::sprite, MaterialAtlasId::actors, \
        {static_cast<float>((column) * 224), static_cast<float>((row) * 224), \
            224.0F, 224.0F}, \
        {112.0F, 224.0F}, 42U, {152.0F, 118.0F}, MaterialLayer::body, MaterialClass::actor, static_cast<std::uint32_t>((column) * 131U + (row) * 17U + 1U)}

#define ARPG_EFFECT_FRAME(sprite, column, row) \
    {MaterialSpriteId::sprite, MaterialAtlasId::effects_ui, \
        {static_cast<float>((column) * 128), static_cast<float>((row) * 128), \
            128.0F, 128.0F}, {64.0F, 110.0F}, 42U, {64.0F, 64.0F}, MaterialLayer::front_effect, MaterialClass::effect, static_cast<std::uint32_t>((column) * 137U + (row) * 19U + 101U)}

#define ARPG_FIRE_MONSTER_FRAME(sprite, atlas, column, row) \
    {MaterialSpriteId::sprite, MaterialAtlasId::atlas, \
        {static_cast<float>((column) * 256), static_cast<float>((row) * 256), \
            256.0F, 256.0F}, {128.0F, 246.0F}, 42U, {178.0F, 126.0F}, MaterialLayer::body, MaterialClass::actor, static_cast<std::uint32_t>((column) * 173U + (row) * 29U + 1009U)}

inline constexpr MaterialFrameDefinition kDefaultMaterialFrames[] = {
    ARPG_ACTOR_FRAME(player_idle, 0, 0),
    ARPG_ACTOR_FRAME(player_move, 1, 0),
    ARPG_ACTOR_FRAME(player_j1, 2, 0),
    ARPG_ACTOR_FRAME(player_j2, 3, 0),
    ARPG_ACTOR_FRAME(player_j3, 4, 0),
    ARPG_ACTOR_FRAME(player_launcher, 5, 0),
    ARPG_ACTOR_FRAME(player_jump_rise, 6, 0),
    ARPG_ACTOR_FRAME(player_jump_fall, 7, 0),
    ARPG_ACTOR_FRAME(player_air_j, 0, 1),
    ARPG_ACTOR_FRAME(player_landing, 1, 1),
    ARPG_ACTOR_FRAME(player_hurt, 2, 1),
    ARPG_ACTOR_FRAME(player_dead, 3, 1),
    ARPG_FIRE_MONSTER_FRAME(fire_bomber_idle, fire_bomber, 0, 0),
    ARPG_FIRE_MONSTER_FRAME(fire_bomber_move, fire_bomber, 1, 0),
    ARPG_FIRE_MONSTER_FRAME(fire_bomber_telegraph, fire_bomber, 2, 0),
    ARPG_FIRE_MONSTER_FRAME(fire_bomber_active, fire_bomber, 3, 0),
    ARPG_FIRE_MONSTER_FRAME(fire_bomber_recovery, fire_bomber, 0, 1),
    ARPG_FIRE_MONSTER_FRAME(fire_bomber_cooldown, fire_bomber, 1, 1),
    ARPG_FIRE_MONSTER_FRAME(fire_bomber_defeated, fire_bomber, 2, 1),
    ARPG_FIRE_MONSTER_FRAME(fire_charger_idle, fire_charger, 0, 0),
    ARPG_FIRE_MONSTER_FRAME(fire_charger_move, fire_charger, 1, 0),
    ARPG_FIRE_MONSTER_FRAME(fire_charger_telegraph, fire_charger, 2, 0),
    ARPG_FIRE_MONSTER_FRAME(fire_charger_active, fire_charger, 3, 0),
    ARPG_FIRE_MONSTER_FRAME(fire_charger_recovery, fire_charger, 0, 1),
    ARPG_FIRE_MONSTER_FRAME(fire_charger_cooldown, fire_charger, 1, 1),
    ARPG_FIRE_MONSTER_FRAME(fire_charger_defeated, fire_charger, 2, 1),
    {MaterialSpriteId::fire_wall, MaterialAtlasId::fire_environment, {256.0F, 0.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U, {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 2001U},
    {MaterialSpriteId::fire_torch, MaterialAtlasId::fire_environment, {768.0F, 256.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U, {128.0F, 128.0F}, MaterialLayer::front_effect, MaterialClass::environment, 2002U},
    {MaterialSpriteId::fire_chain, MaterialAtlasId::fire_environment, {0.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U, {128.0F, 128.0F}, MaterialLayer::front_effect, MaterialClass::environment, 2003U},
    {MaterialSpriteId::fire_banner, MaterialAtlasId::fire_environment, {256.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U, {128.0F, 128.0F}, MaterialLayer::front_effect, MaterialClass::environment, 2004U},
    {MaterialSpriteId::fire_weapon_rack, MaterialAtlasId::fire_environment, {512.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U, {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 2005U},
    {MaterialSpriteId::fire_bone_pile, MaterialAtlasId::fire_environment, {768.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U, {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 2006U},
    {MaterialSpriteId::fire_breakable_crate, MaterialAtlasId::fire_environment, {0.0F, 768.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U, {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 2007U},
    {MaterialSpriteId::fire_solid_brazier, MaterialAtlasId::fire_environment, {256.0F, 768.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U, {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 2008U},
    ARPG_ACTOR_FRAME(water_bulwark_idle, 2, 3),
    ARPG_ACTOR_FRAME(water_bulwark_move, 3, 3),
    ARPG_ACTOR_FRAME(water_bulwark_telegraph, 4, 3),
    ARPG_ACTOR_FRAME(water_bulwark_active, 5, 3),
    ARPG_ACTOR_FRAME(water_bulwark_recovery, 6, 3),
    ARPG_ACTOR_FRAME(water_bulwark_cooldown, 7, 3),
    ARPG_ACTOR_FRAME(water_bulwark_defeated, 0, 4),
    ARPG_ACTOR_FRAME(water_support_idle, 1, 4),
    ARPG_ACTOR_FRAME(water_support_move, 2, 4),
    ARPG_ACTOR_FRAME(water_support_telegraph, 3, 4),
    ARPG_ACTOR_FRAME(water_support_active, 4, 4),
    ARPG_ACTOR_FRAME(water_support_recovery, 5, 4),
    ARPG_ACTOR_FRAME(water_support_cooldown, 6, 4),
    ARPG_ACTOR_FRAME(water_support_defeated, 7, 4),
    ARPG_ACTOR_FRAME(lightning_shooter_idle, 0, 5),
    ARPG_ACTOR_FRAME(lightning_shooter_move, 1, 5),
    ARPG_ACTOR_FRAME(lightning_shooter_telegraph, 2, 5),
    ARPG_ACTOR_FRAME(lightning_shooter_active, 3, 5),
    ARPG_ACTOR_FRAME(lightning_shooter_recovery, 4, 5),
    ARPG_ACTOR_FRAME(lightning_shooter_cooldown, 5, 5),
    ARPG_ACTOR_FRAME(lightning_shooter_defeated, 6, 5),
    ARPG_ACTOR_FRAME(lightning_dasher_idle, 7, 5),
    ARPG_ACTOR_FRAME(lightning_dasher_move, 0, 6),
    ARPG_ACTOR_FRAME(lightning_dasher_telegraph, 1, 6),
    ARPG_ACTOR_FRAME(lightning_dasher_active, 2, 6),
    ARPG_ACTOR_FRAME(lightning_dasher_recovery, 3, 6),
    ARPG_ACTOR_FRAME(lightning_dasher_cooldown, 4, 6),
    ARPG_ACTOR_FRAME(lightning_dasher_defeated, 5, 6),
    ARPG_ACTOR_FRAME(chaos_chaser_idle, 6, 6),
    ARPG_ACTOR_FRAME(chaos_chaser_move, 7, 6),
    ARPG_ACTOR_FRAME(chaos_chaser_telegraph, 0, 7),
    ARPG_ACTOR_FRAME(chaos_chaser_active, 1, 7),
    ARPG_ACTOR_FRAME(chaos_chaser_recovery, 2, 7),
    ARPG_ACTOR_FRAME(chaos_chaser_cooldown, 3, 7),
    ARPG_ACTOR_FRAME(chaos_chaser_defeated, 4, 7),
    ARPG_ACTOR_FRAME(chaos_hazard_idle, 5, 7),
    ARPG_ACTOR_FRAME(chaos_hazard_move, 6, 7),
    ARPG_ACTOR_FRAME(chaos_hazard_telegraph, 7, 7),
    ARPG_ACTOR_FRAME(chaos_hazard_active, 0, 8),
    ARPG_ACTOR_FRAME(chaos_hazard_recovery, 1, 8),
    ARPG_ACTOR_FRAME(chaos_hazard_cooldown, 2, 8),
    ARPG_ACTOR_FRAME(chaos_hazard_defeated, 3, 8),
    {MaterialSpriteId::environment_floor_fire, MaterialAtlasId::environment,
        {0.0F, 0.0F, 1024.0F, 704.0F}, {512.0F, 704.0F}, 0U},
    {MaterialSpriteId::environment_floor_water, MaterialAtlasId::environment,
        {0.0F, 0.0F, 1024.0F, 704.0F}, {512.0F, 704.0F}, 0U},
    {MaterialSpriteId::environment_floor_lightning, MaterialAtlasId::environment,
        {0.0F, 0.0F, 1024.0F, 704.0F}, {512.0F, 704.0F}, 0U},
    {MaterialSpriteId::environment_floor_chaos, MaterialAtlasId::environment,
        {0.0F, 0.0F, 1024.0F, 704.0F}, {512.0F, 704.0F}, 0U},
    {MaterialSpriteId::environment_door_fire, MaterialAtlasId::environment,
        {0.0F, 704.0F, 112.0F, 112.0F}, {56.0F, 112.0F}, 0U},
    {MaterialSpriteId::environment_door_water, MaterialAtlasId::environment,
        {112.0F, 704.0F, 112.0F, 112.0F}, {56.0F, 112.0F}, 0U},
    {MaterialSpriteId::environment_door_lightning, MaterialAtlasId::environment,
        {224.0F, 704.0F, 112.0F, 112.0F}, {56.0F, 112.0F}, 0U},
    {MaterialSpriteId::environment_door_chaos, MaterialAtlasId::environment,
        {336.0F, 704.0F, 112.0F, 112.0F}, {56.0F, 112.0F}, 0U},
    {MaterialSpriteId::environment_hole, MaterialAtlasId::environment,
        {448.0F, 704.0F, 192.0F, 160.0F}, {96.0F, 80.0F}, 0U},
    ARPG_EFFECT_FRAME(effect_fire, 0, 0),
    ARPG_EFFECT_FRAME(effect_water, 1, 0),
    ARPG_EFFECT_FRAME(effect_lightning, 2, 0),
    ARPG_EFFECT_FRAME(effect_chaos, 3, 0),
    ARPG_EFFECT_FRAME(effect_hit_spark, 0, 1),
    ARPG_EFFECT_FRAME(effect_launcher_trail, 1, 1),
    ARPG_EFFECT_FRAME(effect_landing_dust, 2, 1),
    ARPG_EFFECT_FRAME(effect_affix_aura, 3, 1),
    ARPG_EFFECT_FRAME(loot_icon_normal, 0, 2),
    ARPG_EFFECT_FRAME(loot_icon_magic, 1, 2),
    ARPG_EFFECT_FRAME(loot_icon_rare, 2, 2),
    ARPG_EFFECT_FRAME(loot_icon_abyss, 3, 2),
};

inline constexpr AnimationEventDefinition kDefaultAnimationEvents[] = {
    {1U, AnimationEventKind::footstep},
    {2U, AnimationEventKind::hit},
};

inline constexpr AnimationClipDefinition kDefaultAnimationClips[] = {
    {AnimationClipId::player_idle, MaterialSpriteId::player_idle, 0U, 16U, 16U, 24U, 0U, 0U, MaterialEcology::common},
    {AnimationClipId::player_move, MaterialSpriteId::player_move, 0U, 20U, 20U, 24U, 0U, 1U, MaterialEcology::common},
    {AnimationClipId::player_j1, MaterialSpriteId::player_j1, 0U, 18U, 18U, 24U, 1U, 1U, MaterialEcology::common},
    {AnimationClipId::player_j2, MaterialSpriteId::player_j2, 0U, 22U, 22U, 24U, 1U, 1U, MaterialEcology::common},
    {AnimationClipId::player_j3, MaterialSpriteId::player_j3, 0U, 26U, 26U, 24U, 1U, 1U, MaterialEcology::common},
    {AnimationClipId::player_launcher, MaterialSpriteId::player_launcher, 0U, 24U, 24U, 24U, 1U, 1U, MaterialEcology::common},
    {AnimationClipId::player_jump, MaterialSpriteId::player_jump_rise, 0U, 24U, 24U, 24U, 0U, 0U, MaterialEcology::common},
    {AnimationClipId::monster_idle, MaterialSpriteId::fire_bomber_idle, 0U, 12U, 12U, 24U, 0U, 0U, MaterialEcology::fire},
    {AnimationClipId::monster_move, MaterialSpriteId::fire_bomber_move, 0U, 16U, 16U, 24U, 0U, 1U, MaterialEcology::fire},
    {AnimationClipId::monster_attack, MaterialSpriteId::fire_bomber_active, 0U, 20U, 20U, 24U, 1U, 1U, MaterialEcology::fire},
    {AnimationClipId::monster_hurt, MaterialSpriteId::fire_bomber_recovery, 0U, 8U, 8U, 24U, 0U, 0U, MaterialEcology::fire},
    {AnimationClipId::monster_death, MaterialSpriteId::fire_bomber_defeated, 0U, 16U, 16U, 24U, 0U, 0U, MaterialEcology::fire},
};

#undef ARPG_ACTOR_FRAME
#undef ARPG_EFFECT_FRAME
#undef ARPG_FIRE_MONSTER_FRAME

}  // namespace detail

[[nodiscard]] constexpr MaterialManifestDefinition
default_material_manifest() noexcept {
    return {detail::kDefaultMaterialAtlases,
        sizeof(detail::kDefaultMaterialAtlases)
            / sizeof(detail::kDefaultMaterialAtlases[0]),
        detail::kDefaultMaterialFrames,
        sizeof(detail::kDefaultMaterialFrames)
            / sizeof(detail::kDefaultMaterialFrames[0]),
        detail::kDefaultAnimationClips,
        sizeof(detail::kDefaultAnimationClips) / sizeof(detail::kDefaultAnimationClips[0]),
        detail::kDefaultAnimationEvents,
        sizeof(detail::kDefaultAnimationEvents) / sizeof(detail::kDefaultAnimationEvents[0]),
        64U * 1024U * 1024U};
}

[[nodiscard]] constexpr MaterialSpriteId select_loot_sprite(
    items::ItemRarity rarity, bool abyss = false) noexcept {
    if (abyss) return MaterialSpriteId::loot_icon_abyss;
    switch (rarity) {
    case items::ItemRarity::normal: return MaterialSpriteId::loot_icon_normal;
    case items::ItemRarity::magic: return MaterialSpriteId::loot_icon_magic;
    case items::ItemRarity::rare: return MaterialSpriteId::loot_icon_rare;
    }
    return MaterialSpriteId::missing;
}

}  // namespace arpg::platform
