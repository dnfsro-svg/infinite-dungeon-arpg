#pragma once

#include "combat/combat_types.hpp"
#include "dungeon/dungeon_checkpoint.hpp"
#include "dungeon/dungeon_rules.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::dungeon {

using ExitDirection = checkpoint::ExitDirection;
using EntrySide = checkpoint::EntrySide;
using DungeonElement = checkpoint::DungeonElement;
using TransitionKind = checkpoint::TransitionKind;
using DungeonRunState = checkpoint::DungeonRunState;

enum class RoomPhase : std::uint8_t {
    locked,
    combat,
    wave_delay,
    cleared,
    awaiting_exit,
    committing,
    transitioning,
    death_pending,
    faulted,
};

enum class SaveDisposition : std::uint8_t {
    committed,
    not_committed,
    indeterminate,
};

enum class PendingSaveKind : std::uint8_t;

enum class RequestResult : std::uint8_t {
    accepted,
    rejected,
    faulted,
};

enum class DungeonEventKind : std::uint8_t {
    room_entered,
    combat_started,
    room_cleared,
    exits_opened,
    abyss_exit_warning,
    transition_requested,
    transition_committed,
    save_failed,
    // Kept as a source-compatible name for pre-transaction consumers.
    exit_committed,
    room_destroyed,
    room_reset,
    faulted,
    death_detected,
    death_retreat_committed,
    death_continue_requested,
    death_continued,
};

struct DungeonEvent final {
    DungeonEventKind kind{};
    std::uint64_t session_tick{};
    std::uint64_t room_index{};
    std::uint64_t room_seed{};
    std::uint64_t destination_room_index{};
    std::uint64_t destination_room_seed{};
    TransitionKind transition{TransitionKind::none};
    ExitDirection direction{ExitDirection::none};
    std::uint8_t abyss_pending_rewards{};
    std::uint8_t abyss_unpicked_rewards{};
};

struct AbyssExitConfirmation final {
    bool armed{};
    TransitionKind transition{TransitionKind::none};
    ExitDirection direction{ExitDirection::none};
    bool door_input_released{};
    std::uint32_t reward_revision{};
};

struct DungeonDiagnostics final {
    std::uint32_t event_overflow_count{};
    std::uint32_t combat_relay_overflow_count{};
    std::uint32_t rejected_exit_count{};
    std::uint32_t save_failure_count{};
    std::uint32_t ground_saturation_count{};
    DungeonFault fault{DungeonFault::none};
    bool room_index_overflow{};
};

struct DungeonEncounterDiagnostics final {
    std::uint8_t total_budget{};
    std::uint8_t current_wave_budget{};
    std::uint8_t current_wave_spawn_count{};
    bool plan_valid{};
};

struct DungeonSessionConfig final {
    std::uint64_t root_seed{0x6D30305F5241594CULL};
    std::uint64_t initial_room_index{};
};

struct RoomDescriptor final {
    std::uint64_t index{};
    std::uint64_t seed{};
    EntrySide entry{EntrySide::initial};
    combat::CombatLabConfig combat{};
};

inline constexpr std::size_t kGroundDropCapacity = 192U;
inline constexpr float kPickupRadius = 1.5F;

enum class GroundItemSource : std::uint8_t {
    monster_drop,
    abyss_chest,
};

struct GroundItem final {
    bool active{};
    std::uint16_t drop_ordinal{};
    GroundItemSource source{GroundItemSource::monster_drop};
    std::uint8_t abyss_reward_ordinal{0xFFU};
    combat::Vec3 position{};
    items::ItemInstance item{};
};

struct AutoPickupPolicy final {
    items::ItemRarity minimum_rarity{items::ItemRarity::normal};
};

[[nodiscard]] bool auto_pickup_eligible(
    const GroundItem& ground,
    AutoPickupPolicy policy) noexcept;

struct GroundItemSnapshot final {
    std::uint16_t ordinal{};
    GroundItemSource source{GroundItemSource::monster_drop};
    std::uint8_t abyss_reward_ordinal{0xFFU};
    combat::Vec3 position{};
    std::uint64_t item_id{};
    items::ItemSlot slot{items::ItemSlot::weapon};
    items::ItemRarity rarity{items::ItemRarity::normal};
};

