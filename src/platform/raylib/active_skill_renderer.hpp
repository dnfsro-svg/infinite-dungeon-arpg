#pragma once

#include "active_skill_view.hpp"
#include "combat/combat_types.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace arpg::platform {

struct CombatCameraView;

struct DrawSlashVisualPlan final {
    bool visible{};
    combat::Vec3 center{};
    combat::Facing facing{combat::Facing::right};
    float opacity{};
};

struct StormSwordVisual final {
    float angle_radians{};
    bool highlighted{};
};

struct StormSwordsVisualPlan final {
    bool visible{};
    bool finisher_visible{};
    combat::Vec3 center{};
    std::array<StormSwordVisual, 12> swords{};
    std::size_t sword_count{};
    float finisher_opacity{};
};

struct ActiveSkillEffectPlan final {
    DrawSlashVisualPlan draw_slash{};
    StormSwordsVisualPlan storm_swords{};
    float screen_flash_alpha{};
};

[[nodiscard]] ActiveSkillEffectPlan make_active_skill_effect_plan(
    const combat::CombatSnapshot& snapshot,
    const combat::CombatEvent* last_event) noexcept;

class ActiveSkillRenderer final {
public:
    void draw_world(const combat::CombatSnapshot& snapshot,
        const combat::CombatEvent* last_event,
        CombatCameraView view,
        float width, float height) const noexcept;
    void draw_hud(const ActiveSkillHudModel& model,
        const ActiveSkillHudLayout& layout,
        Font hud_font, bool hud_font_ready) const noexcept;
};

}  // namespace arpg::platform
