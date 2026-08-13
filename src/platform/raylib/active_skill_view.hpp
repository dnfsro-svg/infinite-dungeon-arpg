#pragma once

#include "material_asset_types.hpp"
#include "skills/active_skill_types.hpp"
#include "ui_text_bounds_audit.hpp"
#include "ui_text_layout.hpp"

#include <raylib.h>

#include <array>
#include <cstdint>

namespace arpg::platform {

struct ActiveSkillHudSlot final {
    std::uint8_t key_number{};
    std::array<char, 16> key_label{};
    skills::ActiveSkillId id{skills::ActiveSkillId::none};
    MaterialSpriteId icon{MaterialSpriteId::missing};
    std::array<char, 48> name{};
    std::array<char, 16> cooldown_label{};
    float cooldown_ratio{};
    bool empty{};
};

[[nodiscard]] MaterialSpriteId active_skill_icon_sprite(
    skills::ActiveSkillId id) noexcept;

struct ActiveSkillHudModel final {
    std::array<ActiveSkillHudSlot,
        skills::kActiveSkillSlotCount> slots{};
};

inline constexpr std::size_t kActiveSkillTextCapacity = 48U;

struct ActiveSkillHudTextRegions final {
    Rectangle key{};
    Rectangle cooldown{};
    Rectangle name{};
    Rectangle visual_bounds{};
};

struct ActiveSkillTextPlatePlan final {
    Rectangle backing{};
    Vector2 position{};
    Vector2 extent{};
    std::array<char, kActiveSkillTextCapacity> text{};
    float font_size{};
    float spacing{};
    UiTextAuditRole role{UiTextAuditRole::count};
    bool visible{};
    bool truncated{};
};

struct ActiveSkillHudTextPlan final {
    ActiveSkillTextPlatePlan key{};
    ActiveSkillTextPlatePlan cooldown{};
    ActiveSkillTextPlatePlan name{};
};

struct ActiveSkillHudLayout final {
    Rectangle bounds{};
    Rectangle visual_bounds{};
    std::array<Rectangle, skills::kActiveSkillSlotCount> slots{};
    float scale{};
    bool valid{};
    std::array<ActiveSkillHudTextRegions,
        skills::kActiveSkillSlotCount> text_regions{};
};

[[nodiscard]] constexpr Color active_skill_text_plate_backing() noexcept {
    return {5, 9, 16, 255};
}

[[nodiscard]] ActiveSkillHudModel make_active_skill_hud_model(
    const skills::SkillLoadoutState& loadout,
    const std::array<std::uint16_t, skills::kActiveSkillCount>& cooldowns)
    noexcept;

[[nodiscard]] ActiveSkillHudLayout active_skill_hud_layout(
    int width, int height) noexcept;

[[nodiscard]] ActiveSkillHudTextPlan make_active_skill_hud_text_plan(
    const ActiveSkillHudSlot& slot,
    const ActiveSkillHudTextRegions& regions,
    float scale, bool font_ready,
    UiTextMeasureFn measure, void* measure_context) noexcept;

}  // namespace arpg::platform
