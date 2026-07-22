#pragma once

#include "active_skill_assets.hpp"
#include "active_skill_view.hpp"
#include "combat/combat_types.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

struct DrawSlashVisualPlan final {
    bool visible{};
    combat::Vec3 center{};
    combat::Facing facing{combat::Facing::right};
    float opacity{};
    bool use_atlas{};
    ActiveSkillAtlasId atlas{ActiveSkillAtlasId::draw_slash};
    std::size_t atlas_frame{};
};

enum class StormSwordBand : std::uint8_t {
    ground,
    aerial,
};

struct StormSwordVisual final {
    bool visible{};
    StormSwordBand band{StormSwordBand::ground};
    float angle_radians{};
    bool highlighted{};
};

struct StormSwordsVisualPlan final {
    bool visible{};
    bool finisher_visible{};
    combat::Vec3 center{};
    std::array<StormSwordVisual, 24> swords{};
    std::size_t sword_count{};
    float finisher_opacity{};
    bool use_atlas{};
    ActiveSkillAtlasId atlas{ActiveSkillAtlasId::storm_swords};
    std::size_t atlas_frame{};
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
    [[nodiscard]] bool initialize_resources() noexcept;
    void shutdown_resources() noexcept;
    void draw_world(const combat::CombatSnapshot& snapshot,
        const combat::CombatEvent* last_event,
        float width, float height) const noexcept;
    void draw_hud(const ActiveSkillHudModel& model,
        const ActiveSkillHudLayout& layout,
        Font hud_font, bool hud_font_ready) const noexcept;

private:
    ActiveSkillAssets assets_{};
};

}  // namespace arpg::platform
