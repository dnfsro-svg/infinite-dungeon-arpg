#pragma once

#include "skills/active_skill_types.hpp"

#include <cstddef>
#include <cstdint>

namespace arpg::skills {

enum class SkillLoadoutError : std::uint8_t {
    none, invalid_slot, invalid_active_id, invalid_support_id,
    active_not_owned, duplicate_active, destination_occupied
};

[[nodiscard]] SkillLoadoutState default_skill_loadout() noexcept;
[[nodiscard]] SkillLoadoutError validate_skill_loadout(
    const SkillLoadoutState&) noexcept;
[[nodiscard]] SkillLoadoutError remove_active_skill(
    SkillLoadoutState&, std::size_t slot) noexcept;
[[nodiscard]] SkillLoadoutError equip_active_skill(
    SkillLoadoutState&, ActiveSkillId, std::size_t slot) noexcept;
[[nodiscard]] SkillLoadoutError swap_active_skill_slots(
    SkillLoadoutState&, std::size_t left, std::size_t right) noexcept;

}  // namespace arpg::skills
