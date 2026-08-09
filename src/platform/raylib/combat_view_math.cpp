#include "combat_view_math.hpp"

#include "combat/room_bounds.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace arpg::platform {
namespace {

bool actor_precedes(
    const ActorDrawItem& lhs,
    const ActorDrawItem& rhs) noexcept {
    if (lhs.position.y != rhs.position.y) {
        return lhs.position.y < rhs.position.y;
    }
    if (lhs.position.z != rhs.position.z) {
        return lhs.position.z < rhs.position.z;
    }
    if (lhs.position.x != rhs.position.x) {
        return lhs.position.x < rhs.position.x;
    }
    return lhs.index < rhs.index;
}

Rgba8 affix_category_color(combat::MonsterAffixId id) noexcept {
    switch (id) {
    case combat::MonsterAffixId::armored:
    case combat::MonsterAffixId::shielding:
        return {144U, 167U, 196U, 255U};
    case combat::MonsterAffixId::burning_ground:
        return {244U, 104U, 57U, 255U};
    case combat::MonsterAffixId::chilling:
        return {87U, 188U, 246U, 255U};
    case combat::MonsterAffixId::chain_lightning:
        return {250U, 223U, 76U, 255U};
    case combat::MonsterAffixId::chaos_corrosion:
        return {179U, 87U, 223U, 255U};
    case combat::MonsterAffixId::mighty:
    case combat::MonsterAffixId::frenzy:
    case combat::MonsterAffixId::swift:
    case combat::MonsterAffixId::multishot:
    case combat::MonsterAffixId::blink_assault:
    case combat::MonsterAffixId::death_blast:
    case combat::MonsterAffixId::count:
        return {224U, 231U, 241U, 255U};
    }
    return {224U, 231U, 241U, 255U};
}

const char* affix_tier_text(combat::MonsterAffixTier tier) noexcept {
    switch (tier) {
    case combat::MonsterAffixTier::m1: return "M1";
    case combat::MonsterAffixTier::m2: return "M2";
    case combat::MonsterAffixTier::m3: return "M3";
    case combat::MonsterAffixTier::count: return "M1";
    }
    return "M1";
}

}  // namespace

CombatCameraView make_combat_camera_view(
    combat::Vec3 interpolated_player,
    float width,
    float height) noexcept {
    constexpr float kBaselineAspect = 16.0F / 9.0F;
    constexpr float kBaselineVisibleWidth = 24.0F;
    constexpr float kMaximumVisibleWidth = 32.0F;
    constexpr float kVisibleDepth = 11.0F;
    const float aspect = std::isfinite(width) && std::isfinite(height)
            && width > 0.0F && height > 0.0F
        ? width / height
        : kBaselineAspect;

    CombatCameraView view{};
    view.visible_width = std::min(
        combat::room_bounds::width,
        std::clamp(
            kBaselineVisibleWidth * aspect / kBaselineAspect,
            kBaselineVisibleWidth,
            kMaximumVisibleWidth));
    view.visible_depth = std::min(
        combat::room_bounds::depth, kVisibleDepth);

    const auto clamped_center = [](
        float position,
        float minimum,
        float maximum,
        float visible_extent) noexcept {
        if (visible_extent >= maximum - minimum) {
            return (minimum + maximum) * 0.5F;
        }
        const float half_extent = visible_extent * 0.5F;
        return std::clamp(
            position, minimum + half_extent, maximum - half_extent);
    };
    view.center.x = clamped_center(
        interpolated_player.x,
        combat::room_bounds::min_x,
        combat::room_bounds::max_x,
        view.visible_width);
    view.center.y = clamped_center(
        interpolated_player.y,
        combat::room_bounds::min_y,
        combat::room_bounds::max_y,
        view.visible_depth);
    view.center.z = 0.0F;
    return view;
}

dungeon::WorldViewQuery make_world_view_query(
    CombatCameraView camera,
    float width,
    float height,
    std::uint64_t camera_version) noexcept {
    constexpr float kMinimumWorldZ = -1.0F;
    constexpr float kMaximumWorldZ = 32.0F;
    const float half_width = camera.visible_width * 0.5F;
    const float half_depth = camera.visible_depth * 0.5F;
    const int screen_width = std::isfinite(width) && width > 0.0F
        ? static_cast<int>(width) : 0;
    const int screen_height = std::isfinite(height) && height > 0.0F
        ? static_cast<int>(height) : 0;
    return {
        {{camera.center.x - half_width,
             camera.center.y - half_depth, kMinimumWorldZ},
            {camera.center.x + half_width,
             camera.center.y + half_depth, kMaximumWorldZ}},
        screen_width,
        screen_height,
        camera_version,
    };
}

combat::Vec3 interpolate_combat_position(
    combat::Vec3 from,
    combat::Vec3 to,
    float interpolation_alpha) noexcept {
    const float alpha = std::clamp(interpolation_alpha, 0.0F, 1.0F);
    return {
        from.x + (to.x - from.x) * alpha,
        from.y + (to.y - from.y) * alpha,
        from.z + (to.z - from.z) * alpha,
    };
}

