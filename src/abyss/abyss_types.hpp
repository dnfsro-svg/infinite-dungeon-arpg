#pragma once

#include "modifiers/damage_types.hpp"

#include <array>
#include <cstdint>

namespace arpg::abyss {

inline constexpr std::uint32_t kAbyssRulesVersion = 1U;

enum class AbyssDanger : std::uint8_t {
    low = 0,
    medium = 1,
    high = 2,
};

enum class AbyssRuleId : std::uint8_t {
    thunderstorm = 0,
    hunting_flames = 1,
    chaos_expansion = 2,
    swift_pursuit = 3,
    abyss_bulwark = 4,
    abyss_fury = 5,
    heavy_steps = 6,
    exhausted_recovery = 7,
    life_sacrifice = 8,
    none = 0xFF,
};

enum class AbyssLifecycle : std::uint8_t {
    none = 0,
    available = 1,
    started = 2,
    cleared = 3,
    failed = 4,
};

struct AbyssEnvironmentConfig final {
    bool active{};
    modifiers::DamageType damage_type{modifiers::DamageType::physical};
    std::uint16_t damage_bp{};
    std::uint16_t cycle_ticks{};
    std::uint16_t warning_ticks{};
    std::uint16_t duration_ticks{};
    std::uint16_t damage_interval_ticks{};
    std::uint16_t expansion_interval_ticks{};
    std::array<std::uint16_t, 5> radius_milliunits{};
    std::uint8_t radius_count{};
};

struct AbyssCombatConfig final {
    AbyssRuleId rule{AbyssRuleId::none};
    AbyssDanger danger{AbyssDanger::low};
    AbyssEnvironmentConfig environment{};
    std::uint16_t player_ground_move_bp{10000};
    std::uint16_t player_resource_restore_bp{10000};
    std::uint16_t player_max_health_bp{10000};
    std::uint16_t monster_move_bp{10000};
    std::uint16_t monster_cooldown_bp{10000};
    std::uint16_t monster_armor_bp{10000};
    std::uint16_t monster_extra_shield_bp{};
    std::uint16_t monster_damage_bp{10000};
    std::uint16_t monster_attack_speed_bp{10000};
};

struct AbyssSelection final {
    AbyssDanger danger{AbyssDanger::low};
    AbyssRuleId rule{AbyssRuleId::thunderstorm};
    std::uint32_t rules_version{kAbyssRulesVersion};
};

}  // namespace arpg::abyss
