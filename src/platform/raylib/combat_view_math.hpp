#pragma once

#include "combat/combat_types.hpp"

#include <array>
#include <cstdint>

namespace arpg::platform {

struct ScreenProjection final {
    float x{};
    float y{};
    float ground_y{};
    float scale{};
};

struct ActorDrawItem final {
    combat::Vec3 position{};
    std::uint8_t index{};
};

[[nodiscard]] ScreenProjection project_combat_position(
    combat::Vec3 position,
    float width,
    float height) noexcept;

void sort_actor_draw_items(
    std::array<ActorDrawItem, 4>& items) noexcept;

}  // namespace arpg::platform
