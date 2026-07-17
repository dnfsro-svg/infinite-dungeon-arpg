#pragma once

#include "dungeon/dungeon_session.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace arpg::test {

struct DungeonSessionTestAccess final {
    static const dungeon::RoomEncounterPlan& encounter_plan(
        const dungeon::DungeonSession& session) noexcept {
        return session.encounter_plan_;
    }
    static const combat::PlayerCombatBuild& player_build(
        const dungeon::DungeonSession& session) noexcept {
        return session.combat_->encounter_config_.player_build;
    }
    static const combat::CombatWorld* combat_world_address(
        const dungeon::DungeonSession& session) noexcept {
        return session.combat_.has_value() ? &*session.combat_ : nullptr;
    }
    static void set_current_room_hole(
        dungeon::DungeonSession& session, bool has_hole) noexcept {
        session.stable_state_.current_room.has_hole = has_hole;
    }
    static void attempt_exit(dungeon::DungeonSession& session,
        dungeon::ExitDirection direction) noexcept {
        session.attempt_exit(direction);
    }
    static void damage_current_player(
        dungeon::DungeonSession& session, int damage) noexcept {
        if (session.combat_.has_value()) {
            session.combat_->apply_player_damage(
                damage, combat::Vec3{}, combat::FeedbackLevel::light);
        }
    }
    static bool kill_current_player_through_combat(
        dungeon::DungeonSession& session) noexcept {
        if (!session.combat_.has_value()) return false;
        const combat::PlayerDamageSource source{
            combat::PlayerDamageSourceKind::ground_hazard,
            combat::MonsterId::fire_bomber,
            static_cast<std::uint16_t>(combat::HazardKind::burning),
        };
        return session.combat_->apply_player_damage(
            combat::DamagePacket{1000000},
            combat::DamageDelivery::ground_or_environment,
            source, combat::Vec3{}, combat::FeedbackLevel::heavy);
    }
    static const abyss::AbyssCombatConfig* pending_abyss_config(
        const dungeon::DungeonSession& session) noexcept {
        return session.pending_abyss_combat_.has_value()
            ? &session.pending_abyss_combat_->abyss
            : nullptr;
    }
    static const abyss::AbyssCombatConfig* active_abyss_config(
        const dungeon::DungeonSession& session) noexcept {
        return session.combat_.has_value()
            ? &session.combat_->encounter_config_.abyss
            : nullptr;
    }
    static void fill_current_combat_events(
        dungeon::DungeonSession& session, std::size_t count) noexcept {
        if (!session.combat_.has_value()) return;
        combat::CombatEvent event{};
        event.kind = combat::CombatEventKind::reset;
        for (std::size_t index = 0U; index < count; ++index) {
            session.combat_->emit_event(event);
        }
    }
    static std::size_t fill_dungeon_events(
        dungeon::DungeonSession& session, std::size_t count) noexcept {
        dungeon::DungeonEvent event{};
        event.kind = dungeon::DungeonEventKind::room_reset;
        std::size_t pushed = 0U;
        while (pushed < count && session.events_.try_push(event)) {
            ++pushed;
        }
        return pushed;
    }
    static void force_fault(
        dungeon::DungeonSession& session,
        dungeon::DungeonFault fault) noexcept {
        session.enter_fault(fault);
    }
    static void handle_player_defeat(
        dungeon::DungeonSession& session) noexcept {
        session.handle_player_defeat();
    }
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
    static bool defeat_next_live_monster(
        dungeon::DungeonSession& session) noexcept {
        if (!session.combat_.has_value()) {
            return false;
        }
        combat::CombatWorld& world = *session.combat_;
        for (std::size_t index = 0U; index < world.monsters_.slots_.size(); ++index) {
            combat::MonsterRuntime& monster = world.monsters_.slots_[index];
            if (!monster.active || monster.hp <= 0
                    || monster.reaction == combat::ReactionState::defeated) {
                continue;
            }
            monster.hp = 0;
            world.defeat_monster(index, combat::AttackId::j1, true);
            return true;
        }
        return false;
    }
    static bool relay_defeated(
        dungeon::DungeonSession& session,
        std::uint8_t wave_index,
        std::uint8_t target_index,
        combat::Vec3 position,
        bool reward_eligible = true,
        combat::MonsterId monster_id = combat::MonsterId::fire_bomber,
        std::uint16_t spawn_ordinal = 0xFFFFU,
        std::uint16_t affix_score = 0U) noexcept {
        if (!session.combat_.has_value() || wave_index >= 2U) {
            return false;
        }
        session.encounter_plan_.wave_count = 2U;
        session.encounter_plan_.waves[wave_index].spawn_count = 96U;
        session.wave_index_ = wave_index;
        combat::CombatEvent event{};
        event.kind = combat::CombatEventKind::defeated;
        event.target_index = target_index;
        event.position = position;
        event.monster_id = monster_id;
        event.spawn_ordinal = spawn_ordinal == 0xFFFFU
            ? static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(wave_index) * 96U + target_index)
            : spawn_ordinal;
        event.affix_score = affix_score;
        event.reward_eligible = reward_eligible;
        if (!session.combat_->events_.try_push(event)) {
            return false;
        }
        session.relay_combat_events();
        return true;
    }
    static void set_current_room_seed(
        dungeon::DungeonSession& session,
        std::uint64_t seed) noexcept {
        session.stable_state_.current_room.seed = seed;
    }
    static void set_current_room_depth(
        dungeon::DungeonSession& session,
        std::uint64_t depth) noexcept {
        session.stable_state_.current_room.depth = depth;
    }
    static void set_phase(
        dungeon::DungeonSession& session,
        dungeon::RoomPhase phase) noexcept {
        session.phase_ = phase;
    }
    static void set_player_position(
        dungeon::DungeonSession& session,
        combat::Vec3 position) noexcept {
        if (session.combat_.has_value()) {
            session.combat_->player_.position = position;
        }
    }
    static void clear_ground_item(
        dungeon::DungeonSession& session,
        std::uint16_t ordinal) noexcept {
        if (ordinal < session.ground_items_.size()) {
            session.ground_items_[ordinal] = {};
        }
    }
    static void install_ground_item(
        dungeon::DungeonSession& session,
        std::uint16_t ordinal,
        const items::ItemInstance& item,
        combat::Vec3 position) noexcept {
        if (ordinal < session.ground_items_.size()) {
            session.ground_items_[ordinal] = {
                true, ordinal, dungeon::GroundItemSource::monster_drop,
                0xFFU, position, item};
        }
    }
    static void install_abyss_ground_item(
        dungeon::DungeonSession& session,
        std::uint16_t ordinal,
        std::uint8_t reward_ordinal,
        const items::ItemInstance& item,
        combat::Vec3 position) noexcept {
        if (ordinal < session.ground_items_.size()) {
            session.ground_items_[ordinal] = {
                true, ordinal, dungeon::GroundItemSource::abyss_chest,
                reward_ordinal, position, item};
        }
    }
    static combat::CombatWorld* mutable_combat_world(
        dungeon::DungeonSession& session) noexcept {
        return session.combat_.has_value() ? &*session.combat_ : nullptr;
    }
    static void install_combat_world(
        dungeon::DungeonSession& session,
        const combat::CombatEncounterConfig& config) noexcept {
        session.combat_.emplace(config);
    }
    static void fill_ground_pool(
        dungeon::DungeonSession& session,
        const items::ItemInstance& prototype) noexcept {
        for (std::uint16_t index = 0U;
             index < session.ground_items_.size(); ++index) {
            items::ItemInstance item = prototype;
            item.id += index;
            session.ground_items_[index] = {
                true, index, dungeon::GroundItemSource::monster_drop,
                0xFFU, {100.0F + static_cast<float>(index),
                    100.0F, 0.0F}, item};
        }
    }
    static void set_pending_pickup_ordinal(
        dungeon::DungeonSession& session,
        std::uint16_t ordinal) noexcept {
        if (session.pending_save_.has_value()) {
            session.pending_save_->pickup_ordinal = ordinal;
        }
    }
    static void offset_pending_abyss_reward_position(
        dungeon::DungeonSession& session,
        combat::Vec3 offset) noexcept {
        if (session.pending_abyss_reward_.has_value()) {
            auto& position = session.pending_abyss_reward_->ground.position;
            position.x += offset.x;
            position.y += offset.y;
            position.z += offset.z;
        }
    }
    static const std::array<std::uint64_t, 3>& rolled_drop_bits(
        const dungeon::DungeonSession& session) noexcept {
        return session.rolled_drop_bits_;
    }
    static const dungeon::DungeonRunState& stable_state(
        const dungeon::DungeonSession& session) noexcept {
        return session.stable_state_;
    }
    static const std::array<dungeon::GroundItem,
        dungeon::kGroundDropCapacity>& ground_items(
        const dungeon::DungeonSession& session) noexcept {
        return session.ground_items_;
    }
};

