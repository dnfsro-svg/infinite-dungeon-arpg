#include "combat_renderer.hpp"

#include "combat_view_math.hpp"
#include "control_hints.hpp"
#include "dungeon_runtime.hpp"
#include "dungeon_view_math.hpp"
#include "hud_font.hpp"
#include "hud_palette.hpp"
#include "hud_renderer.hpp"

#include "ui_text_contrast.hpp"
#include "render_layout.hpp"
#include "ui_material.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace arpg::platform {
namespace {

const char* ecology_name(dungeon::DungeonElement element) noexcept {
    switch (element) {
    case dungeon::DungeonElement::fire: return "FIRE";
    case dungeon::DungeonElement::water: return "WATER";
    case dungeon::DungeonElement::lightning: return "LIGHTNING";
    case dungeon::DungeonElement::chaos: return "CHAOS";
    }
    return "UNKNOWN";
}

void draw_bar(float x, float y, float width, float ratio, Color color) noexcept {
    if (ratio < 0.0F) ratio = 0.0F;
    else if (ratio > 1.0F) ratio = 1.0F;
    DrawRectangleRec({x, y, width, 5.0F}, Color{20, 23, 31, 230});
    DrawRectangleRec({x + 1.0F, y + 1.0F, (width - 2.0F) * ratio, 3.0F}, color);
}

bool has_requested_glyphs(Font font,
    const DeathOverlayFontPlan& plan) noexcept {
    if (!IsFontValid(font)
            || font.glyphCount < static_cast<int>(plan.codepoint_count)
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

[[nodiscard]] float clamped_ratio(float ratio) noexcept {
    if (!std::isfinite(ratio)) return 0.0F;
    return std::clamp(ratio, 0.0F, 1.0F);
}

[[nodiscard]] HudRect player_bar_bounds(const HudLayout& layout,
    float logical_y) noexcept {
    const float scale = layout.scale;
    return {
        layout.player_panel.x + (94.0F * scale),
        layout.player_panel.y + (logical_y * scale),
        std::max(0.0F, layout.player_panel.width - (106.0F * scale)),
        std::max(0.0F, 14.0F * scale),
    };
}

[[nodiscard]] Color color_for_bar(HudBarKind kind) noexcept {
    switch (kind) {
    case HudBarKind::health: return hud_palette_color(HudPaletteId::health);
    case HudBarKind::barrier: return hud_palette_color(HudPaletteId::barrier);
    case HudBarKind::experience: return hud_palette_color(HudPaletteId::experience);
    }
    return hud_palette().text;
}

void draw_player_bar(const HudBarPlan& bar,
    const MaterialPack& assets) noexcept {
    const Color fill = color_for_bar(bar.kind);
    const Rectangle bounds{bar.bounds.x, bar.bounds.y, bar.bounds.width,
        bar.bounds.height};
    UiMaterialElement track = UiMaterialElement::hud_health_track;
    UiMaterialElement filled = UiMaterialElement::hud_health_fill;
    if (bar.kind == HudBarKind::barrier) {
        track = UiMaterialElement::hud_barrier_track;
        filled = UiMaterialElement::hud_barrier_fill;
    } else if (bar.kind == HudBarKind::experience) {
        track = UiMaterialElement::hud_resource_track;
        filled = UiMaterialElement::hud_resource_fill;
    }
    Rectangle track_source{4.0F, 35.0F, 120.0F, 57.0F};
    Rectangle fill_source{4.0F, 36.0F, 120.0F, 55.0F};
    if (bar.kind == HudBarKind::barrier) {
        track_source = {4.0F, 34.0F, 120.0F, 60.0F};
        fill_source = {4.0F, 32.0F, 120.0F, 64.0F};
    } else if (bar.kind == HudBarKind::experience) {
        track_source = {4.0F, 27.0F, 120.0F, 74.0F};
        fill_source = {4.0F, 33.0F, 120.0F, 61.0F};
    }
    if (!assets.draw_horizontal_slice(ui_material_sprite(track),
            track_source, 24.0F, bounds)) {
        DrawRectangleRec(bounds, Color{20, 23, 31, 230});
    }
    const Rectangle fill_bounds{bar.bounds.x + 1.0F, bar.bounds.y + 1.0F,
        std::max(0.0F, bar.bounds.width - 2.0F) * clamped_ratio(bar.ratio),
        std::max(0.0F, bar.bounds.height - 2.0F)};
    if (fill_bounds.width > 0.0F
            && !assets.draw_horizontal_slice(ui_material_sprite(filled),
                fill_source, 24.0F, fill_bounds)) {
        DrawRectangleRec(fill_bounds, fill);
    }
}

}  // namespace

namespace {

[[nodiscard]] const char* status_tag_label(HudStatusTagKind tag) noexcept {
    switch (tag) {
    case HudStatusTagKind::slow: return u8"减速";
    case HudStatusTagKind::corrosion: return u8"腐蚀";
    case HudStatusTagKind::invulnerable: return u8"无敌";
    }
    return "";
}

}  // namespace

PlayerPanelPlan make_player_panel_plan(const PlayerHudModel& player,
    const HudLayout& layout, float presentation_seconds) noexcept {
    PlayerPanelPlan plan{};
    if (!player.visible || layout.player_panel.width <= 0.0F
            || layout.player_panel.height <= 0.0F || layout.scale <= 0.0F) {
        return plan;
    }

    plan.bars[plan.bar_count++] = {
        player_bar_bounds(layout, 26.0F), clamped_ratio(player.hp_ratio),
        HudBarKind::health};
    if (player.max_barrier > 0) {
        plan.bars[plan.bar_count++] = {
            player_bar_bounds(layout, 52.0F), clamped_ratio(player.barrier_ratio),
            HudBarKind::barrier};
    }
    plan.experience_maxed = player.level >= 100U;
    plan.bars[plan.bar_count++] = {
        player_bar_bounds(layout, 78.0F),
        plan.experience_maxed ? 1.0F : clamped_ratio(player.experience_ratio),
        HudBarKind::experience};
    static_cast<void>(std::snprintf(plan.progression_text.bytes.data(),
        plan.progression_text.bytes.size(), u8"LV %u · 未分配点 %u",
        static_cast<unsigned>(player.level),
        static_cast<unsigned>(player.unspent_passive_points)));

    plan.tag_count = std::min<std::uint8_t>(player.status_tag_count,
        static_cast<std::uint8_t>(plan.tags.size()));
    for (std::size_t index = 0U; index < plan.tag_count; ++index) {
        plan.tags[index] = player.status_tags[index];
    }

    const float seconds = std::max(0.0F, presentation_seconds);
    const float pulse_phase = std::fmod(seconds, 0.5F);
    plan.low_health_emphasis = std::isfinite(player.hp_ratio)
        && clamped_ratio(player.hp_ratio) < 0.25F
        && pulse_phase < 0.25F;
    return plan;
}

ObjectivePanelPlan make_objective_panel_plan(const RoomHudModel& room,
    const HudLayout& layout) noexcept {
    ObjectivePanelPlan plan{};
    if (layout.objective_panel.width <= 0.0F
        || layout.objective_panel.height <= 0.0F
        || room.objective.bytes[0] == '\0') {
        return plan;
    }
    plan.visible = true;
    plan.abyss = room.abyss;
    plan.bounds = layout.objective_panel;
    plan.primary = room.objective;
    plan.secondary = room.secondary;
    return plan;
}

NavigationPanelPlan make_navigation_panel_plan(const NavigationHudModel& navigation,
    const HudLayout& layout) noexcept {
    NavigationPanelPlan plan{};
    if (layout.navigation_panel.width <= 0.0F
        || layout.navigation_panel.height <= 0.0F
        || navigation.primary.bytes[0] == '\0') {
        return plan;
    }
    plan.visible = true;
    plan.bounds = layout.navigation_panel;
    plan.primary = navigation.primary;
    plan.ecology = navigation.ecology_label;
    plan.element_count = std::min<std::uint8_t>(navigation.element_count,
        static_cast<std::uint8_t>(plan.elements.size()));
    for (std::size_t index{}; index < plan.element_count; ++index) {
        plan.elements[index] = navigation.elements[index];
    }
    return plan;
}

ContextPanelPlan make_context_panel_plan(const ContextHudModel& context,
    const HudLayout& layout) noexcept {
    ContextPanelPlan plan{};
    plan.primary_bounds = layout.primary_notice;
    plan.secondary_bounds = layout.secondary_notice;
    plan.primary = context.primary;
    plan.secondary = context.secondary;
    plan.primary_kind = context.primary_kind;
    plan.secondary_kind = context.secondary_kind;
    plan.primary_abyss = context.primary_abyss;
    plan.secondary_abyss = context.secondary_abyss;
    plan.primary_visible = context.primary_kind != HudNoticeKind::none
        && context.primary.bytes[0] != '\0'
        && layout.primary_notice.width > 0.0F
        && layout.primary_notice.height > 0.0F;
    plan.secondary_visible = context.secondary_kind != HudNoticeKind::none
        && context.secondary.bytes[0] != '\0'
        && layout.secondary_notice.width > 0.0F
        && layout.secondary_notice.height > 0.0F;
    return plan;
}

namespace {

[[nodiscard]] std::size_t bounded_text_length(const HudText96& text) noexcept {
    std::size_t length{};
    while (length < text.bytes.size() && text.bytes[length] != '\0') ++length;
    return length;
}

[[nodiscard]] std::size_t previous_utf8_boundary(const char* text,
    std::size_t length) noexcept {
    while (length > 0U
        && (static_cast<unsigned char>(text[length]) & 0xC0U) == 0x80U) {
        --length;
    }
    return length;
}

[[nodiscard]] float measure_text(HudTextMeasureFn measure, const char* text,
    float font_size, void* context) noexcept {
    if (measure == nullptr || text == nullptr || !(font_size > 0.0F)) {
        return (std::numeric_limits<float>::infinity)();
    }
    const float width = measure(text, font_size, context);
    return std::isfinite(width) && width >= 0.0F
        ? width : (std::numeric_limits<float>::infinity)();
}

}  // namespace

HudTextDrawPlan make_hud_text_draw_plan(const HudText96& input,
    float bounds_width, float preferred_font_size, float minimum_font_size,
    HudTextMeasureFn measure, void* context) noexcept {
    HudTextDrawPlan plan{};
    if (input.bytes[0] == '\0' || !(bounds_width > 0.0F)
        || !(preferred_font_size > 0.0F) || measure == nullptr) {
        return plan;
    }
    const float minimum = std::clamp(minimum_font_size, 1.0F,
        preferred_font_size);
    plan.visible = true;
    plan.text = input;
    plan.text.bytes.back() = '\0';
    plan.font_size = preferred_font_size;
    while (plan.font_size > minimum
        && measure_text(measure, plan.text.bytes.data(), plan.font_size, context)
            > bounds_width) {
        plan.font_size = std::max(minimum, plan.font_size - 1.0F);
    }
    if (measure_text(measure, plan.text.bytes.data(), plan.font_size, context)
        <= bounds_width) {
        return plan;
    }

    constexpr char kEllipsis[] = "...";
    const std::size_t input_length = bounded_text_length(plan.text);
    std::size_t copied = input_length;
    while (copied > 0U) {
        copied = previous_utf8_boundary(plan.text.bytes.data(), copied - 1U);
        if (copied + sizeof(kEllipsis) > plan.text.bytes.size()) continue;
        char candidate[96]{};
        std::memcpy(candidate, plan.text.bytes.data(), copied);
        std::memcpy(candidate + copied, kEllipsis, sizeof(kEllipsis));
        if (measure_text(measure, candidate, plan.font_size, context)
            <= bounds_width) {
            std::memcpy(plan.text.bytes.data(), candidate, sizeof(candidate));
            plan.text.truncated = true;
            plan.truncated = true;
            return plan;
        }
    }
    plan.visible = false;
    plan.text = {};
    plan.truncated = true;
    return plan;
}

HudReadabilityStyle hud_readability_style() noexcept {
    return {};
}

namespace {

struct RaylibTextMeasureContext final {
    Font font{};
};

float measure_text_ex(const char* text, float font_size, void* context) noexcept {
    if (context == nullptr) return 0.0F;
    const auto* value = static_cast<const RaylibTextMeasureContext*>(context);
    return MeasureTextEx(value->font, text, font_size, 1.0F).x;
}

void draw_hud_text(Font font, const char* text, Vector2 position, float size,
    Color color, int outline_pixels) noexcept {
    const HudReadabilityStyle style = hud_readability_style();
    const UiTextContrastStyle contrast = ui_text_contrast_style();
    DrawTextEx(font, text,
        {position.x + static_cast<float>(style.shadow_pixels),
         position.y + static_cast<float>(style.shadow_pixels)},
        size, 1.0F, contrast.shadow);
    for (int offset_y = -outline_pixels; offset_y <= outline_pixels; ++offset_y) {
        for (int offset_x = -outline_pixels; offset_x <= outline_pixels; ++offset_x) {
            if (offset_x == 0 && offset_y == 0) continue;
            DrawTextEx(font, text, {position.x + static_cast<float>(offset_x),
                position.y + static_cast<float>(offset_y)}, size, 1.0F,
                contrast.shadow);
        }
    }
    for (int offset = style.embolden_pixels; offset > 0; --offset) {
        DrawTextEx(font, text,
            {position.x + static_cast<float>(offset), position.y},
            size, 1.0F, color);
    }
    DrawTextEx(font, text, position, size, 1.0F, color);
}

void draw_panel_text(Font font, const HudRect& bounds, const HudText96& text,
    float size, Color color) noexcept {
    RaylibTextMeasureContext measure{font};
    const HudTextDrawPlan plan = make_hud_text_draw_plan(text,
        std::max(0.0F, bounds.width - 20.0F), size,
        std::min(size, hud_readability_style().panel_minimum_font_size),
        &measure_text_ex, &measure);
    if (!plan.visible) return;
    const UiTextContrastStyle contrast = ui_text_contrast_style();
    const Rectangle backing{
        bounds.x + 4.0F, bounds.y + 2.0F,
        std::max(0.0F, bounds.width - 8.0F),
        std::max(0.0F, bounds.height - 4.0F)};
    DrawRectangleRounded(backing, 0.16F, 4, contrast.backing);
    BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y),
        std::max(0, static_cast<int>(bounds.width)),
        std::max(0, static_cast<int>(bounds.height)));
    draw_hud_text(font, plan.text.bytes.data(),
        {bounds.x + 10.0F,
            bounds.y + std::max(0.0F,
                (bounds.height - plan.font_size) * 0.5F)},
        plan.font_size, color,
        hud_readability_style().outline_pixels);
    EndScissorMode();
}

void draw_context_notice(Font font, const HudRect& bounds,
    const HudText96& text, HudNoticeKind kind, Color color,
    bool abyss, const MaterialPack& assets) noexcept {
    if (kind == HudNoticeKind::none || text.bytes[0] == '\0') return;
    const bool urgent = kind == HudNoticeKind::save_error
        || kind == HudNoticeKind::recovery_required
        || kind == HudNoticeKind::abyss_abandon;
    const Color fill = urgent ? Color{54, 18, 31, 238} : Color{15, 22, 33, 232};
    const Rectangle rectangle{bounds.x, bounds.y, bounds.width, bounds.height};
    const UiMaterialElement notice = abyss
        ? UiMaterialElement::hud_notice_abyss : UiMaterialElement::hud_notice;
    const Rectangle notice_source = abyss
        ? Rectangle{4.0F, 17.0F, 120.0F, 94.0F}
        : Rectangle{4.0F, 22.0F, 120.0F, 83.0F};
    if (!assets.draw_horizontal_slice(ui_material_sprite(notice),
            notice_source, 24.0F, rectangle)) {
        DrawRectangleRounded(rectangle, 0.18F, 6, fill);
        DrawRectangleLinesEx(rectangle, 1.0F, color);
    }
    draw_panel_text(font, bounds, text, 17.0F, color);
}

}  // namespace

