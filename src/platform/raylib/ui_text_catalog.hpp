#pragma once

#include "combat/combat_types.hpp"
#include "combat/monster_affix_types.hpp"
#include "items/item_catalog.hpp"
#include "passives/passive_tree_types.hpp"
#include "platform/settings/settings_types.hpp"
#include "skills/active_skill_types.hpp"

#include <cstdint>

namespace arpg::platform {

enum class UiTextId : std::uint16_t {
    pause_title, pause_continue, pause_settings, pause_quit,
    settings_title, settings_audio, settings_display, settings_bindings,
    settings_restore_defaults, settings_apply, settings_cancel,
    settings_saved_prefix, settings_unknown,
    hud_health, hud_barrier, hud_experience, hud_level,
    save_saving, save_saved, save_recovered, save_error,
    recovery_title, recovery_invalid_slots,
    inventory_title, equipment_title, item_detail_title,
    passive_title, passive_points, passive_close,
    debug_ready, debug_unavailable,
    unknown,
    settings_invalid_binding, settings_invalid, settings_revision_limit,
    settings_preview_failed, settings_save_failed_retry, settings_save_busy,
    settings_rollback_failed, settings_saved, settings_reloaded_elsewhere,
    settings_storage_conflict, settings_recovered_defaults,
    hud_experience_maxed,
    recovery_dual_slot_instruction,
    inventory_page_skill_stones,
    inventory_page_materials,
    inventory_empty_title,
    inventory_empty_hint,
    inventory_all_equipped_title,
    inventory_all_equipped_hint,
    inventory_filter_empty_title,
    inventory_filter_empty_hint,
    inventory_skill_empty,
    inventory_skill_equip_hint,
    inventory_skill_swap_hint,
    inventory_skill_remove_hint,
    inventory_comparison_title,
    inventory_no_change,
    inventory_material_selected_format,
    inventory_recipe_action_format,
    inventory_recipe_danger,
    inventory_slot_filter_format,
    inventory_rarity_filter_format,
    inventory_context_hint,
    inventory_action_remove,
    inventory_filter_all,
    inventory_reinforcement_success_format,
    inventory_reinforcement_failure_format,
    inventory_reinforcement_destroyed_format,
    inventory_coupon_success_format,
    inventory_material_rejected,
    inventory_material_target_hint_format,
    inventory_directed_material_hint,
    recovery_conflicting_slots_instruction,
    recovery_confirm_new_run_instruction,
    recovery_new_run_submitted_instruction,
    save_faulted_title,
    save_faulted_instruction,
    save_faulted_error_format,
    save_directory_busy_title,
    save_directory_busy_instruction,
    inventory_level_locked_format,
    count,
};

[[nodiscard]] const char* ui_text(UiTextId id) noexcept;
[[nodiscard]] const char* ui_item_base_name(std::uint8_t base_id) noexcept;
[[nodiscard]] const char* ui_material_name(items::MaterialId id) noexcept;
[[nodiscard]] const char* ui_material_description(
    items::MaterialId id) noexcept;
[[nodiscard]] const char* ui_item_slot_name(items::ItemSlot id) noexcept;
[[nodiscard]] const char* ui_item_rarity_name(items::ItemRarity id) noexcept;
[[nodiscard]] const char* ui_item_affix_name(std::uint16_t affix_id) noexcept;
[[nodiscard]] const char* ui_passive_node_name(
    passives::PassiveNodeId id) noexcept;
[[nodiscard]] const char* ui_monster_name(combat::MonsterId id) noexcept;
[[nodiscard]] const char* ui_monster_affix_name(
    combat::MonsterAffixId id) noexcept;
[[nodiscard]] const char* ui_monster_affix_short_name(
    combat::MonsterAffixId id) noexcept;
[[nodiscard]] const char* ui_setting_action_name(
    settings::SettingAction id) noexcept;
[[nodiscard]] const char* ui_skill_name(skills::ActiveSkillId id) noexcept;

using UiTextVisitor = void (*)(const char*, void*) noexcept;
void visit_ui_text_catalog(UiTextVisitor visitor, void* context) noexcept;

}  // namespace arpg::platform
