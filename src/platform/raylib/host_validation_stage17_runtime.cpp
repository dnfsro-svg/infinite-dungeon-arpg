#include "host_validation_stage17.hpp"

#include "active_skill_assets.hpp"
#include "active_skill_renderer.hpp"
#include "host_input.hpp"
#include "host_validation_input.hpp"
#include "host_validation_navigation.hpp"
#include "inventory_renderer.hpp"
#include "inventory_view_math.hpp"
#include "platform/settings/settings_types.hpp"
#include "raylib_host.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::platform::host_validation {
namespace {
void stage17_press_action(PhysicalKeySnapshot& snapshot,
    const settings::SettingsData& input_settings,
    const settings::SettingAction action) noexcept {
    inject_validation_pressed(snapshot,
        settings::binding_for(input_settings, action));
}

void stage17_apply_movement(PhysicalKeySnapshot& snapshot,
    const settings::SettingsData& input_settings,
    const combat::MovementInput movement) noexcept {
    inject_validation_movement(snapshot, input_settings, movement);
}

struct Stage17IsolatedStormTarget final {
    const combat::MonsterSnapshot* monster{};
    combat::Vec3 projected_center{};
    float route_distance_squared{};
    float clearance_squared{};
};

[[nodiscard]] Stage17IsolatedStormTarget stage17_isolated_storm_target(
    const combat::CombatSnapshot& combat_state) noexcept {
    Stage17IsolatedStormTarget best{};
    combat::Vec3 projected_center = combat_state.player.position;
    projected_center.x += combat::kStormCenterForward;
    const float finisher_radius_squared =
        combat::kStormFinisherRadius * combat::kStormFinisherRadius;
    for (const combat::MonsterSnapshot& candidate : combat_state.monsters) {
        if (!candidate.active || candidate.hp <= 0
                || candidate.id != combat::MonsterId::fire_bomber) {
            continue;
        }
        const float route_x = candidate.position.x - projected_center.x;
        const float route_y = candidate.position.y - projected_center.y;
        const float route_distance_squared =
            route_x * route_x + route_y * route_y;
        if (route_distance_squared > finisher_radius_squared) continue;
        float clearance_squared = (std::numeric_limits<float>::max)();
        std::size_t targets_in_finisher = 0U;
        for (const combat::MonsterSnapshot& other : combat_state.monsters) {
            if (!other.active || other.hp <= 0) continue;
            const float x = other.position.x - projected_center.x;
            const float y = other.position.y - projected_center.y;
            const float distance_squared = x * x + y * y;
            targets_in_finisher += distance_squared <= finisher_radius_squared;
            if (other.monster_ordinal != candidate.monster_ordinal) {
                clearance_squared = (std::min)(
                    clearance_squared, distance_squared);
            }
        }
        if (targets_in_finisher != 1U) continue;
        if (best.monster == nullptr
                || clearance_squared > best.clearance_squared
                || (clearance_squared == best.clearance_squared
                    && route_distance_squared < best.route_distance_squared)) {
            best = {&candidate, projected_center, route_distance_squared,
                clearance_squared};
        }
    }
    return best;
}

[[nodiscard]] bool stage17_prepare_isolated_storm(
    PhysicalKeySnapshot& snapshot,
    const settings::SettingsData& input_settings,
    const combat::CombatSnapshot& combat_state,
    const bool fire_room,
    Stage17SkillStonesValidationState& state) noexcept {
    if (state.storm_isolation_invalidated) return false;
    ++state.storm_approach_frames;
    if (!state.storm_approach_locked) {
        const Stage17IsolatedStormTarget target =
            stage17_isolated_storm_target(combat_state);
        if (target.monster == nullptr) return false;
        state.storm_approach_center = target.projected_center;
        state.storm_approach_target_start = target.monster->position;
        state.storm_approach_player_start = combat_state.player.position;
        state.storm_approach_initial_clearance =
            std::sqrt(target.clearance_squared);
        state.storm_approach_target_ordinal = target.monster->monster_ordinal;
        state.storm_approach_locked = true;
    }
    state.storm_approach_player_last = combat_state.player.position;
    if (state.storm_approach_frames % 300U == 0U) {
        const std::size_t sample = state.storm_approach_frames / 300U - 1U;
        if (sample < state.storm_approach_samples.size()) {
            state.storm_approach_samples[sample] = combat_state.player.position;
            state.storm_approach_sampled[sample] = true;
        }
    }

    std::size_t targets_in_finisher = 0U;
    bool locked_target_in_finisher = false;
    const combat::MonsterSnapshot* locked_target = nullptr;
    combat::Vec3 projected_center = combat_state.player.position;
    projected_center.x += combat::kStormCenterForward;
    const float finisher_radius_squared =
        combat::kStormFinisherRadius * combat::kStormFinisherRadius;
    for (const combat::MonsterSnapshot& monster : combat_state.monsters) {
        if (!monster.active || monster.hp <= 0) continue;
        const float x = monster.position.x - projected_center.x;
        const float y = monster.position.y - projected_center.y;
        if (x * x + y * y > finisher_radius_squared) continue;
        ++targets_in_finisher;
        if (monster.monster_ordinal == state.storm_approach_target_ordinal) {
            locked_target_in_finisher = true;
            locked_target = &monster;
        }
    }
    if (targets_in_finisher != 1U || !locked_target_in_finisher) {
        state.storm_isolation_invalidated = true;
        state.storm_isolation_invalidated_frame = state.storm_approach_frames;
        state.storm_isolation_invalidated_target_count = targets_in_finisher;
        return false;
    }

    const combat::Vec3& player = combat_state.player.position;
    combat::MovementInput movement{};
    const float target_x = locked_target->position.x - projected_center.x;
    const float target_y = locked_target->position.y - projected_center.y;
    constexpr float kRouteTargetRadius = 2.65F;
    if (target_x * target_x + target_y * target_y
            > kRouteTargetRadius * kRouteTargetRadius) {
        movement.x = static_cast<std::int8_t>(target_x > 0.0F ? 1 : -1);
        movement.y = static_cast<std::int8_t>(target_y > 0.0F ? 1 : -1);
    } else if (combat_state.player.facing != combat::Facing::right) {
        movement.x = 1;
    }
    if (fire_room && (movement.x != 0 || movement.y != 0)) {
        movement = validation_route_fire_movement(
            player, locked_target->position, movement);
    }
    if (movement.x != 0 || movement.y != 0) {
        ++state.storm_approach_movement_injections;
        stage17_apply_movement(snapshot, input_settings, movement);
        return false;
    }

    return true;
}

[[nodiscard]] Vector2 stage17_center(const Rectangle rectangle) noexcept {
    return {rectangle.x + rectangle.width * 0.5F,
        rectangle.y + rectangle.height * 0.5F};
}

void stage17_click(PhysicalKeySnapshot& snapshot,
    const Rectangle rectangle) noexcept {
    snapshot.mouse_left = true;
    snapshot.mouse_position = stage17_center(rectangle);
}


[[nodiscard]] bool stage17_same_point(
    combat::Vec3 left, combat::Vec3 right) noexcept {
    constexpr float kTolerance = 0.0001F;
    return std::fabs(left.x - right.x) <= kTolerance
        && std::fabs(left.y - right.y) <= kTolerance
        && std::fabs(left.z - right.z) <= kTolerance;
}

[[nodiscard]] bool stage17_all_cooldowns_zero(
    const combat::CombatSnapshot& combat_state) noexcept {
    return std::all_of(combat_state.skill_cooldowns.begin(),
        combat_state.skill_cooldowns.end(),
        [](const std::uint16_t ticks) noexcept { return ticks == 0U; });
}

[[nodiscard]] bool stage17_final_loadout(
    const skills::SkillLoadoutState& loadout) noexcept {
    return loadout.slots[0].active == skills::ActiveSkillId::none
        && loadout.slots[1].active == skills::ActiveSkillId::draw_slash
        && loadout.slots[2].active == skills::ActiveSkillId::none
        && loadout.slots[3].active == skills::ActiveSkillId::none
        && loadout.slots[4].active == skills::ActiveSkillId::storm_swords;
}
}  // namespace