struct DeathSnapshot final {
    checkpoint::DeathCheckpoint checkpoint{};
    bool saving{};
    bool can_continue{};
    bool continue_failed{};
};

struct DungeonSnapshot final {
    std::uint64_t session_tick{};
    std::uint64_t root_seed{};
    std::uint64_t commit_generation{};
    std::uint64_t room_index{};
    std::uint64_t room_seed{};
    std::uint64_t depth{1};
    std::uint64_t floor_room_index{1};
    std::array<std::uint32_t, 4> biases{};
    RoomPhase phase{RoomPhase::locked};
    bool has_active_room{};
    std::array<bool, 4> exits_open{};
    std::array<bool, 4> abyss_doors{};
    std::uint8_t wave_index{};
    std::uint8_t wave_count{};
    std::uint16_t wave_delay_ticks{};
    std::uint8_t remaining_targets{};
    EntrySide entry_side{EntrySide::initial};
    ExitDirection last_exit{ExitDirection::none};
    TransitionKind last_transition{TransitionKind::none};
    DungeonElement ecology{DungeonElement::fire};
    bool has_hole{};
    bool is_abyss{};
    abyss::AbyssDanger abyss_danger{abyss::AbyssDanger::low};
    abyss::AbyssRuleId abyss_rule{abyss::AbyssRuleId::none};
    std::uint8_t abyss_pending_rewards{};
    std::uint8_t abyss_unpicked_rewards{};
    bool abyss_exit_confirmation_armed{};
    TransitionKind abyss_exit_confirmation_transition{TransitionKind::none};
    ExitDirection abyss_exit_confirmation_direction{ExitDirection::none};
    bool has_pending_transition{};
    passives::PassiveTreeState passive_tree{};
    bool passive_save_pending{};
    passives::PassiveTreeError passive_tree_error{
        passives::PassiveTreeError::none};
    std::uint32_t inventory_count{};
    std::array<std::uint64_t, 6> equipped_ids{};
    std::uint16_t ground_item_count{};
    std::array<GroundItemSnapshot, kGroundDropCapacity> ground_items{};
    std::optional<PendingSaveKind> pending_save_kind{};
    std::optional<DeathSnapshot> death{};
    std::optional<combat::CombatSnapshot> combat{};
    DungeonEncounterDiagnostics encounter{};
    DungeonDiagnostics diagnostics{};
    progression::ProgressionState progression{};
    std::uint64_t pending_room_experience{};
    std::uint64_t last_room_experience{};
    std::uint8_t last_levels_gained{};
};

struct PendingTransition final {
    TransitionKind kind{TransitionKind::none};
    ExitDirection direction{ExitDirection::none};
    std::uint64_t expected_generation{};
    DungeonRunState next_state{};
};

enum class PendingSaveKind : std::uint8_t {
    transition,
    passive_tree,
    loot_pickup,
    equipment,
    recipe,
    abyss_start,
    abyss_fail,
    abyss_clear,
    abyss_reward_materialized,
    abyss_reward_claim,
    abyss_abandon,
    death_retreat,
    death_continue,
};

struct PendingSave final {
    PendingSaveKind kind{PendingSaveKind::transition};
    std::uint64_t expected_generation{};
    DungeonRunState next_state{};
    TransitionKind transition{TransitionKind::none};
    ExitDirection direction{ExitDirection::none};
    RoomPhase resume_phase{RoomPhase::awaiting_exit};
    std::uint16_t pickup_ordinal{0xFFFFU};
    std::optional<combat::CombatDeathSnapshot> death_snapshot{};
};

struct PendingSaveResult final {
    SaveDisposition disposition{SaveDisposition::indeterminate};
    std::uint64_t generation{};
    DungeonRunState verified_state{};
    std::optional<PendingSaveKind> kind{};
};

using TransitionSaveResult = PendingSaveResult;

}  // namespace arpg::dungeon