HudRenderer::~HudRenderer() noexcept {
    shutdown();
}

bool HudRenderer::initialize() noexcept {
    shutdown();
    const HudFontPlan plan = hud_font_plan();
    if (!plan.covers_required_text) {
        TraceLog(LOG_WARNING, "HUD: Shared font plan misses required Chinese text");
        font_ = GetFontDefault();
        return false;
    }

    for (std::size_t index = 0U; index < plan.shared.candidate_count; ++index) {
        const char* path = plan.shared.candidate_paths[index];
        if (path == nullptr || !FileExists(path)) continue;
        Font candidate = LoadFontEx(path, kUiFontSourceBaseSize,
            plan.shared.codepoints.data(),
            static_cast<int>(plan.shared.codepoint_count));
        if (has_requested_glyphs(candidate, plan.shared)) {
            SetTextureFilter(candidate.texture, TEXTURE_FILTER_BILINEAR);
            font_ = candidate;
            font_ready_ = true;
            TraceLog(LOG_INFO, "HUD: Loaded Chinese font %s (%i glyphs)",
                path, candidate.glyphCount);
            return true;
        }
        if (IsFontValid(candidate)) UnloadFont(candidate);
    }

    font_ = GetFontDefault();
    TraceLog(LOG_WARNING,
        "HUD: No complete Chinese font found; using default font fallback");
    return false;
}

