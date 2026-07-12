#pragma once

#include "dungeon/dungeon_session.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace arpg::test {

struct DungeonSessionTestAccess final {
    static void force_defeat_current_wave(
        dungeon::DungeonSession& session) noexcept {
        if (!session.combat_.has_value()) {
            return;
        }
        combat::CombatWorld& world = *session.combat_;
        for (std::size_t index = 0; index < world.monsters_.slots_.size(); ++index) {
            combat::MonsterRuntime& monster = world.monsters_.slots_[index];
            if (!monster.active || monster.hp <= 0) {
                continue;
            }
            monster.hp = 0;
            combat::AttackDefinition defeat{};
            defeat.feedback = combat::FeedbackLevel::light;
            world.apply_dummy_impact(index, defeat);
        }
    }
};

inline void force_defeat_current_wave(dungeon::DungeonSession& session) noexcept {
    DungeonSessionTestAccess::force_defeat_current_wave(session);
}

inline void trace_wave_fixture(const char* label,
    const dungeon::DungeonSnapshot& state) noexcept {
    std::fprintf(stderr,
        "[wave-trace] %s tick=%llu room=%llu phase=%u wave=%u/%u delay=%u targets=%u exits=%u%u%u%u hole=%u player=(%.2f,%.2f) hp=%d state=%u\n",
        label, static_cast<unsigned long long>(state.session_tick),
        static_cast<unsigned long long>(state.room_index),
        static_cast<unsigned>(state.phase), static_cast<unsigned>(state.wave_index),
        static_cast<unsigned>(state.wave_count),
        static_cast<unsigned>(state.wave_delay_ticks),
        static_cast<unsigned>(state.remaining_targets), state.exits_open[0],
        state.exits_open[1], state.exits_open[2], state.exits_open[3],
        state.has_hole, state.combat ? state.combat->player.position.x : 0.0F,
        state.combat ? state.combat->player.position.y : 0.0F,
        state.combat ? state.combat->player.hp : 0,
        static_cast<unsigned>(state.combat ? state.combat->player.state
                                           : combat::PlayerState::idle));
    if (!state.combat) return;
    for (const auto& monster : state.combat->monsters) {
        if (monster.active) {
            std::fprintf(stderr, "[wave-trace] monster id=%u hp=%d pos=(%.2f,%.2f) ai=%u\n",
                static_cast<unsigned>(monster.id), monster.hp,
                monster.position.x, monster.position.y,
                static_cast<unsigned>(monster.ai_phase));
        }
    }
}

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

inline const combat::MonsterSnapshot* nearest_living_monster(
    const combat::CombatSnapshot& state) noexcept {
    const combat::MonsterSnapshot* nearest = nullptr;
    float nearest_distance_squared = 0.0F;
    for (const auto& monster : state.monsters) {
        if (!monster.active || monster.hp <= 0) {
            continue;
        }
        const float delta_x = monster.position.x - state.player.position.x;
        const float delta_y = monster.position.y - state.player.position.y;
        const float distance_squared = delta_x * delta_x + delta_y * delta_y;
        if (nearest == nullptr || distance_squared < nearest_distance_squared) {
            nearest = &monster;
            nearest_distance_squared = distance_squared;
        }
    }
    return nearest;
}

inline combat::MovementInput movement_toward(
    const combat::Vec3& player,
    const combat::Vec3& target) noexcept {
    constexpr float kHorizontalTolerance = 1.00F;
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
    const combat::MonsterSnapshot& target) noexcept {
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
    trace_wave_fixture("drive-start", session.snapshot());
    for (int tick = 0; tick < max_ticks; ++tick) {
        const dungeon::DungeonSnapshot state = session.snapshot();
        if (state.phase == dungeon::RoomPhase::cleared
                || state.phase == dungeon::RoomPhase::awaiting_exit) {
            trace_wave_fixture("drive-terminal", state);
            return true;
        }
        if (state.phase == dungeon::RoomPhase::combat) {
            force_defeat_current_wave(session);
        }
        session.tick({});
        drain_all_events(session, summary);
    }
    trace_wave_fixture("drive-timeout", session.snapshot());
    return false;
}

}  // namespace arpg::test
