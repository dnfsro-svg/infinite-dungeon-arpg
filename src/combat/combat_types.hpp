#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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
    heavy,
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

enum class CombatEventKind : std::uint8_t {
    swing,
    hit,
    impact_summary,
    landing,
    break_started,
    defeated,
    respawned,
    reset,
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
};

struct DummySnapshot final {
    Vec3 position{};
    Vec3 velocity{};
    DummyKind kind{DummyKind::light};
    ReactionState reaction{ReactionState::idle};
    ArmorState armor{ArmorState::none};
    int hp{};
    int max_hp{};
    int break_value{};
    int max_break{};
    std::uint16_t break_window_ticks{};
    std::uint16_t hit_stop_ticks{};
};

struct CombatDiagnostics final {
    std::size_t input_size{};
    std::uint32_t input_expired_count{};
    std::uint32_t input_overflow_count{};
    std::uint32_t event_overflow_count{};
};

struct CombatSnapshot final {
    std::uint64_t tick{};
    PlayerSnapshot player{};
    std::array<DummySnapshot, kDummyCount> dummies{};
    CombatDiagnostics diagnostics{};
};

}  // namespace arpg::combat