void HudRenderer::shutdown() noexcept {
    const HudFontSelectionPlan selection =
        make_hud_font_selection_plan(font_ready_);
    if (selection.owns_loaded_font && IsWindowReady()) UnloadFont(font_);
    font_ = {};
    font_ready_ = false;
}

bool HudRenderer::font_ready() const noexcept {
    return font_ready_;
}

Font HudRenderer::hud_font() const noexcept {
    return font_;
}

void HudRenderer::draw_ground_loot(
    const GroundLootView& view) const noexcept {
    if (!IsWindowReady()) return;
    const HudFontSelectionPlan selection =
        make_hud_font_selection_plan(font_ready_);
    const Font draw_font = selection.use_default_font
        ? GetFontDefault() : font_;
    const HudPalette palette = hud_palette();
    const HudReadabilityStyle style = hud_readability_style();
    for (std::size_t index = 0U; index < view.count; ++index) {
        const GroundLootLabel& label = view.labels[index];
        const Rectangle bounds{label.rect.x, label.rect.y,
            label.rect.width, label.rect.height};
        const Color text{label.text_color.r, label.text_color.g,
            label.text_color.b, label.text_color.a};
        const Color border{label.border_color.r, label.border_color.g,
            label.border_color.b, label.border_color.a};
        const Color fill = label.abyss
            ? Fade(palette.chaos, 0.42F)
            : Fade(palette.text, 0.13F);
        DrawRectangleRounded(bounds, 0.18F, 6, fill);
        DrawRectangleLinesEx(bounds, label.abyss ? 2.0F : 1.0F, border);
        BeginScissorMode(static_cast<int>(bounds.x),
            static_cast<int>(bounds.y),
            (std::max)(0, static_cast<int>(bounds.width)),
            (std::max)(0, static_cast<int>(bounds.height)));
        draw_hud_text(draw_font, label.text.data(),
            {bounds.x + 8.0F, bounds.y + 3.0F}, 16.0F, text,
            style.outline_pixels);
        EndScissorMode();
    }
}