ScreenProjection project_combat_position(
    combat::Vec3 position,
    CombatCameraView view,
    float width,
    float height) noexcept {
    constexpr float kDefaultVisibleWidth = 24.0F;
    constexpr float kDefaultVisibleDepth = 11.0F;
    const float visible_width = view.visible_width > 0.0F
        ? view.visible_width : kDefaultVisibleWidth;
    const float visible_depth = view.visible_depth > 0.0F
        ? view.visible_depth : kDefaultVisibleDepth;
    const float depth = (position.y
        - (view.center.y - visible_depth * 0.5F)) / visible_depth;
    const float visual_depth = std::clamp(depth, 0.0F, 1.0F);
    const float scale = 0.70F + 0.30F * visual_depth;
    const float ground_y = height * (0.38F + 0.50F * depth);
    const float horizontal = (position.x - view.center.x) / visible_width;
    return {
        width * 0.50F + horizontal * width * 0.92F * scale,
        ground_y - position.z * 70.0F * scale,
        ground_y,
        scale,
    };
}

ScreenProjection project_combat_position(
    combat::Vec3 position,
    float width,
    float height) noexcept {
    const float depth = std::clamp(
        (position.y - combat::room_bounds::min_y)
            / combat::room_bounds::depth,
        0.0F, 1.0F);
    const float scale = 0.70F + 0.30F * depth;
    const float ground_y = height * (0.38F + 0.50F * depth);
    return {
        width * 0.50F + position.x
            * (width * 0.46F / combat::room_bounds::max_x) * scale,
        ground_y - position.z * 70.0F * scale,
        ground_y,
        scale,
    };
}

void sort_actor_draw_items(
    std::array<ActorDrawItem, 4>& items) noexcept {
    for (std::size_t index = 1; index < items.size(); ++index) {
        const ActorDrawItem value = items[index];
        std::size_t insertion = index;
        while (insertion > 0
               && actor_precedes(value, items[insertion - 1])) {
            items[insertion] = items[insertion - 1];
            --insertion;
        }
        items[insertion] = value;
    }
}

Rgba8 monster_ecology_color(dungeon::DungeonElement ecology) noexcept {
    switch (ecology) {
    case dungeon::DungeonElement::fire: return {236U, 92U, 54U, 255U};
    case dungeon::DungeonElement::water: return {64U, 156U, 236U, 255U};
    case dungeon::DungeonElement::lightning: return {236U, 218U, 72U, 255U};
    case dungeon::DungeonElement::chaos: return {154U, 76U, 210U, 255U};
    }
    return {154U, 76U, 210U, 255U};
}

MonsterVisual monster_visual(
    combat::MonsterId id,
    combat::MonsterAiPhase phase,
    dungeon::DungeonElement ecology) noexcept {
    MonsterVisual visual{};
    visual.accent = monster_ecology_color(ecology);
    visual.warning = {255U, 106U, 92U, 255U};
    switch (id) {
    case combat::MonsterId::fire_bomber:
        visual = {{195U, 65U, 54U, 255U}, visual.accent, visual.warning,
            MonsterShapeId::bomber, "BOMBER", MonsterWarningMode::none};
        break;
    case combat::MonsterId::fire_charger:
        visual = {{173U, 79U, 49U, 255U}, visual.accent, visual.warning,
            MonsterShapeId::charger, "CHARGER", MonsterWarningMode::none};
        break;
    case combat::MonsterId::water_bulwark:
        visual = {{54U, 117U, 173U, 255U}, visual.accent, visual.warning,
            MonsterShapeId::bulwark, "BULWARK", MonsterWarningMode::none};
        break;
    case combat::MonsterId::water_support:
        visual = {{72U, 142U, 190U, 255U}, visual.accent, visual.warning,
            MonsterShapeId::support, "SUPPORT", MonsterWarningMode::none};
        break;
    case combat::MonsterId::lightning_shooter:
        visual = {{191U, 171U, 58U, 255U}, visual.accent, visual.warning,
            MonsterShapeId::shooter, "SHOOTER", MonsterWarningMode::none};
        break;
    case combat::MonsterId::lightning_dasher:
        visual = {{209U, 188U, 65U, 255U}, visual.accent, visual.warning,
            MonsterShapeId::dasher, "DASHER", MonsterWarningMode::none};
        break;
    case combat::MonsterId::chaos_chaser:
        visual = {{121U, 67U, 173U, 255U}, visual.accent, visual.warning,
            MonsterShapeId::chaser, "CHASER", MonsterWarningMode::none};
        break;
    case combat::MonsterId::chaos_hazard:
        visual = {{145U, 70U, 183U, 255U}, visual.accent, visual.warning,
            MonsterShapeId::hazard_caster, "HAZARD", MonsterWarningMode::none};
        break;
    case combat::MonsterId::count:
        break;
    }

    const bool priority = id == combat::MonsterId::fire_bomber
        || id == combat::MonsterId::fire_charger
        || id == combat::MonsterId::lightning_dasher
        || id == combat::MonsterId::chaos_hazard;
    const bool direct_attacker = id != combat::MonsterId::water_support
        && id != combat::MonsterId::count;
    visual.priority_warning = priority;
    if (direct_attacker && phase == combat::MonsterAiPhase::telegraph) {
        visual.warning_mode = MonsterWarningMode::telegraph;
    } else if (direct_attacker && phase == combat::MonsterAiPhase::active) {
        visual.warning_mode = MonsterWarningMode::active;
    }
    return visual;
}

