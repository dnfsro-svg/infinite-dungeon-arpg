#include "passive_tree_renderer.hpp"

#include "dungeon_runtime.hpp"
#include "passive_tree_view_math.hpp"
#include "passives/passive_tree_catalog.hpp"
#include "ui_text_catalog.hpp"
#include "ui_text_contrast.hpp"
#include "ui_text_layout.hpp"
#include "ui_text_renderer.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace arpg::platform {
namespace {

Color route_color(passives::PassiveNodeId node) noexcept {
    if (node >= 8U && node <= 21U) return {239, 96, 69, 255};
    if (node >= 22U && node <= 35U) return {78, 181, 240, 255};
    if (node >= 36U && node <= 49U) return {246, 211, 76, 255};
    if (node >= 50U) return {183, 82, 226, 255};
    return {176, 195, 218, 255};
}

Color state_color(PassiveNodeVisualState state,
    passives::PassiveNodeId node) noexcept {
    switch (state) {
    case PassiveNodeVisualState::available: return route_color(node);
    case PassiveNodeVisualState::allocated: return {244, 248, 255, 255};
    case PassiveNodeVisualState::pending: return {255, 190, 79, 255};
    case PassiveNodeVisualState::locked: return {76, 87, 106, 255};
    }
    return GRAY;
}

bool use_passive_tree_hud_font(Font hud_font, bool hud_font_ready) noexcept {
    return passive_tree_text_draw_mode(hud_font_ready)
            == PassiveTreeTextDrawMode::hud_font
        && IsFontValid(hud_font);
}

struct PassiveTextMeasureContext final {
    Font font{};
};

UiTextExtent measure_passive_tree_text(const char* text,
    float font_size, float spacing, void* context) noexcept {
    if (context == nullptr || text == nullptr) return {};
    const auto* measure_context =
        static_cast<const PassiveTextMeasureContext*>(context);
    if (!IsFontValid(measure_context->font)) return {};
    const Vector2 extent = MeasureTextEx(
        measure_context->font, text, font_size, spacing);
    return {extent.x, extent.y};
}

void draw_centered_passive_tree_text(Font hud_font, bool hud_font_ready,
    const char* text, Rectangle bounds, float preferred_font_size,
    float minimum_font_size, Color color, Color opaque_backing) noexcept {
    if (!use_passive_tree_hud_font(hud_font, hud_font_ready)
        || text == nullptr || text[0] == '\0' || opaque_backing.a != 255U
        || ui_luma_contrast_ratio(color, opaque_backing) < 4.5F) {
        return;
    }
    std::array<char, kPassiveNodeBenefitCapacity> fitted{};
    PassiveTextMeasureContext context{hud_font};
    const UiCenteredTextLayout layout = fit_centered_ui_text(
        fitted.data(), fitted.size(), text, bounds,
        preferred_font_size, minimum_font_size, 0.5F,
        &measure_passive_tree_text, &context);
    if (!layout.drawable) return;
    draw_crisp_ui_text(hud_font, layout.text, layout.position,
        layout.font_size, layout.spacing, color, 1);
}

void draw_node_state_mark(PassiveNodeStateMark mark,
    PassiveNodeProjection projection, Color color) noexcept {
    const Vector2 center{projection.center.x, projection.center.y};
    const float radius = projection.radius;
    const Vector2 badge{center.x + radius * 0.62F,
        center.y - radius * 0.62F};
    const float arm = std::max(1.5F, radius * 0.18F);
    switch (mark) {
    case PassiveNodeStateMark::locked_cross: {
        DrawLineEx({badge.x - arm, badge.y - arm},
            {badge.x + arm, badge.y + arm}, 2.0F, color);
        DrawLineEx({badge.x - arm, badge.y + arm},
            {badge.x + arm, badge.y - arm}, 2.0F, color);
        break;
    }
    case PassiveNodeStateMark::available_ring:
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y),
            radius + 3.0F, color);
        break;
    case PassiveNodeStateMark::allocated_dot:
        DrawCircle(static_cast<int>(badge.x), static_cast<int>(badge.y),
            std::max(2.0F, radius * 0.2F), color);
        break;
    case PassiveNodeStateMark::pending_hourglass: {
        DrawLineEx({badge.x - arm, badge.y - arm},
            {badge.x + arm, badge.y - arm}, 2.0F, color);
        DrawLineEx({badge.x - arm, badge.y + arm},
            {badge.x + arm, badge.y + arm}, 2.0F, color);
        DrawLineEx({badge.x - arm, badge.y - arm},
            {badge.x + arm, badge.y + arm}, 2.0F, color);
        DrawLineEx({badge.x + arm, badge.y - arm},
            {badge.x - arm, badge.y + arm}, 2.0F, color);
        break;
    }
    }
}

void draw_node_tooltip(const dungeon::DungeonSnapshot& snapshot,
    passives::PassiveNodeId node,
    Font hud_font, bool hud_font_ready) noexcept {
    constexpr Color kTooltipBacking{9, 13, 22, 255};
    const PassiveTreeTooltipLayout layout = passive_tree_tooltip_layout(
        GetScreenWidth(), GetScreenHeight());
    const PassiveNodeTextView text = passive_node_text_view(snapshot, node);
    if (!layout.valid || !text.valid) return;
    DrawRectangleRounded(layout.panel, 0.08F, 6, kTooltipBacking);
    DrawRectangleLinesEx(layout.panel, 2.0F, route_color(node));
    draw_centered_passive_tree_text(hud_font, hud_font_ready,
        text.name.data(), layout.name, 22.0F, 11.0F,
        Color{236, 242, 252, 255}, kTooltipBacking);
    draw_centered_passive_tree_text(hud_font, hud_font_ready,
        text.type_state.data(), layout.type_state, 15.0F, 9.0F,
        Color{201, 213, 232, 255}, kTooltipBacking);
    draw_centered_passive_tree_text(hud_font, hud_font_ready,
        text.benefit.data(), layout.benefit, 15.0F, 9.0F,
        Color{188, 222, 196, 255}, kTooltipBacking);
    draw_centered_passive_tree_text(hud_font, hud_font_ready,
        text.cost.data(), layout.cost, 15.0F, 9.0F,
        Color{255, 174, 150, 255}, kTooltipBacking);
    draw_centered_passive_tree_text(hud_font, hud_font_ready,
        text.action.data(), layout.action, 14.0F, 9.0F,
        Color{201, 213, 232, 255}, kTooltipBacking);
}

}  // namespace

