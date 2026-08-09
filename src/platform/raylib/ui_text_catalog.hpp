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
    count,
};

[[nodiscard]] const char* ui_text(UiTextId id) noexcept;
[[nodiscard]] const char* ui_item_base_name(std::uint8_t base_id) noexcept;
[[nodiscard]] const char* ui_material_name(items::MaterialId id) noexcept;
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
