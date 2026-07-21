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
        config.player_spawn = {-10.50F, 0.0F, 0.0F};
        break;
    case checkpoint::EntrySide::right:
        config.player_spawn = {10.50F, 0.0F, 0.0F};
        config.dummy_spawns = {{
            {-2.30F, -0.35F, 0.0F},
            {-2.80F, 0.0F, 0.0F},
            {-3.30F, 0.35F, 0.0F},
        }};
        config.initial_facing = combat::Facing::left;
        break;
    case checkpoint::EntrySide::top:
        config.player_spawn = {0.0F, -4.75F, 0.0F};
        config.dummy_spawns = {{
            {2.30F, 2.30F, 0.0F},
            {2.80F, 2.30F, 0.0F},
            {3.30F, 2.30F, 0.0F},
        }};
        break;
    case checkpoint::EntrySide::bottom:
        config.player_spawn = {0.0F, 4.75F, 0.0F};
        config.dummy_spawns = {{
            {2.30F, -2.30F, 0.0F},
            {2.80F, -2.30F, 0.0F},
            {3.30F, -2.30F, 0.0F},
        }};
        break;
    }
    return config;
}

std::optional<combat::CombatEncounterConfig> make_combat_encounter_config(
    checkpoint::EntrySide entry,
    std::uint32_t rules_version,
    const combat::EncounterWave& wave,
    bool reset_player_health,
    abyss::AbyssCombatConfig abyss_config,
    combat::PlayerCombatBuild player_build,
    std::uint64_t evasion_seed) noexcept {
    const auto legacy = make_combat_lab_config(entry, rules_version);
    if (!legacy.has_value()) {
        return std::nullopt;
    }
    combat::CombatEncounterConfig config{};
    config.player_spawn = legacy->player_spawn;
    config.initial_facing = legacy->initial_facing;
    config.wave = wave;
    config.reset_player_health = reset_player_health;
    config.abyss = abyss_config;
    config.player_build = player_build;
    config.evasion_seed = evasion_seed;
    return config;
}

}  // namespace arpg::dungeon
