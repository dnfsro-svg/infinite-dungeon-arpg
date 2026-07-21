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

struct ActiveSkillRuntime final {
    ActiveSkillSnapshot snapshot{};
    Facing locked_facing{Facing::right};
    std::array<std::uint16_t, skills::kActiveSkillCount> cooldowns{};
    std::array<bool, kMonsterCapacity> hit_latch{};
};

}  // namespace arpg::combat
