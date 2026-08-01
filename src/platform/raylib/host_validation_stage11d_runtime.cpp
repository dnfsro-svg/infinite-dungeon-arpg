#include "host_validation_stage11d.hpp"

#include "combat/active_skill_runtime.hpp"
#include "combat/fire_room_obstacle.hpp"
#include "combat/room_bounds.hpp"
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

[[nodiscard]] const combat::MonsterSnapshot* stage11d_priority_monster(
    const combat::CombatSnapshot& snapshot) noexcept {
    const combat::MonsterSnapshot* best = nullptr;
    for (std::size_t index = 0U; index < snapshot.monster_count; ++index) {
        const auto& monster = snapshot.monsters[index];
        if (!monster.active || monster.hp <= 0) continue;
        const bool bomber = monster.id == combat::MonsterId::fire_bomber;
        const bool best_bomber = best != nullptr
            && best->id == combat::MonsterId::fire_bomber;
        if (best == nullptr || (bomber && !best_bomber)
                || (bomber == best_bomber
                    && monster.spawn_ordinal < best->spawn_ordinal)) {
            best = &monster;
        }
    }
    return best;
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
            || snapshot.focus_lost) {
        return snapshot;
    }
    ++state.injected_frames;
    if (config.stage11d_loot_validation == Scenario::rare_only_abyss
            && state.captured && current.combat.has_value()) {
        for (std::size_t index = 0U; index < current.ground_item_count; ++index) {
            const auto& item = current.ground_items[index];
            if (item.item_id != state.abyss_item_id) continue;
            combat::MovementInput movement = validation_movement_toward(
                current.combat->player.position, item.position);
            if (current.ecology
                    == dungeon::checkpoint::DungeonElement::fire) {
                movement = validation_route_fire_movement(
                    current.combat->player.position, item.position, movement);
            }
            inject_validation_movement(snapshot, settings_data, movement);
            state.abyss_claim_requested = true;
            break;
        }
        return snapshot;
    }
    const bool ordinary_ready = stage11d_has_three_ordinary_rarities(current);
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
    const auto& player = current.combat->player;
    const bool aggressive_abyss = config.stage11d_loot_validation
        == Scenario::rare_only_abyss;
    const auto* target = aggressive_abyss
        ? nearest_living_monster(*current.combat)
        : stage11d_priority_monster(*current.combat);
    if (target == nullptr) return snapshot;
    state.target_ordinal = target->spawn_ordinal;
    if (aggressive_abyss) {
        combat::MovementInput movement = validation_movement_toward(
            player.position, target->position);
        if (current.ecology
                == dungeon::checkpoint::DungeonElement::fire) {
            movement = validation_route_fire_movement(
                player.position, target->position, movement);
        }
        inject_validation_movement(snapshot, settings_data, movement);
        if (player.hurt_ticks == 0U
                && current.combat->diagnostics.input_size == 0U
                && validation_attack_lane(*current.combat, *target)) {
            inject_validation_action(snapshot, settings_data,
                settings::SettingAction::light_attack, true);
        }
        return snapshot;
    }
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
