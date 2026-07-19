#pragma once

#include "material_asset_types.hpp"

#include <cstddef>

namespace arpg::platform {

struct MaterialAtlasDefinition final {
    MaterialAtlasId id{MaterialAtlasId::actors};
    int width{};
    int height{};
    std::size_t rgba_bytes{};
};

struct MaterialManifestDefinition final {
    const MaterialAtlasDefinition* atlases{};
    std::size_t atlas_count{};
    const MaterialFrameDefinition* frames{};
    std::size_t frame_count{};
};

namespace detail {

inline constexpr MaterialAtlasDefinition kDefaultMaterialAtlases[] = {
    {MaterialAtlasId::environment, 1024, 1024, 4U * 1024U * 1024U},
    {MaterialAtlasId::actors, 2048, 2048, 4U * 2048U * 2048U},
    {MaterialAtlasId::effects_ui, 1024, 1024, 4U * 1024U * 1024U},
};

#define ARPG_ACTOR_FRAME(sprite, column, row) \
    {MaterialSpriteId::sprite, MaterialAtlasId::actors, \
        {static_cast<float>((column) * 224), static_cast<float>((row) * 224), \
            224.0F, 224.0F}, \
        {112.0F, 224.0F}, 0U}

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
    ARPG_ACTOR_FRAME(fire_bomber_idle, 4, 1),
    ARPG_ACTOR_FRAME(fire_bomber_move, 5, 1),
    ARPG_ACTOR_FRAME(fire_bomber_telegraph, 6, 1),
    ARPG_ACTOR_FRAME(fire_bomber_active, 7, 1),
    ARPG_ACTOR_FRAME(fire_bomber_recovery, 0, 2),
    ARPG_ACTOR_FRAME(fire_bomber_cooldown, 1, 2),
    ARPG_ACTOR_FRAME(fire_bomber_defeated, 2, 2),
    ARPG_ACTOR_FRAME(fire_charger_idle, 3, 2),
    ARPG_ACTOR_FRAME(fire_charger_move, 4, 2),
    ARPG_ACTOR_FRAME(fire_charger_telegraph, 5, 2),
    ARPG_ACTOR_FRAME(fire_charger_active, 6, 2),
    ARPG_ACTOR_FRAME(fire_charger_recovery, 7, 2),
    ARPG_ACTOR_FRAME(fire_charger_cooldown, 0, 3),
    ARPG_ACTOR_FRAME(fire_charger_defeated, 1, 3),
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
};

#undef ARPG_ACTOR_FRAME

}  // namespace detail

[[nodiscard]] constexpr MaterialManifestDefinition
default_material_manifest() noexcept {
    return {detail::kDefaultMaterialAtlases,
        sizeof(detail::kDefaultMaterialAtlases)
            / sizeof(detail::kDefaultMaterialAtlases[0]),
        detail::kDefaultMaterialFrames,
        sizeof(detail::kDefaultMaterialFrames)
            / sizeof(detail::kDefaultMaterialFrames[0])};
}

}  // namespace arpg::platform