void observe_stage17_draw_runtime(const RaylibHostConfig& config,
    Stage17SkillStonesValidationState& state,
    const dungeon::DungeonSnapshot& presented,
    const ActiveSkillDrawRuntimeStatus& status) noexcept {
    if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none
        || config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::restarted_loadout
        || !presented.combat.has_value()) {
        return;
    }
    const combat::ActiveSkillSnapshot& skill =
        presented.combat->active_skill;
    Stage17SkillDrawRuntimeEvidence* evidence = nullptr;
    if (skill.id == skills::ActiveSkillId::draw_slash) {
        evidence = &state.draw_runtime;
    } else if (skill.id == skills::ActiveSkillId::storm_swords) {
        evidence = &state.storm_runtime;
    } else {
        return;
    }
    ++evidence->samples;
    evidence->material_frame_drawn = evidence->material_frame_drawn
        || status.material_frame_drawn;
    evidence->base_player_drawn = evidence->base_player_drawn
        || status.base_player_drawn;
    evidence->procedural_main_visual_peak = (std::max)(
        evidence->procedural_main_visual_peak,
        status.procedural_main_visual_count);
    const bool sample_valid = status.mode == ActiveSkillVisualMode::material
        && status.atlas == active_skill_material_atlas(skill.id)
        && status.atlas_frame == active_skill_visual_frame_index(
            skill.id, skill.elapsed_ticks)
        && status.material_frame_drawn
        && !status.base_player_drawn
        && status.procedural_main_visual_count == 0U;
    evidence->valid = evidence->valid && sample_valid;
    state.renderer_status_failure_latched =
        state.renderer_status_failure_latched || !sample_valid;
}

