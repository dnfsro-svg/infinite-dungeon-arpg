#include "active_skill_view.hpp"

#include "skills/active_skill_catalog.hpp"
#include "ui_typography.hpp"
#include "ui_utf8.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdio>

namespace arpg::platform {
namespace {

constexpr float kSlotSize = 76.0F;
constexpr float kSlotGap = 8.0F;
constexpr float kBottomMargin = 20.0F;

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool valid_rectangle(Rectangle bounds) noexcept {
    return finite(bounds.x) && finite(bounds.y)
        && finite(bounds.width) && finite(bounds.height)
        && bounds.width > 0.0F && bounds.height > 0.0F
        && finite(bounds.x + bounds.width)
        && finite(bounds.y + bounds.height);
}

[[nodiscard]] bool c_string_is_valid_utf8(
    const char* text, std::size_t capacity) noexcept {
    return text != nullptr && std::memchr(text, '\0', capacity) != nullptr
        && ui_utf8_is_valid(text);
}

[[nodiscard]] Rectangle rectangle_union(
    Rectangle first, Rectangle second) noexcept {
    const float left = std::min(first.x, second.x);
    const float top = std::min(first.y, second.y);
    const float right = std::max(first.x + first.width,
        second.x + second.width);
    const float bottom = std::max(first.y + first.height,
        second.y + second.height);
    return {left, top, right - left, bottom - top};
}

[[nodiscard]] ActiveSkillTextPlatePlan make_text_plate_plan(
    const char* source, Rectangle backing, float scale, UiTextAuditRole role,
    UiTextMeasureFn measure, void* measure_context) noexcept {
    ActiveSkillTextPlatePlan result{};
    result.backing = backing;
    result.role = role;
    const UiCenteredTextLayout fitted = fit_centered_ui_text(
        result.text.data(), result.text.size(), source, backing,
        16.0F * scale, 9.0F * scale, 0.5F * scale, measure,
        measure_context);
    if (!fitted.drawable) return {};
    result.position = fitted.position;
    result.extent = fitted.size;
    result.font_size = fitted.font_size;
    result.spacing = fitted.spacing;
    result.role = role;
    result.visible = true;
    result.truncated = fitted.truncated;
    return result;
}

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
        static_cast<void>(ui_utf8_format(slot.key_label.data(),
            slot.key_label.size(), u8"小键盘%u",
            static_cast<unsigned>(slot.key_number)));
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
        static_cast<void>(ui_utf8_copy(slot.name.data(), slot.name.size(),
            active_skill_hud_name(slot.id, *definition)));
        const std::size_t cooldown_index = static_cast<std::size_t>(slot.id);
        if (cooldown_index < cooldowns.size()) {
            slot.cooldown_ratio = std::clamp(
                static_cast<float>(cooldowns[cooldown_index])
                    / static_cast<float>(definition->cooldown_ticks),
                0.0F, 1.0F);
            if (slot.cooldown_ratio > 0.0F) {
                const float seconds = std::ceil(slot.cooldown_ratio
                    * static_cast<float>(definition->cooldown_ticks) / 60.0F);
                static_cast<void>(ui_utf8_format(slot.cooldown_label.data(),
                    slot.cooldown_label.size(), u8"%.0f秒", seconds));
            }
        }
    }
    return result;
}

