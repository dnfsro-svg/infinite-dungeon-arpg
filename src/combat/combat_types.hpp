#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "abyss/abyss_types.hpp"
#include "combat/monster_affix_types.hpp"
#include "modifiers/damage_types.hpp"
#include "modifiers/player_modifier_values.hpp"

namespace arpg::test {
struct CombatWorldTestAccess;
}

namespace arpg::combat {

struct Vec3 final {
    float x{};
    float y{};
    float z{};
};

struct Aabb final {
    Vec3 minimum{};
    Vec3 maximum{};
};

enum class AttackId : std::uint8_t {
    j1 = 0,
    j2,
    j3,
    launcher,
    air_j,
    none = 0xFF,
};

enum class AttackPhase : std::uint8_t {
    startup,
    active,
    recovery,
    finished,
};

enum class FeedbackLevel : std::uint8_t {
    light,
    medium,
    heavy,
};

struct DamagePacket final {
    std::array<int, modifiers::kDamageTypeCount> amount{};

    constexpr DamagePacket() noexcept = default;
    constexpr DamagePacket(int physical) noexcept {
        amount[modifiers::damage_index(modifiers::DamageType::physical)] =
            physical;
    }
    constexpr DamagePacket(
        std::array<int, modifiers::kDamageTypeCount> values) noexcept
        : amount(values) {}