inline void force_defeat_current_wave(dungeon::DungeonSession& session) noexcept {
    DungeonSessionTestAccess::force_defeat_current_wave(session);
}

inline bool defeat_next_live_monster(dungeon::DungeonSession& session) noexcept {
    return DungeonSessionTestAccess::defeat_next_live_monster(session);
}

inline void damage_current_player(
    dungeon::DungeonSession& session, int damage) noexcept {
    DungeonSessionTestAccess::damage_current_player(session, damage);
}

inline bool kill_current_player_through_combat(
    dungeon::DungeonSession& session) noexcept {
    return DungeonSessionTestAccess::kill_current_player_through_combat(
        session);
}

inline std::size_t fill_dungeon_events(
    dungeon::DungeonSession& session, std::size_t count) noexcept {
    return DungeonSessionTestAccess::fill_dungeon_events(session, count);
}

inline const dungeon::RoomEncounterPlan& encounter_plan(
    const dungeon::DungeonSession& session) noexcept {
    return DungeonSessionTestAccess::encounter_plan(session);
}

inline const combat::PlayerCombatBuild& player_build(
    const dungeon::DungeonSession& session) noexcept {
    return DungeonSessionTestAccess::player_build(session);
}

inline const combat::CombatWorld* combat_world_address(
    const dungeon::DungeonSession& session) noexcept {
    return DungeonSessionTestAccess::combat_world_address(session);
}

