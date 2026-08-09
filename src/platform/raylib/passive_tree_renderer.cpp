#include "passive_tree_renderer.hpp"

#include "dungeon_runtime.hpp"
#include "passive_tree_view_math.hpp"
#include "passives/passive_tree_catalog.hpp"
#include "ui_text_renderer.hpp"

#include <raylib.h>

#include <algorithm>
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
    case PassiveNodeVisualState::rejected: return {244, 99, 99, 255};
    case PassiveNodeVisualState::pending: return {255, 190, 79, 255};
    case PassiveNodeVisualState::locked: return {76, 87, 106, 255};
    }
    return GRAY;
}

std::size_t route_index(passives::PassiveNodeId node) noexcept {
    return node < 8U ? 0U : static_cast<std::size_t>((node - 8U) / 14U);
}

const char* passive_benefit_text(passives::PassiveNodeId node) noexcept {
    constexpr const char* kFlat[] = {
        u8"\u6536\u76ca: +3 Fire damage", u8"\u6536\u76ca: +3 Water damage",
        u8"\u6536\u76ca: +3 Lightning damage", u8"\u6536\u76ca: +3 Chaos damage"};
    constexpr const char* kDamage[] = {
        u8"\u6536\u76ca: +8% Fire damage", u8"\u6536\u76ca: +8% Water damage",
        u8"\u6536\u76ca: +8% Lightning damage", u8"\u6536\u76ca: +8% Chaos damage"};
    constexpr const char* kResistance[] = {
        u8"\u6536\u76ca: +7% Fire resistance", u8"\u6536\u76ca: +7% Water resistance",
        u8"\u6536\u76ca: +7% Lightning resistance", u8"\u6536\u76ca: +7% Chaos resistance"};
    constexpr const char* kNotableDamage[] = {
        u8"\u6536\u76ca: +8 Fire, +15% Fire damage", u8"\u6536\u76ca: +8 Water, +15% Water damage",
        u8"\u6536\u76ca: +8 Lightning, +15% Lightning damage", u8"\u6536\u76ca: +8 Chaos, +15% Chaos damage"};
    constexpr const char* kNotableResistance[] = {
        u8"\u6536\u76ca: +15% Fire resistance, +8 barrier",
        u8"\u6536\u76ca: +15% Water resistance, +8 barrier",
        u8"\u6536\u76ca: +15% Lightning resistance, +8 barrier",
        u8"\u6536\u76ca: +15% Chaos resistance, +8 barrier"};
    constexpr const char* kKeystone[] = {
        u8"\u6536\u76ca: +30% Fire damage", u8"\u6536\u76ca: +25% Water damage, +24 barrier",
        u8"\u6536\u76ca: +25% Lightning damage, +12% move/attack speed",
        u8"\u6536\u76ca: +30% Chaos damage"};
    switch (node) {
    case 0U: return u8"\u6536\u76ca: \u8d77\u59cb\u8282\u70b9";
    case 1U: return u8"\u6536\u76ca: +20 maximum health";
    case 2U: return u8"\u6536\u76ca: +10 barrier, -3% damage taken";
    case 3U: return u8"\u6536\u76ca: +6% melee damage";
    case 4U: return u8"\u6536\u76ca: +5% attack speed";
    case 5U: return u8"\u6536\u76ca: +10% knockback and launch";
    case 6U: return u8"\u6536\u76ca: +6% move speed";
    case 7U: return u8"\u6536\u76ca: +10% jump and air control";
    default: break;
    }
    if (node < 8U || node >= passives::kPassiveNodeCount) return u8"\u6536\u76ca: \u65e0";
    const std::size_t route = route_index(node);
    switch ((node - 8U) % 14U) {
    case 0U:
    case 1U: return u8"\u6536\u76ca: \u8def\u7ebf\u8fde\u63a5";
    case 2U:
    case 3U: return kFlat[route];
    case 4U:
    case 7U: return kDamage[route];
    case 5U:
    case 6U: return kResistance[route];
    case 8U: return u8"\u6536\u76ca: +4% melee damage";
    case 9U: return u8"\u6536\u76ca: -2% damage taken";
    case 10U: return kNotableDamage[route];
    case 11U: return kNotableResistance[route];
    case 12U: return u8"\u6536\u76ca: +12 maximum health, +3% move speed";
    case 13U: return kKeystone[route];
    default: return u8"\u6536\u76ca: \u65e0";
    }
}

