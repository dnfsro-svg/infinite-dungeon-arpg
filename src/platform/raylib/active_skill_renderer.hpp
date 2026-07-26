#pragma once

#include "active_skill_assets.hpp"
#include "active_skill_view.hpp"
#include "combat/combat_types.hpp"
#include "material_pack.hpp"

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

enum class ActiveSkillVisualMode : std::uint8_t {
    none,
    material,
    procedural_fallback,
};

struct ActiveSkillEffectPlan final {
    ActiveSkillVisualMode mode{ActiveSkillVisualMode::none};
    combat::Vec3 player_position{};
    combat::Vec3 effect_center{};
    MaterialAtlasId atlas{MaterialAtlasId::count};
    std::size_t atlas_frame{};
    bool suppress_base_player{};
    std::size_t procedural_main_visual_count{};
    DrawSlashVisualPlan draw_slash{};
    StormSwordsVisualPlan storm_swords{};
    float screen_flash_alpha{};
};

[[nodiscard]] std::size_t active_skill_visual_frame_index(
    skills::ActiveSkillId id, std::uint16_t elapsed_ticks) noexcept;

[[nodiscard]] ActiveSkillEffectPlan make_active_skill_effect_plan(
    const combat::CombatSnapshot& snapshot,
    const combat::CombatEvent* last_event,
    bool material_ready) noexcept;

class ActiveSkillRenderer final {
public:
    [[nodiscard]] bool assets_ready() const noexcept;
    void draw_world(const ActiveSkillEffectPlan& plan,
        const MaterialPack& material_pack,
        float width, float height) const noexcept;
    void draw_hud(const ActiveSkillHudModel& model,
        const ActiveSkillHudLayout& layout,
        Font hud_font, bool hud_font_ready,
        const MaterialPack& material_pack) const noexcept;

};

struct ActiveSkillCooldownOverlayPlan final {
    bool visible{};
    Rectangle bounds{};
};

[[nodiscard]] ActiveSkillCooldownOverlayPlan
make_active_skill_cooldown_overlay(
    Rectangle bounds, float cooldown_ratio, bool empty) noexcept;

}  // namespace arpg::platform
