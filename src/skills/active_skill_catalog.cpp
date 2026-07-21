#include "skills/active_skill_catalog.hpp"

#include <array>
#include <cstddef>

namespace arpg::skills {
namespace {

constexpr std::array<ActiveSkillDefinition, kActiveSkillCount> kDefinitions{{
    {ActiveSkillId::draw_slash, "Draw Slash", kDrawSlashCooldownTicks},
    {ActiveSkillId::storm_swords, "Storm Swords", kStormSwordsCooldownTicks},
}};

}  // namespace

const ActiveSkillDefinition* active_skill_definition(
    const ActiveSkillId id) noexcept {
    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= kDefinitions.size())
        return nullptr;
    return &kDefinitions[index];
}

}  // namespace arpg::skills
