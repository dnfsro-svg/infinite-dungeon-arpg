#include "dungeon/room_combat_template.hpp"

namespace arpg::dungeon {

std::optional<combat::CombatLabConfig> make_combat_lab_config(
    checkpoint::EntrySide entry,
    std::uint32_t rules_version) noexcept {
    if (rules_version != 1U) {
        return std::nullopt;
    }

    combat::CombatLabConfig config;
    config.respawn_defeated_dummies = false;

    switch (entry) {
    case checkpoint::EntrySide::initial:
        break;
    case checkpoint::EntrySide::left:
        config.player_spawn = {-6.50F, 0.0F, 0.0F};
        break;
    case checkpoint::EntrySide::right:
        config.player_spawn = {6.50F, 0.0F, 0.0F};
        config.dummy_spawns = {{
            {-2.30F, -0.35F, 0.0F},
            {-2.80F, 0.0F, 0.0F},
            {-3.30F, 0.35F, 0.0F},
        }};
        config.initial_facing = combat::Facing::left;
        break;
    case checkpoint::EntrySide::top:
        config.player_spawn = {0.0F, -2.75F, 0.0F};
        config.dummy_spawns = {{
            {2.30F, 2.30F, 0.0F},
            {2.80F, 2.30F, 0.0F},
            {3.30F, 2.30F, 0.0F},
        }};
        break;
    case checkpoint::EntrySide::bottom:
        config.player_spawn = {0.0F, 2.75F, 0.0F};
        config.dummy_spawns = {{
            {2.30F, -2.30F, 0.0F},
            {2.80F, -2.30F, 0.0F},
            {3.30F, -2.30F, 0.0F},
        }};
        break;
    }
    return config;
}

}  // namespace arpg::dungeon
