#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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
    int contact_damage{10};
    float projectile_speed{};
    std::uint16_t hazard_ticks{};
    FeedbackLevel feedback{FeedbackLevel::light};
    int shield_points{};
    std::uint16_t shield_duration_ticks{};
};

inline constexpr std::size_t kMonsterCapacity = 96;
inline constexpr std::size_t kProjectileCapacity = 384;
inline constexpr std::size_t kHazardCapacity = 96;
inline constexpr std::size_t kEncounterWaveCapacity = 2;
inline constexpr std::size_t kEncounterSpawnCapacity = 96;

struct MonsterSpawnSpec final {
    MonsterId id{MonsterId::chaos_chaser};
    Vec3 position{};
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
    int damage{};
    float radius{};
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
    std::uint16_t hurt_ticks{};
    std::uint16_t invulnerability_ticks{};
};

struct MonsterSnapshot final {
    bool active{};
    std::uint16_t generation{};
    MonsterId id{MonsterId::chaos_chaser};
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
};

struct ProjectileSnapshot final {
    bool active{};
    std::uint16_t generation{};
    MonsterHandle owner{};
    Vec3 position{};
    Vec3 velocity{};
    std::uint16_t lifetime_ticks{};
    int damage{};
    float radius{};
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
};

struct CombatSnapshot final {
    std::uint64_t tick{};
    PlayerSnapshot player{};
    std::array<MonsterSnapshot, kMonsterCapacity> monsters{};
    std::size_t monster_count{};
    std::array<ProjectileSnapshot, kProjectileCapacity> projectiles{};
    std::size_t projectile_count{};
    // Compatibility projection only; runtime state is owned by monsters.
    std::array<DummySnapshot, kDummyCount> dummies{};
    CombatDiagnostics diagnostics{};
};

}  // namespace arpg::combat