void HudRenderer::draw_material_loot(
    const MaterialLootView& view) const noexcept {
    if (!IsWindowReady()) return;
    const HudFontSelectionPlan selection =
        make_hud_font_selection_plan(font_ready_);
    const Font draw_font = selection.use_default_font
        ? GetFontDefault() : font_;
    const HudReadabilityStyle style = hud_readability_style();
    for (std::size_t index = 0U; index < view.count; ++index) {
        const MaterialLootLabel& label = view.labels[index];
        const Rectangle bounds{label.rect.x, label.rect.y,
            label.rect.width, label.rect.height};
        const Color color{label.text_color.r, label.text_color.g,
            label.text_color.b, label.text_color.a};
        DrawRectangleRounded(bounds, 0.18F, 6,
            Fade(color, label.emphasized ? 0.31F : 0.15F));
        DrawRectangleRoundedLinesEx(bounds, 0.18F, 6,
            label.emphasized ? 2.5F : 1.0F, color);
        BeginScissorMode(static_cast<int>(bounds.x),
            static_cast<int>(bounds.y),
            (std::max)(0, static_cast<int>(bounds.width)),
            (std::max)(0, static_cast<int>(bounds.height)));
        draw_hud_text(draw_font, label.text.data(),
            {bounds.x + 8.0F, bounds.y + 3.0F}, 16.0F, color,
            style.outline_pixels);
        EndScissorMode();
    }
}

