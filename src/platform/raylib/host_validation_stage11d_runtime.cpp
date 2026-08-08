#include "host_validation_stage11d.hpp"

#include "combat/active_skill_runtime.hpp"
#include "combat/fire_room_obstacle.hpp"
#include "combat/room_bounds.hpp"
#include "combat/room_spatial_grid.hpp"
#include "host_input.hpp"
#include "host_validation_input.hpp"
#include "host_validation_navigation.hpp"
#include "platform/settings/settings_types.hpp"
#include "raylib_host.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace arpg::platform::host_validation {

// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN selectors
bool stage11d_has_three_ordinary_rarities(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    bool normal = false;
    bool magic = false;
    bool rare = false;
    for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
        const auto& item = snapshot.ground_items[index];
        if (item.source != dungeon::GroundItemSource::monster_drop) continue;
        normal = normal || item.rarity == items::ItemRarity::normal;
        magic = magic || item.rarity == items::ItemRarity::magic;
        rare = rare || item.rarity == items::ItemRarity::rare;
    }
    return normal && magic && rare;
}

std::uint16_t stage11d_priority_monster_ordinal(
    const dungeon::DungeonSnapshot& snapshot,
    Stage11DLootValidationState& state) noexcept {
    if (!snapshot.combat.has_value()) {
        return combat::kInvalidMonsterOrdinal;
    }
    const auto& combat_snapshot = *snapshot.combat;
    if (state.target_ordinal != combat::kInvalidMonsterOrdinal) {
        bool target_alive = false;
        bool target_defeated = false;
        for (std::size_t index = 0U;
             index < combat_snapshot.monster_count; ++index) {
            const auto& monster = combat_snapshot.monsters[index];
            if (monster.spawn_ordinal != state.target_ordinal) continue;
            target_alive = monster.active && monster.hp > 0;
            target_defeated = monster.hp <= 0
                || monster.reaction == combat::ReactionState::defeated
                || monster.ai_phase == combat::MonsterAiPhase::defeated;
            break;
        }
        bool target_drop_observed = false;
        for (std::size_t index = 0U; index < snapshot.ground_item_count;
             ++index) {
            const auto& item = snapshot.ground_items[index];
            if (item.source == dungeon::GroundItemSource::monster_drop
                    && item.ordinal == state.target_ordinal) {
                target_drop_observed = true;
                break;
            }
        }
        if (target_alive) return state.target_ordinal;
        if (!target_defeated && !target_drop_observed) {
            return combat::kInvalidMonsterOrdinal;
        }
    }
    std::uint16_t selected = combat::kInvalidMonsterOrdinal;
    for (std::size_t index = 0U;
         index < combat_snapshot.monster_count; ++index) {
        const auto& monster = combat_snapshot.monsters[index];
        if (!monster.active || monster.hp <= 0
                || (state.target_ordinal
                        != combat::kInvalidMonsterOrdinal
                    && monster.spawn_ordinal <= state.target_ordinal)) {
            continue;
        }
        if (selected == combat::kInvalidMonsterOrdinal
                || monster.spawn_ordinal < selected) {
            selected = monster.spawn_ordinal;
        }
    }
    if (selected != combat::kInvalidMonsterOrdinal) {
        state.target_ordinal = selected;
    }
    return selected;
}

namespace {

[[nodiscard]] const dungeon::GroundItemSnapshot* stage11d_nearest_ground(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    if (!snapshot.combat.has_value()) return nullptr;
    const auto& player = snapshot.combat->player.position;
    const dungeon::GroundItemSnapshot* nearest = nullptr;
    float nearest_distance = 0.0F;
    for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
        const auto& item = snapshot.ground_items[index];
        const float x = item.position.x - player.x;
        const float y = item.position.y - player.y;
        const float distance = x * x + y * y;
        if (nearest == nullptr || distance < nearest_distance) {
            nearest = &item;
            nearest_distance = distance;
        }
    }
    return nearest;
}

[[nodiscard]] const dungeon::GroundItemSnapshot*
stage11d_rare_abyss_ground(const dungeon::DungeonSnapshot& snapshot,
    const Stage11DLootValidationState& state) noexcept {
    for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
        const auto& item = snapshot.ground_items[index];
        if (item.source != dungeon::GroundItemSource::abyss_chest
                || item.rarity == items::ItemRarity::rare) {
            continue;
        }
        if (state.abyss_item_id == 0U
                || item.item_id == state.abyss_item_id) {
            return &item;
        }
    }
    return nullptr;
}

