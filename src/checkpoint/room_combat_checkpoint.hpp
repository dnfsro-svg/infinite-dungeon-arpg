#pragma once

#include "abyss/abyss_types.hpp"
#include "checkpoint/room_checkpoint_schema.hpp"
#include "core/deterministic_rng.hpp"
#include "modifiers/effect_set.hpp"
#include "skills/active_skill_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::checkpoint {

struct CheckpointVec3 final {
    float x{};
    float y{};
    float z{};
};

enum class AttackId : std::uint8_t {
    j1 = 0,
    j2 = 1,
    j3 = 2,
    launcher = 3,
    air_j = 4,
    none = 0xFF,
};

enum class Facing : std::int8_t {
    left = -1,
    right = 1,
};

enum class PlayerState : std::uint8_t {
    idle = 0,
    move = 1,
    attack_startup = 2,
    attack_active = 3,
    attack_recovery = 4,
    jump_rise = 5,
    jump_fall = 6,
    landing = 7,
};

enum class MonsterId : std::uint8_t {
    fire_bomber = 0,
    fire_charger = 1,
    water_bulwark = 2,
    water_support = 3,
    lightning_shooter = 4,
    lightning_dasher = 5,
    chaos_chaser = 6,
    chaos_hazard = 7,
    count = 8,
};

enum class PlayerDamageSourceKind : std::uint8_t {
    monster_attack = 0,
    projectile = 1,
    ground_hazard = 2,
    monster_affix = 3,
    abyss_environment = 4,
    unknown = 5,
};

enum class DamageType : std::uint8_t {
    physical = 0,
    fire = 1,
    water = 2,
    lightning = 3,
    chaos = 4,
    count = 5,
};

inline constexpr std::size_t kDamageTypeCount =
    static_cast<std::size_t>(DamageType::count);
inline constexpr std::size_t kElementCount = 4U;

enum class MonsterAffixId : std::uint8_t {
    mighty = 0,
    frenzy = 1,
    swift = 2,
    armored = 3,
    shielding = 4,
    multishot = 5,
    burning_ground = 6,
    chilling = 7,
    chain_lightning = 8,
    chaos_corrosion = 9,
    blink_assault = 10,
    death_blast = 11,
    count = 12,
};

enum class MonsterAffixTier : std::uint8_t {
    m1 = 0,
    m2 = 1,
    m3 = 2,
    count = 3,
};

enum class DummyKind : std::uint8_t {
    light = 0,
    normal = 1,
    heavy = 2,
};

enum class ReactionState : std::uint8_t {
    idle = 0,
    hitstun = 1,
    airborne = 2,
    knockdown = 3,
    rising = 4,
    defeated = 5,
    respawning = 6,
};

enum class ArmorState : std::uint8_t {
    none = 0,
    armored = 1,
    broken = 2,
};

enum class MonsterAiPhase : std::uint8_t {
    idle = 0,
    move = 1,
    telegraph = 2,
    active = 3,
    recovery = 4,
    cooldown = 5,
    defeated = 6,
};

enum class MonsterAffixWarning : std::uint8_t {
    none = 0,
    blink = 1,
    chain_lightning = 2,
    death_blast = 3,
};

enum class HazardKind : std::uint8_t {
    native = 0,
    burning = 1,
    chain_lightning = 2,
    death_blast = 3,
    thunderstorm = 4,
    hunting_flame = 5,
    chaos_expansion = 6,
};

using MonsterOrdinal = std::uint16_t;
inline constexpr MonsterOrdinal kInvalidMonsterOrdinal = 0xFFFFU;

struct MonsterOrdinalSet final {
    std::array<std::uint64_t, kMonsterOrdinalWordCount> words{};

    void clear() noexcept { words.fill(0U); }

    [[nodiscard]] bool contains(const MonsterOrdinal ordinal) const noexcept {
        if (ordinal >= limits::kRoomMonsterCapacity) return false;
        return (words[ordinal / 64U]
            & (std::uint64_t{1U} << (ordinal % 64U))) != 0U;
    }

    [[nodiscard]] bool insert(const MonsterOrdinal ordinal) noexcept {
        if (ordinal >= limits::kRoomMonsterCapacity) return false;
        words[ordinal / 64U] |=
            std::uint64_t{1U} << (ordinal % 64U);
        return true;
    }
};

struct MonsterAffixInstance final {
    MonsterAffixId id{MonsterAffixId::mighty};
    MonsterAffixTier tier{MonsterAffixTier::m1};

