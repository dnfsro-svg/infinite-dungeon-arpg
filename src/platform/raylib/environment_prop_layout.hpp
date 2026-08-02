#pragma once

#include "combat_view_math.hpp"
#include "material_asset_types.hpp"

#include "dungeon/room_environment.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

inline constexpr float kEnvironmentGameplayHoleScale = 0.77F;

struct EnvironmentPropDefinition final {
    MaterialSpriteId sprite{MaterialSpriteId::missing};
    MaterialLayer layer{MaterialLayer::body};
    Rectangle alpha_bounds{};
    Vector2 foot_anchor{};
    float recommended_scale{1.0F};
};

struct EnvironmentPropPlacement final {
    MaterialSpriteId sprite{MaterialSpriteId::missing};
    combat::Vec3 world_foot_position{};
    bool flip_x{};
    float scale{1.0F};
    std::uint16_t ordinal{};
    std::uint8_t quarter_turns{};
    combat::RoomObstacleKind obstacle_kind{combat::RoomObstacleKind::none};
};

struct EnvironmentPropLayout final {
    std::array<EnvironmentPropPlacement,
        dungeon::kVisibleEnvironmentCapacity> props{};
    std::size_t count{};
};

[[nodiscard]] EnvironmentPropLayout environment_prop_layout(
    const dungeon::DungeonRenderSnapshot& world) noexcept;
[[nodiscard]] EnvironmentPropLayout environment_prop_layout(
    dungeon::DungeonElement ecology,
    const dungeon::VisibleEnvironmentSet& visible) noexcept;
// Compatibility fixture for material-only tests. Production rendering consumes
// the camera-bounded VisibleEnvironmentSet overload above.
[[nodiscard]] EnvironmentPropLayout environment_prop_layout(
    dungeon::DungeonElement ecology, float width, float height) noexcept;
[[nodiscard]] const EnvironmentPropDefinition*
environment_prop_definition(MaterialSpriteId sprite) noexcept;
[[nodiscard]] Rectangle project_environment_prop_bounds(
    const EnvironmentPropDefinition& definition,
    const EnvironmentPropPlacement& placement,
    const CombatCameraView& camera,
    float width, float height) noexcept;
[[nodiscard]] Rectangle project_environment_prop_bounds(
    const EnvironmentPropDefinition& definition,
    const EnvironmentPropPlacement& placement,
    float width, float height) noexcept;

struct ProjectedEnvironmentProp final {
    Vector2 foot_position{};
    float scale{};
};

[[nodiscard]] ProjectedEnvironmentProp project_environment_prop(
    const EnvironmentPropPlacement& placement,
    const CombatCameraView& camera,
    float width, float height) noexcept;

}  // namespace arpg::platform