void HudRenderer::draw(const HudViewModel& view,
    const HudLayout& layout) const noexcept {
    static const MaterialPack unavailable_assets{};
    draw(view, layout, unavailable_assets);
}

void HudRenderer::draw(const HudViewModel& view,
    const HudLayout& layout, const MaterialPack& assets) const noexcept {
    if (!IsWindowReady()) return;
    const HudFontSelectionPlan selection =
        make_hud_font_selection_plan(font_ready_);
    const Font draw_font = selection.use_default_font ? GetFontDefault() : font_;
    const PlayerPanelPlan plan = make_player_panel_plan(view.player, layout,
        static_cast<float>(GetTime()));
    const ObjectivePanelPlan objective = make_objective_panel_plan(view.room, layout);
    const NavigationPanelPlan navigation = make_navigation_panel_plan(
        view.navigation, layout);
    const ContextPanelPlan context = make_context_panel_plan(view.context, layout);
    const HudTextSafeLayout text_layout = make_hud_text_safe_layout(layout);

    const HudPalette palette = hud_palette();
    const HudReadabilityStyle style = hud_readability_style();
    if (plan.bar_count != 0U) {
        const Rectangle panel_bounds{layout.player_panel.x, layout.player_panel.y,
            layout.player_panel.width, layout.player_panel.height};
        if (!assets.draw_nine_slice(
                ui_material_sprite(UiMaterialElement::hud_panel),
                panel_bounds)) {
            DrawRectangleRounded(panel_bounds, 0.08F, 6,
                Color{7, 10, 17, 220});
        }
        static_cast<void>(assets.draw_region_fit(
            ui_material_sprite(UiMaterialElement::label_plate),
            {4.0F, 30.0F, 120.0F, 67.0F},
            {panel_bounds.x + 8.0F * layout.scale,
                panel_bounds.y + 3.0F * layout.scale,
                panel_bounds.width - 16.0F * layout.scale,
                23.0F * layout.scale}));

        char text[96]{};
        for (std::size_t index = 0U; index < plan.bar_count; ++index) {
            const HudBarPlan& bar = plan.bars[index];
            switch (bar.kind) {
            case HudBarKind::health:
                std::snprintf(text, sizeof(text), u8"生命 HP %d/%d", view.player.hp,
                    view.player.max_hp);
                break;
            case HudBarKind::barrier:
                std::snprintf(text, sizeof(text), u8"护盾 %d/%d", view.player.barrier,
                    view.player.max_barrier);
                break;
            case HudBarKind::experience:
                if (plan.experience_maxed) {
                    std::snprintf(text, sizeof(text), "XP MAX");
                } else {
                    std::snprintf(text, sizeof(text), "XP %llu/%llu",
                        static_cast<unsigned long long>(view.player.experience),
                        static_cast<unsigned long long>(view.player.required_experience));
                }
                break;
            }
            draw_player_bar(bar, assets);
            draw_hud_text(draw_font, text,
                {layout.player_panel.x + (12.0F * layout.scale),
                bar.bounds.y - (2.0F * layout.scale)},
                style.player_bar_font_size * layout.scale,
                palette.text, style.outline_pixels);
        }
        if (plan.low_health_emphasis) {
            DrawRectangleLinesEx({layout.player_panel.x, layout.player_panel.y,
                layout.player_panel.width, layout.player_panel.height},
                2.0F * layout.scale,
                palette.health);
        }
        draw_hud_text(draw_font, plan.progression_text.bytes.data(),
            {layout.player_panel.x + (12.0F * layout.scale),
                layout.player_panel.y + (102.0F * layout.scale)},
            style.progression_font_size * layout.scale, palette.text,
            style.outline_pixels);
        for (std::size_t index = 0U; index < plan.tag_count; ++index) {
            const float tag_x = layout.player_panel.x
                + ((12.0F + static_cast<float>(index) * 64.0F) * layout.scale);
            const float tag_y = layout.player_panel.y + (122.0F * layout.scale);
            UiMaterialElement tag_material = UiMaterialElement::hud_status_slow;
            if (plan.tags[index] == HudStatusTagKind::corrosion) {
                tag_material = UiMaterialElement::hud_status_corrosion;
            } else if (plan.tags[index] == HudStatusTagKind::invulnerable) {
                tag_material = UiMaterialElement::hud_status_invulnerable;
            }
            const Rectangle tag_bounds{tag_x, tag_y, 56.0F * layout.scale,
                18.0F * layout.scale};
            Rectangle tag_source{5.0F, 4.0F, 117.0F, 120.0F};
            if (tag_material == UiMaterialElement::hud_status_invulnerable) {
                tag_source = {5.0F, 4.0F, 117.0F, 120.0F};
            }
            if (!assets.draw_region_fit(ui_material_sprite(tag_material),
                    tag_source, tag_bounds)) {
                DrawRectangleRounded(tag_bounds, 0.18F, 4,
                    Color{30, 39, 55, 235});
            }
            draw_hud_text(draw_font, status_tag_label(plan.tags[index]),
                {tag_x + (8.0F * layout.scale), tag_y + (1.0F * layout.scale)},
                style.status_tag_font_size * layout.scale, palette.text,
                style.outline_pixels);
        }
    }
    if (objective.visible) {
        const Rectangle bounds{objective.bounds.x, objective.bounds.y,
            objective.bounds.width, objective.bounds.height};
        if (!assets.draw_nine_slice(ui_material_sprite(
                UiMaterialElement::hud_objective_panel), bounds)) {
            DrawRectangleRounded(bounds, 0.12F, 6,
                objective.abyss ? Color{47, 18, 47, 228}
                                : Color{7, 10, 17, 220});
        }
        static_cast<void>(assets.draw_region_fit(
            ui_material_sprite(UiMaterialElement::label_plate),
            {4.0F, 30.0F, 120.0F, 67.0F},
            {bounds.x + 8.0F * layout.scale,
                bounds.y + 5.0F * layout.scale,
                bounds.width - 16.0F * layout.scale,
                24.0F * layout.scale}));
        draw_panel_text(draw_font, text_layout.objective_title,
            objective.primary,
            style.objective_primary_font_size * layout.scale,
            objective.abyss ? palette.chaos : palette.text);
        draw_panel_text(draw_font, text_layout.objective_hint,
            objective.secondary,
            style.objective_secondary_font_size * layout.scale,
            ui_text_contrast_style().primary);
    }
    if (navigation.visible) {
        const Rectangle bounds{navigation.bounds.x, navigation.bounds.y,
            navigation.bounds.width, navigation.bounds.height};
        if (!assets.draw_nine_slice(ui_material_sprite(
                UiMaterialElement::hud_navigation_panel), bounds)) {
            DrawRectangleRounded(bounds, 0.12F, 6, Color{7, 10, 17, 220});
        }
        draw_panel_text(draw_font, text_layout.navigation_title,
            navigation.primary,
            style.navigation_primary_font_size * layout.scale, palette.text);
        draw_panel_text(draw_font, text_layout.navigation_ecology,
            navigation.ecology,
            style.navigation_secondary_font_size * layout.scale,
            ui_text_contrast_style().primary);
        for (std::size_t index{}; index < navigation.element_count; ++index) {
            HudRect element = navigation.bounds;
            element.x += 18.0F * layout.scale;
            element.width -= 36.0F * layout.scale;
            element.y += (58.0F + static_cast<float>(index) * 14.0F)
                * layout.scale;
            element.height = 14.0F * layout.scale;
            draw_panel_text(draw_font, element, navigation.elements[index].label,
                style.navigation_element_font_size * layout.scale,
                hud_palette_color(navigation.elements[index].color_id));
        }
    }
    draw_context_notice(draw_font, context.primary_bounds, context.primary,
        context.primary_kind, context.primary_kind == HudNoticeKind::save_error
            ? palette.error : context.primary_abyss
                ? hud_palette_color(HudPaletteId::chaos) : palette.text,
        context.primary_abyss, assets);
    draw_context_notice(draw_font, context.secondary_bounds, context.secondary,
        context.secondary_kind, context.secondary_abyss
            ? hud_palette_color(HudPaletteId::chaos) : palette.text,
        context.secondary_abyss, assets);
}

void CombatRenderer::draw_abyss_hud(
    const dungeon::DungeonSnapshot& current,
    float x,
    int& y,
    int line_step) const noexcept {
    // Task 6 routes the confirmation into ContextHudModel.  Host composition
    // of that model is deliberately deferred to Task 7.
    static_cast<void>(current);
    static_cast<void>(x);
    static_cast<void>(y);
    static_cast<void>(line_step);
}

void CombatRenderer::draw_hud() const noexcept {
    hud_renderer_.draw(hud_model_, hud_layout_, material_pack_);
}

}  // namespace arpg::platform
