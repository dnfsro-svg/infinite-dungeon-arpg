#include "active_skill_view.hpp"

#include "skills/active_skill_catalog.hpp"
#include "ui_typography.hpp"

#include <algorithm>
#include <cstdio>

namespace arpg::platform {
namespace {

constexpr float kSlotSize = 76.0F;
constexpr float kSlotGap = 8.0F;
constexpr float kBottomMargin = 20.0F;

[[nodiscard]] const char* active_skill_hud_name(
    skills::ActiveSkillId id,
    const skills::ActiveSkillDefinition& definition) noexcept {
    if (id == skills::ActiveSkillId::storm_swords) return u8"暴风式";
    return definition.display_name;
}

}  // namespace

MaterialSpriteId active_skill_icon_sprite(skills::ActiveSkillId id) noexcept {
    switch (id) {
    case skills::ActiveSkillId::draw_slash:
        return MaterialSpriteId::skill_icon_draw_slash;
    case skills::ActiveSkillId::storm_swords:
        return MaterialSpriteId::skill_icon_storm_swords;
    case skills::ActiveSkillId::none:
    case skills::ActiveSkillId::count:
        return MaterialSpriteId::missing;
    }
    return MaterialSpriteId::missing;
}

ActiveSkillHudModel make_active_skill_hud_model(
    const skills::SkillLoadoutState& loadout,
    const std::array<std::uint16_t, skills::kActiveSkillCount>& cooldowns)
    noexcept {
    ActiveSkillHudModel result{};
    for (std::size_t index = 0U; index < result.slots.size(); ++index) {
        ActiveSkillHudSlot& slot = result.slots[index];
        slot.key_number = static_cast<std::uint8_t>(index + 1U);
        static_cast<void>(std::snprintf(slot.key_label.data(),
            slot.key_label.size(), "Num%u",
            static_cast<unsigned>(slot.key_number)));
        slot.key_label.back() = '\0';
        slot.id = loadout.slots[index].active;
        slot.icon = active_skill_icon_sprite(slot.id);
        slot.empty = slot.id == skills::ActiveSkillId::none;
        if (slot.empty) continue;
        const skills::ActiveSkillDefinition* const definition =
            skills::active_skill_definition(slot.id);
        if (definition == nullptr || definition->cooldown_ticks == 0U) {
            slot.id = skills::ActiveSkillId::none;
            slot.icon = MaterialSpriteId::missing;
            slot.empty = true;
            continue;
        }
        static_cast<void>(std::snprintf(slot.name.data(), slot.name.size(),
            "%s", active_skill_hud_name(slot.id, *definition)));
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
    const float scale = ui_viewport_scale(width, height);
    const float total_width = scale * (
        static_cast<float>(skills::kActiveSkillSlotCount) * kSlotSize
        + static_cast<float>(skills::kActiveSkillSlotCount - 1U) * kSlotGap);
    const float x = (static_cast<float>(width) - total_width) * 0.5F;
    const float y = static_cast<float>(height)
        - (kSlotSize + kBottomMargin) * scale;
    ActiveSkillHudLayout result{};
    result.bounds = {x, y, total_width, kSlotSize * scale};
    for (std::size_t index = 0U; index < result.slots.size(); ++index) {
        result.slots[index] = {
            x + static_cast<float>(index) * (kSlotSize + kSlotGap) * scale,
            y, kSlotSize * scale, kSlotSize * scale,
        };
    }
    return result;
}

}  // namespace arpg::platform
