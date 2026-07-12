#pragma once

#include "dungeon/dungeon_session.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace arpg::test {

inline bool commit_pending(
    dungeon::DungeonSession& session) noexcept {
    const auto pending = session.pending_transition();
    if (!pending.has_value()) {
        return false;
    }
    session.resolve_pending_transition({
        dungeon::SaveDisposition::committed,
        pending->next_state.commit_generation,
        pending->next_state,
    });
    return true;
}

struct EventSummary final {
    static constexpr std::size_t kKindCapacity = 1024;

    std::array<dungeon::DungeonEventKind, kKindCapacity> dungeon_kinds{};
    std::array<combat::CombatEventKind, kKindCapacity> combat_kinds{};
    std::size_t dungeon_count{};
    std::size_t combat_count{};
    std::uint32_t room_cleared_count{};
    std::uint32_t exits_opened_count{};
    std::uint32_t defeated_count{};
};

inline void record_event(
    EventSummary& summary,
    const dungeon::DungeonEvent& event) noexcept {
    if (summary.dungeon_count < summary.dungeon_kinds.size()) {
        summary.dungeon_kinds[summary.dungeon_count] = event.kind;
    }
    ++summary.dungeon_count;
    if (event.kind == dungeon::DungeonEventKind::room_cleared) {
        ++summary.room_cleared_count;
    } else if (event.kind == dungeon::DungeonEventKind::exits_opened) {
        ++summary.exits_opened_count;
    }
}

inline void record_event(
    EventSummary& summary,
    const combat::CombatEvent& event) noexcept {
    if (summary.combat_count < summary.combat_kinds.size()) {
        summary.combat_kinds[summary.combat_count] = event.kind;
    }
    ++summary.combat_count;
    if (event.kind == combat::CombatEventKind::defeated) {
        ++summary.defeated_count;
    }
}

inline void drain_all_events(
    dungeon::DungeonSession& session,
    EventSummary& summary) noexcept {
    while (const auto event = session.try_pop_event()) {
        record_event(summary, *event);
    }
    while (const auto event = session.try_pop_combat_event()) {
        record_event(summary, *event);
    }
}

inline const combat::DummySnapshot* nearest_living_dummy(
    const combat::CombatSnapshot& state) noexcept {
    const combat::DummySnapshot* nearest = nullptr;
    float nearest_distance_squared = 0.0F;
    for (const auto& dummy : state.dummies) {
        if (dummy.hp <= 0) {
            continue;
        }
        const float delta_x = dummy.position.x - state.player.position.x;
        const float delta_y = dummy.position.y - state.player.position.y;
        const float distance_squared = delta_x * delta_x + delta_y * delta_y;
        if (nearest == nullptr || distance_squared < nearest_distance_squared) {
            nearest = &dummy;
            nearest_distance_squared = distance_squared;
        }
    }
    return nearest;
}

inline combat::MovementInput movement_toward(
    const combat::Vec3& player,
    const combat::Vec3& target) noexcept {
    constexpr float kHorizontalTolerance = 1.45F;
    constexpr float kDepthTolerance = 0.30F;
    combat::MovementInput movement{};
    const float delta_x = target.x - player.x;
    const float delta_y = target.y - player.y;
    if (delta_x > kHorizontalTolerance) {
        movement.x = 1;
    } else if (delta_x < -kHorizontalTolerance) {
        movement.x = -1;
    }
    if (delta_y > kDepthTolerance) {
        movement.y = 1;
    } else if (delta_y < -kDepthTolerance) {
        movement.y = -1;
    }
    return movement;
}

inline bool in_light_attack_lane(
    const combat::PlayerSnapshot& player,
    const combat::DummySnapshot& target) noexcept {
    constexpr float kMaximumHorizontalDistance = 1.75F;
    constexpr float kMaximumDepthDistance = 0.60F;
    constexpr float kFacingOverlapTolerance = 0.20F;
    const float delta_x = target.position.x - player.position.x;
    const bool facing_target =
        std::fabs(delta_x) <= kFacingOverlapTolerance
        || (delta_x >= 0.0F && player.facing == combat::Facing::right)
        || (delta_x <= 0.0F && player.facing == combat::Facing::left);
    return facing_target
        && std::fabs(delta_x) <= kMaximumHorizontalDistance
        && std::fabs(target.position.y - player.position.y)
            <= kMaximumDepthDistance;
}

inline bool drive_until_cleared(
    dungeon::DungeonSession& session,
    EventSummary& summary,
    int max_ticks = 4096) noexcept {
    for (int tick = 0; tick < max_ticks; ++tick) {
        const dungeon::DungeonSnapshot state = session.snapshot();
        if (state.phase == dungeon::RoomPhase::cleared
                || state.phase == dungeon::RoomPhase::awaiting_exit) {
            return true;
        }
        combat::MovementInput movement{};
        if (state.combat.has_value()
                && state.phase == dungeon::RoomPhase::combat) {
            const auto& combat_state = *state.combat;
            const combat::DummySnapshot* target =
                nearest_living_dummy(combat_state);
            if (target != nullptr) {
                movement = movement_toward(
                    combat_state.player.position, target->position);
                if (in_light_attack_lane(combat_state.player, *target)
                        && combat_state.player.active_attack
                            == combat::AttackId::none) {
                    static_cast<void>(
                        session.queue_action(combat::Action::light));
                }
            }
        }
        session.tick(movement);
        drain_all_events(session, summary);
    }
    return false;
}

}  // namespace arpg::test