[[nodiscard]] const combat::MonsterSnapshot* stage11d_priority_monster(
    const dungeon::DungeonSnapshot& snapshot,
    Stage11DLootValidationState& state) noexcept {
    const std::uint16_t ordinal = stage11d_priority_monster_ordinal(
        snapshot, state);
    if (ordinal == combat::kInvalidMonsterOrdinal
            || !snapshot.combat.has_value()) return nullptr;
    for (std::size_t index = 0U;
         index < snapshot.combat->monster_count; ++index) {
        const auto& monster = snapshot.combat->monsters[index];
        if (monster.active && monster.hp > 0
                && monster.spawn_ordinal == ordinal) return &monster;
    }
    return nullptr;
}

[[nodiscard]] bool stage11d_attack_lane(
    const combat::CombatSnapshot& state,
    const combat::MonsterSnapshot& target,
    float minimum_x, float maximum_x) noexcept {
    const float x = target.position.x - state.player.position.x;
    const float y = target.position.y - state.player.position.y;
    const bool facing = std::fabs(x) <= 0.20F
        || (x > 0.0F && state.player.facing == combat::Facing::right)
        || (x < 0.0F && state.player.facing == combat::Facing::left);
    const float distance_x = std::fabs(x);
    return facing && distance_x >= minimum_x && distance_x <= maximum_x
        && std::fabs(y) <= 0.55F;
}

enum class Stage11DRareAbyssSkillInput : std::uint8_t {
    no_geometry,
    unavailable,
    waiting,
    injected,
};

enum class Stage11DEquippedSkillInput : std::uint8_t {
    absent,
    waiting,
    injected,
};

[[nodiscard]] Stage11DRareAbyssSkillInput
inject_stage11d_rare_abyss_area_skill(
    PhysicalKeySnapshot& snapshot,
    const dungeon::DungeonSnapshot& dungeon_state) noexcept {
    if (!dungeon_state.combat.has_value()) {
        return Stage11DRareAbyssSkillInput::no_geometry;
    }
    const combat::CombatSnapshot& combat_state = *dungeon_state.combat;
    const auto& player = combat_state.player;
    if (player.hp <= 0 || player.hurt_ticks != 0U
            || player.hit_stop_ticks != 0U
            || player.active_attack != combat::AttackId::none
            || combat_state.active_skill.id != skills::ActiveSkillId::none
            || combat_state.diagnostics.input_size != 0U) {
        return Stage11DRareAbyssSkillInput::waiting;
    }
    const float facing = player.facing == combat::Facing::right
        ? 1.0F : -1.0F;
    const float storm_center_x = player.position.x
        + facing * combat::kStormCenterForward;
    bool storm_target = false;
    bool draw_target = false;
    for (std::size_t index = 0U;
         index < combat_state.monster_count; ++index) {
        const auto& monster = combat_state.monsters[index];
        if (!monster.active || monster.hp <= 0) continue;
        const float storm_x = monster.position.x - storm_center_x;
        const float storm_y = monster.position.y - player.position.y;
        storm_target = storm_target
            || storm_x * storm_x + storm_y * storm_y
                <= combat::kStormStrikeRadius * combat::kStormStrikeRadius;
        const float forward =
            (monster.position.x - player.position.x) * facing;
        const float half_width = forward >= 0.0F
                && forward <= combat::kDrawSlashRange
            ? combat::kDrawSlashHalfWidthAtEnd
                * (forward / combat::kDrawSlashRange)
            : -1.0F;
        draw_target = draw_target || (half_width >= 0.0F
            && std::fabs(monster.position.y - player.position.y)
                <= half_width);
    }
    const auto inject_equipped_skill = [&](skills::ActiveSkillId skill) {
        const std::size_t cooldown = static_cast<std::size_t>(skill);
        if (cooldown >= combat_state.skill_cooldowns.size()) {
            return Stage11DEquippedSkillInput::absent;
        }
        for (std::size_t slot = 0U;
             slot < dungeon_state.skill_loadout.slots.size(); ++slot) {
            if (dungeon_state.skill_loadout.slots[slot].active != skill) {
                continue;
            }
            if (combat_state.skill_cooldowns[cooldown] != 0U) {
                return Stage11DEquippedSkillInput::waiting;
            }
            snapshot.active_skill_slots[slot] = true;
            return Stage11DEquippedSkillInput::injected;
        }
        return Stage11DEquippedSkillInput::absent;
    };
    bool area_skill_equipped = false;
    for (const auto& slot : dungeon_state.skill_loadout.slots) {
        area_skill_equipped = area_skill_equipped
            || slot.active == skills::ActiveSkillId::storm_swords
            || slot.active == skills::ActiveSkillId::draw_slash;
    }
    bool waiting = false;
    bool unavailable = false;
    if (storm_target) {
        const Stage11DEquippedSkillInput storm = inject_equipped_skill(
            skills::ActiveSkillId::storm_swords);
        if (storm == Stage11DEquippedSkillInput::injected) {
            return Stage11DRareAbyssSkillInput::injected;
        }
        waiting = waiting || storm == Stage11DEquippedSkillInput::waiting;
        unavailable = unavailable
            || storm == Stage11DEquippedSkillInput::absent;
    }
    if (draw_target) {
        const Stage11DEquippedSkillInput draw = inject_equipped_skill(
            skills::ActiveSkillId::draw_slash);
        if (draw == Stage11DEquippedSkillInput::injected) {
            return Stage11DRareAbyssSkillInput::injected;
        }
        waiting = waiting || draw == Stage11DEquippedSkillInput::waiting;
        unavailable = unavailable
            || draw == Stage11DEquippedSkillInput::absent;
    }
    return waiting ? Stage11DRareAbyssSkillInput::waiting
         : (unavailable || !area_skill_equipped)
             ? Stage11DRareAbyssSkillInput::unavailable
                   : Stage11DRareAbyssSkillInput::no_geometry;
}