const char* passive_cost_text(passives::PassiveNodeId node) noexcept {
    switch (node) {
    case 21U: return u8"\u4ee3\u4ef7: -20% Water resistance";
    case 35U: return u8"\u4ee3\u4ef7: -15% melee damage";
    case 49U: return u8"\u4ee3\u4ef7: -20 maximum health";
    case 63U: return u8"\u4ee3\u4ef7: +15% damage taken";
    default: return u8"\u4ee3\u4ef7: \u65e0";
    }
}

const char* type_label(passives::PassiveNodeType type) noexcept {
    switch (type) {
    case passives::PassiveNodeType::start: return "START";
    case passives::PassiveNodeType::connector: return "CONNECTOR";
    case passives::PassiveNodeType::small: return "SMALL";
    case passives::PassiveNodeType::notable: return "NOTABLE";
    case passives::PassiveNodeType::keystone: return "KEYSTONE";
    }
    return "NODE";
}

bool use_passive_tree_hud_font(Font hud_font, bool hud_font_ready) noexcept {
    return passive_tree_text_draw_mode(hud_font_ready)
            == PassiveTreeTextDrawMode::hud_font
        && IsFontValid(hud_font);
}

float measure_passive_tree_text(Font hud_font, bool hud_font_ready,
    const char* text, float font_size) noexcept {
    if (use_passive_tree_hud_font(hud_font, hud_font_ready)) {
        return MeasureTextEx(hud_font, text, font_size, 0.5F).x;
    }
    return static_cast<float>(MeasureText(text,
        static_cast<int>(std::round(font_size))));
}

void draw_passive_tree_text(Font hud_font, bool hud_font_ready,
    const char* text, float x, float y, float font_size, Color color) noexcept {
    if (use_passive_tree_hud_font(hud_font, hud_font_ready)) {
        draw_crisp_ui_text(hud_font, text, {x, y}, font_size, 0.5F,
            color, 1);
        return;
    }
    DrawText(text, static_cast<int>(std::round(x)),
        static_cast<int>(std::round(y)),
        static_cast<int>(std::round(font_size)), color);
}

void draw_node_tooltip(const dungeon::DungeonSnapshot& snapshot,
    passives::PassiveNodeId node,
    Font hud_font, bool hud_font_ready) noexcept {
    const passives::PassiveNode& source = passives::passive_nodes()[node];
    const int panel_width = 290;
    const bool detailed = source.type == passives::PassiveNodeType::notable
        || source.type == passives::PassiveNodeType::keystone;
    const int panel_height = detailed ? 112 : 90;
    const int x = std::max(16, GetScreenWidth() - panel_width - 24);
    const int y = std::max(76, GetScreenHeight() - panel_height - 26);
    DrawRectangleRounded({static_cast<float>(x), static_cast<float>(y),
        static_cast<float>(panel_width), static_cast<float>(panel_height)},
        0.08F, 6, Color{9, 13, 22, 242});
    DrawRectangleLinesEx({static_cast<float>(x), static_cast<float>(y),
        static_cast<float>(panel_width), static_cast<float>(panel_height)},
        2.0F, route_color(node));
    DrawText(source.name, x + 14, y + 12, 20, RAYWHITE);
    DrawText(type_label(source.type), x + 14, y + 38, 13, route_color(node));
    draw_passive_tree_text(hud_font, hud_font_ready,
        passive_benefit_text(node), static_cast<float>(x + 14),
        static_cast<float>(y + 56), 14.0F, Color{188, 222, 196, 255});
    if (detailed) {
        draw_passive_tree_text(hud_font, hud_font_ready,
            passive_cost_text(node), static_cast<float>(x + 14),
            static_cast<float>(y + 76), 14.0F,
            Color{255, 174, 150, 255});
    } else if (passive_node_visual_state(snapshot, node)
        == PassiveNodeVisualState::allocated) {
        DrawText("Click to refund (autosaves)", x + 14, y + 72, 13,
            Color{189, 206, 229, 255});
    } else {
        DrawText("Click to allocate (autosaves)", x + 14, y + 72, 13,
            Color{189, 206, 229, 255});
    }
}

}  // namespace