void observe_stage17_combat_event(
    Stage17SkillStonesValidationState* const state,
    const combat::CombatEvent& event) noexcept {
    if (state == nullptr || event.kind != combat::CombatEventKind::hit) return;
    if (event.skill == skills::ActiveSkillId::draw_slash) {
        ++state->draw_hit_count;
        if (!state->suppress_draw_captures && !state->draw_captured) {
            state->capture_pending = Stage17Capture::draw_hit;
        }
    } else if (event.skill == skills::ActiveSkillId::storm_swords) {
        if (event.finisher) {
            ++state->storm_finisher_hit_count;
            if (!state->storm_finisher_captured) {
                state->capture_pending = Stage17Capture::storm_finisher;
            }
        } else {
            ++state->storm_strike_hit_count;
            state->storm_strike_count = std::max(
                state->storm_strike_count, event.strike_index);
            if (!state->storm_array_captured) {
                state->capture_pending = Stage17Capture::storm_array;
            }
        }
    }
}

[[nodiscard]] PhysicalKeySnapshot inject_stage17_physical_edges(
    PhysicalKeySnapshot snapshot, const RaylibHostConfig& config,
    const settings::SettingsData& input_settings,
    const dungeon::DungeonSnapshot& current,
    Stage17SkillStonesValidationState& state) noexcept {
    if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none
        || snapshot.focus_lost || state.suspend_injection) {
        return snapshot;
    }
    const ActiveSkillLoadoutLayout layout = active_skill_loadout_layout(
        config.window_width, config.window_height);
    switch (state.step) {
    case Stage17ValidationStep::initial:
    case Stage17ValidationStep::draw_active:
    case Stage17ValidationStep::complete:
    case Stage17ValidationStep::restart_capture:
    case Stage17ValidationStep::cooldown_drain:
        break;
    case Stage17ValidationStep::empty_slot_3:
        snapshot.active_skill_slots[2] = true;
        break;
    case Stage17ValidationStep::empty_slot_4:
        snapshot.active_skill_slots[3] = true;
        break;
    case Stage17ValidationStep::empty_slot_5:
        snapshot.active_skill_slots[4] = true;
        break;
    case Stage17ValidationStep::approach_draw:
        // Seed 753 starts with the locked two-target draw-slash geometry.
        // Exercise the production binding directly before AI movement changes it.
        snapshot.active_skill_slots[0] = true;
        break;
    case Stage17ValidationStep::approach_storm:
        if (current.combat.has_value()
                && stage17_prepare_isolated_storm(
                    snapshot, input_settings, *current.combat,
                    current.ecology
                        == dungeon::checkpoint::DungeonElement::fire,
                    state)) {
            snapshot.active_skill_slots[1] = true;
        }
        break;
    case Stage17ValidationStep::storm_active:
        if (state.storm_threat_pull_locked) {
            stage17_apply_movement(
                snapshot, input_settings, state.storm_threat_pull_movement);
        }
        break;
    case Stage17ValidationStep::open_inventory:
    case Stage17ValidationStep::restart_open_inventory:
        stage17_press_action(snapshot, input_settings,
            settings::SettingAction::inventory);
        break;
    case Stage17ValidationStep::open_skill_page:
        stage17_click(snapshot, layout.skill_stones_page_button);
        state.step = Stage17ValidationStep::remove_slot_1;
        break;
    case Stage17ValidationStep::remove_slot_1:
        stage17_click(snapshot, layout.remove_button);
        break;
    case Stage17ValidationStep::select_draw_inventory:
        stage17_click(snapshot, layout.inventory_slots[0]);
        state.step = Stage17ValidationStep::equip_slot_5;
        break;
    case Stage17ValidationStep::equip_slot_5:
        stage17_click(snapshot, layout.main_slots[4]);
        break;
    case Stage17ValidationStep::swap_slots_2_5:
        stage17_click(snapshot, layout.main_slots[1]);
        break;
    case Stage17ValidationStep::close_inventory:
        stage17_press_action(snapshot, input_settings,
            settings::SettingAction::inventory);
        break;
    case Stage17ValidationStep::restart_open_skill_page:
        stage17_click(snapshot, layout.skill_stones_page_button);
        state.step = Stage17ValidationStep::restart_capture;
        state.capture_pending = Stage17Capture::restarted;
        break;
    }
    return snapshot;
}