    friend bool operator==(
        const DamagePacket& left, const DamagePacket& right) noexcept {
        return left.amount == right.amount;
    }
    friend bool operator!=(
        const DamagePacket& left, const DamagePacket& right) noexcept {
        return !(left == right);
    }
};

enum class DamageDelivery : std::uint8_t {
    direct,
    ground_or_environment,
};

struct PlayerStatusRuntime final {
    std::int32_t slow_bp{};
    std::uint16_t slow_ticks{};
    int corrosion_damage_per_second{};
    std::uint16_t corrosion_ticks{};
    std::uint8_t corrosion_tick_phase{};
};

struct PlayerCombatBuild final {
    modifiers::PlayerModifierValues values{};
    std::int64_t weapon_physical{};
    std::int32_t local_attack_speed_bp{};
};

struct ResolvedPlayerDamage final {
    std::array<std::uint64_t, modifiers::kDamageTypeCount> by_type{};
    std::uint64_t total{};
};

[[nodiscard]] std::optional<DamagePacket> build_player_hit_packet(
    int base_physical, const PlayerCombatBuild& build) noexcept;
[[nodiscard]] std::optional<ResolvedPlayerDamage>
resolve_player_damage_packet(
    DamagePacket packet, const PlayerCombatBuild& build) noexcept;
[[nodiscard]] std::optional<int> resolve_player_damage(
    DamagePacket packet, const PlayerCombatBuild& build) noexcept;
[[nodiscard]] std::uint16_t scaled_phase_ticks(
    std::uint16_t base, std::int64_t attack_speed) noexcept;

enum class ImpactKind : std::uint8_t {
    light_hitstun,
    medium_hitstun,
    knockdown,
    launch,
};

enum class MonsterId : std::uint8_t {
    fire_bomber,
    fire_charger,
    water_bulwark,
    water_support,
    lightning_shooter,
    lightning_dasher,
    chaos_chaser,
    chaos_hazard,
    count,
};

enum class MonsterTag : std::uint16_t {
    none = 0,
    melee = 1U << 0U,
    ranged = 1U << 1U,
    support = 1U << 2U,
    high_priority = 1U << 3U,
    ground_hazard = 1U << 4U,
    direct_target = 1U << 5U,
    projectile_capable = 1U << 6U,
};

struct MonsterDefinition final {
    MonsterId id{MonsterId::chaos_chaser};
    std::uint8_t preferred_ecology{};
    std::uint16_t tags{};
    std::uint8_t threat_cost{1};
    int max_hp{100};
    int max_break{};
    float move_speed{0.04F};
    float preferred_range{1.0F};
    std::uint16_t telegraph_ticks{20};
    std::uint16_t active_ticks{4};
    std::uint16_t recovery_ticks{20};
    std::uint16_t cooldown_ticks{60};
    DamagePacket contact_damage{10};
    float projectile_speed{};
    std::uint16_t hazard_ticks{};
    FeedbackLevel feedback{FeedbackLevel::light};
    int shield_points{};
    std::uint16_t shield_duration_ticks{};
};

[[nodiscard]] constexpr bool has_tag(
    const MonsterDefinition& definition, MonsterTag value) noexcept {
    return (definition.tags & static_cast<std::uint16_t>(value)) != 0U;
}

inline constexpr std::size_t kMonsterCapacity = 96;
inline constexpr std::size_t kProjectileCapacity = 384;
inline constexpr std::size_t kHazardCapacity = 96;
inline constexpr std::size_t kEncounterWaveCapacity = 2;
inline constexpr std::size_t kEncounterSpawnCapacity = 96;

struct MonsterSpawnSpec final {
    MonsterId id{MonsterId::chaos_chaser};
    Vec3 position{};
    MonsterAffixSet affixes{};
    std::uint16_t spawn_ordinal{};
};

enum class MonsterAffixWarning : std::uint8_t {
    none,
    blink,
    chain_lightning,
    death_blast,
};

enum class HazardKind : std::uint8_t {
    native,
    burning,
    chain_lightning,
    death_blast,
    thunderstorm,
    hunting_flame,
    chaos_expansion,
};

enum class HazardSource : std::uint8_t {
    monster,
    abyss_environment,
};

struct MonsterHandle final {
    std::uint16_t index{0xFFFF};
    std::uint16_t generation{};
};

struct ProjectileHandle final {
    std::uint16_t index{0xFFFF};
    std::uint16_t generation{};
};

struct ProjectileRuntime final {
    bool active{};
    std::uint16_t generation{};
    MonsterHandle owner{};
    Vec3 position{};
    Vec3 velocity{};
    std::uint16_t lifetime_ticks{};
    DamagePacket damage{};
    float radius{};
    bool trigger_chain_on_end{};
    MonsterAffixSet owner_affixes{};
};

struct HazardRuntime final {
    bool active{};
    std::uint16_t generation{};
    MonsterHandle owner{};
    HazardSource source{HazardSource::monster};
    HazardKind kind{HazardKind::native};
    Vec3 center{};
    float radius{};
    std::uint16_t telegraph_ticks{};
    std::uint16_t active_ticks{};
    std::uint16_t lifetime_ticks{};
    std::uint16_t damage_interval_ticks{};
    std::uint16_t damage_cooldown_ticks{};
    bool player_latched{};
    bool persists_after_owner_death{};
    DamagePacket damage{};
    std::uint16_t environment_damage_bp{};
    modifiers::DamageType environment_damage_type{
        modifiers::DamageType::physical};
};

struct AbyssEnvironmentRuntime final {
    abyss::AbyssRuleId rule{abyss::AbyssRuleId::none};
    std::uint16_t cycle_tick{};
    std::uint16_t stage_tick{};
    Vec3 locked_center{};
    std::uint8_t expansion_stage{};
    bool warning{};
    bool active{};
};

struct EncounterWave final {
    std::array<MonsterSpawnSpec, kEncounterSpawnCapacity> spawns{};
    std::uint8_t spawn_count{};
    std::uint8_t spent_budget{};
};

enum class CombatEventKind : std::uint8_t {
    swing,
    hit,
    impact_summary,
    landing,
    break_started,
    defeated,
    respawned,
    reset,
    player_hit,
    player_hurt_started,
    player_health_reset,
    player_defeated,
    affix_blink_warning,
    affix_chain_warning,
    affix_death_warning,
};

struct DefeatPayload final {
    MonsterId monster_id{MonsterId::count};
    std::uint16_t spawn_ordinal{};
    std::uint16_t affix_score{};
    bool reward_eligible{};
};

struct CombatEvent final {
    CombatEventKind kind{};
    std::uint64_t tick{};
    AttackId attack{AttackId::none};
    std::uint8_t target_index{0xFF};
    std::uint8_t hit_count{};
    FeedbackLevel feedback{};
    Vec3 position{};
    int value{};
    MonsterId monster_id{MonsterId::count};
    std::uint16_t spawn_ordinal{};
    std::uint16_t affix_score{};
    bool reward_eligible{};
};

struct AttackDefinition final {
    AttackId id{AttackId::none};
    std::uint16_t startup_ticks{};
    std::uint16_t active_ticks{};
    std::uint16_t recovery_ticks{};
    int damage{};
    int break_damage{};
    ImpactKind impact{ImpactKind::light_hitstun};
    Aabb local_hitbox{};
    float lunge_distance{};
    float knockback_speed{};
    float launch_speed{};
    FeedbackLevel feedback{FeedbackLevel::light};
};

enum class Facing : std::int8_t {
    left = -1,
    right = 1,
};

enum class PlayerState : std::uint8_t {
    idle,
    move,
    attack_startup,
    attack_active,
    attack_recovery,
    jump_rise,
    jump_fall,
    landing,
};

enum class DummyKind : std::uint8_t {
    light,
    normal,
    heavy,
};

enum class ReactionState : std::uint8_t {
    idle,
    hitstun,
    airborne,
    knockdown,
    rising,
    defeated,
    respawning,
};

enum class ArmorState : std::uint8_t {
    none,
    armored,
    broken,
};

enum class MonsterAiPhase : std::uint8_t {
    idle,
    move,
    telegraph,
    active,
    recovery,
    cooldown,
    defeated,
};

struct MovementInput final {
    std::int8_t x{};
    std::int8_t y{};
};

inline constexpr std::size_t kDummyCount = 3;

struct CombatLabConfig final {
    Vec3 player_spawn{0.0F, 0.0F, 0.0F};
    std::array<Vec3, kDummyCount> dummy_spawns{{
        {2.30F, -0.35F, 0.0F},
        {2.80F, 0.0F, 0.0F},
        {3.30F, 0.35F, 0.0F},
    }};
    Facing initial_facing{Facing::right};
    bool respawn_defeated_dummies{true};
};

struct CombatEncounterConfig final {
    Vec3 player_spawn{};
    Facing initial_facing{Facing::right};
    EncounterWave wave{};
    bool reset_player_health{true};
    PlayerCombatBuild player_build{};
    std::uint64_t evasion_seed{};
    abyss::AbyssCombatConfig abyss{};
};

struct PlayerSnapshot final {
    Vec3 position{};
    Vec3 velocity{};
    Facing facing{Facing::right};
    PlayerState state{PlayerState::idle};
    AttackId active_attack{AttackId::none};
    AttackPhase attack_phase{AttackPhase::finished};
    std::uint16_t attack_elapsed_ticks{};
    std::uint8_t combo_stage{};
    std::uint16_t hit_stop_ticks{};
    bool air_attack_available{true};
    int hp{};
    int max_hp{};
    int barrier{};
    int max_barrier{};
    std::array<std::int32_t, modifiers::kElementCount> damage_reduction{};
    std::array<std::int32_t, modifiers::kElementCount> damage_reduction_cap{};
    std::int64_t armor{};
    std::int64_t evasion{};
    std::int32_t armor_reduction_bp{};
    std::int32_t evasion_rate_bp{};
    std::uint16_t hurt_ticks{};
    std::uint16_t invulnerability_ticks{};
    std::int32_t slow_bp{};
    std::uint16_t slow_ticks{};
    int corrosion_damage_per_second{};
    std::uint16_t corrosion_ticks{};
    std::uint8_t corrosion_tick_phase{};
};

struct MonsterSnapshot final {
    bool active{};
    std::uint16_t generation{};
    MonsterId id{MonsterId::chaos_chaser};
    MonsterAffixSet affixes{};
    std::uint16_t spawn_ordinal{};
    Vec3 spawn{};
    Vec3 position{};
    Vec3 velocity{};
    DummyKind kind{DummyKind::light};
    Facing facing{Facing::right};
    ReactionState reaction{ReactionState::idle};
    ArmorState armor{ArmorState::none};
    int hp{};
    int max_hp{};
    int break_value{};
    int max_break{};
    int shield{};
    int max_shield{};
    std::uint16_t shield_ticks{};
    std::uint16_t max_shield_ticks{};
    std::uint16_t break_window_ticks{};
    std::uint16_t hit_stop_ticks{};
    MonsterAiPhase ai_phase{MonsterAiPhase::idle};
    Vec3 attack_target_position{};
    Vec3 attack_vector{};
    MonsterAffixWarning affix_warning{MonsterAffixWarning::none};
    std::uint16_t affix_warning_ticks{};
    bool blink_empowered{};
};

struct ProjectileSnapshot final {
    bool active{};
    std::uint16_t generation{};
    MonsterHandle owner{};
    Vec3 position{};
    Vec3 velocity{};
    std::uint16_t lifetime_ticks{};
    DamagePacket damage{};
    float radius{};
    bool trigger_chain_on_end{};
};

struct HazardSnapshot final {
    bool active{};
    std::uint16_t generation{};
    MonsterHandle owner{};
    HazardSource source{HazardSource::monster};
    HazardKind kind{HazardKind::native};
    Vec3 center{};
    float radius{};
    std::uint16_t telegraph_ticks{};
    std::uint16_t active_ticks{};
    std::uint16_t lifetime_ticks{};
    std::uint16_t damage_interval_ticks{};
    bool player_latched{};
    bool persists_after_owner_death{};
    DamagePacket damage{};
    std::uint16_t environment_damage_bp{};
    modifiers::DamageType environment_damage_type{
        modifiers::DamageType::physical};
};

// Temporary presentation alias for pre-Task 3 dungeon tests. Task 8 removes
// this compatibility name after all consumers use MonsterSnapshot.
using DummySnapshot = MonsterSnapshot;

struct CombatDiagnostics final {
    std::size_t input_size{};
    std::uint32_t input_expired_count{};
    std::uint32_t input_overflow_count{};
    std::uint32_t event_overflow_count{};
    std::uint32_t projectile_saturation_count{};
    std::uint32_t projectile_invalid_owner_count{};
    std::uint32_t hazard_saturation_count{};
    std::uint32_t hazard_invalid_owner_count{};
    std::size_t effect_owner_count{};
    std::size_t active_effect_count{};
    std::uint32_t effect_overflow_count{};
    std::uint32_t effect_command_overflow_count{};
};

struct CombatSnapshot final {
    std::uint64_t tick{};
    PlayerSnapshot player{};
    std::array<MonsterSnapshot, kMonsterCapacity> monsters{};
    std::size_t monster_count{};
    std::array<ProjectileSnapshot, kProjectileCapacity> projectiles{};
    std::size_t projectile_count{};
    std::array<HazardSnapshot, kHazardCapacity> hazards{};
    std::size_t hazard_count{};
    // Compatibility projection only; runtime state is owned by monsters.
    std::array<DummySnapshot, kDummyCount> dummies{};
    CombatDiagnostics diagnostics{};
};

}  // namespace arpg::combat