PassiveTreeTextDrawMode passive_tree_text_draw_mode(
    bool hud_font_ready) noexcept {
    return hud_font_ready ? PassiveTreeTextDrawMode::hud_font
                          : PassiveTreeTextDrawMode::unavailable;
}

void draw_passive_tree_overlay(const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status,
    const char* passive_tree_binding_label,
    Font hud_font, bool hud_font_ready) noexcept {
    constexpr Color kOverlayBacking{5, 8, 17, 255};
    const int screen_width = GetScreenWidth();
    const int screen_height = GetScreenHeight();
    const float width = static_cast<float>(screen_width);
    const float height = static_cast<float>(screen_height);
    DrawRectangle(0, 0, screen_width, screen_height, kOverlayBacking);
    DrawRectangleLinesEx({16.0F, 16.0F, width - 32.0F, height - 32.0F}, 2.0F,
        Color{83, 115, 160, 255});

    const auto& nodes = passives::passive_nodes();
    for (const passives::PassiveNode& node : nodes) {
        const PassiveNodeProjection from = project_passive_node(node.id, width, height);
        for (std::size_t index = 0U; index < node.neighbor_count; ++index) {
            const passives::PassiveNodeId neighbor = node.neighbors[index];
            if (neighbor <= node.id) continue;
            const PassiveNodeProjection to = project_passive_node(neighbor, width, height);
            const bool lit = passive_node_visual_state(snapshot, node.id)
                    == PassiveNodeVisualState::allocated
                && passive_node_visual_state(snapshot, neighbor)
                    == PassiveNodeVisualState::allocated;
            DrawLineEx({from.center.x, from.center.y}, {to.center.x, to.center.y},
                lit ? 3.0F : 1.0F,
                lit ? route_color(node.id) : Color{56, 67, 86, 255});
        }
    }

    for (const passives::PassiveNode& node : nodes) {
        const PassiveNodeProjection projection = project_passive_node(node.id, width, height);
        const PassiveNodeVisualState state = passive_node_visual_state(snapshot, node.id);
        const Color color = state_color(state, node.id);
        DrawCircle(static_cast<int>(projection.center.x), static_cast<int>(projection.center.y),
            projection.radius, Color{16, 23, 38, 255});
        DrawCircleLines(static_cast<int>(projection.center.x), static_cast<int>(projection.center.y),
            projection.radius, color);
        const PassiveNodeVisualCue cue = passive_node_visual_cue(state);
        const PassiveNodeTextView text = passive_node_text_view(
            snapshot, node.id);
        const PassiveNodeNameBounds name_bounds = passive_node_name_bounds(
            node.id, screen_width, screen_height);
        if (text.valid && name_bounds.valid) {
            draw_centered_passive_tree_text(hud_font, hud_font_ready,
                text.name.data(), name_bounds.bounds, 12.0F, 7.0F,
                Color{236, 242, 252, 255}, kOverlayBacking);
        }
        draw_node_state_mark(cue.mark, projection, color);
    }

    const PassiveTreeOverlayLayout layout = passive_tree_overlay_layout(
        screen_width, screen_height);
    const PassiveTreeHeaderText header = passive_tree_header_text(
        snapshot.progression.unspent_passive_points,
        passive_tree_binding_label);
    const PassiveTreeStatusView status = passive_tree_status_view({
        snapshot.passive_save_pending
            || runtime_status.indicator == SaveIndicator::saving,
        runtime_status.indicator == SaveIndicator::error,
        snapshot.passive_tree_error});
    if (layout.valid && header.valid) {
        draw_centered_passive_tree_text(hud_font, hud_font_ready,
            ui_text(UiTextId::passive_title), layout.title,
            28.0F, 16.0F, Color{226, 237, 255, 255}, kOverlayBacking);
        draw_centered_passive_tree_text(hud_font, hud_font_ready,
            header.points.data(), layout.points,
            18.0F, 11.0F, Color{157, 220, 255, 255}, kOverlayBacking);
        draw_centered_passive_tree_text(hud_font, hud_font_ready,
            header.close.data(), layout.close,
            18.0F, 11.0F, Color{201, 213, 232, 255}, kOverlayBacking);
        const Color status_color = status.tone == PassiveTreeStatusTone::error
            ? Color{255, 139, 139, 255}
            : status.tone == PassiveTreeStatusTone::saving
                ? Color{255, 211, 125, 255}
                : Color{156, 224, 183, 255};
        draw_centered_passive_tree_text(hud_font, hud_font_ready,
            status.text, layout.status, 16.0F, 10.0F,
            status_color, kOverlayBacking);
    }

    const Vector2 mouse = GetMousePosition();
    if (const auto hovered = hit_test_passive_node({mouse.x, mouse.y}, width, height)) {
        draw_node_tooltip(snapshot, *hovered, hud_font, hud_font_ready);
    }
}

}  // namespace arpg::platform