void observe_stage17_submitted_actions(
    const RaylibHostConfig& config,
    Stage17SkillStonesValidationState& state,
    const SubmittedFrameActions& submitted) noexcept {
    if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none) {
        return;
    }
    switch (state.step) {
    case Stage17ValidationStep::empty_slot_3:
    case Stage17ValidationStep::empty_slot_4:
    case Stage17ValidationStep::empty_slot_5: {
        const std::size_t index = state.step
                == Stage17ValidationStep::empty_slot_3 ? 2U
            : state.step == Stage17ValidationStep::empty_slot_4 ? 3U : 4U;
        state.empty_slots_none[index - 2U] =
            submitted.skills[index] == combat::SkillCastResult::none;
        state.public_input_path = true;
        state.step = index == 2U ? Stage17ValidationStep::empty_slot_4
            : index == 3U ? Stage17ValidationStep::empty_slot_5
                          : Stage17ValidationStep::approach_draw;
        break;
    }
    case Stage17ValidationStep::approach_draw:
        if (submitted.skills[0] == combat::SkillCastResult::accepted) {
            state.draw_accepted = true;
            state.public_input_path = true;
            state.step = Stage17ValidationStep::draw_active;
        }
        break;
    case Stage17ValidationStep::approach_storm:
        if (submitted.skills[1] == combat::SkillCastResult::accepted) {
            state.storm_accepted = true;
            state.public_input_path = true;
            state.step = Stage17ValidationStep::storm_active;
        }
        break;
    default:
        break;
    }
}

