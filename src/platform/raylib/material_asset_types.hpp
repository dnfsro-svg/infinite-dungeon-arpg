#pragma once

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <limits>

namespace arpg::platform {

struct MaterialScreenQuad final {
    Vector2 top_left{};
    Vector2 bottom_left{};
    Vector2 bottom_right{};
    Vector2 top_right{};
};

[[nodiscard]] inline bool valid_material_screen_quad_geometry(
    MaterialScreenQuad quad) noexcept {
    const Vector2 points[] = {
        quad.top_left, quad.bottom_left, quad.bottom_right, quad.top_right};
    double maximum_edge_component = 0.0;
    for (std::size_t index{}; index < 4U; ++index) {
        const Vector2 point = points[index];
        const Vector2 next = points[(index + 1U) % 4U];
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
        maximum_edge_component = std::max(maximum_edge_component,
            std::abs(static_cast<double>(next.x)
                - static_cast<double>(point.x)));
        maximum_edge_component = std::max(maximum_edge_component,
            std::abs(static_cast<double>(next.y)
                - static_cast<double>(point.y)));
    }
    const double tolerance = static_cast<double>(
        std::numeric_limits<float>::epsilon())
        * maximum_edge_component * maximum_edge_component * 16.0;
    const auto orientation = [](Vector2 first, Vector2 second,
                                 Vector2 third) noexcept {
        const double first_x = static_cast<double>(second.x)
            - static_cast<double>(first.x);
        const double first_y = static_cast<double>(second.y)
            - static_cast<double>(first.y);
        const double second_x = static_cast<double>(third.x)
            - static_cast<double>(first.x);
        const double second_y = static_cast<double>(third.y)
            - static_cast<double>(first.y);
        return first_x * second_y - first_y * second_x;
    };
    const double first = orientation(points[0U], points[1U], points[2U]);
    if (std::abs(first) <= tolerance) return false;
    const bool positive = first > 0.0;
    for (std::size_t index = 1U; index < 4U; ++index) {
        const double cross = orientation(points[index],
            points[(index + 1U) % 4U], points[(index + 2U) % 4U]);
        if (std::abs(cross) <= tolerance || (cross > 0.0) != positive) {
            return false;
        }
    }
    return true;
}

enum class MaterialAtlasId : std::uint8_t {
    environment,
    actors,
    effects_ui,
    player_locomotion,
    player_combo_a,
    player_combo_b,
    player_reaction,
    player_air,
    fire_environment,
    fire_bomber,
    fire_charger,
    water_environment,
    water_bulwark,
    water_support,
    lightning_environment,
    lightning_shooter,
    lightning_dasher,
    chaos_environment,
    chaos_chaser,
    chaos_hazard,
    items_ui,
    ui_material,
    fire_room_background,
    water_room_background,
    lightning_room_background,
    chaos_room_background,
    element_doors,
    skill_draw_slash,
    skill_storm_swords,
    count,
};

enum class MaterialLayer : std::uint8_t {
    shadow,
    back_effect,
    body,
    weapon,
    front_effect,
    count,
};

enum class MaterialClass : std::uint8_t {
    environment,
    actor,
    effect,
    ui,
    loot,
    count,
};

enum class MaterialEcology : std::uint8_t {
    common,
    fire,
    water,
    lightning,
    chaos,
    skill,
    count,
};

enum class AnimationEventKind : std::uint8_t {
    none,
    hit,
    footstep,
    spawn_effect,
    recovery,
};

enum class AnimationClipId : std::uint8_t {
    player_idle,
    player_move,
    player_j1,
    player_j2,
    player_j3,
    player_launcher,
    player_jump,
    monster_idle,
    monster_move,
    monster_attack,
    monster_hurt,
    monster_death,
    count,
};

enum class MaterialSpriteId : std::uint16_t {
    missing,
    player_idle,
    player_move,
    player_j1,
    player_j2,
    player_j3,
    player_launcher,
    player_jump_rise,
    player_jump_fall,
    player_air_j,
    player_landing,
    player_hurt,
    player_dead,
    fire_bomber_idle,
    fire_bomber_move,
    fire_bomber_telegraph,
    fire_bomber_active,
    fire_bomber_recovery,
    fire_bomber_cooldown,
    fire_bomber_defeated,
    fire_charger_idle,
    fire_charger_move,
    fire_charger_telegraph,
    fire_charger_active,
    fire_charger_recovery,
    fire_charger_cooldown,
    fire_charger_defeated,
    water_bulwark_idle,
    water_bulwark_move,
    water_bulwark_telegraph,
    water_bulwark_active,
    water_bulwark_recovery,
    water_bulwark_cooldown,
    water_bulwark_defeated,
    water_support_idle,
    water_support_move,
    water_support_telegraph,
    water_support_active,
    water_support_recovery,
    water_support_cooldown,
    water_support_defeated,
    lightning_shooter_idle,
    lightning_shooter_move,
    lightning_shooter_telegraph,
    lightning_shooter_active,
    lightning_shooter_recovery,
    lightning_shooter_cooldown,
    lightning_shooter_defeated,
    lightning_dasher_idle,
    lightning_dasher_move,
    lightning_dasher_telegraph,
    lightning_dasher_active,
    lightning_dasher_recovery,
    lightning_dasher_cooldown,
    lightning_dasher_defeated,
    chaos_chaser_idle,
    chaos_chaser_move,
    chaos_chaser_telegraph,
    chaos_chaser_active,
    chaos_chaser_recovery,
    chaos_chaser_cooldown,
    chaos_chaser_defeated,
    chaos_hazard_idle,
    chaos_hazard_move,
    chaos_hazard_telegraph,
    chaos_hazard_active,
    chaos_hazard_recovery,
    chaos_hazard_cooldown,
    chaos_hazard_defeated,
    environment_floor_fire,
    environment_floor_water,
    environment_floor_lightning,
    environment_floor_chaos,
    environment_door_fire,
    environment_door_water,
    environment_door_lightning,
    environment_door_chaos,
    environment_hole,
    fire_wall,
    fire_torch,
    fire_chain,
    fire_banner,
    fire_weapon_rack,
    fire_bone_pile,
    fire_breakable_crate,
    fire_solid_brazier,
    water_wall,
    water_hole,
    water_lantern,
    water_coral,
    water_grate,
    lightning_wall,
    lightning_hole,
    lightning_arc_lamp,
    lightning_capacitor_bank,
    lightning_grounding_rod,
    chaos_wall,
    chaos_hole,
    chaos_rift_lantern,
    chaos_anomaly_condenser,
    chaos_warning_obelisk,
    effect_fire,
    effect_water,
    effect_lightning,
    effect_chaos,
    effect_hit_spark,
    effect_launcher_trail,
    effect_landing_dust,
    effect_affix_aura,
    item_weapon,
    item_helmet,
    item_chest,
    item_gloves,
    item_boots,
    item_accessory,
    loot_rarity_normal,
    loot_rarity_magic,
    loot_rarity_rare,
    loot_rarity_abyss,
    skill_stone_active,
    skill_stone_support,
    material_transmute,
    material_augment,
    material_regal,
    material_chaos,
    material_exalt,
    material_annul,
    material_divine,
    material_scour,
    material_directed,
    material_reinforcement,
    material_coupon_6,
    material_coupon_9,
    material_coupon_12,
    material_coupon_15,
    health_potion,
    bag_frame_nw,
    bag_frame_ne,
    bag_frame_sw,
    bag_frame_se,
    ui_hud_panel,
    ui_hud_health_track,
    ui_hud_health_fill,
    ui_hud_barrier_track,
    ui_hud_barrier_fill,
    ui_hud_resource_track,
    ui_hud_resource_fill,
    ui_hud_status_slow,
    ui_hud_status_corrosion,
    ui_hud_status_invulnerable,
    ui_hud_objective_panel,
    ui_hud_navigation_panel,
    ui_hud_notice,
    ui_hud_notice_abyss,
    ui_hud_skill_empty,
    ui_hud_skill_ready,
    ui_hud_skill_cooldown,
    ui_inventory_panel_equipment,
    ui_inventory_panel_grid,
    ui_inventory_panel_detail,
    ui_inventory_tab_idle,
    ui_inventory_tab_active,
    ui_inventory_slot_idle,
    ui_inventory_slot_selected,
    ui_inventory_button_idle,
    ui_inventory_button_active,
    ui_inventory_button_disabled,
    ui_skill_panel,
    ui_skill_slot_empty,
    ui_skill_slot_ready,
    ui_skill_slot_selected,
    ui_skill_slot_support,
    ui_pause_panel,
    ui_pause_row_idle,
    ui_pause_row_selected,
    ui_pause_footer,
    ui_warning_modal,
    ui_label_plate,
    ui_reinforcement_confirm,
    ui_reinforcement_cancel,
    skill_icon_draw_slash,
    skill_icon_storm_swords,
    count,
};

struct MaterialFrameDefinition final {
    MaterialSpriteId id{MaterialSpriteId::missing};
    MaterialAtlasId atlas{MaterialAtlasId::actors};
    Rectangle source{};
    Vector2 foot_anchor{};
    std::uint16_t duration_ms{};
    Vector2 weapon_anchor{};
    MaterialLayer layer{MaterialLayer::body};
    MaterialClass material_class{MaterialClass::actor};
    std::uint32_t perceptual_hash{};
};

struct AnimationEventDefinition final {
    std::uint16_t frame_index{};
    AnimationEventKind kind{AnimationEventKind::none};
};

struct AnimationClipDefinition final {
    AnimationClipId id{AnimationClipId::player_idle};
    MaterialSpriteId resource_id{MaterialSpriteId::missing};
    std::uint16_t first_frame{};
    std::uint16_t frame_count{};
    std::uint16_t minimum_frames{};
    std::uint8_t frames_per_second{24U};
    std::uint16_t first_event{};
    std::uint16_t event_count{};
    MaterialEcology ecology{MaterialEcology::common};
};

}  // namespace arpg::platform