MonsterLabelTextStyle monster_label_text_style(float viewport_scale) noexcept {
    const float scale = std::clamp(viewport_scale, 1.0F, 1.5F);
    const int font_size = static_cast<int>(std::ceil(16.0F * scale));
    return {font_size, font_size, font_size, 0};
}

MonsterPresentationPlan monster_presentation_plan(
    std::size_t label_lane, bool draw_debug) noexcept {
    return {-128.0F, label_lane % 4U, true, draw_debug, draw_debug};
}

AffixBadge monster_affix_badge(combat::MonsterAffixInstance affix) noexcept {
    const combat::MonsterAffixDefinition* const definition =
        combat::monster_affix_definition(affix.id);
    if (definition == nullptr) {
        return {"?", affix_tier_text(affix.tier),
            combat::MonsterAffixDanger::low, {224U, 231U, 241U, 255U}};
    }
    return {definition->short_name.data(), affix_tier_text(affix.tier),
        definition->danger, affix_category_color(affix.id)};
}

AffixOutline monster_affix_outline(
    combat::MonsterAffixInstance affix,
    std::uint64_t tick) noexcept {
    const AffixBadge badge = monster_affix_badge(affix);
    if (badge.danger != combat::MonsterAffixDanger::high) {
        return {badge.color, 255U};
    }
    const std::uint64_t phase = tick % 30U;
    const std::uint64_t ramp = phase <= 15U ? phase : 30U - phase;
    return {{255U, 78U, 78U, 255U},
        static_cast<std::uint8_t>(160U + ramp * 6U)};
}

bool blink_affix_warning_visible(
    const combat::MonsterSnapshot& monster) noexcept {
    return monster.affix_warning == combat::MonsterAffixWarning::blink
        && monster.affix_warning_ticks != 0U;
}

float blink_affix_warning_actor_radius(
    const combat::MonsterSnapshot& monster) noexcept {
    return blink_affix_warning_visible(monster) ? 16.0F : 0.0F;
}

float blink_affix_warning_ground_radius(
    const combat::MonsterSnapshot& monster) noexcept {
    return blink_affix_warning_visible(monster) ? 28.0F : 0.0F;
}

bool monster_visible(const combat::MonsterSnapshot& monster) noexcept {
    return monster.active
        && monster.ai_phase != combat::MonsterAiPhase::defeated;
}

float player_hp_ratio(const combat::PlayerSnapshot& player) noexcept {
    if (player.max_hp <= 0) {
        return 0.0F;
    }
    return std::clamp(static_cast<float>(player.hp)
            / static_cast<float>(player.max_hp),
        0.0F, 1.0F);
}

HazardVisualMode hazard_visual_mode(
    const combat::HazardSnapshot& hazard) noexcept {
    if (!hazard.active) {
        return HazardVisualMode::hidden;
    }
    return hazard.telegraph_ticks != 0U
        ? HazardVisualMode::telegraph
        : hazard.active_ticks != 0U
            ? HazardVisualMode::active : HazardVisualMode::hidden;
}

bool uses_generic_hazard_pass(
    const combat::HazardSnapshot& hazard) noexcept {
    return hazard.active
        && hazard.source == combat::HazardSource::monster;
}

Rgba8 hazard_color(combat::HazardKind kind) noexcept {
    switch (kind) {
    case combat::HazardKind::native: return {190U, 73U, 229U, 150U};
    case combat::HazardKind::burning: return {242U, 92U, 54U, 210U};
    case combat::HazardKind::chain_lightning: return {255U, 218U, 72U, 210U};
    case combat::HazardKind::death_blast: return {245U, 68U, 68U, 220U};
    }
    return {190U, 73U, 229U, 150U};
}

ScreenProjection project_projectile_position(
    const combat::ProjectileSnapshot& projectile,
    CombatCameraView view,
    float width,
    float height) noexcept {
    return project_combat_position(projectile.position, view, width, height);
}

ScreenProjection project_projectile_position(
    const combat::ProjectileSnapshot& projectile,
    float width,
    float height) noexcept {
    return project_combat_position(projectile.position, width, height);
}

ScreenProjection project_hazard_center(
    const combat::HazardSnapshot& hazard,
    CombatCameraView view,
    float width,
    float height) noexcept {
    return project_combat_position(hazard.center, view, width, height);
}

ScreenProjection project_hazard_center(
    const combat::HazardSnapshot& hazard,
    float width,
    float height) noexcept {
    return project_combat_position(hazard.center, width, height);
}

}  // namespace arpg::platform
