#include "material_animation.hpp"

namespace arpg::platform {
namespace {

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

MaterialSpriteId select_monster_sprite(
    combat::MonsterId monster,
    combat::MonsterAiPhase phase) noexcept {
    static_cast<void>(phase);
    switch (monster) {
    case combat::MonsterId::fire_bomber:
        return MaterialSpriteId::fire_bomber_idle;
    case combat::MonsterId::fire_charger:
        return MaterialSpriteId::fire_charger_idle;
    case combat::MonsterId::water_bulwark:
        return MaterialSpriteId::water_bulwark_idle;
    case combat::MonsterId::water_support:
        return MaterialSpriteId::water_support_idle;
    case combat::MonsterId::lightning_shooter:
        return MaterialSpriteId::lightning_shooter_idle;
    case combat::MonsterId::lightning_dasher:
        return MaterialSpriteId::lightning_dasher_idle;
    case combat::MonsterId::chaos_chaser:
        return MaterialSpriteId::chaos_chaser_idle;
    case combat::MonsterId::chaos_hazard:
        return MaterialSpriteId::chaos_hazard_idle;
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

}  // namespace arpg::platform