[[nodiscard]] bool stage11d_rare_abyss_player_available(
    const combat::CombatSnapshot& state) noexcept {
    const auto& player = state.player;
    return player.hp > 0 && player.hurt_ticks == 0U
        && player.hit_stop_ticks == 0U
        && player.active_attack == combat::AttackId::none
        && state.active_skill.id == skills::ActiveSkillId::none
        && state.diagnostics.input_size == 0U;
}

[[nodiscard]] bool stage11d_rare_abyss_danger_near_player(
    const combat::CombatSnapshot& state, float radius) noexcept {
    const float radius_squared = radius * radius;
    for (std::size_t index = 0U; index < state.monster_count; ++index) {
        const auto& monster = state.monsters[index];
        if (!monster.active || monster.hp <= 0
                || monster.reaction != combat::ReactionState::idle
                || monster.ai_phase == combat::MonsterAiPhase::recovery
                || monster.ai_phase == combat::MonsterAiPhase::cooldown
                || monster.ai_phase == combat::MonsterAiPhase::defeated) {
            continue;
        }
        const float x = monster.position.x - state.player.position.x;
        const float y = monster.position.y - state.player.position.y;
        if (x * x + y * y <= radius_squared) return true;
    }
    return false;
}

[[nodiscard]] const combat::MonsterSnapshot*
stage11d_monster_by_ordinal(const combat::CombatSnapshot& state,
    combat::MonsterOrdinal ordinal) noexcept {
    for (std::size_t index = 0U; index < state.monster_count; ++index) {
        const auto& monster = state.monsters[index];
        if (monster.active && monster.hp > 0
                && monster.monster_ordinal == ordinal) {
            return &monster;
        }
    }
    return nullptr;
}

[[nodiscard]] bool stage11d_outer_sweep_route_is_default(
    const Stage10GridRouteState& route) noexcept {
    return route.phase == Stage10GridRoutePhase::need_join
        && route.boundary_column == 0U
        && route.route_rejoins == 0U
        && !route.pending_movement_progress_check
        && route.previous_position.x == 0.0F
        && route.previous_position.y == 0.0F
        && route.previous_position.z == 0.0F;
}

void initialize_stage11d_sweep_cursor(Stage11DLootValidationState& state,
    combat::Vec3 player) noexcept {
    if (state.sweep_cursor_initialized) return;
    state.sweep_cursor_initialized = true;
    if (state.sweep_waypoint != 0U
            || !stage11d_outer_sweep_route_is_default(state.sweep_grid)) {
        return;
    }
    constexpr std::size_t waypoint_count =
        combat::room_spatial::rows * 2U;
    static_assert(waypoint_count <= 0x100U);
    std::size_t nearest = 0U;
    combat::Vec3 target = stage10_validation_sweep_waypoint(0U);
    float best_distance = (target.x - player.x) * (target.x - player.x)
        + (target.y - player.y) * (target.y - player.y);
    for (std::size_t index = 1U; index < waypoint_count; ++index) {
        target = stage10_validation_sweep_waypoint(
            static_cast<std::uint8_t>(index));
        const float x = target.x - player.x;
        const float y = target.y - player.y;
        const float distance = x * x + y * y;
        if (distance < best_distance) {
            best_distance = distance;
            nearest = index;
        }
    }
    state.sweep_waypoint = static_cast<std::uint8_t>(nearest);
}
}  // namespace
// STAGE11D_LOOT_VALIDATION_SEAM_END selectors

