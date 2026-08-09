#pragma once

#include "dungeon/dungeon_types.hpp"

namespace arpg::platform {

struct DungeonRenderStatus;

void draw_passive_tree_overlay(const dungeon::DungeonSnapshot& snapshot,
    const DungeonRenderStatus& runtime_status,
    const char* passive_tree_binding_label) noexcept;

}  // namespace arpg::platform
