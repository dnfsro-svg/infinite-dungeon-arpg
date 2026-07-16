#include "dungeon/abyss_checkpoint_migration.hpp"

#include "abyss/abyss_rules.hpp"

namespace arpg::dungeon {

checkpoint::DungeonRunState migrate_legacy_abyss_checkpoint(
    const checkpoint::DungeonRunState& legacy) {
    checkpoint::DungeonRunState migrated = legacy;
    migrated.abyss = {};
    migrated.last_abyss_resolution = {};

    const bool legal_door_target = legacy.current_room.is_abyss
        && legacy.current_room.entry != checkpoint::EntrySide::initial
        && legacy.last_transition == checkpoint::TransitionKind::door;
    if (!legal_door_target) {
        migrated.current_room.is_abyss = false;
        return migrated;
    }

    const auto selection = abyss::select_abyss_rule(
        legacy.current_room.seed, legacy.current_room.depth);
    if (!selection.has_value()) {
        migrated.current_room.is_abyss = false;
        return migrated;
    }
    migrated.abyss.lifecycle = abyss::AbyssLifecycle::available;
    migrated.abyss.danger = selection->danger;
    migrated.abyss.rule = selection->rule;
    migrated.abyss.rules_version = selection->rules_version;
    return migrated;
}

}  // namespace arpg::dungeon