namespace {
// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN safe_movement
combat::MovementInput stage11d_safe_movement_toward(
    combat::Vec3 from, combat::Vec3 to,
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    constexpr std::array<combat::MovementInput, 9> kCandidates{{
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0}, {0,  0}, {1,  0},
        {-1,  1}, {0,  1}, {1,  1},
    }};
    constexpr float kStep = 0.10F;
    constexpr float kDiagonalStep = 0.07071068F;
    constexpr float kPickupGuardSquared = 1.60F * 1.60F;
    constexpr float kApproachGuardSquared = 1.75F * 1.75F;
    const auto near_from = [&](combat::Vec3 position) noexcept {
        const float x = position.x - from.x;
        const float y = position.y - from.y;
        return x * x + y * y < kApproachGuardSquared;
    };
    bool avoidance_active = false;
    for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
        avoidance_active = avoidance_active
            || near_from(snapshot.ground_items[index].position);
    }
    if (snapshot.combat.has_value()) {
        for (std::size_t index = 0U;
             index < snapshot.combat->monster_count; ++index) {
            const auto& monster = snapshot.combat->monsters[index];
            const bool defeated = monster.hp <= 0
                || monster.reaction == combat::ReactionState::defeated
                || monster.ai_phase == combat::MonsterAiPhase::defeated;
            avoidance_active = avoidance_active
                || (defeated && near_from(monster.position));
        }
    }
    combat::MovementInput best{};
    float best_score = 1.0e30F;
    for (const auto candidate : kCandidates) {
        const bool diagonal = candidate.x != 0 && candidate.y != 0;
        const float step = diagonal ? kDiagonalStep : kStep;
        const combat::Vec3 next{
            from.x + static_cast<float>(candidate.x) * step,
            from.y + static_cast<float>(candidate.y) * step,
            from.z,
        };
        bool safe = snapshot.ecology
                != dungeon::checkpoint::DungeonElement::fire
            || !combat::fire_room_obstacle::blocks_player(from, next);
        const auto approaches_pickup = [&](combat::Vec3 position) noexcept {
            const float current_x = position.x - from.x;
            const float current_y = position.y - from.y;
            const float next_x = position.x - next.x;
            const float next_y = position.y - next.y;
            const float current_distance = current_x * current_x
                + current_y * current_y;
            const float next_distance = next_x * next_x + next_y * next_y;
            return next_distance < kPickupGuardSquared
                || (current_distance < kApproachGuardSquared
                    && next_distance + 0.0001F < current_distance);
        };
        for (std::size_t index = 0U; index < snapshot.ground_item_count; ++index) {
            if (approaches_pickup(snapshot.ground_items[index].position)) {
                safe = false;
                break;
            }
        }
        if (safe && snapshot.combat.has_value()) {
            for (std::size_t index = 0U;
                 index < snapshot.combat->monster_count; ++index) {
                const auto& monster = snapshot.combat->monsters[index];
                const bool defeated = monster.hp <= 0
                    || monster.reaction == combat::ReactionState::defeated
                    || monster.ai_phase == combat::MonsterAiPhase::defeated;
                if (defeated && approaches_pickup(monster.position)) {
                    safe = false;
                    break;
                }
            }
        }
        if (!safe) continue;
        const float target_x = to.x - next.x;
        const float target_y = to.y - next.y;
        const bool stopped = candidate.x == 0 && candidate.y == 0;
        const float current_target_x = to.x - from.x;
        const float current_target_y = to.y - from.y;
        const bool fire_route_pending = snapshot.ecology
                == dungeon::checkpoint::DungeonElement::fire
            && current_target_x * current_target_x
                    + current_target_y * current_target_y > 0.25F;
        const float score = target_x * target_x + target_y * target_y
            + (stopped && (avoidance_active || fire_route_pending)
                ? 1.0F : 0.0F);
        if (score < best_score) {
            best = candidate;
            best_score = score;
        }
    }
    return best;
}
// STAGE11D_LOOT_VALIDATION_SEAM_END safe_movement

}  // namespace

// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN physical_driver
PhysicalKeySnapshot inject_stage11d_physical_edges(
    PhysicalKeySnapshot snapshot, const RaylibHostConfig& config,
    const settings::SettingsData& settings_data,
    const dungeon::DungeonSnapshot& current,
    Stage11DLootValidationState& state) noexcept {
    using Scenario = Stage11DLootValidationScenario;
    if (config.stage11d_loot_validation == Scenario::none
            || snapshot.focus_lost || state.suspend_injection) {
        return snapshot;
    }
    ++state.injected_frames;
    if (config.stage11d_loot_validation == Scenario::rare_only_abyss
            && current.combat.has_value()) {
        const auto* item = stage11d_rare_abyss_ground(current, state);
        if (item != nullptr) {
            combat::MovementInput movement = validation_movement_toward(
                current.combat->player.position, item->position);
            if (current.ecology
                    == dungeon::checkpoint::DungeonElement::fire) {
                movement = validation_route_fire_movement(
                    current.combat->player.position, item->position, movement);
            }
            inject_validation_movement(snapshot, settings_data, movement);
            state.abyss_claim_requested = state.captured;
            return snapshot;
        }
        if (state.captured) return snapshot;
    }
    const bool ordinary_ready = config.stage11d_loot_validation
            != Scenario::rare_only_abyss
        && stage11d_has_three_ordinary_rarities(current);
    if (config.stage11d_loot_validation == Scenario::preview_cancel
            && ordinary_ready) {
        const std::uint8_t phase = ++state.preview_phase;
        if (phase == 1U || phase == 12U || phase == 13U) snapshot.escape = true;
        else if (phase == 2U || (phase >= 4U && phase <= 10U)) {
            inject_validation_pressed(snapshot, settings::StableKey::arrow_down);
        } else if (phase == 3U) snapshot.enter = true;
        else if (phase == 11U) {
            inject_validation_pressed(snapshot, settings::StableKey::arrow_right);
        }
        return snapshot;
    }
    if (ordinary_ready) {
        if (config.stage11d_loot_validation == Scenario::pickup_feedback) {
            const auto* ground = stage11d_nearest_ground(current);
            if (ground != nullptr && current.combat.has_value()) {
                combat::MovementInput movement = validation_movement_toward(
                    current.combat->player.position, ground->position);
                if (current.ecology
                        == dungeon::checkpoint::DungeonElement::fire) {
                    movement = validation_route_fire_movement(
                        current.combat->player.position,
                        ground->position, movement);
                }
                inject_validation_movement(snapshot, settings_data, movement);
            }
        }
        return snapshot;
    }
    if (!current.combat.has_value()
            || current.phase != dungeon::RoomPhase::combat) {
        return snapshot;
    }
    const bool aggressive_abyss = config.stage11d_loot_validation
        == Scenario::rare_only_abyss;
    if (aggressive_abyss) {
        const auto& combat_state = *current.combat;
        if (config.validation_exit_after_presented_frames != 0U
                && !state.player_damage_observed) {
            return snapshot;
        }
        if (!stage11d_rare_abyss_player_available(combat_state)) {
            return snapshot;
        }
        const bool low_health = combat_state.player.max_hp > 0
            && combat_state.player.hp <= combat_state.player.max_hp / 4;
        if (!state.sweep_cursor_initialized
                && state.abyss_ranged.sweep_escape
                && !state.abyss_ranged.recovery_target_valid) {
            initialize_stage11d_sweep_cursor(
                state, combat_state.player.position);
            state.abyss_ranged.sweep_waypoint = state.sweep_waypoint;
            state.abyss_ranged.sweep_grid = {};
        }
        if (!low_health && state.abyss_ranged.sweep_escape
                && !state.abyss_ranged.recovery_target_valid) {
            if (inject_stage11d_rare_abyss_area_skill(snapshot, current)
                    == Stage11DRareAbyssSkillInput::injected) {
                return snapshot;
            }
            for (std::size_t index = 0U;
                    index < combat_state.monster_count; ++index) {
                const auto& monster = combat_state.monsters[index];
                if (!monster.active || monster.hp <= 0
                        || !validation_attack_lane(
                            combat_state, monster)) {
                    continue;
                }
                inject_validation_action(snapshot, settings_data,
                    settings::SettingAction::light_attack, true);
                return snapshot;
            }
        }
        const auto inner_global_sweep_active = [&state]() noexcept {
            return state.abyss_ranged.sweep_escape
                && !state.abyss_ranged.recovery_target_valid;
        };
        const auto plan_abyss_ranged = [&]() noexcept {
            const bool inner_was_global = inner_global_sweep_active();
            Stage10RangedValidationPlan next = stage10_validation_ranged_plan(
                combat_state, state.abyss_ranged);
            bool inner_is_global = inner_global_sweep_active();
            const bool inner_took_global = !inner_was_global
                && inner_is_global;
            if (inner_took_global) {
                initialize_stage11d_sweep_cursor(
                    state, combat_state.player.position);
                state.abyss_ranged.sweep_waypoint = state.sweep_waypoint;
                state.abyss_ranged.sweep_grid = {};
                state.sweep_grid = {};
                next = stage10_validation_ranged_plan(
                    combat_state, state.abyss_ranged);
                inner_is_global = inner_global_sweep_active();
            }
            if ((inner_was_global || inner_took_global)
                    && !inner_is_global) {
                state.sweep_waypoint = state.abyss_ranged.sweep_waypoint;
            }
            if (state.abyss_ranged.sweep_escape
                    || next.target_ordinal
                        != combat::kInvalidMonsterOrdinal) {
                state.sweep_grid = {};
            }
            return next;
        };
        Stage10RangedValidationPlan plan = plan_abyss_ranged();
        if (plan.target_ordinal == combat::kInvalidMonsterOrdinal
                && plan.movement.x == 0 && plan.movement.y == 0
                && state.abyss_ranged.recover_until_light_lane
                && !state.abyss_ranged.sweep_escape
                && state.abyss_ranged.stalled_target_ordinal
                    == combat::kInvalidMonsterOrdinal
                && !state.abyss_ranged.recovery_target_valid) {
            state.abyss_ranged.recover_until_light_lane = false;
            plan = plan_abyss_ranged();
        }
        const combat::MonsterSnapshot* target =
            stage11d_monster_by_ordinal(
                combat_state, plan.target_ordinal);
        const float danger_radius = low_health ? 4.0F : 2.5F;
        if (target != nullptr
                && low_health
                && !validation_attack_lane(combat_state, *target)
                && stage11d_rare_abyss_danger_near_player(
                    combat_state, danger_radius)) {
            const combat::MonsterOrdinal threatened_target =
                plan.target_ordinal;
            stage10_validation_release_ranged_target(
                state.abyss_ranged);
            state.abyss_ranged.stalled_target_ordinal = threatened_target;
            initialize_stage11d_sweep_cursor(
                state, combat_state.player.position);
            state.abyss_ranged.sweep_waypoint = state.sweep_waypoint;
            state.abyss_ranged.sweep_grid = {};
            state.sweep_grid = {};
            state.abyss_ranged.sweep_escape = true;
            plan = plan_abyss_ranged();
            target = stage11d_monster_by_ordinal(
                combat_state, plan.target_ordinal);
        }
        if (target == nullptr) {
            if (plan.movement.x != 0 || plan.movement.y != 0) {
                combat::MovementInput recovery = plan.movement;
                if (current.ecology
                            == dungeon::checkpoint::DungeonElement::fire
                        && plan.movement_target_valid) {
                    recovery = validation_route_fire_movement(
                        combat_state.player.position,
                        plan.movement_target, recovery);
                }
                inject_validation_movement(
                    snapshot, settings_data, recovery);
                return snapshot;
            }
            stage10_validation_release_ranged_target(state.abyss_ranged);
            if (current.remaining_targets == 0U) return snapshot;
            initialize_stage11d_sweep_cursor(
                state, combat_state.player.position);
            combat::MovementInput movement =
                stage10_validation_sweep_movement(
                    combat_state, state.sweep_grid,
                    state.sweep_waypoint);
            if ((movement.x != 0 || movement.y != 0)
                    && current.ecology
                        == dungeon::checkpoint::DungeonElement::fire) {
                movement = validation_route_fire_movement(
                    combat_state.player.position,
                    stage10_validation_sweep_waypoint(
                        state.sweep_waypoint), movement);
            }
            inject_validation_movement(snapshot, settings_data, movement);
            return snapshot;
        }
        state.target_ordinal = target->spawn_ordinal;
        combat::MovementInput movement = plan.movement;
        if ((movement.x != 0 || movement.y != 0)
                && current.ecology
                        == dungeon::checkpoint::DungeonElement::fire
                && plan.movement_target_valid) {
            movement = validation_route_fire_movement(
                combat_state.player.position,
                plan.movement_target, movement);
        }
        inject_validation_movement(snapshot, settings_data, movement);
        if (movement.x != 0 || movement.y != 0
                || !plan.stance_reached || !plan.facing_target) {
            if (state.abyss_ranged.close_for_light
                    && (movement.x != 0 || movement.y != 0)
                    && inject_stage11d_rare_abyss_area_skill(
                        snapshot, current)
                        == Stage11DRareAbyssSkillInput::injected) {
                return snapshot;
            }
            return snapshot;
        }
        const Stage11DRareAbyssSkillInput skill_input =
            inject_stage11d_rare_abyss_area_skill(snapshot, current);
        if (skill_input == Stage11DRareAbyssSkillInput::injected) {
            return snapshot;
        }
        if (state.abyss_ranged.melee_chain
                && validation_attack_lane(combat_state, *target)) {
            inject_validation_action(snapshot, settings_data,
                settings::SettingAction::light_attack, true);
            return snapshot;
        }
        if (skill_input == Stage11DRareAbyssSkillInput::no_geometry) {
            stage10_validation_release_ranged_target(state.abyss_ranged);
            return snapshot;
        }
        if (skill_input == Stage11DRareAbyssSkillInput::unavailable
                || skill_input == Stage11DRareAbyssSkillInput::waiting) {
            state.abyss_ranged.melee_chain = true;
            state.abyss_ranged.close_for_light = true;
            state.abyss_ranged.stance_reached = false;
            const Stage10RangedValidationPlan close_plan =
                plan_abyss_ranged();
            combat::MovementInput close = close_plan.movement;
            if ((close.x != 0 || close.y != 0)
                    && current.ecology
                        == dungeon::checkpoint::DungeonElement::fire
                    && close_plan.movement_target_valid) {
                close = validation_route_fire_movement(
                    combat_state.player.position,
                    close_plan.movement_target, close);
            }
            inject_validation_movement(snapshot, settings_data, close);
            if (close.x != 0 || close.y != 0
                    || !close_plan.stance_reached) {
                return snapshot;
            }
        }
        if (validation_attack_lane(combat_state, *target)) {
            inject_validation_action(snapshot, settings_data,
                settings::SettingAction::light_attack, true);
        }
        return snapshot;
    }
    const auto& player = current.combat->player;
    const auto* target = stage11d_priority_monster(current, state);
    if (target == nullptr) return snapshot;
    state.target_ordinal = target->spawn_ordinal;
    if (current.combat->active_skill.id != skills::ActiveSkillId::none) {
        const float active_x = target->position.x - player.position.x;
        const float active_y = target->position.y - player.position.y;
        if (current.combat->active_skill.id
                    == skills::ActiveSkillId::draw_slash
                && active_x * active_x + active_y * active_y
                    < 2.40F * 2.40F) {
            combat::MovementInput retreat{};
            retreat.x = active_x >= 0.0F ? -1 : 1;
            if ((player.position.x <= combat::room_bounds::min_x + 0.20F
                        && retreat.x < 0)
                    || (player.position.x
                            >= combat::room_bounds::max_x - 0.20F
                        && retreat.x > 0)) {
                retreat.x = 0;
                retreat.y = active_y >= 0.0F ? -1 : 1;
            }
            if (current.ecology
                    == dungeon::checkpoint::DungeonElement::fire) {
                combat::Vec3 retreat_target = player.position;
                retreat_target.x += 3.0F * static_cast<float>(retreat.x);
                retreat_target.y += 3.0F * static_cast<float>(retreat.y);
                retreat = validation_route_fire_movement(
                    player.position, retreat_target, retreat);
            }
            inject_validation_movement(snapshot, settings_data, retreat);
        }
        return snapshot;
    }
    const bool facing_target = std::fabs(target->position.x - player.position.x)
            <= 0.20F
        || (target->position.x > player.position.x
            && player.facing == combat::Facing::right)
        || (target->position.x < player.position.x
            && player.facing == combat::Facing::left);
    const float draw_forward = std::fabs(
        target->position.x - player.position.x);
    const float draw_half_width = combat::kDrawSlashHalfWidthAtEnd
        * (draw_forward / combat::kDrawSlashRange);
    if (current.combat->skill_cooldowns[0] == 0U
            && player.hurt_ticks == 0U
            && player.active_attack == combat::AttackId::none
            && current.combat->diagnostics.input_size == 0U
            && facing_target
            && draw_forward <= combat::kDrawSlashRange
            && std::fabs(target->position.y - player.position.y)
                <= draw_half_width) {
        snapshot.active_skill_slots[0] = true;
        return snapshot;
    }
    const bool needs_launcher_setup = target->hp == target->max_hp;
    constexpr float kLauncherDistance = 1.68F;
    constexpr float kComboDistance = 1.98F;
    const float action_distance = needs_launcher_setup
        ? kLauncherDistance : kComboDistance;
    combat::Vec3 destination = target->position;
    const float near_side = target->position.x
        + (player.position.x <= target->position.x
            ? -action_distance : action_distance);
    const float far_side = target->position.x
        + (player.position.x <= target->position.x
            ? action_distance : -action_distance);
    destination.x = near_side >= combat::room_bounds::min_x
            && near_side <= combat::room_bounds::max_x
        ? near_side : far_side;
    combat::MovementInput movement = stage11d_safe_movement_toward(
        player.position, destination, current);
    if (movement.x == 0 && movement.y == 0 && !facing_target) {
        combat::Vec3 facing_step = player.position;
        movement.x = target->position.x > player.position.x ? 1 : -1;
        facing_step.x += 0.10F * static_cast<float>(movement.x);
        if (current.ecology == dungeon::checkpoint::DungeonElement::fire
                && combat::fire_room_obstacle::blocks_player(
                    player.position, facing_step)) {
            movement = {};
        }
    }
    inject_validation_movement(snapshot, settings_data, movement);
    bool nearby_threat = false;
    for (std::size_t index = 0U;
         index < current.combat->monster_count; ++index) {
        const auto& monster = current.combat->monsters[index];
        if (!monster.active || monster.hp <= 0
                || (monster.ai_phase != combat::MonsterAiPhase::telegraph
                    && monster.ai_phase != combat::MonsterAiPhase::active)) {
            continue;
        }
        const float threat_x = monster.position.x - player.position.x;
        const float threat_y = monster.position.y - player.position.y;
        nearby_threat = threat_x * threat_x + threat_y * threat_y <= 9.0F;
        if (nearby_threat) break;
    }
    const bool action_ready = player.hurt_ticks == 0U
        && player.active_attack == combat::AttackId::none
        && current.combat->diagnostics.input_size == 0U;
    const bool light_combo_can_start_or_buffer =
        player.active_attack == combat::AttackId::none
        || player.active_attack == combat::AttackId::j1
        || player.active_attack == combat::AttackId::j2;
    const bool priority_combo = !needs_launcher_setup
        && light_combo_can_start_or_buffer
        && player.hurt_ticks == 0U
        && current.combat->diagnostics.input_size == 0U
        && stage11d_attack_lane(*current.combat, *target, 1.88F, 2.08F);
    if (needs_launcher_setup && action_ready
            && stage11d_attack_lane(
                *current.combat, *target, 1.55F, 1.72F)) {
        inject_validation_action(snapshot, settings_data,
            settings::SettingAction::launcher, true);
    } else if (priority_combo) {
        inject_validation_action(snapshot, settings_data,
            settings::SettingAction::light_attack, true);
    } else if (action_ready && player.position.z <= 0.01F && nearby_threat) {
        inject_validation_action(snapshot, settings_data,
            settings::SettingAction::jump, true);
    }
    return snapshot;
}
// STAGE11D_LOOT_VALIDATION_SEAM_END physical_driver

// STAGE11D_LOOT_VALIDATION_SEAM_BEGIN fixed_step_runtime
bool stage11d_validation_active(const RaylibHostConfig& config) noexcept {
    return config.stage11d_loot_validation
        != Stage11DLootValidationScenario::none;
}

void observe_stage11d_abyss_claim(Stage11DLootValidationState& state,
    const dungeon::DungeonSnapshot& current,
    const items::ItemOwnershipState& item_state) noexcept {
    if (state.abyss_claim_requested) {
        bool still_ground = false;
        for (std::size_t index = 0U;
             index < current.ground_item_count; ++index) {
            still_ground = still_ground
                || current.ground_items[index].item_id == state.abyss_item_id;
        }
        bool now_owned = false;
        for (const auto& item : item_state.items) {
            now_owned = now_owned || item.id == state.abyss_item_id;
        }
        state.abyss_claimed = !still_ground && now_owned;
    }
}
// STAGE11D_LOOT_VALIDATION_SEAM_END fixed_step_runtime

}  // namespace arpg::platform::host_validation
