#pragma once

#include "dungeon/dungeon_checkpoint.hpp"

namespace arpg::dungeon {

[[nodiscard]] checkpoint::DungeonRunState migrate_legacy_abyss_checkpoint(
    const checkpoint::DungeonRunState& legacy);

}  // namespace arpg::dungeon