void observe_stage17_snapshot(const RaylibHostConfig& config,
    Stage17SkillStonesValidationState& state,
    const dungeon::DungeonSnapshot& current) noexcept {
    if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none) {
        return;
    }
    if (!state.initial_recorded) {
        state.initial_loadout = current.skill_loadout;
        state.initial_recorded = true;
        if (config.stage17_skill_stones_validation
                == Stage17SkillStonesValidationScenario::storm_sequence) {
            state.suppress_draw_captures = true;
            state.step = Stage17ValidationStep::approach_draw;
        } else if (config.stage17_skill_stones_validation
                == Stage17SkillStonesValidationScenario::restarted_loadout) {
            state.restart_persisted = stage17_final_loadout(current.skill_loadout);
            state.restart_cooldowns_zero = current.combat.has_value()
                && stage17_all_cooldowns_zero(*current.combat);
            state.step = Stage17ValidationStep::restart_open_inventory;
        }
    }
    if (current.death.has_value()) {
        state.aborted_by_death = true;
        return;
    }
    if (!current.combat.has_value()) return;
    const combat::CombatSnapshot& combat_state = *current.combat;
    if (state.step == Stage17ValidationStep::draw_active
            && combat_state.active_skill.id == skills::ActiveSkillId::draw_slash) {
        state.draw_frame_peak = std::max(state.draw_frame_peak,
            combat_state.active_skill.frame_index);
        if (config.stage17_skill_stones_validation
                != Stage17SkillStonesValidationScenario::storm_sequence
                && !state.draw_windup_captured
                && combat_state.active_skill.frame_index >= 6U
                && combat_state.active_skill.frame_index < 18U) {
            state.capture_pending = Stage17Capture::draw_windup;
        }
    } else if (state.step == Stage17ValidationStep::draw_active
            && combat_state.active_skill.id == skills::ActiveSkillId::none) {
        if (config.stage17_skill_stones_validation
                == Stage17SkillStonesValidationScenario::storm_sequence) {
            if (state.draw_accepted && state.draw_hit_count == 2U) {
                state.step = Stage17ValidationStep::approach_storm;
            }
        } else if (state.draw_windup_captured && state.draw_captured) {
            state.step = Stage17ValidationStep::open_inventory;
        }
    }
    if (state.step == Stage17ValidationStep::cooldown_drain
            && state.production_cooldown_wait_started
            && stage17_all_cooldowns_zero(combat_state)) {
        state.production_cooldown_wait_end_tick = combat_state.tick;
        state.production_cooldowns_zero_before_shutdown =
            state.production_cooldown_wait_end_tick
                > state.production_cooldown_wait_start_tick;
        state.step = Stage17ValidationStep::complete;
    }
    if (state.step != Stage17ValidationStep::storm_active) return;
    const combat::ActiveSkillSnapshot& skill = combat_state.active_skill;
    if (skill.id == skills::ActiveSkillId::storm_swords) {
        if (!state.storm_lock_recorded) {
            state.storm_lock_recorded = true;
            state.storm_center = skill.locked_center;
            state.storm_player_start = combat_state.player.position;
            state.storm_player_last = combat_state.player.position;
            const combat::MonsterSnapshot* threat = nullptr;
            float threat_distance_squared =
                (std::numeric_limits<float>::max)();
            for (const combat::MonsterSnapshot& monster :
                    combat_state.monsters) {
                if (!monster.active || monster.hp <= 0
                        || monster.monster_ordinal
                            == state.storm_approach_target_ordinal) {
                    continue;
                }
                const float x = monster.position.x - state.storm_center.x;
                const float y = monster.position.y - state.storm_center.y;
                const float distance_squared = x * x + y * y;
                if (threat == nullptr
                        || distance_squared < threat_distance_squared) {
                    threat = &monster;
                    threat_distance_squared = distance_squared;
                }
            }
            if (threat != nullptr) {
                const float x = threat->position.x - state.storm_center.x;
                const float y = threat->position.y - state.storm_center.y;
                state.storm_threat_pull_movement = {
                    static_cast<std::int8_t>(x > 0.0F ? 1
                        : (x < 0.0F ? -1 : 0)),
                    static_cast<std::int8_t>(y > 0.0F ? 1
                        : (y < 0.0F ? -1 : 0)),
                };
                state.storm_threat_pull_locked =
                    state.storm_threat_pull_movement.x != 0
                    || state.storm_threat_pull_movement.y != 0;
            }
        } else {
            state.storm_center_locked = state.storm_center_locked
                && stage17_same_point(state.storm_center, skill.locked_center);
            state.storm_player_moved = state.storm_player_moved
                || !stage17_same_point(
                    state.storm_player_start, combat_state.player.position);
        }
        state.storm_player_last = combat_state.player.position;
        state.storm_strike_count = std::max(
            state.storm_strike_count, skill.strike_index);
        state.storm_sword_peak = std::max(state.storm_sword_peak,
            skill.spawned_sword_count);
        if (skill.strike_index > state.storm_sampled_strike_count
                && skill.strike_index <= combat::kStormStrikeCount) {
            std::uint16_t targets = 0U;
            const float strike_radius_squared =
                combat::kStormStrikeRadius * combat::kStormStrikeRadius;
            for (const combat::MonsterSnapshot& monster :
                    combat_state.monsters) {
                if (!monster.active || monster.hp <= 0) continue;
                const float x = monster.position.x - skill.locked_center.x;
                const float y = monster.position.y - skill.locked_center.y;
                targets += x * x + y * y <= strike_radius_squared;
            }
            state.storm_targets_per_strike[skill.strike_index - 1U] = targets;
            state.storm_sampled_strike_count = skill.strike_index;
        }
        state.storm_invulnerable_seen = state.storm_invulnerable_seen
            || skill.player_invulnerable;
        if (skill.phase == combat::ActiveSkillPhase::strikes
                && !state.storm_array_captured) {
            state.capture_pending = Stage17Capture::storm_array;
        }
        if (skill.spawned_sword_count > 12U && !state.storm_aerial_captured) {
            state.capture_pending = Stage17Capture::storm_aerial;
        }
        if (skill.phase == combat::ActiveSkillPhase::finisher
                && !state.storm_finisher_captured) {
            state.capture_pending = Stage17Capture::storm_finisher;
        }
        state.storm_finisher_phase_seen = state.storm_finisher_phase_seen
            || skill.phase == combat::ActiveSkillPhase::finisher;
    } else if (state.storm_array_captured && state.storm_aerial_captured
            && state.storm_finisher_captured) {
        state.step = config.stage17_skill_stones_validation
                == Stage17SkillStonesValidationScenario::storm_sequence
            ? Stage17ValidationStep::complete
            : Stage17ValidationStep::open_inventory;
    }
}

