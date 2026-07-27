#pragma once

#include "combat/fire_room_obstacle.hpp"
#include "combat/room_bounds.hpp"
#include "abyss/abyss_rules.hpp"
#include "dungeon/dungeon_session.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace arpg::test {

struct DungeonSessionTestAccess final {
    static bool commit_fixture_health_potion_if_eligible(
        dungeon::DungeonSession& session) noexcept {
        for (std::uint16_t spawn = 0U;
             spawn < session.ground_health_potions_.size(); ++spawn) {
            if (!session.ground_health_potions_[spawn].active) continue;
            const dungeon::RequestResult requested =
                session.request_health_potion_pickup(spawn);
            if (requested == dungeon::RequestResult::rejected) continue;
            if (requested == dungeon::RequestResult::faulted
                    || !session.pending_save_.has_value()) {
                return false;
            }
            const dungeon::PendingSaveResult committed{
                dungeon::SaveDisposition::committed,
                session.pending_save_->expected_generation,
                session.pending_save_->next_state,
                session.pending_save_->kind,
            };
            session.commit_pending_save(committed);
            return session.phase_ != dungeon::RoomPhase::faulted;
        }
        return true;
    }
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
        session.relay_combat_events();
        const bool has_later_wave = session.wave_index_ + 1U
            < session.encounter_plan_.wave_count;
        if (has_later_wave) {
            static_cast<void>(
                commit_fixture_health_potion_if_eligible(session));
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
        std::uint16_t affix_score = 0U,
        bool commit_health_potion = true) noexcept {
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
        return !commit_health_potion
            || commit_fixture_health_potion_if_eligible(session);
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
    static void set_player_health(
        dungeon::DungeonSession& session, int hp, int max_hp) noexcept {
        if (session.combat_.has_value()) {
            session.combat_->player_.max_hp = max_hp;
            session.combat_->player_.hp = hp;
        }
    }
    static void clear_combat_world(
        dungeon::DungeonSession& session) noexcept {
        session.combat_.reset();
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
    static void set_started_abyss_room(
        dungeon::DungeonSession& session,
        abyss::AbyssDanger danger) noexcept {
        session.stable_state_.current_room.is_abyss = true;
        session.stable_state_.abyss.lifecycle = abyss::AbyssLifecycle::started;
        session.stable_state_.abyss.danger = danger;
        session.stable_state_.abyss.rule = danger == abyss::AbyssDanger::low
            ? abyss::AbyssRuleId::thunderstorm
            : (danger == abyss::AbyssDanger::medium
                ? abyss::AbyssRuleId::hunting_flames
                : abyss::AbyssRuleId::chaos_expansion);
        session.stable_state_.abyss.rules_version = abyss::kAbyssRulesVersion;
    }
    static void set_started_life_sacrifice_abyss_room(
        dungeon::DungeonSession& session) noexcept {
        session.stable_state_.current_room.is_abyss = true;
        session.stable_state_.abyss.lifecycle = abyss::AbyssLifecycle::started;
        session.stable_state_.abyss.danger = abyss::AbyssDanger::high;
        session.stable_state_.abyss.rule = abyss::AbyssRuleId::life_sacrifice;
        session.stable_state_.abyss.rules_version = abyss::kAbyssRulesVersion;
        if (session.combat_.has_value()) {
            session.combat_->encounter_config_.abyss =
                abyss::combat_config_for(abyss::AbyssRuleId::life_sacrifice);
            session.combat_->apply_player_build(
                session.combat_->encounter_config_.player_build);
        }
    }
    static void clear_all_ground_items(
        dungeon::DungeonSession& session) noexcept {
        session.ground_items_ = {};
    }
    static void install_ground_material(
        dungeon::DungeonSession& session,
        std::uint16_t ordinal,
        items::MaterialId material,
        combat::Vec3 position,
        dungeon::GroundMaterialSource source =
            dungeon::GroundMaterialSource::monster_common) noexcept {
        if (ordinal < session.ground_materials_.size()) {
            session.ground_materials_[ordinal] = {
                true, ordinal, source, position, material};
        }
    }
    static void install_ground_health_potion(
        dungeon::DungeonSession& session,
        std::uint16_t spawn_ordinal,
        combat::Vec3 position) noexcept {
        if (spawn_ordinal < session.ground_health_potions_.size()) {
            session.ground_health_potions_[spawn_ordinal] = {
                true, spawn_ordinal,
                dungeon::health_potion_claim_ordinal(spawn_ordinal),
                position};
        }
    }
    static void replace_ground_health_potion(
        dungeon::DungeonSession& session,
        std::uint16_t slot,
        dungeon::GroundHealthPotion replacement) noexcept {
        if (slot < session.ground_health_potions_.size()) {
            session.ground_health_potions_[slot] = replacement;
        }
    }
    static void quiesce_current_room_for_clear_retry(
        dungeon::DungeonSession& session) noexcept {
        session.encounter_plan_.wave_count = static_cast<std::uint8_t>(
            session.wave_index_ + 1U);
        if (!session.combat_.has_value()) return;
        for (combat::MonsterRuntime& monster
                : session.combat_->monsters_.slots_) {
            monster.active = false;
            monster.hp = 0;
        }
    }
    static bool install_delayed_abyss_environment_hazard(
        dungeon::DungeonSession& session) noexcept {
        if (!session.combat_.has_value()) return false;
        combat::CombatWorld& world = *session.combat_;
        return world.spawn_environment_hazard(
            combat::HazardKind::thunderstorm,
            world.player_.position,
            1.0F,
            2U,
            8U,
            1U,
            combat::DamagePacket{1},
            500U,
            modifiers::DamageType::lightning);
    }
    static bool health_potion_abyss_clear_retry_pending(
        const dungeon::DungeonSession& session) noexcept {
        return session.retry_health_potion_abyss_clear_before_combat_;
    }
    static void offset_pending_next_room_depth(
        dungeon::DungeonSession& session, std::uint64_t offset) noexcept {
        if (session.pending_save_.has_value()) {
            session.pending_save_->next_state.current_room.depth += offset;
        }
    }
    static void set_pending_health_potion_claim_count(
        dungeon::DungeonSession& session, std::uint8_t count) noexcept {
        if (session.pending_save_.has_value()
                && session.pending_save_->health_potion_claim.has_value()) {
            session.pending_save_->health_potion_claim->count = count;
        }
    }
    static void set_pending_next_material_claim_bit(
        dungeon::DungeonSession& session, std::uint16_t ordinal) noexcept {
        if (!session.pending_save_.has_value()) return;
        auto& bits = session.pending_save_->next_state.item_ownership
            .material_claimed_drop_bits;
        const std::size_t word = ordinal / 64U;
        const std::uint8_t bit = static_cast<std::uint8_t>(ordinal % 64U);
        if (word < bits.size()) {
            bits[word] |= std::uint64_t{1U} << bit;
        }
    }
    static bool pending_material_cache_consistent(
        const dungeon::DungeonSession& session) noexcept {
        return session.pending_material_cache_consistent();
    }
    static void prepare_room_clear(
        dungeon::DungeonSession& session) noexcept {
        session.prepare_room_clear();
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
    static void set_room_progression(
        dungeon::DungeonSession& session,
        progression::ProgressionState state) noexcept {
        session.room_progression_ = state;
    }
    static bool copy_run_state_reusing_items(
        dungeon::DungeonRunState& destination,
        const dungeon::DungeonRunState& source) noexcept {
        return dungeon::DungeonSession::copy_run_state_reusing_items(
            destination, source);
    }
    static void publish_run_state_reusing_items(
        dungeon::DungeonRunState& destination,
        dungeon::DungeonRunState& source) noexcept {
        dungeon::DungeonSession::publish_run_state_reusing_items(
            destination, source);
    }
    static const std::array<dungeon::GroundItem,
        dungeon::kGroundDropCapacity>& ground_items(
        const dungeon::DungeonSession& session) noexcept {
        return session.ground_items_;
    }
    static const std::array<dungeon::GroundMaterial,
        dungeon::kGroundMaterialCapacity>& ground_materials(
        const dungeon::DungeonSession& session) noexcept {
        return session.ground_materials_;
    }
    static void clear_rolled_material_claims(
        dungeon::DungeonSession& session) noexcept {
        session.rolled_material_bits_ = {};
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
    std::uint16_t affix_score = 0U,
    bool commit_health_potion = true) noexcept {
    return DungeonSessionTestAccess::relay_defeated(
        session, wave_index, target_index, position, reward_eligible,
        monster_id, spawn_ordinal, affix_score, commit_health_potion);
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

inline void set_player_health(
    dungeon::DungeonSession& session, int hp, int max_hp) noexcept {
    DungeonSessionTestAccess::set_player_health(session, hp, max_hp);
}

inline void clear_combat_world(
    dungeon::DungeonSession& session) noexcept {
    DungeonSessionTestAccess::clear_combat_world(session);
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

inline void set_started_abyss_room(
    dungeon::DungeonSession& session,
    abyss::AbyssDanger danger) noexcept {
    DungeonSessionTestAccess::set_started_abyss_room(session, danger);
}

inline void set_started_life_sacrifice_abyss_room(
    dungeon::DungeonSession& session) noexcept {
    DungeonSessionTestAccess::set_started_life_sacrifice_abyss_room(session);
}

inline void clear_all_ground_items(
    dungeon::DungeonSession& session) noexcept {
    DungeonSessionTestAccess::clear_all_ground_items(session);
}

inline void install_ground_material(
    dungeon::DungeonSession& session,
    std::uint16_t ordinal,
    items::MaterialId material,
    combat::Vec3 position,
    dungeon::GroundMaterialSource source =
        dungeon::GroundMaterialSource::monster_common) noexcept {
    DungeonSessionTestAccess::install_ground_material(
        session, ordinal, material, position, source);
}

inline void install_ground_health_potion(
    dungeon::DungeonSession& session,
    std::uint16_t spawn_ordinal,
    combat::Vec3 position) noexcept {
    DungeonSessionTestAccess::install_ground_health_potion(
        session, spawn_ordinal, position);
}

inline void replace_ground_health_potion(
    dungeon::DungeonSession& session,
    std::uint16_t slot,
    dungeon::GroundHealthPotion replacement) noexcept {
    DungeonSessionTestAccess::replace_ground_health_potion(
        session, slot, replacement);
}

inline void quiesce_current_room_for_clear_retry(
    dungeon::DungeonSession& session) noexcept {
    DungeonSessionTestAccess::quiesce_current_room_for_clear_retry(session);
}

inline bool install_delayed_abyss_environment_hazard(
    dungeon::DungeonSession& session) noexcept {
    return DungeonSessionTestAccess::install_delayed_abyss_environment_hazard(
        session);
}

inline void offset_pending_next_room_depth(
    dungeon::DungeonSession& session, std::uint64_t offset) noexcept {
    DungeonSessionTestAccess::offset_pending_next_room_depth(session, offset);
}

inline void set_pending_health_potion_claim_count(
    dungeon::DungeonSession& session, std::uint8_t count) noexcept {
    DungeonSessionTestAccess::set_pending_health_potion_claim_count(
        session, count);
}

inline void set_pending_next_material_claim_bit(
    dungeon::DungeonSession& session, std::uint16_t ordinal) noexcept {
    DungeonSessionTestAccess::set_pending_next_material_claim_bit(
        session, ordinal);
}

inline bool pending_material_cache_consistent(
    const dungeon::DungeonSession& session) noexcept {
    return DungeonSessionTestAccess::pending_material_cache_consistent(
        session);
}

inline void prepare_room_clear(
    dungeon::DungeonSession& session) noexcept {
    DungeonSessionTestAccess::prepare_room_clear(session);
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

inline void set_room_progression(
    dungeon::DungeonSession& session,
    progression::ProgressionState state) noexcept {
    DungeonSessionTestAccess::set_room_progression(session, state);
}

inline bool copy_run_state_reusing_items(
    dungeon::DungeonRunState& destination,
    const dungeon::DungeonRunState& source) noexcept {
    return DungeonSessionTestAccess::copy_run_state_reusing_items(
        destination, source);
}

inline void publish_run_state_reusing_items(
    dungeon::DungeonRunState& destination,
    dungeon::DungeonRunState& source) noexcept {
    DungeonSessionTestAccess::publish_run_state_reusing_items(
        destination, source);
}

inline const std::array<dungeon::GroundItem,
    dungeon::kGroundDropCapacity>& ground_items(
    const dungeon::DungeonSession& session) noexcept {
    return DungeonSessionTestAccess::ground_items(session);
}

inline void clear_rolled_material_claims(
    dungeon::DungeonSession& session) noexcept {
    DungeonSessionTestAccess::clear_rolled_material_claims(session);
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

inline bool segment_crosses_fire_brazier(
    combat::Vec3 from,
    combat::Vec3 to) noexcept {
    float enter = 0.0F;
    float leave = 1.0F;
    const auto clip_axis = [&enter, &leave](float start, float delta,
                               float minimum, float maximum) noexcept {
        constexpr float kParallelTolerance = 1.0e-6F;
        if (delta >= -kParallelTolerance && delta <= kParallelTolerance) {
            return start >= minimum && start <= maximum;
        }
        float first = (minimum - start) / delta;
        float second = (maximum - start) / delta;
        if (first > second) std::swap(first, second);
        enter = (std::max)(enter, first);
        leave = (std::min)(leave, second);
        return enter <= leave;
    };

    return clip_axis(from.x, to.x - from.x,
               -combat::fire_room_obstacle::half_width,
               combat::fire_room_obstacle::half_width)
        && clip_axis(from.y, to.y - from.y,
               -combat::fire_room_obstacle::half_height,
               combat::fire_room_obstacle::half_height)
        && enter < 1.0F && leave > 0.0F;
}

inline float planar_distance(
    combat::Vec3 from,
    combat::Vec3 to) noexcept {
    const float x = to.x - from.x;
    const float y = to.y - from.y;
    return std::sqrt(x * x + y * y);
}

inline combat::MovementInput movement_to_waypoint(
    combat::Vec3 from,
    combat::Vec3 to) noexcept {
    constexpr float kTolerance = 0.10F;
    return {
        static_cast<std::int8_t>(to.x > from.x + kTolerance ? 1
            : (to.x < from.x - kTolerance ? -1 : 0)),
        static_cast<std::int8_t>(to.y > from.y + kTolerance ? 1
            : (to.y < from.y - kTolerance ? -1 : 0)),
    };
}

inline combat::MovementInput fire_room_robot_movement(
    combat::Vec3 player,
    combat::Vec3 target) noexcept {
    if (combat::fire_room_obstacle::contains(player)) {
        const float x_progress = player.x
            / combat::fire_room_obstacle::half_width;
        const float y_progress = player.y
            / combat::fire_room_obstacle::half_height;
        if (x_progress * x_progress >= y_progress * y_progress) {
            return {static_cast<std::int8_t>(
                        player.x < 0.0F ? -1 : 1), 0};
        }
        return {0, static_cast<std::int8_t>(
                       player.y < 0.0F ? -1 : 1)};
    }
    if (!segment_crosses_fire_brazier(player, target)) {
        return movement_toward(player, target);
    }

    constexpr float kLaneX =
        combat::fire_room_obstacle::half_width + 1.10F;
    constexpr float kLaneY =
        combat::fire_room_obstacle::half_height + 0.80F;
    constexpr std::size_t kNodeCount = 6U;
    constexpr std::size_t kTargetNode = kNodeCount - 1U;
    const std::array<combat::Vec3, kNodeCount> nodes{{
        player,
        {-kLaneX, -kLaneY, 0.0F},
        {-kLaneX, kLaneY, 0.0F},
        {kLaneX, -kLaneY, 0.0F},
        {kLaneX, kLaneY, 0.0F},
        target,
    }};
    std::array<float, kNodeCount> distance{};
    distance.fill(1.0e30F);
    std::array<std::size_t, kNodeCount> first_hop{};
    first_hop.fill(kNodeCount);
    std::array<bool, kNodeCount> visited{};
    distance[0U] = 0.0F;

    for (std::size_t iteration = 0U; iteration < kNodeCount; ++iteration) {
        std::size_t current = kNodeCount;
        for (std::size_t node = 0U; node < kNodeCount; ++node) {
            if (!visited[node] && (current == kNodeCount
                    || distance[node] < distance[current])) {
                current = node;
            }
        }
        if (current == kNodeCount || distance[current] >= 1.0e29F) break;
        visited[current] = true;
        for (std::size_t next = 0U; next < kNodeCount; ++next) {
            if (visited[next] || next == current
                    || segment_crosses_fire_brazier(
                        nodes[current], nodes[next])) {
                continue;
            }
            const float edge = planar_distance(nodes[current], nodes[next]);
            if (current == 0U && edge < 0.20F) continue;
            const float candidate = distance[current] + edge;
            if (candidate >= distance[next]) continue;
            distance[next] = candidate;
            first_hop[next] = current == 0U
                ? next : first_hop[current];
        }
    }

    const std::size_t waypoint = first_hop[kTargetNode];
    return waypoint < kNodeCount
        ? movement_to_waypoint(player, nodes[waypoint])
        : movement_toward(player, target);
}

inline combat::MovementInput exit_outward(
    dungeon::ExitDirection direction) noexcept {
    switch (direction) {
    case dungeon::ExitDirection::up: return {0, -1};
    case dungeon::ExitDirection::down: return {0, 1};
    case dungeon::ExitDirection::left: return {-1, 0};
    case dungeon::ExitDirection::right: return {1, 0};
    case dungeon::ExitDirection::none: return {};
    }
    return {};
}

inline combat::Vec3 exit_boundary_position(
    dungeon::ExitDirection direction) noexcept {
    switch (direction) {
    case dungeon::ExitDirection::up:
        return {0.0F, combat::room_bounds::min_y, 0.0F};
    case dungeon::ExitDirection::down:
        return {0.0F, combat::room_bounds::max_y, 0.0F};
    case dungeon::ExitDirection::left:
        return {combat::room_bounds::min_x, 0.0F, 0.0F};
    case dungeon::ExitDirection::right:
        return {combat::room_bounds::max_x, 0.0F, 0.0F};
    case dungeon::ExitDirection::none:
        return {};
    }
    return {};
}

inline combat::MovementInput exit_alignment_movement(
    const dungeon::DungeonSnapshot& state,
    dungeon::ExitDirection direction) noexcept {
    constexpr float kTolerance = 0.10F;
    constexpr float kBypassLane = 2.50F;
    constexpr float kBrazierClearX =
        combat::fire_room_obstacle::half_width + 0.15F;
    constexpr float kBrazierClearY =
        combat::fire_room_obstacle::half_height + 0.15F;
    combat::MovementInput movement{};
    const combat::Vec3 position = state.combat->player.position;

    if (state.ecology == dungeon::checkpoint::DungeonElement::fire) {
        if (direction == dungeon::ExitDirection::left
                || direction == dungeon::ExitDirection::right) {
            const bool must_cross_brazier =
                direction == dungeon::ExitDirection::left
                ? position.x >= -kBrazierClearX
                : position.x <= kBrazierClearX;
            if (must_cross_brazier) {
                const float lane = position.y < -kTolerance
                    ? -kBypassLane : kBypassLane;
                movement.y = position.y > lane + kTolerance ? -1
                    : (position.y < lane - kTolerance ? 1 : 0);
                return movement.y == 0
                    ? exit_outward(direction) : movement;
            }
        } else if (direction == dungeon::ExitDirection::up
                || direction == dungeon::ExitDirection::down) {
            const bool must_cross_brazier =
                direction == dungeon::ExitDirection::up
                ? position.y >= -kBrazierClearY
                : position.y <= kBrazierClearY;
            if (must_cross_brazier) {
                const float lane = position.x < -kTolerance
                    ? -kBypassLane : kBypassLane;
                movement.x = position.x > lane + kTolerance ? -1
                    : (position.x < lane - kTolerance ? 1 : 0);
                return movement.x == 0
                    ? exit_outward(direction) : movement;
            }
        }
    }

    if (direction == dungeon::ExitDirection::left
            || direction == dungeon::ExitDirection::right) {
        movement.y = position.y > kTolerance ? -1
            : (position.y < -kTolerance ? 1 : 0);
    } else {
        movement.x = position.x > kTolerance ? -1
            : (position.x < -kTolerance ? 1 : 0);
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
        if (state.phase == dungeon::RoomPhase::committing
                && state.pending_save_kind.has_value()
                && (*state.pending_save_kind
                        == dungeon::PendingSaveKind::room_clear
                    || *state.pending_save_kind
                        == dungeon::PendingSaveKind::abyss_clear)) {
            if (!commit_pending(session)) return false;
            drain_all_events(session, summary);
            continue;
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
