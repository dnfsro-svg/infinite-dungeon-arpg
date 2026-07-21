#pragma once

#include "skills/active_skill_types.hpp"

#include <raylib.h>

#include <array>
#include <cstdint>

namespace arpg::platform {

struct ActiveSkillHudSlot final {
    std::uint8_t key_number{};
    skills::ActiveSkillId id{skills::ActiveSkillId::none};
    std::array<char, 48> name{};
    float cooldown_ratio{};
    bool empty{};
};

struct ActiveSkillHudModel final {
    std::array<ActiveSkillHudSlot,
        skills::kActiveSkillSlotCount> slots{};
};

struct ActiveSkillHudLayout final {
    Rectangle bounds{};
    std::array<Rectangle, skills::kActiveSkillSlotCount> slots{};
};

[[nodiscard]] ActiveSkillHudModel make_active_skill_hud_model(
    const skills::SkillLoadoutState& loadout,
    const std::array<std::uint16_t, skills::kActiveSkillCount>& cooldowns)
    noexcept;

[[nodiscard]] ActiveSkillHudLayout active_skill_hud_layout(
    int width, int height) noexcept;

}  // namespace arpg::platform