void observe_stage17_inventory(const RaylibHostConfig& config,
    Stage17SkillStonesValidationState& state,
    const InventoryRenderer& inventory,
    const dungeon::DungeonSnapshot& current) noexcept {
    if (config.stage17_skill_stones_validation
            == Stage17SkillStonesValidationScenario::none) {
        return;
    }
    if (state.step == Stage17ValidationStep::open_inventory
            && inventory.is_open()) {
        state.step = Stage17ValidationStep::open_skill_page;
    } else if (state.step == Stage17ValidationStep::remove_slot_1
            && current.skill_loadout.slots[0].active
                == skills::ActiveSkillId::none) {
        state.step = Stage17ValidationStep::select_draw_inventory;
    } else if (state.step == Stage17ValidationStep::equip_slot_5
            && current.skill_loadout.slots[4].active
                == skills::ActiveSkillId::draw_slash) {
        state.step = Stage17ValidationStep::swap_slots_2_5;
    } else if (state.step == Stage17ValidationStep::swap_slots_2_5
            && stage17_final_loadout(current.skill_loadout)) {
        state.final_loadout = current.skill_loadout;
        state.production_transactions =
            !current.pending_save_kind.has_value();
        if (state.production_transactions) {
            state.step = Stage17ValidationStep::close_inventory;
        }
    } else if (state.step == Stage17ValidationStep::close_inventory
            && !inventory.is_open() && current.combat.has_value()) {
        state.production_inventory_closed = true;
        state.production_cooldown_start_ticks =
            current.combat->skill_cooldowns[0U];
        state.production_cooldown_wait_start_tick = current.combat->tick;
        state.production_cooldown_wait_started =
            state.production_cooldown_start_ticks != 0U;
        state.step = Stage17ValidationStep::cooldown_drain;
    } else if (state.step == Stage17ValidationStep::restart_open_inventory
            && inventory.is_open()) {
        state.step = Stage17ValidationStep::restart_open_skill_page;
    }
}
}  // namespace arpg::platform::host_validation
