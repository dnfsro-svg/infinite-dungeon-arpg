#pragma once

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_types.hpp"
#include "dungeon/room_environment.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::dungeon {

inline constexpr std::size_t kVisibleEquipmentDropCapacity = 192U;
inline constexpr std::size_t kVisibleMaterialDropCapacity = 400U;
inline constexpr std::size_t kVisibleHealthPotionCapacity = 192U;

struct WorldViewQuery final {
    combat::Aabb world_bounds{};
    int screen_width{};
    int screen_height{};
    std::uint64_t camera_version{};
};

struct DoorRenderSnapshot final {
    combat::Vec3 position{};
    ExitDirection direction{ExitDirection::none};
    bool open{};
    bool abyss{};
};

struct HoleRenderSnapshot final {
    combat::Vec3 position{};
    bool present{};
    bool open{};
};

struct EnvironmentObstacleRenderSnapshot final {
    std::uint16_t ordinal{0xFFFFU};
    combat::RoomObstacleKind kind{combat::RoomObstacleKind::none};
    std::uint16_t hp{};
    std::uint16_t max_hp{};
    std::uint64_t broken_tick{};
    bool present{};
    bool intact{};
};

struct DungeonRenderSnapshot final {
    WorldViewQuery query{};
    RoomPhase phase{RoomPhase::locked};
    DungeonElement ecology{DungeonElement::fire};
    bool has_active_room{};
    bool has_combat{};
    bool exits_unlocked{};
    bool full_clear{};
    combat::CombatSnapshot combat{};
    VisibleEnvironmentQueryResult environment_query{};
    VisibleEnvironmentSet environment{};
    // Index-aligned with environment.records. Decorations have present=false.
    std::array<EnvironmentObstacleRenderSnapshot,
        kVisibleEnvironmentCapacity> environment_obstacles{};
    std::array<GroundItemSnapshot, kVisibleEquipmentDropCapacity>
        equipment{};
    std::uint16_t equipment_count{};
    std::array<GroundMaterialSnapshot, kVisibleMaterialDropCapacity>
        materials{};
    std::uint16_t material_count{};
    std::array<GroundHealthPotionSnapshot, kVisibleHealthPotionCapacity>
        health_potions{};
    std::uint16_t health_potion_count{};
    std::uint16_t drop_candidates_examined{};
    std::array<DoorRenderSnapshot, 4U> doors{};
    HoleRenderSnapshot hole{};
};

static_assert(kVisibleEquipmentDropCapacity == kGroundDropCapacity);
static_assert(kVisibleMaterialDropCapacity == kGroundMaterialCapacity);
static_assert(kVisibleHealthPotionCapacity == kGroundHealthPotionCapacity);
static_assert(combat::kMonsterCapacity == 128U);
static_assert(combat::kProjectileCapacity == 512U);
static_assert(combat::kHazardCapacity == 128U);

}  // namespace arpg::dungeon