    friend bool operator==(const MonsterAffixInstance left,
        const MonsterAffixInstance right) noexcept {
        return left.id == right.id && left.tier == right.tier;
    }
};

struct MonsterAffixSet final {
    std::array<MonsterAffixInstance, 3U> values{};
    std::uint8_t count{};

    friend bool operator==(const MonsterAffixSet& left,
        const MonsterAffixSet& right) noexcept {
        return left.count == right.count && left.values == right.values;
    }
};

struct MonsterAffixProfile final {
    std::int32_t max_hp{};
    std::int32_t armor_rating{};
    std::int32_t max_shield{};
    std::uint16_t shield_recharge_delay_ticks{};
    std::int32_t damage_bp{10000};
    std::int32_t attack_timing_bp{10000};
    std::int32_t move_bp{10000};
    std::int32_t cooldown_bp{10000};
    std::int32_t horizontal_impulse_bp{10000};
};

struct PlayerDamageSource final {
    PlayerDamageSourceKind kind{PlayerDamageSourceKind::unknown};
    MonsterId monster{MonsterId::count};
    std::uint16_t detail_id{};
};

struct PlayerDefenseSnapshot final {
    std::int32_t hp{};
    std::int32_t max_hp{};
    std::int32_t barrier{};
    std::int32_t max_barrier{};
    std::int64_t armor{};
    std::int64_t evasion{};
    std::int32_t armor_reduction_bp{};
    std::int32_t evasion_rate_bp{};
    std::array<std::int32_t, kElementCount> damage_reduction{};
    std::array<std::int32_t, kElementCount> damage_reduction_cap{};
};

struct CombatDeathSnapshot final {
    std::uint64_t tick{};
    PlayerDamageSource source{};
    DamageType primary_type{DamageType::physical};
    std::uint64_t raw_damage{};
    std::uint64_t barrier_loss{};
    std::uint64_t health_loss{};
    std::uint64_t final_damage{};
    std::array<std::uint64_t, kDamageTypeCount> recent_damage{};
    PlayerDefenseSnapshot defense{};
};

struct PlayerStatusRuntime final {
    std::int32_t slow_bp{};
    std::uint16_t slow_ticks{};
    std::int32_t corrosion_damage_per_second{};
    std::uint16_t corrosion_ticks{};
    std::uint8_t corrosion_tick_phase{};
    PlayerDamageSource corrosion_source{};
};

struct PlayerCombatCheckpoint final {
    CheckpointVec3 position{};
    CheckpointVec3 velocity{};
    Facing facing{Facing::right};
    PlayerState state{PlayerState::idle};
    std::uint8_t combo_stage{};
    bool air_attack_available{true};
    std::int32_t hp{};
    std::int32_t max_hp{};
    std::int32_t barrier{};
    std::int32_t max_barrier{};
    std::array<std::int32_t, kElementCount> damage_reduction{};
    std::array<std::int32_t, kElementCount> damage_reduction_cap{};
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
    CheckpointVec3 spawn{};
    CheckpointVec3 position{};
    CheckpointVec3 velocity{};
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
    std::uint16_t engagement_latch{};
    std::uint16_t burning_ground_ticks{};
    std::uint16_t blink_assault_ticks{};
    bool blink_empowered{};
    CheckpointVec3 attack_target_position{};
    CheckpointVec3 attack_vector{};
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

struct FireRoomCrateSnapshot final {
    CheckpointVec3 position{};
    std::uint64_t broken_tick{};
    bool intact{true};
};

struct PlayerDamageHistoryCheckpoint final {
    std::array<std::array<std::uint64_t, kDamageTypeCount>,
        kPlayerDamageHistoryTicks> buckets{};
    std::uint64_t active_tick{};
    bool initialized{};
};

struct AbyssEnvironmentRuntime final {
    abyss::AbyssRuleId rule{abyss::AbyssRuleId::none};
    std::uint16_t cycle_tick{};
    std::uint16_t stage_tick{};
    CheckpointVec3 locked_center{};
    std::uint8_t expansion_stage{};
    bool warning{};
    bool active{};
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

static_assert(static_cast<std::int8_t>(Facing::left) == -1);
static_assert(static_cast<std::int8_t>(Facing::right) == 1);
static_assert(static_cast<std::size_t>(MonsterAffixTier::count)
    == kBurningGroundTimings.size());
static_assert(static_cast<std::size_t>(MonsterAffixTier::count)
    == kBlinkAssaultTimings.size());

}  // namespace arpg::checkpoint