ActiveSkillHudLayout active_skill_hud_layout(
    int width, int height) noexcept {
    if (width <= 0 || height <= 0) return {};
    const float scale = ui_viewport_scale(width, height);
    if (!finite(scale) || scale <= 0.0F) return {};
    const float total_width = scale * (
        static_cast<float>(skills::kActiveSkillSlotCount) * kSlotSize
        + static_cast<float>(skills::kActiveSkillSlotCount - 1U) * kSlotGap);
    const float x = (static_cast<float>(width) - total_width) * 0.5F;
    const float y = static_cast<float>(height)
        - (kSlotSize + kBottomMargin) * scale;
    const Rectangle viewport{0.0F, 0.0F, static_cast<float>(width),
        static_cast<float>(height)};
    ActiveSkillHudLayout result{};
    result.bounds = {x, y, total_width, kSlotSize * scale};
    result.scale = scale;
    if (!valid_rectangle(viewport) || !valid_rectangle(result.bounds)) {
        return {};
    }
    for (std::size_t index = 0U; index < result.slots.size(); ++index) {
        result.slots[index] = {
            x + static_cast<float>(index) * (kSlotSize + kSlotGap) * scale,
            y, kSlotSize * scale, kSlotSize * scale,
        };
        ActiveSkillHudTextRegions& regions = result.text_regions[index];
        const Rectangle& slot = result.slots[index];
        regions.key = {slot.x + 4.0F * scale, slot.y - 20.0F * scale,
            slot.width - 8.0F * scale, 18.0F * scale};
        regions.cooldown = {slot.x + 16.0F * scale,
            slot.y + 28.0F * scale, slot.width - 32.0F * scale,
            20.0F * scale};
        regions.name = {slot.x + 7.0F * scale, slot.y + 77.0F * scale,
            slot.width - 14.0F * scale, 18.0F * scale};
        regions.visual_bounds = rectangle_union(slot, regions.key);
        regions.visual_bounds = rectangle_union(
            regions.visual_bounds, regions.cooldown);
        regions.visual_bounds = rectangle_union(
            regions.visual_bounds, regions.name);
        if (!valid_rectangle(slot) || !valid_rectangle(regions.key)
                || !valid_rectangle(regions.cooldown)
                || !valid_rectangle(regions.name)
                || !valid_rectangle(regions.visual_bounds)
                || !ui_text_bounds_inside(regions.visual_bounds, viewport)) {
            return {};
        }
        if (index == 0U) {
            result.visual_bounds = regions.visual_bounds;
        } else {
            result.visual_bounds = rectangle_union(
                result.visual_bounds, regions.visual_bounds);
        }
    }
    if (!valid_rectangle(result.visual_bounds)
            || !ui_text_bounds_inside(result.visual_bounds, viewport)) {
        return {};
    }
    result.valid = true;
    return result;
}

ActiveSkillHudTextPlan make_active_skill_hud_text_plan(
    const ActiveSkillHudSlot& slot,
    const ActiveSkillHudTextRegions& regions,
    float scale, bool font_ready,
    UiTextMeasureFn measure, void* measure_context) noexcept {
    if (!font_ready || !finite(scale) || scale <= 0.0F || measure == nullptr
            || !valid_rectangle(regions.key)
            || !valid_rectangle(regions.cooldown)
            || !valid_rectangle(regions.name)
            || !valid_rectangle(regions.visual_bounds)
            || !c_string_is_valid_utf8(slot.key_label.data(),
                slot.key_label.size())
            || !c_string_is_valid_utf8(slot.name.data(), slot.name.size())
            || !c_string_is_valid_utf8(slot.cooldown_label.data(),
                slot.cooldown_label.size())) {
        return {};
    }

    ActiveSkillHudTextPlan result{};
    result.key = make_text_plate_plan(slot.key_label.data(), regions.key,
        scale, UiTextAuditRole::hud_skill_key, measure, measure_context);
    if (!result.key.visible) return {};
    if (slot.empty) return result;

    result.name = make_text_plate_plan(slot.name.data(), regions.name, scale,
        UiTextAuditRole::hud_skill_name, measure, measure_context);
    if (!finite(slot.cooldown_ratio) || slot.cooldown_ratio <= 0.0F
            || slot.cooldown_label[0U] == '\0') {
        return result;
    }

    result.cooldown = make_text_plate_plan(slot.cooldown_label.data(),
        regions.cooldown, scale, UiTextAuditRole::hud_skill_cooldown,
        measure, measure_context);
    return result;
}

}  // namespace arpg::platform
