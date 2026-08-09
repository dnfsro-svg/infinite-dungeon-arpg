#include "shared_ui_font.hpp"

#include "ui_text_catalog.hpp"
#include "ui_utf8.hpp"

namespace arpg::platform {
namespace {

[[nodiscard]] bool has_requested_glyphs(Font font,
    const UiGlyphPlan& plan) noexcept {
    if (!IsFontValid(font)
            || font.glyphCount != static_cast<int>(plan.codepoint_count)
            || font.glyphs == nullptr) {
        return false;
    }
    for (std::size_t requested = 0U; requested < plan.codepoint_count;
         ++requested) {
        bool found = false;
        for (int glyph = 0; glyph < font.glyphCount; ++glyph) {
            if (font.glyphs[glyph].value == plan.codepoints[requested]) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

void release_font(Font& font) noexcept {
    UnloadFont(font);
    font = {};
}

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

SharedUiFont::~SharedUiFont() noexcept {
    shutdown();
}

bool SharedUiFont::initialize() noexcept {
    if (ready_) return true;
    plan_ = ui_glyph_plan();
    if (plan_.capacity_exceeded || plan_.codepoint_count == 0U
            || plan_.codepoint_count > plan_.codepoints.size()) {
        TraceLog(LOG_ERROR, "Shared UI font glyph plan is invalid");
        return false;
    }

    for (std::size_t index = 0U; index < plan_.candidate_count; ++index) {
        const char* const path = plan_.candidate_paths[index];
        if (path == nullptr || !FileExists(path)) continue;
        Font candidate = LoadFontEx(path, kUiFontSourceBaseSize,
            plan_.codepoints.data(), static_cast<int>(plan_.codepoint_count));
        const std::size_t candidate_atlas_bytes = ui_font_atlas_bytes(
            candidate.texture.width, candidate.texture.height,
            candidate.texture.format);
        if (ui_font_candidate_is_acceptable(candidate, plan_)) {
            SetTextureFilter(candidate.texture, TEXTURE_FILTER_BILINEAR);
            font_ = candidate;
            ready_ = true;
            TraceLog(LOG_INFO,
                "Shared UI font loaded %s (%i glyphs, %llu atlas bytes)",
                path, candidate.glyphCount,
                static_cast<unsigned long long>(candidate_atlas_bytes));
            return true;
        }
        release_font(candidate);
    }

    font_ = {};
    TraceLog(LOG_ERROR, "Shared UI font has no valid bundled candidate");
    return false;
}

void SharedUiFont::shutdown() noexcept {
    if (ready_ && IsWindowReady()) release_font(font_);
    else font_ = {};
    ready_ = false;
}

bool SharedUiFont::ready() const noexcept {
    return ready_;
}

Font SharedUiFont::font() const noexcept {
    return font_;
}

std::size_t SharedUiFont::atlas_bytes() const noexcept {
    return ready_ ? ui_font_atlas_bytes(
        font_.texture.width, font_.texture.height,
        font_.texture.format) : 0U;
}

std::size_t SharedUiFont::active_instance_count() const noexcept {
    return ready_ ? 1U : 0U;
}

std::size_t SharedUiFont::total_atlas_bytes() const noexcept {
    return atlas_bytes();
}

const UiGlyphPlan& SharedUiFont::plan() const noexcept {
    return plan_;
}

UiGlyphPlan build_ui_glyph_plan(
    UiGlyphPlanCatalogVisitor visit_catalog) noexcept {
    UiGlyphPlan plan{};
    plan.candidate_paths = {{"assets/fonts/NotoSansCJKsc-Medium.otf"}};
    plan.candidate_count = 1U;
    for (int codepoint = 32; codepoint <= 126; ++codepoint) {
        add_codepoint(plan, codepoint);
    }
    if (visit_catalog != nullptr) visit_catalog(&add_catalog_text, &plan);
    return plan;
}

UiGlyphPlan ui_glyph_plan() noexcept {
    return build_ui_glyph_plan(&visit_ui_text_catalog);
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

bool ui_font_candidate_is_acceptable(Font font,
    const UiGlyphPlan& plan) noexcept {
    if (!IsTextureValid(font.texture)
        || ui_font_atlas_bytes(font.texture.width, font.texture.height,
               font.texture.format) == 0U
        || ui_font_atlas_bytes(font.texture.width, font.texture.height,
               font.texture.format) > kUiFontAtlasByteBudget) {
        return false;
    }
    return has_requested_glyphs(font, plan);
}

}  // namespace arpg::platform
