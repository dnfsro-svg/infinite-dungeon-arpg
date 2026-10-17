#pragma once

#include "skills/active_skill_types.hpp"

#include <cstdint>

namespace arpg::skills {

struct ActiveSkillDefinition final {
    ActiveSkillId id{ActiveSkillId::none};
    const char* display_name{};
    std::uint16_t cooldown_ticks{};
};

inline constexpr std::uint16_t kDrawSlashCooldownTicks = 240U;
inline constexpr std::uint16_t kStormSwordsCooldownTicks = 1800U;

[[nodiscard]] const ActiveSkillDefinition* active_skill_definition(
    ActiveSkillId id) noexcept;

}  // namespace arpg::skills
