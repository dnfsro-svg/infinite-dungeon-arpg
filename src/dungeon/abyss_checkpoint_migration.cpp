#include "dungeon/abyss_checkpoint_migration.hpp"

#include "abyss/abyss_rules.hpp"

namespace arpg::dungeon {
namespace {

bool valid_door_entry(
    checkpoint::ExitDirection direction,
    checkpoint::EntrySide entry) noexcept {
    switch (direction) {
    case checkpoint::ExitDirection::up:
        return entry == checkpoint::EntrySide::bottom;
    case checkpoint::ExitDirection::down:
        return entry == checkpoint::EntrySide::top;
    case checkpoint::ExitDirection::left:
        return entry == checkpoint::EntrySide::right;
    case checkpoint::ExitDirection::right:
        return entry == checkpoint::EntrySide::left;
    case checkpoint::ExitDirection::none:
        return false;
    }
    return false;
}

}  // namespace

checkpoint::DungeonRunState migrate_legacy_abyss_checkpoint(
    const checkpoint::DungeonRunState& legacy) {
    checkpoint::DungeonRunState migrated = legacy;
    migrated.abyss = {};
    migrated.last_abyss_resolution = {};

    const bool legal_door_target = legacy.current_room.is_abyss
        && legacy.last_transition == checkpoint::TransitionKind::door
        && valid_door_entry(
            legacy.last_direction, legacy.current_room.entry)
        && abyss::is_abyss_roll(legacy.current_room.seed);
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
