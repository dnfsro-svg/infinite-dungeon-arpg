#include "host_validation_stage11d.hpp"

#include "dungeon_runtime.hpp"
#include "pause_menu_state.hpp"
#include "platform/settings/settings_types.hpp"
#include "raylib_host.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace arpg::platform::host_validation {

// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN evidence_semantics
namespace {

[[nodiscard]] bool stage11d_view_has_rarity(
    const GroundLootView& view, const dungeon::DungeonSnapshot& snapshot,
    items::ItemRarity rarity, bool abyss) noexcept {
    for (std::size_t label_index = 0U; label_index < view.count; ++label_index) {
        const auto& label = view.labels[label_index];
        for (std::size_t item_index = 0U;
             item_index < snapshot.ground_item_count; ++item_index) {
            const auto& item = snapshot.ground_items[item_index];
            if (item.ordinal == label.ordinal && item.rarity == rarity
                    && (item.source == dungeon::GroundItemSource::abyss_chest)
                        == abyss) {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] bool stage11d_view_has_abyss_ordinal(
    const GroundLootView& view, std::uint16_t ordinal) noexcept {
    for (std::size_t index = 0U; index < view.count; ++index) {
        if (view.labels[index].ordinal == ordinal
                && view.labels[index].abyss) return true;
    }
    return false;
}

}  // namespace

void stage11d_record_semantics(Stage11DLootValidationState& state,
    const dungeon::DungeonSnapshot& snapshot,
    const items::ItemOwnershipState* ownership,
    const GroundLootView& view, HudNoticeView notices) noexcept {
    state.view = view;
    state.notices = notices;
    state.snapshot_item_count = snapshot.ground_item_count;
    state.snapshot_item_ids.fill(0U);
    for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
        const auto& item = snapshot.ground_items[index];
        state.snapshot_item_ids[index] = item.item_id;
        if (item.source == dungeon::GroundItemSource::abyss_chest
                && stage11d_view_has_abyss_ordinal(view, item.ordinal)) {
            state.abyss_item_id = item.item_id;
            state.abyss_rarity = item.rarity;
        } else if (item.rarity == items::ItemRarity::normal) {
            state.normal_item_id = item.item_id;
        } else if (item.rarity == items::ItemRarity::magic) {
            state.magic_item_id = item.item_id;
        } else if (item.rarity == items::ItemRarity::rare) {
            state.rare_item_id = item.item_id;
        }
    }
    state.inventory_item_count = 0U;
    state.inventory_item_ids.fill(0U);
    if (ownership != nullptr) {
        for (const auto& item : ownership->items) {
            if (state.inventory_item_count >= state.inventory_item_ids.size()) break;
            state.inventory_item_ids[state.inventory_item_count++] = item.id;
        }
    }
}

[[nodiscard]] bool stage11d_target_visible(
    const RaylibHostConfig& config, const dungeon::DungeonSnapshot& snapshot,
    const PauseMenuState& pause_menu, const DungeonRenderStatus& status,
    settings::LootFilterMode mode, const GroundLootView& view,
    HudNoticeView notices, Stage11DLootValidationState& state,
    int screen_width, int screen_height) noexcept {
    using Scenario = Stage11DLootValidationScenario;
    const Scenario scenario = config.stage11d_loot_validation;
    if (scenario == Scenario::none) return false;
    state.ordinary_normal_count = 0U;
    state.ordinary_magic_count = 0U;
    state.ordinary_rare_count = 0U;
    state.remaining_targets = snapshot.remaining_targets;
    state.initial_monster_count = (std::max)(
        state.initial_monster_count, snapshot.initial_monster_count);
    state.max_defeated_monsters = (std::max)(
        state.max_defeated_monsters, snapshot.defeated_monster_count);
    if (snapshot.initial_monster_count != 0U) {
        state.max_remaining_targets = (std::max)(
            state.max_remaining_targets, snapshot.remaining_targets);
        state.min_remaining_targets = (std::min)(
            state.min_remaining_targets, snapshot.remaining_targets);
    }
    if (snapshot.monster_generator_version != 0U) {
        state.monster_generator_version = snapshot.monster_generator_version;
        state.monster_blueprint_hash = snapshot.monster_blueprint_hash;
    }
    state.live_inventory_count = snapshot.inventory_count;
    state.last_phase = snapshot.phase;
    if (snapshot.combat.has_value()) {
        const std::int32_t current_hp = snapshot.combat->player.hp;
        const std::int32_t current_max_hp = snapshot.combat->player.max_hp;
        state.player_damage_observed = state.player_damage_observed
            || (state.player_hp_sampled
                && current_max_hp == state.player_max_hp
                && current_hp < state.player_hp);
        state.player_hp = current_hp;
        state.player_max_hp = current_max_hp;
        state.player_hp_sampled = true;
    } else {
        state.player_hp = 0;
        state.player_max_hp = 0;
        state.player_hp_sampled = false;
    }
    if (status.loot_pickup.valid) {
        state.pickup_item_id = status.loot_pickup.item_id;
        state.pickup_commit_generation = status.loot_pickup.commit_generation;
    }
    if (notices.primary.kind == HudNoticeKind::loot_pickup
            || notices.secondary.kind == HudNoticeKind::loot_pickup) {
        const HudLayout layout = make_hud_layout(
            screen_width, screen_height, false);
        state.pickup_notice_rect = notices.primary.kind
                == HudNoticeKind::loot_pickup
            ? layout.primary_notice : layout.secondary_notice;
    }
    if (snapshot.combat.has_value()) {
        for (std::size_t index = 0U;
             index < snapshot.combat->monster_count; ++index) {
            const auto& monster = snapshot.combat->monsters[index];
            if (monster.active && monster.hp > 0
                    && monster.hp < monster.max_hp) {
                state.monster_damage_observed = true;
            }
        }
    }
    for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
        const auto& item = snapshot.ground_items[index];
        if (item.source != dungeon::GroundItemSource::monster_drop) continue;
        std::size_t distance_index = 0U;
        if (item.rarity == items::ItemRarity::normal) {
            ++state.ordinary_normal_count;
            state.observed_normal_item_id = item.item_id;
            state.observed_normal_ordinal = item.ordinal;
        } else if (item.rarity == items::ItemRarity::magic) {
            distance_index = 1U;
            ++state.ordinary_magic_count;
            state.observed_magic_item_id = item.item_id;
            state.observed_magic_ordinal = item.ordinal;
        } else if (item.rarity == items::ItemRarity::rare) {
            distance_index = 2U;
            ++state.ordinary_rare_count;
            state.observed_rare_item_id = item.item_id;
            state.observed_rare_ordinal = item.ordinal;
        }
        if (snapshot.combat.has_value()) {
            const float x = item.position.x
                - snapshot.combat->player.position.x;
            const float y = item.position.y
                - snapshot.combat->player.position.y;
            state.observed_drop_distance_milli[distance_index] =
                static_cast<std::uint32_t>(std::lround(
                    std::sqrt(x * x + y * y) * 1000.0F));
        }
    }
    state.max_ordinary_normal_count = (std::max)(
        state.max_ordinary_normal_count, state.ordinary_normal_count);
    state.max_ordinary_magic_count = (std::max)(
        state.max_ordinary_magic_count, state.ordinary_magic_count);
    state.max_ordinary_rare_count = (std::max)(
        state.max_ordinary_rare_count, state.ordinary_rare_count);
    state.max_ground_item_count = (std::max)(state.max_ground_item_count,
        static_cast<std::uint32_t>(snapshot.ground_item_count));
    state.max_inventory_count = (std::max)(state.max_inventory_count,
        snapshot.inventory_count);
    if (scenario == Scenario::pickup_feedback) {
        const bool notice = notices.primary.kind == HudNoticeKind::loot_pickup
            || notices.secondary.kind == HudNoticeKind::loot_pickup;
        return status.loot_pickup.valid && notice;
    }
    if (scenario == Scenario::rare_only_abyss) {
        for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
            const auto& item = snapshot.ground_items[index];
            if (item.source == dungeon::GroundItemSource::abyss_chest
                    && item.rarity != items::ItemRarity::rare
                    && ground_loot_visible(item, mode)
                    && stage11d_view_has_abyss_ordinal(
                        view, item.ordinal)) {
                return true;
            }
        }
        return false;
    }
    if (!stage11d_has_three_ordinary_rarities(snapshot)) return false;
    if (scenario == Scenario::preview_cancel) {
        if (state.preview_phase == 11U) state.preview_visible_count = view.count;
        if (state.preview_phase < 13U || pause_menu.screen != PauseScreen::closed) {
            return false;
        }
        state.restored_visible_count = view.count;
        return state.preview_visible_count < state.restored_visible_count
            && pause_menu.committed.loot_filter_mode
                == settings::LootFilterMode::show_all
            && pause_menu.draft.loot_filter_mode
                == settings::LootFilterMode::show_all;
    }
    const bool normal = stage11d_view_has_rarity(
        view, snapshot, items::ItemRarity::normal, false);
    const bool magic = stage11d_view_has_rarity(
        view, snapshot, items::ItemRarity::magic, false);
    const bool rare = stage11d_view_has_rarity(
        view, snapshot, items::ItemRarity::rare, false);
    if (scenario == Scenario::show_all) return normal && magic && rare;
    if (scenario == Scenario::magic_or_better) return !normal && magic && rare;
    return scenario == Scenario::rare_only && !normal && !magic && rare;
}

