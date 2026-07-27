#pragma once

#include "combat/combat_types.hpp"

#include <array>
#include <cstdint>

namespace arpg::combat {

inline constexpr std::uint16_t kDrawSlashStartupTicks = 10U;
inline constexpr std::uint16_t kDrawSlashRecoveryTicks = 14U;
inline constexpr int kDrawSlashBasePhysical = 220;
inline constexpr int kDrawSlashBreakDamage = 36;
inline constexpr float kDrawSlashRange = 5.0F;
inline constexpr float kDrawSlashHalfWidthAtEnd = 3.2F;
inline constexpr float kDrawSlashKnockbackSpeed = 0.22F;

inline constexpr std::uint16_t kStormStartupTicks = 24U;
inline constexpr std::uint8_t kStormStrikeCount = 12U;
inline constexpr std::uint16_t kStormStrikeIntervalTicks = 6U;
inline constexpr std::uint16_t kStormRecoveryTicks = 24U;
inline constexpr float kStormCenterForward = 3.5F;
inline constexpr float kStormStrikeRadius = 2.8F;
inline constexpr float kStormFinisherRadius = 3.8F;
inline constexpr int kStormStrikeBasePhysical = 42;
inline constexpr int kStormFinisherBasePhysical = 360;
inline constexpr float kStormFinisherKnockbackSpeed = 0.26F;
inline constexpr float kStormFinisherLaunchSpeed = 0.24F;

struct ActiveSkillRuntime final {
    ActiveSkillSnapshot snapshot{};
    Facing locked_facing{Facing::right};
    std::array<std::uint16_t, skills::kActiveSkillCount> cooldowns{};
    MonsterOrdinalSet hit_latch{};
};

}  // namespace arpg::combat
