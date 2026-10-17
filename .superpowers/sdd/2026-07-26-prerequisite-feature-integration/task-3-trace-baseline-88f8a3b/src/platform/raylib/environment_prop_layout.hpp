#pragma once

#include "material_asset_types.hpp"

#include "dungeon/dungeon_types.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>

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
    Vector2 normalized_foot_position{};
    bool flip_x{};
    float scale{1.0F};
};

struct EnvironmentPropLayout final {
    std::array<EnvironmentPropPlacement, 9> props{};
    std::size_t count{};
};

[[nodiscard]] EnvironmentPropLayout environment_prop_layout(
    dungeon::DungeonElement ecology, float width, float height) noexcept;
[[nodiscard]] const EnvironmentPropDefinition*
environment_prop_definition(MaterialSpriteId sprite) noexcept;
[[nodiscard]] Rectangle project_environment_prop_bounds(
    const EnvironmentPropDefinition& definition,
    const EnvironmentPropPlacement& placement,
    float width, float height) noexcept;

}  // namespace arpg::platform
