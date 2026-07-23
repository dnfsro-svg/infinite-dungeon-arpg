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
    std::size_t memory_budget_bytes{256U * 1024U * 1024U};
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
    {MaterialAtlasId::water_environment, 768, 768, 4U * 768U * 768U, "assets/stage12/water_environment.png", "assets/stage12/water_environment_material.png", MaterialEcology::water},
    {MaterialAtlasId::water_bulwark, 864, 864, 4U * 864U * 864U, "assets/stage12/water_bulwark.png", "assets/stage12/water_bulwark_material.png", MaterialEcology::water},
    {MaterialAtlasId::water_support, 864, 864, 4U * 864U * 864U, "assets/stage12/water_support.png", "assets/stage12/water_support_material.png", MaterialEcology::water},
    {MaterialAtlasId::lightning_environment, 768, 768, 4U * 768U * 768U, "assets/stage12/lightning_environment.png", "assets/stage12/lightning_environment_material.png", MaterialEcology::lightning},
    {MaterialAtlasId::lightning_shooter, 864, 864, 4U * 864U * 864U, "assets/stage12/lightning_shooter.png", "assets/stage12/lightning_shooter_material.png", MaterialEcology::lightning},
    {MaterialAtlasId::lightning_dasher, 864, 864, 4U * 864U * 864U, "assets/stage12/lightning_dasher.png", "assets/stage12/lightning_dasher_material.png", MaterialEcology::lightning},
    {MaterialAtlasId::chaos_environment, 768, 768, 4U * 768U * 768U, "assets/stage12/chaos_environment.png", "assets/stage12/chaos_environment_material.png", MaterialEcology::chaos},
    {MaterialAtlasId::chaos_chaser, 864, 864, 4U * 864U * 864U, "assets/stage12/chaos_chaser.png", "assets/stage12/chaos_chaser_material.png", MaterialEcology::chaos},
    {MaterialAtlasId::chaos_hazard, 864, 864, 4U * 864U * 864U, "assets/stage12/chaos_hazard.png", "assets/stage12/chaos_hazard_material.png", MaterialEcology::chaos},
    {MaterialAtlasId::items_ui, 1024, 1024, 4U * 1024U * 1024U, "assets/stage12/items_ui.png", "assets/stage12/items_ui_material.png", MaterialEcology::common},
    {MaterialAtlasId::ui_material, 1024, 1024, 4U * 1024U * 1024U, "assets/stage12/ui_material.png", "assets/stage12/ui_material_material.png", MaterialEcology::common},
    {MaterialAtlasId::fire_room_background, 2560, 1440, 14'745'600U, "assets/stage12/fire_room_background.png", "assets/stage12/fire_room_background_material.png", MaterialEcology::fire},
    {MaterialAtlasId::water_room_background, 2560, 1440, 14'745'600U, "assets/stage12/water_room_background.png", "assets/stage12/water_room_background_material.png", MaterialEcology::water},
    {MaterialAtlasId::lightning_room_background, 2560, 1440, 14'745'600U, "assets/stage12/lightning_room_background.png", "assets/stage12/lightning_room_background_material.png", MaterialEcology::lightning},
    {MaterialAtlasId::chaos_room_background, 2560, 1440, 14'745'600U, "assets/stage12/chaos_room_background.png", "assets/stage12/chaos_room_background_material.png", MaterialEcology::chaos},
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

#define ARPG_WATER_MONSTER_FRAME(sprite, atlas, cell) \
    {MaterialSpriteId::sprite, MaterialAtlasId::atlas, \
        {static_cast<float>(((cell) % 9) * 96), \
         static_cast<float>(((cell) / 9) * 96), 96.0F, 96.0F}, \
        {48.0F, 93.0F}, 42U, {68.0F, 48.0F}, MaterialLayer::body, \
        MaterialClass::actor, static_cast<std::uint32_t>((cell) * 211U + 4001U)}

#define ARPG_LIGHTNING_MONSTER_FRAME(sprite, atlas, cell) \
    {MaterialSpriteId::sprite, MaterialAtlasId::atlas, \
        {static_cast<float>(((cell) % 9) * 96), \
         static_cast<float>(((cell) / 9) * 96), 96.0F, 96.0F}, \
        {48.0F, 93.0F}, 42U, {68.0F, 48.0F}, MaterialLayer::body, \
        MaterialClass::actor, static_cast<std::uint32_t>((cell) * 223U + 6001U)}

