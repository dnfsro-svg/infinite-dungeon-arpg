#include "active_skill_view.hpp"

#include "skills/active_skill_catalog.hpp"

#include <algorithm>
#include <cstdio>

namespace arpg::platform {
namespace {

constexpr float kSlotSize = 58.0F;
constexpr float kSlotGap = 8.0F;
constexpr float kBottomMargin = 20.0F;

}  // namespace

ActiveSkillHudModel make_active_skill_hud_model(
    const skills::SkillLoadoutState& loadout,
    const std::array<std::uint16_t, skills::kActiveSkillCount>& cooldowns)
    noexcept {
    ActiveSkillHudModel result{};
    for (std::size_t index = 0U; index < result.slots.size(); ++index) {
        ActiveSkillHudSlot& slot = result.slots[index];
        slot.key_number = static_cast<std::uint8_t>(index + 1U);
        slot.id = loadout.slots[index].active;
        slot.empty = slot.id == skills::ActiveSkillId::none;
        if (slot.empty) continue;
        const skills::ActiveSkillDefinition* const definition =
            skills::active_skill_definition(slot.id);
        if (definition == nullptr || definition->cooldown_ticks == 0U) {
            slot.id = skills::ActiveSkillId::none;
            slot.empty = true;
            continue;
        }
        static_cast<void>(std::snprintf(slot.name.data(), slot.name.size(),
            "%s", definition->display_name));
        slot.name.back() = '\0';
        const std::size_t cooldown_index = static_cast<std::size_t>(slot.id);
        if (cooldown_index < cooldowns.size()) {
            slot.cooldown_ratio = std::clamp(
                static_cast<float>(cooldowns[cooldown_index])
                    / static_cast<float>(definition->cooldown_ticks),
                0.0F, 1.0F);
        }
    }
    return result;
}

ActiveSkillHudLayout active_skill_hud_layout(
    int width, int height) noexcept {
    if (width <= 0 || height <= 0) return {};
    constexpr float kTotalWidth =
        static_cast<float>(skills::kActiveSkillSlotCount) * kSlotSize
        + static_cast<float>(skills::kActiveSkillSlotCount - 1U) * kSlotGap;
    const float x = (static_cast<float>(width) - kTotalWidth) * 0.5F;
    const float y = static_cast<float>(height) - kSlotSize - kBottomMargin;
    ActiveSkillHudLayout result{};
    result.bounds = {x, y, kTotalWidth, kSlotSize};
    for (std::size_t index = 0U; index < result.slots.size(); ++index) {
        result.slots[index] = {
            x + static_cast<float>(index) * (kSlotSize + kSlotGap),
            y, kSlotSize, kSlotSize,
        };
    }
    return result;
}

}  // namespace arpg::platform
