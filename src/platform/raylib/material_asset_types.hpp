#pragma once

#include <raylib.h>

#include <cstdint>

namespace arpg::platform {

enum class MaterialAtlasId : std::uint8_t {
    environment,
    actors,
    effects_ui,
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
    fire_charger_idle,
    water_bulwark_idle,
    water_support_idle,
    lightning_shooter_idle,
    lightning_dasher_idle,
    chaos_chaser_idle,
    chaos_hazard_idle,
    environment_floor_fire,
    environment_floor_water,
    environment_floor_lightning,
    environment_floor_chaos,
    environment_door_fire,
    environment_door_water,
    environment_door_lightning,
    environment_door_chaos,
    environment_hole,
    count,
};

struct MaterialFrameDefinition final {
    MaterialSpriteId id{MaterialSpriteId::missing};
    MaterialAtlasId atlas{MaterialAtlasId::actors};
    Rectangle source{};
    Vector2 foot_anchor{};
    std::uint16_t duration_ms{};
};

}  // namespace arpg::platform