namespace {

[[nodiscard]] const char* stage11d_scenario_name(
    Stage11DLootValidationScenario scenario) noexcept {
    switch (scenario) {
    case Stage11DLootValidationScenario::none: return "none";
    case Stage11DLootValidationScenario::show_all: return "show_all";
    case Stage11DLootValidationScenario::magic_or_better: return "magic_or_better";
    case Stage11DLootValidationScenario::rare_only: return "rare_only";
    case Stage11DLootValidationScenario::rare_only_abyss: return "rare_only_abyss";
    case Stage11DLootValidationScenario::preview_cancel: return "preview_cancel";
    case Stage11DLootValidationScenario::pickup_feedback: return "pickup_feedback";
    }
    return "invalid";
}

}  // namespace

void write_stage11d_loot_validation_summary(const RaylibHostConfig& config,
    const Stage11DLootValidationState& state,
    const PauseMenuState& pause_menu) noexcept {
    if (!config.validation_summary_file.has_value()
            || config.stage11d_loot_validation
                == Stage11DLootValidationScenario::none) return;
    try {
        std::ofstream stream(*config.validation_summary_file,
            std::ios::out | std::ios::trunc);
        if (!stream) return;
        stream << "scenario=" << stage11d_scenario_name(
            config.stage11d_loot_validation) << '\n'
            << "committed_mode=" << static_cast<unsigned>(
                pause_menu.committed.loot_filter_mode) << '\n'
            << "draft_mode=" << static_cast<unsigned>(
                pause_menu.draft.loot_filter_mode) << '\n'
            << "snapshot_count=" << state.snapshot_item_count << '\n'
            << "visible_count=" << state.view.count << '\n'
            << "inventory_count=" << state.inventory_item_count << '\n'
            << "normal_item_id=" << state.normal_item_id << '\n'
            << "magic_item_id=" << state.magic_item_id << '\n'
            << "rare_item_id=" << state.rare_item_id << '\n'
            << "abyss_item_id=" << state.abyss_item_id << '\n'
            << "abyss_rarity=" << static_cast<unsigned>(
                state.abyss_rarity) << '\n'
            << "preview_visible_count=" << state.preview_visible_count << '\n'
            << "restored_visible_count=" << state.restored_visible_count << '\n'
            << "abyss_claimed=" << (state.abyss_claimed ? 1 : 0) << '\n'
            << "progress_phase=" << static_cast<unsigned>(state.last_phase) << '\n'
            << "progress_initial=" << state.initial_monster_count << '\n'
            << "progress_defeated=" << state.max_defeated_monsters << '\n'
            << "progress_remaining=" << static_cast<unsigned>(
                state.remaining_targets) << '\n'
            << "progress_inventory=" << state.live_inventory_count << '\n'
            << "progress_hp=" << state.player_hp << '\n'
            << "progress_max_hp=" << state.player_max_hp << '\n'
            << "progress_rarities=" << state.ordinary_normal_count << ','
                << state.ordinary_magic_count << ','
                << state.ordinary_rare_count << '\n'
            << "max_ground_rarities=" << state.max_ordinary_normal_count << ','
                << state.max_ordinary_magic_count << ','
                << state.max_ordinary_rare_count << '\n'
            << "max_ground_count=" << state.max_ground_item_count << '\n'
            << "max_remaining=" << state.max_remaining_targets << '\n'
            << "min_remaining=" << static_cast<unsigned>(
                state.min_remaining_targets) << '\n'
            << "max_inventory=" << state.max_inventory_count << '\n'
            << "monster_generator_version="
                << state.monster_generator_version << '\n'
            << "monster_blueprint_hash="
                << state.monster_blueprint_hash << '\n'
            << "monster_damage_observed="
                << (state.monster_damage_observed ? 1 : 0) << '\n'
            << "player_damage_observed="
                << (state.player_damage_observed ? 1 : 0) << '\n'
            << "observed_normal=" << state.observed_normal_item_id << ','
                << state.observed_normal_ordinal << '\n'
            << "observed_magic=" << state.observed_magic_item_id << ','
                << state.observed_magic_ordinal << '\n'
            << "observed_rare=" << state.observed_rare_item_id << ','
                << state.observed_rare_ordinal << '\n'
            << "drop_distance_milli="
                << state.observed_drop_distance_milli[0] << ','
                << state.observed_drop_distance_milli[1] << ','
                << state.observed_drop_distance_milli[2] << '\n'
            << "target_ordinal=" << state.target_ordinal << '\n'
            << "pickup_item_id=" << state.pickup_item_id << '\n'
            << "pickup_commit_generation="
                << state.pickup_commit_generation << '\n'
            << "pickup_notice_rect=" << state.pickup_notice_rect.x << ','
                << state.pickup_notice_rect.y << ','
                << state.pickup_notice_rect.width << ','
                << state.pickup_notice_rect.height << '\n'
            << "notice_text=";
        const HudNotice& notice = state.notices.primary.kind
                == HudNoticeKind::loot_pickup
            ? state.notices.primary : state.notices.secondary;
        stream << notice.text.bytes.data() << '\n' << "snapshot_ids=";
        for (std::size_t index = 0U; index < state.snapshot_item_count; ++index) {
            if (index != 0U) stream << ',';
            stream << state.snapshot_item_ids[index];
        }
        stream << '\n' << "inventory_ids=";
        for (std::size_t index = 0U; index < state.inventory_item_count; ++index) {
            if (index != 0U) stream << ',';
            stream << state.inventory_item_ids[index];
        }
        stream << '\n' << "label_rects=";
        for (std::size_t index = 0U; index < state.view.count; ++index) {
            if (index != 0U) stream << ';';
            const auto& label = state.view.labels[index];
            stream << label.ordinal << ',' << label.rect.x << ',' << label.rect.y
                << ',' << label.rect.width << ',' << label.rect.height;
        }
        stream << '\n' << "result=" << (state.captured
            && (config.stage11d_loot_validation
                    != Stage11DLootValidationScenario::rare_only_abyss
                || state.abyss_claimed) ? "pass" : "fail") << '\n';
    } catch (...) {
        TraceLog(LOG_WARNING, "failed to write stage11d loot validation summary");
    }
}
// STAGE11D_LOOT_VALIDATION_SEAM_END evidence_semantics

}  // namespace arpg::platform::host_validation