inline void set_current_room_hole(
    dungeon::DungeonSession& session, bool has_hole) noexcept {
    DungeonSessionTestAccess::set_current_room_hole(session, has_hole);
}

inline void attempt_exit(dungeon::DungeonSession& session,
    dungeon::ExitDirection direction) noexcept {
    DungeonSessionTestAccess::attempt_exit(session, direction);
}

inline bool relay_defeated(
    dungeon::DungeonSession& session,
    std::uint8_t wave_index,
    std::uint8_t target_index,
    combat::Vec3 position,
    bool reward_eligible = true,
    combat::MonsterId monster_id = combat::MonsterId::fire_bomber,
    std::uint16_t spawn_ordinal = 0xFFFFU,
    std::uint16_t affix_score = 0U) noexcept {
    return DungeonSessionTestAccess::relay_defeated(
        session, wave_index, target_index, position, reward_eligible,
        monster_id, spawn_ordinal, affix_score);
}

inline void set_current_room_seed(
    dungeon::DungeonSession& session,
    std::uint64_t seed) noexcept {
    DungeonSessionTestAccess::set_current_room_seed(session, seed);
}

inline void set_current_room_depth(
    dungeon::DungeonSession& session,
    std::uint64_t depth) noexcept {
    DungeonSessionTestAccess::set_current_room_depth(session, depth);
}

inline void set_phase(
    dungeon::DungeonSession& session,
    dungeon::RoomPhase phase) noexcept {
    DungeonSessionTestAccess::set_phase(session, phase);
}

inline void set_player_position(
    dungeon::DungeonSession& session,
    combat::Vec3 position) noexcept {
    DungeonSessionTestAccess::set_player_position(session, position);
}

