#include "skills/active_skill_catalog.hpp"

#include <array>
#include <cstddef>

namespace arpg::skills {
namespace {

constexpr std::array<ActiveSkillDefinition, kActiveSkillCount> kDefinitions{{
    {ActiveSkillId::draw_slash,
     u8"\u62D4\u5200\u65A9", kDrawSlashCooldownTicks},
    {ActiveSkillId::storm_swords,
     u8"\u6781\u00B7\u9B3C\u5251\u672F\uFF08\u66B4\u98CE\u5F0F\uFF09",
     kStormSwordsCooldownTicks},
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