#define ARPG_CHAOS_MONSTER_FRAME(sprite, atlas, cell) \
    {MaterialSpriteId::sprite, MaterialAtlasId::atlas, \
        {static_cast<float>(((cell) % 9) * 96), \
         static_cast<float>(((cell) / 9) * 96), 96.0F, 96.0F}, \
        {48.0F, 93.0F}, 42U, {68.0F, 48.0F}, MaterialLayer::body, \
        MaterialClass::actor, static_cast<std::uint32_t>((cell) * 227U + 7001U)}

#define ARPG_ITEM_FRAME(sprite, cell, layer_value, class_value) \
    {MaterialSpriteId::sprite, MaterialAtlasId::items_ui, \
        {static_cast<float>(((cell) % 8) * 128), \
         static_cast<float>(((cell) / 8) * 128), 128.0F, 128.0F}, \
        {64.0F, 64.0F}, 0U, {64.0F, 64.0F}, layer_value, class_value, \
        static_cast<std::uint32_t>((cell) * 239U + 8009U)}

#define ARPG_UI_FRAME(sprite, cell) \
    {MaterialSpriteId::sprite, MaterialAtlasId::ui_material, \
        {static_cast<float>(((cell) % 8) * 128), \
         static_cast<float>(((cell) / 8) * 128), 128.0F, 128.0F}, \
        {64.0F, 64.0F}, 0U, {64.0F, 64.0F}, MaterialLayer::front_effect, \
        MaterialClass::ui, static_cast<std::uint32_t>((cell) * 251U + 12007U)}

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
    ARPG_WATER_MONSTER_FRAME(water_bulwark_idle, water_bulwark, 0),
    ARPG_WATER_MONSTER_FRAME(water_bulwark_move, water_bulwark, 12),
    ARPG_WATER_MONSTER_FRAME(water_bulwark_telegraph, water_bulwark, 28),
    ARPG_WATER_MONSTER_FRAME(water_bulwark_active, water_bulwark, 35),
    ARPG_WATER_MONSTER_FRAME(water_bulwark_recovery, water_bulwark, 42),
    ARPG_WATER_MONSTER_FRAME(water_bulwark_cooldown, water_bulwark, 47),
    ARPG_WATER_MONSTER_FRAME(water_bulwark_defeated, water_bulwark, 56),
    ARPG_WATER_MONSTER_FRAME(water_support_idle, water_support, 0),
    ARPG_WATER_MONSTER_FRAME(water_support_move, water_support, 12),
    ARPG_WATER_MONSTER_FRAME(water_support_telegraph, water_support, 28),
    ARPG_WATER_MONSTER_FRAME(water_support_active, water_support, 35),
    ARPG_WATER_MONSTER_FRAME(water_support_recovery, water_support, 42),
    ARPG_WATER_MONSTER_FRAME(water_support_cooldown, water_support, 47),
    ARPG_WATER_MONSTER_FRAME(water_support_defeated, water_support, 56),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_shooter_idle, lightning_shooter, 0),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_shooter_move, lightning_shooter, 12),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_shooter_telegraph, lightning_shooter, 28),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_shooter_active, lightning_shooter, 35),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_shooter_recovery, lightning_shooter, 42),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_shooter_cooldown, lightning_shooter, 47),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_shooter_defeated, lightning_shooter, 56),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_dasher_idle, lightning_dasher, 0),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_dasher_move, lightning_dasher, 12),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_dasher_telegraph, lightning_dasher, 28),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_dasher_active, lightning_dasher, 35),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_dasher_recovery, lightning_dasher, 42),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_dasher_cooldown, lightning_dasher, 47),
    ARPG_LIGHTNING_MONSTER_FRAME(lightning_dasher_defeated, lightning_dasher, 56),
    ARPG_CHAOS_MONSTER_FRAME(chaos_chaser_idle, chaos_chaser, 0),
    ARPG_CHAOS_MONSTER_FRAME(chaos_chaser_move, chaos_chaser, 12),
    ARPG_CHAOS_MONSTER_FRAME(chaos_chaser_telegraph, chaos_chaser, 28),
    ARPG_CHAOS_MONSTER_FRAME(chaos_chaser_active, chaos_chaser, 35),
    ARPG_CHAOS_MONSTER_FRAME(chaos_chaser_recovery, chaos_chaser, 42),
    ARPG_CHAOS_MONSTER_FRAME(chaos_chaser_cooldown, chaos_chaser, 47),
    ARPG_CHAOS_MONSTER_FRAME(chaos_chaser_defeated, chaos_chaser, 56),
    ARPG_CHAOS_MONSTER_FRAME(chaos_hazard_idle, chaos_hazard, 0),
    ARPG_CHAOS_MONSTER_FRAME(chaos_hazard_move, chaos_hazard, 12),
    ARPG_CHAOS_MONSTER_FRAME(chaos_hazard_telegraph, chaos_hazard, 28),
    ARPG_CHAOS_MONSTER_FRAME(chaos_hazard_active, chaos_hazard, 35),
    ARPG_CHAOS_MONSTER_FRAME(chaos_hazard_recovery, chaos_hazard, 42),
    ARPG_CHAOS_MONSTER_FRAME(chaos_hazard_cooldown, chaos_hazard, 47),
    ARPG_CHAOS_MONSTER_FRAME(chaos_hazard_defeated, chaos_hazard, 56),
    {MaterialSpriteId::environment_floor_fire, MaterialAtlasId::environment,
        {0.0F, 0.0F, 1024.0F, 704.0F}, {512.0F, 704.0F}, 0U},
    {MaterialSpriteId::environment_floor_water, MaterialAtlasId::water_environment,
        {0.0F, 0.0F, 512.0F, 512.0F}, {256.0F, 512.0F}, 0U},
    {MaterialSpriteId::environment_floor_lightning, MaterialAtlasId::lightning_environment,
        {0.0F, 0.0F, 512.0F, 512.0F}, {256.0F, 512.0F}, 0U},
    {MaterialSpriteId::environment_floor_chaos, MaterialAtlasId::chaos_environment,
        {0.0F, 0.0F, 512.0F, 512.0F}, {256.0F, 512.0F}, 0U},
    {MaterialSpriteId::environment_door_fire, MaterialAtlasId::environment,
        {0.0F, 704.0F, 112.0F, 112.0F}, {56.0F, 112.0F}, 0U},
    {MaterialSpriteId::environment_door_water, MaterialAtlasId::water_environment,
        {512.0F, 0.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::front_effect,
        MaterialClass::environment, 5001U},
    {MaterialSpriteId::environment_door_lightning, MaterialAtlasId::lightning_environment,
        {512.0F, 0.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::front_effect,
        MaterialClass::environment, 6001U},
    {MaterialSpriteId::environment_door_chaos, MaterialAtlasId::chaos_environment,
        {512.0F, 0.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::front_effect,
        MaterialClass::environment, 7001U},
    {MaterialSpriteId::environment_hole, MaterialAtlasId::environment,
        {448.0F, 704.0F, 192.0F, 160.0F}, {96.0F, 80.0F}, 0U},
    {MaterialSpriteId::water_wall, MaterialAtlasId::water_environment,
        {0.0F, 0.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 5002U},
    {MaterialSpriteId::water_hole, MaterialAtlasId::water_environment,
        {0.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 128.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 5003U},
    {MaterialSpriteId::water_lantern, MaterialAtlasId::water_environment,
        {256.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::front_effect,
        MaterialClass::environment, 5004U},
    {MaterialSpriteId::water_coral, MaterialAtlasId::water_environment,
        {512.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 5005U},
    {MaterialSpriteId::water_grate, MaterialAtlasId::water_environment,
        {512.0F, 256.0F, 256.0F, 256.0F}, {128.0F, 128.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 5006U},
    {MaterialSpriteId::lightning_wall, MaterialAtlasId::lightning_environment,
        {0.0F, 0.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 6002U},
    {MaterialSpriteId::lightning_hole, MaterialAtlasId::lightning_environment,
        {0.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 128.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 6003U},
    {MaterialSpriteId::lightning_arc_lamp, MaterialAtlasId::lightning_environment,
        {256.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::front_effect,
        MaterialClass::environment, 6004U},
    {MaterialSpriteId::lightning_capacitor_bank, MaterialAtlasId::lightning_environment,
        {512.0F, 256.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 6005U},
    {MaterialSpriteId::lightning_grounding_rod, MaterialAtlasId::lightning_environment,
        {512.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 6006U},
    {MaterialSpriteId::chaos_wall, MaterialAtlasId::chaos_environment,
        {0.0F, 0.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 7002U},
    {MaterialSpriteId::chaos_hole, MaterialAtlasId::chaos_environment,
        {0.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 128.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 7003U},
    {MaterialSpriteId::chaos_rift_lantern, MaterialAtlasId::chaos_environment,
        {256.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::front_effect,
        MaterialClass::environment, 7004U},
    {MaterialSpriteId::chaos_anomaly_condenser, MaterialAtlasId::chaos_environment,
        {512.0F, 256.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 7005U},
    {MaterialSpriteId::chaos_warning_obelisk, MaterialAtlasId::chaos_environment,
        {512.0F, 512.0F, 256.0F, 256.0F}, {128.0F, 246.0F}, 0U,
        {128.0F, 128.0F}, MaterialLayer::body, MaterialClass::environment, 7006U},
    ARPG_EFFECT_FRAME(effect_fire, 0, 0),
    ARPG_EFFECT_FRAME(effect_water, 1, 0),
    ARPG_EFFECT_FRAME(effect_lightning, 2, 0),
    ARPG_EFFECT_FRAME(effect_chaos, 3, 0),
    ARPG_EFFECT_FRAME(effect_hit_spark, 0, 1),
    ARPG_EFFECT_FRAME(effect_launcher_trail, 1, 1),
    ARPG_EFFECT_FRAME(effect_landing_dust, 2, 1),
    ARPG_EFFECT_FRAME(effect_affix_aura, 3, 1),
    ARPG_ITEM_FRAME(item_weapon, 0, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(item_helmet, 1, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(item_chest, 2, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(item_gloves, 3, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(item_boots, 4, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(item_accessory, 5, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(loot_rarity_normal, 6, MaterialLayer::back_effect, MaterialClass::loot),
    ARPG_ITEM_FRAME(loot_rarity_magic, 7, MaterialLayer::back_effect, MaterialClass::loot),
    ARPG_ITEM_FRAME(loot_rarity_rare, 8, MaterialLayer::back_effect, MaterialClass::loot),
    ARPG_ITEM_FRAME(loot_rarity_abyss, 9, MaterialLayer::back_effect, MaterialClass::loot),
    ARPG_ITEM_FRAME(skill_stone_active, 10, MaterialLayer::front_effect, MaterialClass::ui),
    ARPG_ITEM_FRAME(skill_stone_support, 11, MaterialLayer::front_effect, MaterialClass::ui),
    ARPG_ITEM_FRAME(material_transmute, 12, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_augment, 13, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_regal, 14, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_chaos, 15, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_exalt, 16, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_annul, 17, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_divine, 18, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_scour, 19, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_directed, 20, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_reinforcement, 21, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_coupon_6, 22, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_coupon_9, 23, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_coupon_12, 24, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(material_coupon_15, 25, MaterialLayer::body, MaterialClass::loot),
    ARPG_ITEM_FRAME(bag_frame_nw, 26, MaterialLayer::front_effect, MaterialClass::ui),
    ARPG_ITEM_FRAME(bag_frame_ne, 27, MaterialLayer::front_effect, MaterialClass::ui),
    ARPG_ITEM_FRAME(bag_frame_sw, 28, MaterialLayer::front_effect, MaterialClass::ui),
    ARPG_ITEM_FRAME(bag_frame_se, 29, MaterialLayer::front_effect, MaterialClass::ui),
    ARPG_UI_FRAME(ui_hud_panel, 0),
    ARPG_UI_FRAME(ui_hud_health_track, 1),
    ARPG_UI_FRAME(ui_hud_health_fill, 2),
    ARPG_UI_FRAME(ui_hud_barrier_track, 3),
    ARPG_UI_FRAME(ui_hud_barrier_fill, 4),
    ARPG_UI_FRAME(ui_hud_resource_track, 5),
    ARPG_UI_FRAME(ui_hud_resource_fill, 6),
    ARPG_UI_FRAME(ui_hud_status_slow, 7),
    ARPG_UI_FRAME(ui_hud_status_corrosion, 8),
    ARPG_UI_FRAME(ui_hud_status_invulnerable, 9),
    ARPG_UI_FRAME(ui_hud_objective_panel, 10),
    ARPG_UI_FRAME(ui_hud_navigation_panel, 11),
    ARPG_UI_FRAME(ui_hud_notice, 12),
    ARPG_UI_FRAME(ui_hud_notice_abyss, 13),
    ARPG_UI_FRAME(ui_hud_skill_empty, 14),
    ARPG_UI_FRAME(ui_hud_skill_ready, 15),
    ARPG_UI_FRAME(ui_hud_skill_cooldown, 16),
    ARPG_UI_FRAME(ui_inventory_panel_equipment, 17),
    ARPG_UI_FRAME(ui_inventory_panel_grid, 18),
    ARPG_UI_FRAME(ui_inventory_panel_detail, 19),
    ARPG_UI_FRAME(ui_inventory_tab_idle, 20),
    ARPG_UI_FRAME(ui_inventory_tab_active, 21),
    ARPG_UI_FRAME(ui_inventory_slot_idle, 22),
    ARPG_UI_FRAME(ui_inventory_slot_selected, 23),
    ARPG_UI_FRAME(ui_inventory_button_idle, 24),
    ARPG_UI_FRAME(ui_inventory_button_active, 25),
    ARPG_UI_FRAME(ui_inventory_button_disabled, 26),
    ARPG_UI_FRAME(ui_skill_panel, 27),
    ARPG_UI_FRAME(ui_skill_slot_empty, 28),
    ARPG_UI_FRAME(ui_skill_slot_ready, 29),
    ARPG_UI_FRAME(ui_skill_slot_selected, 30),
    ARPG_UI_FRAME(ui_skill_slot_support, 31),
    ARPG_UI_FRAME(ui_pause_panel, 32),
    ARPG_UI_FRAME(ui_pause_row_idle, 33),
    ARPG_UI_FRAME(ui_pause_row_selected, 34),
    ARPG_UI_FRAME(ui_pause_footer, 35),
    ARPG_UI_FRAME(ui_warning_modal, 36),
    ARPG_UI_FRAME(ui_label_plate, 37),
    ARPG_UI_FRAME(ui_reinforcement_confirm, 38),
    ARPG_UI_FRAME(ui_reinforcement_cancel, 39),
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
#undef ARPG_WATER_MONSTER_FRAME
#undef ARPG_LIGHTNING_MONSTER_FRAME
#undef ARPG_CHAOS_MONSTER_FRAME
#undef ARPG_ITEM_FRAME
#undef ARPG_UI_FRAME

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
        256U * 1024U * 1024U};
}

[[nodiscard]] std::size_t resident_peak_bytes(
    const MaterialManifestDefinition& manifest) noexcept;
[[nodiscard]] std::size_t full_pack_bytes(
    const MaterialManifestDefinition& manifest) noexcept;

[[nodiscard]] constexpr const MaterialFrameDefinition* find_material_frame(
    const MaterialManifestDefinition& manifest, MaterialSpriteId id) noexcept {
    for (std::size_t index{}; index < manifest.frame_count; ++index) {
        if (manifest.frames[index].id == id) return &manifest.frames[index];
    }
    return nullptr;
}

[[nodiscard]] constexpr MaterialSpriteId select_loot_sprite(
    items::ItemRarity rarity, bool abyss = false) noexcept {
    if (abyss) return MaterialSpriteId::loot_rarity_abyss;
    switch (rarity) {
    case items::ItemRarity::normal: return MaterialSpriteId::loot_rarity_normal;
    case items::ItemRarity::magic: return MaterialSpriteId::loot_rarity_magic;
    case items::ItemRarity::rare: return MaterialSpriteId::loot_rarity_rare;
    }
    return MaterialSpriteId::missing;
}

}  // namespace arpg::platform
