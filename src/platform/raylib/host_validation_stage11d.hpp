#pragma once

#include "dungeon/dungeon_types.hpp"
#include "ground_loot_view.hpp"
#include "host_validation_stage10_11.hpp"
#include "hud_layout.hpp"
#include "hud_notice_state.hpp"
#include "items/item_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::settings {
enum class LootFilterMode : std::uint8_t;
struct SettingsData;
}

namespace arpg::platform {
struct DungeonRenderStatus;
struct PauseMenuState;
struct PhysicalKeySnapshot;
struct RaylibHostConfig;

namespace host_validation {

// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN state
struct Stage11DLootValidationState final {
    std::uint32_t injected_frames{};
    std::uint32_t target_presented_frames{};
    std::array<std::uint64_t, dungeon::kGroundDropCapacity> snapshot_item_ids{};
    std::array<std::uint64_t, dungeon::kGroundDropCapacity> inventory_item_ids{};
    GroundLootView view{};
    HudNoticeView notices{};
    std::uint16_t snapshot_item_count{};
    std::uint16_t inventory_item_count{};
    std::uint64_t normal_item_id{};
    std::uint64_t magic_item_id{};
    std::uint64_t rare_item_id{};
    std::uint64_t observed_normal_item_id{};
    std::uint64_t observed_magic_item_id{};
    std::uint64_t observed_rare_item_id{};
    std::uint16_t observed_normal_ordinal{0xFFFFU};
    std::uint16_t observed_magic_ordinal{0xFFFFU};
    std::uint16_t observed_rare_ordinal{0xFFFFU};
    std::uint64_t abyss_item_id{};
    items::ItemRarity abyss_rarity{items::ItemRarity::normal};
    HudRect pickup_notice_rect{};
    std::size_t preview_visible_count{};
    std::size_t restored_visible_count{};
    std::uint8_t preview_phase{};
    std::uint32_t ordinary_normal_count{};
    std::uint32_t ordinary_magic_count{};
    std::uint32_t ordinary_rare_count{};
    std::uint32_t max_ordinary_normal_count{};
    std::uint32_t max_ordinary_magic_count{};
    std::uint32_t max_ordinary_rare_count{};
    std::uint32_t max_ground_item_count{};
    std::uint32_t max_inventory_count{};
    std::array<std::uint32_t, 3> observed_drop_distance_milli{};
    std::uint32_t initial_monster_count{};
    std::uint32_t max_remaining_targets{};
    std::uint32_t max_defeated_monsters{};
    std::uint32_t monster_generator_version{};
    std::uint64_t monster_blueprint_hash{};
    std::uint16_t target_ordinal{0xFFFFU};
    std::uint32_t target_defeated_count{};
    std::uint32_t remaining_targets{};
    std::uint32_t min_remaining_targets{
        (std::numeric_limits<std::uint32_t>::max)()};
    std::uint32_t live_inventory_count{};
    dungeon::RoomPhase last_phase{dungeon::RoomPhase::locked};
    std::int32_t player_hp{};
    std::int32_t player_max_hp{};
    std::uint64_t pickup_item_id{};
    std::uint64_t pickup_commit_generation{};
    bool monster_damage_observed{};
    bool player_damage_observed{};
    bool player_hp_sampled{};
    bool target_visible{};
    bool captured{};
    bool abyss_claim_requested{};
    bool abyss_claimed{};
    bool suspend_injection{};
    bool sweep_cursor_initialized{};
    std::uint8_t sweep_waypoint{};
    Stage10GridRouteState sweep_grid{};
    Stage10ValidationState abyss_ranged{};
};
// STAGE11D_LOOT_VALIDATION_SEAM_END state

[[nodiscard]] bool stage11d_has_three_ordinary_rarities(
    const dungeon::DungeonSnapshot&) noexcept;
[[nodiscard]] std::uint16_t stage11d_priority_monster_ordinal(
    const dungeon::DungeonSnapshot&, Stage11DLootValidationState&) noexcept;
[[nodiscard]] PhysicalKeySnapshot inject_stage11d_physical_edges(
    PhysicalKeySnapshot, const RaylibHostConfig&,
    const settings::SettingsData&, const dungeon::DungeonSnapshot&,
    Stage11DLootValidationState&) noexcept;
[[nodiscard]] bool stage11d_validation_active(
    const RaylibHostConfig&) noexcept;
void observe_stage11d_abyss_claim(Stage11DLootValidationState&,
    const dungeon::DungeonSnapshot&,
    const items::ItemOwnershipState&) noexcept;
[[nodiscard]] bool stage11d_target_visible(const RaylibHostConfig&,
    const dungeon::DungeonSnapshot&, const PauseMenuState&,
    const DungeonRenderStatus&, settings::LootFilterMode,
    const GroundLootView&, HudNoticeView,
    Stage11DLootValidationState&, int, int) noexcept;
void stage11d_record_semantics(Stage11DLootValidationState&,
    const dungeon::DungeonSnapshot&, const items::ItemOwnershipState*,
    const GroundLootView&, HudNoticeView) noexcept;
void write_stage11d_loot_validation_summary(const RaylibHostConfig&,
    const Stage11DLootValidationState&, const PauseMenuState&) noexcept;

}  // namespace host_validation
}  // namespace arpg::platform