PassiveTreeTextDrawMode passive_tree_text_draw_mode(
    bool hud_font_ready) noexcept {
    return hud_font_ready ? PassiveTreeTextDrawMode::hud_font
                          : PassiveTreeTextDrawMode::fallback;
}

void draw_passive_tree_overlay(const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status,
    const char* passive_tree_binding_label,
    Font hud_font, bool hud_font_ready) noexcept {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{5, 8, 17, 242});
    DrawRectangleLinesEx({16.0F, 16.0F, width - 32.0F, height - 32.0F}, 2.0F,
        Color{83, 115, 160, 220});

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
                lit ? 3.0F : 1.0F, lit ? route_color(node.id) : Color{56, 67, 86, 210});
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
        if (node.type == passives::PassiveNodeType::notable
            || node.type == passives::PassiveNodeType::keystone) {
            constexpr int kNameFontSize = 12;
            const int label_width = MeasureText(node.name, kNameFontSize);
            DrawText(node.name, static_cast<int>(projection.center.x) - label_width / 2,
                static_cast<int>(projection.center.y - projection.radius - 18.0F),
                kNameFontSize, color);
            const int benefit_font_size = node.type == passives::PassiveNodeType::keystone
                ? 10 : 9;
            const char* benefit = passive_benefit_text(node.id);
            draw_passive_tree_text(hud_font, hud_font_ready, benefit,
                projection.center.x - measure_passive_tree_text(
                    hud_font, hud_font_ready, benefit,
                    static_cast<float>(benefit_font_size)) * 0.5F,
                projection.center.y + projection.radius + 7.0F,
                static_cast<float>(benefit_font_size),
                Color{188, 222, 196, 255});
            if (node.type == passives::PassiveNodeType::keystone) {
                const char* cost = passive_cost_text(node.id);
                draw_passive_tree_text(hud_font, hud_font_ready, cost,
                    projection.center.x - measure_passive_tree_text(
                        hud_font, hud_font_ready, cost,
                        static_cast<float>(benefit_font_size)) * 0.5F,
                    projection.center.y + projection.radius + 20.0F,
                    static_cast<float>(benefit_font_size),
                    Color{248, 166, 145, 255});
            }
        }
    }

    DrawText("PASSIVE STAR CHART", 38, 34, 26, Color{226, 237, 255, 255});
    DrawText(TextFormat("Passive Points %u",
        static_cast<unsigned>(snapshot.progression.unspent_passive_points)),
        40, 68, 18, Color{157, 220, 255, 255});
    const bool saving = snapshot.passive_save_pending
        || runtime_status.indicator == SaveIndicator::saving;
    const bool error = runtime_status.indicator == SaveIndicator::error
        || snapshot.passive_tree_error != passives::PassiveTreeError::none;
    DrawText(saving ? "Autosave SAVING" : error ? "Autosave ERROR" : "Autosave READY",
        40, 94, 16, error ? Color{255, 119, 119, 255}
            : saving ? Color{255, 201, 98, 255} : Color{156, 224, 183, 255});
    constexpr int kCloseFontSize = 17;
    constexpr const char* kCloseSuffix = " Close";
    const int binding_width = MeasureText(
        passive_tree_binding_label, kCloseFontSize);
    const int close_width = binding_width
        + MeasureText(kCloseSuffix, kCloseFontSize);
    const int close_x = (std::max)(38, GetScreenWidth() - 38 - close_width);
    const Color close_color{201, 213, 232, 255};
    DrawText(passive_tree_binding_label, close_x, 38, kCloseFontSize,
        close_color);
    DrawText(kCloseSuffix, close_x + binding_width, 38, kCloseFontSize,
        close_color);

    const Vector2 mouse = GetMousePosition();
    if (const auto hovered = hit_test_passive_node({mouse.x, mouse.y}, width, height)) {
        draw_node_tooltip(snapshot, *hovered, hud_font, hud_font_ready);
    }
}

}  // namespace arpg::platform
