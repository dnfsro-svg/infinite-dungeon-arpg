#pragma once

#include "combat/combat_types.hpp"
#include "combat/monster_affix_runtime.hpp"
#include "combat/player_damage_history.hpp"
#include "combat/room_environment_plan.hpp"
#include "core/deterministic_rng.hpp"
#include "modifiers/effect_set.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::combat {

struct PlayerCombatCheckpoint final {
    Vec3 position{};
    Vec3 velocity{};
    Facing facing{Facing::right};
    PlayerState state{PlayerState::idle};
    std::uint8_t combo_stage{};
    bool air_attack_available{true};
    std::int32_t hp{};
    std::int32_t max_hp{};
    std::int32_t barrier{};
    std::int32_t max_barrier{};
    std::array<std::int32_t, modifiers::kElementCount> damage_reduction{};
    std::array<std::int32_t, modifiers::kElementCount> damage_reduction_cap{};
    std::int64_t armor{};
    std::int64_t evasion{};
    std::int32_t armor_reduction_bp{};
    std::int32_t evasion_rate_bp{};
    std::uint16_t hurt_ticks{};
    std::uint16_t invulnerability_ticks{};
    PlayerStatusRuntime status{};
    std::array<std::uint16_t, skills::kActiveSkillCount> skill_cooldowns{};
};

struct AttackCheckpoint final {
    AttackId id{AttackId::none};
    std::uint16_t elapsed_ticks{};
    std::uint16_t startup_ticks{};
    std::uint16_t recovery_ticks{};
    bool connected{};
    bool impact_event_emitted{};
    MonsterOrdinalSet hit_targets{};
};

struct MonsterCombatCheckpoint final {
    MonsterOrdinal ordinal{kInvalidMonsterOrdinal};
    MonsterId id{MonsterId::count};
    MonsterAffixSet affixes{};
    MonsterAffixProfile affix_profile{};
    DummyKind kind{DummyKind::normal};
    Vec3 spawn{};
    Vec3 position{};
    Vec3 velocity{};
    Facing facing{Facing::right};
    ReactionState reaction{ReactionState::idle};
    ArmorState armor{ArmorState::none};
    std::uint16_t reaction_ticks{};
    MonsterAiPhase ai_phase{MonsterAiPhase::idle};
    std::uint16_t ai_ticks{};
    std::uint32_t attack_serial{};
    bool contact_attack_resolved{};
    std::int32_t hp{};
    std::int32_t max_hp{};
    std::int32_t break_value{};
    std::int32_t max_break{};
    std::int32_t shield{};
    std::int32_t max_shield{};
    std::uint16_t shield_ticks{};
    std::uint16_t max_shield_ticks{};
    std::uint16_t shield_recharge_ticks{};
    std::uint16_t break_window_ticks{};
    std::uint16_t owner_transient_counter{};
    std::uint16_t burning_ground_ticks{};
    std::uint16_t blink_assault_ticks{};
    bool blink_empowered{};
    Vec3 attack_target_position{};
    Vec3 attack_vector{};
    MonsterAffixWarning affix_warning{MonsterAffixWarning::none};
    std::uint16_t affix_warning_ticks{};
    modifiers::EffectSetCheckpoint effects{};
    bool effects_touched{};
};

struct RoomObstacleCheckpoint final {
    std::uint16_t ordinal{0xFFFFU};
    std::uint16_t hp{};
    std::uint16_t max_hp{};
    std::uint64_t broken_tick{};
    bool intact{};
    modifiers::EffectSetCheckpoint effects{};
};

struct RoomCombatCheckpoint final {
    RoomCombatCheckpoint() noexcept = default;
    RoomCombatCheckpoint(const RoomCombatCheckpoint&) = delete;
    RoomCombatCheckpoint& operator=(const RoomCombatCheckpoint&) = delete;
    RoomCombatCheckpoint(RoomCombatCheckpoint&&) = delete;
    RoomCombatCheckpoint& operator=(RoomCombatCheckpoint&&) = delete;

    std::uint64_t tick{};
    core::DeterministicRng::State evasion_rng_state{};
    PlayerCombatCheckpoint player{};
    AbyssEnvironmentRuntime abyss_environment{};
    AttackCheckpoint attack{};
    std::array<MonsterCombatCheckpoint, limits::kRoomMonsterCapacity>
        monsters{};
    std::uint16_t monster_count{};
    std::array<RoomObstacleCheckpoint, kRoomEnvironmentCellCount> obstacles{};
    std::uint16_t obstacle_count{};
    std::array<FireRoomCrateSnapshot, kFireRoomCrateCapacity> fire_crates{};
    std::uint8_t fire_crate_count{};
    PlayerDamageHistoryCheckpoint player_damage_history{};
    bool has_death_snapshot{};
    CombatDeathSnapshot death_snapshot{};
};

[[nodiscard]] bool valid_room_combat_checkpoint_structural(
    const RoomCombatCheckpoint& checkpoint,
    std::uint32_t generated_monsters) noexcept;

}  // namespace arpg::combat
