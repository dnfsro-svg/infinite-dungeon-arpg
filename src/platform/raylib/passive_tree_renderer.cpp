#include "passive_tree_renderer.hpp"

#include "dungeon_runtime.hpp"
#include "passive_tree_view_math.hpp"
#include "passives/passive_tree_catalog.hpp"

#include <raylib.h>

#include <algorithm>
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

const char* keystone_cost(passives::PassiveNodeId node) noexcept {
    switch (node) {
    case 21U: return "Cost: -20% Water resistance";
    case 35U: return "Cost: -15% melee damage";
    case 49U: return "Cost: -20 maximum life";
    case 63U: return "Cost: +15% damage taken";
    default: return "Cost: none";
    }
}

void draw_node_tooltip(const dungeon::DungeonSnapshot& snapshot,
    passives::PassiveNodeId node) noexcept {
    const passives::PassiveNode& source = passives::passive_nodes()[node];
    const int panel_width = 290;
    const int panel_height = source.type == passives::PassiveNodeType::keystone ? 112 : 90;
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
    DrawText(TextFormat("Benefit: %u modifiers", static_cast<unsigned>(source.modifier_count)),
        x + 14, y + 56, 14, Color{188, 222, 196, 255});
    if (source.type == passives::PassiveNodeType::keystone) {
        DrawText(keystone_cost(node), x + 14, y + 76, 14, Color{255, 174, 150, 255});
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

void draw_passive_tree_overlay(const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status) noexcept {
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
            const int font_size = node.type == passives::PassiveNodeType::keystone ? 14 : 12;
            const int label_width = MeasureText(node.name, font_size);
            DrawText(node.name, static_cast<int>(projection.center.x) - label_width / 2,
                static_cast<int>(projection.center.y + projection.radius + 7.0F),
                font_size, color);
            if (node.type == passives::PassiveNodeType::keystone) {
                const char* cost = keystone_cost(node.id);
                DrawText(cost, static_cast<int>(projection.center.x) - MeasureText(cost, 11) / 2,
                    static_cast<int>(projection.center.y + projection.radius + 22.0F),
                    11, Color{248, 166, 145, 255});
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
    DrawText("P Close", GetScreenWidth() - 115, 38, 17, Color{201, 213, 232, 255});

    const Vector2 mouse = GetMousePosition();
    if (const auto hovered = hit_test_passive_node({mouse.x, mouse.y}, width, height)) {
        draw_node_tooltip(snapshot, *hovered);
    }
}

}  // namespace arpg::platform
