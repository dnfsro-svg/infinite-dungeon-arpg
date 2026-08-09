#include "shared_ui_font.hpp"

#include "ui_text_catalog.hpp"
#include "ui_utf8.hpp"

namespace arpg::platform {
namespace {

[[nodiscard]] bool is_continuation(unsigned char byte) noexcept {
    return byte >= 0x80U && byte <= 0xBFU;
}

[[nodiscard]] bool decode_codepoint(const char*& cursor,
    int& codepoint) noexcept {
    const auto* const bytes = reinterpret_cast<const unsigned char*>(cursor);
    const unsigned char first = bytes[0];
    if (first <= 0x7FU) {
        codepoint = static_cast<int>(first);
        ++cursor;
        return true;
    }

    const unsigned char second = bytes[1];
    if (first >= 0xC2U && first <= 0xDFU) {
        if (!is_continuation(second)) return false;
        codepoint = static_cast<int>(((first & 0x1FU) << 6U)
            | (second & 0x3FU));
        cursor += 2;
        return true;
    }

    if (first >= 0xE0U && first <= 0xEFU) {
        const unsigned char third = bytes[2];
        if (!is_continuation(second) || !is_continuation(third)
            || (first == 0xE0U && second < 0xA0U)
            || (first == 0xEDU && second > 0x9FU)) {
            return false;
        }
        codepoint = static_cast<int>(((first & 0x0FU) << 12U)
            | ((second & 0x3FU) << 6U) | (third & 0x3FU));
        cursor += 3;
        return true;
    }

    if (first >= 0xF0U && first <= 0xF4U) {
        const unsigned char third = bytes[2];
        const unsigned char fourth = bytes[3];
        if (!is_continuation(second) || !is_continuation(third)
            || !is_continuation(fourth)
            || (first == 0xF0U && second < 0x90U)
            || (first == 0xF4U && second > 0x8FU)) {
            return false;
        }
        codepoint = static_cast<int>(((first & 0x07U) << 18U)
            | ((second & 0x3FU) << 12U) | ((third & 0x3FU) << 6U)
            | (fourth & 0x3FU));
        cursor += 4;
        return true;
    }

    return false;
}

void add_codepoint(UiGlyphPlan& plan, int codepoint) noexcept {
    if (ui_glyph_plan_has_codepoint(plan, codepoint)) return;
    if (plan.codepoint_count == plan.codepoints.size()) {
        plan.capacity_exceeded = true;
        return;
    }
    plan.codepoints[plan.codepoint_count++] = codepoint;
}

void add_catalog_text(const char* text, void* context) noexcept {
    auto& plan = *static_cast<UiGlyphPlan*>(context);
    if (text == nullptr || !ui_utf8_is_valid(text)) {
        plan.capacity_exceeded = true;
        return;
    }

    const char* cursor = text;
    while (*cursor != '\0') {
        int codepoint{};
        if (!decode_codepoint(cursor, codepoint)) {
            plan.capacity_exceeded = true;
            return;
        }
        add_codepoint(plan, codepoint);
    }
}

}  // namespace

UiGlyphPlan ui_glyph_plan() noexcept {
    UiGlyphPlan plan{};
    plan.candidate_paths = {{"assets/fonts/NotoSansCJKsc-Medium.otf"}};
    plan.candidate_count = 1U;
    for (int codepoint = 32; codepoint <= 126; ++codepoint) {
        add_codepoint(plan, codepoint);
    }
    visit_ui_text_catalog(&add_catalog_text, &plan);
    return plan;
}

bool ui_glyph_plan_has_codepoint(const UiGlyphPlan& plan,
    int codepoint) noexcept {
    for (std::size_t index = 0U; index < plan.codepoint_count; ++index) {
        if (plan.codepoints[index] == codepoint) return true;
    }
    return false;
}

bool ui_glyph_plan_covers_text(const UiGlyphPlan& plan,
    const char* utf8_text) noexcept {
    if (plan.capacity_exceeded || utf8_text == nullptr
        || !ui_utf8_is_valid(utf8_text)) {
        return false;
    }

    const char* cursor = utf8_text;
    while (*cursor != '\0') {
        int codepoint{};
        if (!decode_codepoint(cursor, codepoint)
            || !ui_glyph_plan_has_codepoint(plan, codepoint)) {
            return false;
        }
    }
    return true;
}

}  // namespace arpg::platform
