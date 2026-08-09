#pragma once

#include "dungeon/dungeon_types.hpp"

#include <raylib.h>

namespace arpg::platform {

struct DungeonRenderStatus;

enum class PassiveTreeTextDrawMode {
    fallback,
    hud_font,
};

[[nodiscard]] PassiveTreeTextDrawMode passive_tree_text_draw_mode(
    bool hud_font_ready) noexcept;

void draw_passive_tree_overlay(const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status,
    const char* passive_tree_binding_label,
    Font hud_font, bool hud_font_ready) noexcept;

}  // namespace arpg::platform
