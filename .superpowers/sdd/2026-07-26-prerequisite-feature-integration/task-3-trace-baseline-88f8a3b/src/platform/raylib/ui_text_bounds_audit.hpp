#pragma once

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

enum class UiTextAuditPage : std::uint8_t {
    hud,
    inventory,
    skill,
    pause,
    count,
};

enum class UiTextAuditRole : std::uint8_t {
    hud_health,
    hud_barrier,
    hud_experience,
    hud_progression,
    hud_objective,
    hud_navigation,
    hud_skill_name,
    inventory_page_title,
    inventory_equipment_slot,
    inventory_statistics,
    inventory_grid_entry,
    inventory_material_entry,
    inventory_detail,
    inventory_status,
    skill_page_title,
    skill_main_slot,
    skill_description,
    skill_inventory_entry,
    pause_title,
    pause_row,
    pause_footer,
    count,
};

struct UiTextAuditPageStatus final {
    bool bounds_safe{true};
    bool sizes_readable{true};
    std::uint64_t observed_roles{};
    std::uint64_t failed_bounds_roles{};
    std::uint64_t failed_size_roles{};
    std::uint32_t measured_text_count{};
    float minimum_display_font_size{};
};

struct UiTextBoundsAuditStatus final {
    std::array<UiTextAuditPageStatus,
        static_cast<std::size_t>(UiTextAuditPage::count)> pages{};
};

void reset_ui_text_bounds_audit() noexcept;
[[nodiscard]] bool ui_text_bounds_inside(
    Rectangle inner, Rectangle outer) noexcept;
[[nodiscard]] bool ui_text_bounds_separated(
    Rectangle first, Rectangle second) noexcept;
void record_ui_text_bounds(UiTextAuditPage page, UiTextAuditRole role,
    Font font, const char* text, Vector2 position, float font_size,
    float spacing, Rectangle container, float minimum_font_size,
    const Rectangle* blockers = nullptr, std::size_t blocker_count = 0U)
    noexcept;
[[nodiscard]] UiTextBoundsAuditStatus ui_text_bounds_audit_status() noexcept;

}  // namespace arpg::platform
