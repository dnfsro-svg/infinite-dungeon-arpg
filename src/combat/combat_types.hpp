#pragma once

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

}  // namespace arpg::combat