inline void clear_ground_item(
    dungeon::DungeonSession& session,
    std::uint16_t ordinal) noexcept {
    DungeonSessionTestAccess::clear_ground_item(session, ordinal);
}

inline void install_ground_item(
    dungeon::DungeonSession& session,
    std::uint16_t ordinal,
    const items::ItemInstance& item,
    combat::Vec3 position) noexcept {
    DungeonSessionTestAccess::install_ground_item(
        session, ordinal, item, position);
}

inline void install_abyss_ground_item(
    dungeon::DungeonSession& session,
    std::uint16_t ordinal,
    std::uint8_t reward_ordinal,
    const items::ItemInstance& item,
    combat::Vec3 position) noexcept {
    DungeonSessionTestAccess::install_abyss_ground_item(
        session, ordinal, reward_ordinal, item, position);
}

inline void install_combat_world(
    dungeon::DungeonSession& session,
    const combat::CombatEncounterConfig& config) noexcept {
    DungeonSessionTestAccess::install_combat_world(
        session, config);
}

inline combat::CombatWorld* mutable_combat_world(
    dungeon::DungeonSession& session) noexcept {
    return DungeonSessionTestAccess::mutable_combat_world(session);
}

inline void fill_ground_pool(
    dungeon::DungeonSession& session,
    const items::ItemInstance& prototype) noexcept {
    DungeonSessionTestAccess::fill_ground_pool(session, prototype);
}

inline void set_pending_pickup_ordinal(
    dungeon::DungeonSession& session,
    std::uint16_t ordinal) noexcept {
    DungeonSessionTestAccess::set_pending_pickup_ordinal(session, ordinal);
}

inline void offset_pending_abyss_reward_position(
    dungeon::DungeonSession& session,
    combat::Vec3 offset) noexcept {
    DungeonSessionTestAccess::offset_pending_abyss_reward_position(
        session, offset);
}

inline const std::array<std::uint64_t, 3>& rolled_drop_bits(
    const dungeon::DungeonSession& session) noexcept {
    return DungeonSessionTestAccess::rolled_drop_bits(session);
}

inline const dungeon::DungeonRunState& stable_state(
    const dungeon::DungeonSession& session) noexcept {
    return DungeonSessionTestAccess::stable_state(session);
}

inline const std::array<dungeon::GroundItem,
    dungeon::kGroundDropCapacity>& ground_items(
    const dungeon::DungeonSession& session) noexcept {
    return DungeonSessionTestAccess::ground_items(session);
}

inline bool same_encounter_plan(const dungeon::RoomEncounterPlan& left,
    const dungeon::RoomEncounterPlan& right) noexcept {
    if (left.wave_count != right.wave_count
            || left.total_budget != right.total_budget) return false;
    for (std::size_t wave = 0; wave < left.wave_count; ++wave) {
        const auto& a = left.waves[wave];
        const auto& b = right.waves[wave];
        if (a.spawn_count != b.spawn_count || a.spent_budget != b.spent_budget) return false;
        for (std::size_t spawn = 0; spawn < a.spawn_count; ++spawn) {
            if (a.spawns[spawn].id != b.spawns[spawn].id
                    || a.spawns[spawn].position.x != b.spawns[spawn].position.x
                    || a.spawns[spawn].position.y != b.spawns[spawn].position.y
                    || a.spawns[spawn].position.z != b.spawns[spawn].position.z) return false;
        }
    }
    return true;
}

inline bool commit_pending(
    dungeon::DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()) {
        return false;
    }
    session.resolve_pending_save({
        dungeon::SaveDisposition::committed,
        pending->expected_generation,
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
    for (int tick = 0; tick < max_ticks; ++tick) {
        const dungeon::DungeonSnapshot state = session.snapshot();
        if (state.phase == dungeon::RoomPhase::cleared
                || state.phase == dungeon::RoomPhase::awaiting_exit) {
            return true;
        }
        if (state.phase == dungeon::RoomPhase::combat) {
            force_defeat_current_wave(session);
        }
        session.tick({});
        drain_all_events(session, summary);
    }
    return false;
}

}  // namespace arpg::test
