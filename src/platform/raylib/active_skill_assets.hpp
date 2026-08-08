#pragma once

#include "material_asset_types.hpp"

#include "skills/active_skill_types.hpp"

#include <raylib.h>

#include <cstddef>
#include <optional>

namespace arpg::platform {

using ActiveSkillAtlasId = skills::ActiveSkillId;

struct ActiveSkillAtlasFrame final {
    MaterialAtlasId atlas{MaterialAtlasId::count};
    Rectangle source{};
    Vector2 foot_anchor{};
    Vector2 weapon_anchor{};
};

[[nodiscard]] MaterialAtlasId active_skill_material_atlas(
    skills::ActiveSkillId id) noexcept;
[[nodiscard]] std::optional<ActiveSkillAtlasFrame> active_skill_atlas_frame(
    skills::ActiveSkillId id, std::size_t frame_index) noexcept;
[[nodiscard]] float active_skill_material_draw_scale(
    skills::ActiveSkillId id, float projection_scale) noexcept;
[[nodiscard]] float active_skill_material_effect_scale(
    skills::ActiveSkillId id, float projection_scale) noexcept;
[[nodiscard]] bool active_skill_assets_ready() noexcept;

}  // namespace arpg::platform
