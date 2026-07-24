#pragma once

#include "material_asset_types.hpp"

#include <array>
#include <cstddef>

namespace arpg::platform {

enum class UiMaterialElement : std::uint8_t {
    hud_panel,
    hud_health_track,
    hud_health_fill,
    hud_barrier_track,
    hud_barrier_fill,
    hud_resource_track,
    hud_resource_fill,
    hud_status_slow,
    hud_status_corrosion,
    hud_status_invulnerable,
    hud_objective_panel,
    hud_navigation_panel,
    hud_notice,
    hud_notice_abyss,
    hud_skill_empty,
    hud_skill_ready,
    hud_skill_cooldown,
    inventory_panel_equipment,
    inventory_panel_grid,
    inventory_panel_detail,
    inventory_tab_idle,
    inventory_tab_active,
    inventory_slot_idle,
    inventory_slot_selected,
    inventory_button_idle,
    inventory_button_active,
    inventory_button_disabled,
    skill_panel,
    skill_slot_empty,
    skill_slot_ready,
    skill_slot_selected,
    skill_slot_support,
    pause_panel,
    pause_row_idle,
    pause_row_selected,
    pause_footer,
    warning_modal,
    label_plate,
    reinforcement_confirm,
    reinforcement_cancel,
    count,
};

inline constexpr std::array<MaterialSpriteId,
    static_cast<std::size_t>(UiMaterialElement::count)> kUiMaterialSprites{{
    MaterialSpriteId::ui_hud_panel,
    MaterialSpriteId::ui_hud_health_track,
    MaterialSpriteId::ui_hud_health_fill,
    MaterialSpriteId::ui_hud_barrier_track,
    MaterialSpriteId::ui_hud_barrier_fill,
    MaterialSpriteId::ui_hud_resource_track,
    MaterialSpriteId::ui_hud_resource_fill,
    MaterialSpriteId::ui_hud_status_slow,
    MaterialSpriteId::ui_hud_status_corrosion,
    MaterialSpriteId::ui_hud_status_invulnerable,
    MaterialSpriteId::ui_hud_objective_panel,
    MaterialSpriteId::ui_hud_navigation_panel,
    MaterialSpriteId::ui_hud_notice,
    MaterialSpriteId::ui_hud_notice_abyss,
    MaterialSpriteId::ui_hud_skill_empty,
    MaterialSpriteId::ui_hud_skill_ready,
    MaterialSpriteId::ui_hud_skill_cooldown,
    MaterialSpriteId::ui_inventory_panel_equipment,
    MaterialSpriteId::ui_inventory_panel_grid,
    MaterialSpriteId::ui_inventory_panel_detail,
    MaterialSpriteId::ui_inventory_tab_idle,
    MaterialSpriteId::ui_inventory_tab_active,
    MaterialSpriteId::ui_inventory_slot_idle,
    MaterialSpriteId::ui_inventory_slot_selected,
    MaterialSpriteId::ui_inventory_button_idle,
    MaterialSpriteId::ui_inventory_button_active,
    MaterialSpriteId::ui_inventory_button_disabled,
    MaterialSpriteId::ui_skill_panel,
    MaterialSpriteId::ui_skill_slot_empty,
    MaterialSpriteId::ui_skill_slot_ready,
    MaterialSpriteId::ui_skill_slot_selected,
    MaterialSpriteId::ui_skill_slot_support,
    MaterialSpriteId::ui_pause_panel,
    MaterialSpriteId::ui_pause_row_idle,
    MaterialSpriteId::ui_pause_row_selected,
    MaterialSpriteId::ui_pause_footer,
    MaterialSpriteId::ui_warning_modal,
    MaterialSpriteId::ui_label_plate,
    MaterialSpriteId::ui_reinforcement_confirm,
    MaterialSpriteId::ui_reinforcement_cancel,
}};

[[nodiscard]] constexpr MaterialSpriteId ui_material_sprite(
    UiMaterialElement element) noexcept {
    const std::size_t index = static_cast<std::size_t>(element);
    return index < kUiMaterialSprites.size()
        ? kUiMaterialSprites[index] : MaterialSpriteId::missing;
}

[[nodiscard]] constexpr std::size_t ui_material_decoded_bytes() noexcept {
    return 2U * 4U * 1024U * 1024U;
}

}  // namespace arpg::platform
