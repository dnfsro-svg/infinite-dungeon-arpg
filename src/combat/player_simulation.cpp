#include "combat/combat_world.hpp"

#include "combat/attack_catalog.hpp"
#include "combat/combat_scaling.hpp"
#include "combat/room_bounds.hpp"

#include <algorithm>

namespace arpg::combat {
namespace {

constexpr float kTickSeconds = 1.0F / 60.0F;
constexpr float kGroundSpeed = 5.4F;
constexpr float kAirRatio = 0.70F;
constexpr float kJumpSpeed = 8.5F;
constexpr float kGravity = 24.0F;
constexpr float kDiagonal = 0.7071067811865475F;
constexpr std::uint16_t kJ1HitCancelTick = 8;
constexpr std::uint16_t kJ1WhiffCancelTick = 13;
constexpr std::uint16_t kJ2HitCancelTick = 9;
constexpr std::uint16_t kJ2WhiffCancelTick = 15;

int direction(std::int8_t value) noexcept {
    return value < 0 ? -1 : (value > 0 ? 1 : 0);
}

bool inside_fire_brazier(Vec3 position) noexcept {
    return position.x >= -1.40F && position.x <= 1.40F
        && position.y >= -1.70F && position.y <= 1.70F;
}

std::uint8_t combo_stage_for(AttackId id) noexcept {
    switch (id) {
    case AttackId::j1:
        return 1;
    case AttackId::j2:
        return 2;
    case AttackId::j3:
        return 3;
    default:
        return 0;
    }
}

PlayerState state_for_phase(AttackPhase phase) noexcept {
    switch (phase) {
    case AttackPhase::startup:
        return PlayerState::attack_startup;
    case AttackPhase::active:
        return PlayerState::attack_active;
    case AttackPhase::recovery:
        return PlayerState::attack_recovery;
    case AttackPhase::finished:
        return PlayerState::idle;
    }
    return PlayerState::idle;
}

}  // namespace

void CombatWorld::simulate_player(MovementInput movement) noexcept {
    const auto is_airborne = [this]() noexcept {
        return player_.position.z > 0.0F || player_.velocity.z != 0.0F;
    };

    const auto advance_vertical = [this, &is_airborne](
                                      bool preserve_attack_state) noexcept {
        if (!is_airborne()) {
            player_.velocity.z = 0.0F;
            return;
        }

        player_.position.z += player_.velocity.z * kTickSeconds;
        player_.velocity.z -= kGravity * kTickSeconds;
        if (player_.position.z <= 0.0F) {
            player_.position.z = 0.0F;
            player_.velocity.z = 0.0F;
            player_.state = PlayerState::landing;
            player_.air_attack_available = true;

            CombatEvent landing{};
            landing.kind = CombatEventKind::landing;
            landing.tick = tick_;
            landing.position = player_.position;
            emit_event(landing);
        } else if (!preserve_attack_state) {
            player_.state = player_.velocity.z > 0.0F
                                ? PlayerState::jump_rise
                                : PlayerState::jump_fall;
        }
    };

    const auto start_attack = [this](AttackId id) noexcept {
        const AttackDefinition* definition = find_attack_definition(id);
        if (definition == nullptr) {
            return;
        }

        attack_.id = id;
        attack_.elapsed_ticks = 0;
        const auto& build = encounter_config_.player_build;
        const std::int64_t local_multiplier = modifiers::kFixedOne
            + static_cast<std::int64_t>(build.local_attack_speed_bp);
        const std::int64_t attack_speed =
            build.values.attack_speed * local_multiplier
            / modifiers::kFixedOne;
        attack_.startup_ticks = scaled_phase_ticks(
            definition->startup_ticks, attack_speed);
        attack_.recovery_ticks = scaled_phase_ticks(
            definition->recovery_ticks, attack_speed);
        attack_.connected = false;
        attack_.impact_event_emitted = false;
        attack_.hit_targets.fill(false);

        player_.combo_stage = combo_stage_for(id);
        player_.velocity.x = 0.0F;
        player_.velocity.y = 0.0F;
        const float facing = player_.facing == Facing::right ? 1.0F : -1.0F;
        player_.position.x = std::clamp(
            player_.position.x + definition->lunge_distance * facing,
            room_bounds::min_x,
            room_bounds::max_x);
        apply_attack_assist(*definition);
        player_.state = PlayerState::attack_startup;
        if (id == AttackId::air_j) {
            player_.air_attack_available = false;
        }

        CombatEvent swing{};
        swing.kind = CombatEventKind::swing;
        swing.tick = tick_;
        swing.attack = id;
        swing.feedback = definition->feedback;
        swing.position = player_.position;
        emit_event(swing);
    };

    const auto finish_attack = [this, &is_airborne]() noexcept {
        attack_.id = AttackId::none;
        attack_.elapsed_ticks = 0;
        attack_.startup_ticks = 0;
        attack_.recovery_ticks = 0;
        attack_.connected = false;
        attack_.impact_event_emitted = false;
        attack_.hit_targets.fill(false);
        player_.combo_stage = 0;
        player_.state = is_airborne()
                            ? (player_.velocity.z > 0.0F
                                   ? PlayerState::jump_rise
                                   : PlayerState::jump_fall)
                            : PlayerState::idle;
    };

    if (player_.state == PlayerState::landing && attack_.id == AttackId::none) {
        player_.state = PlayerState::idle;
    }

    if (attack_.id != AttackId::none) {
        player_.velocity.x = 0.0F;
        player_.velocity.y = 0.0F;

        const bool j1_cancel = attack_.id == AttackId::j1
                            && attack_.elapsed_ticks
                                   >= (attack_.connected
                                           ? kJ1HitCancelTick
                                           : kJ1WhiffCancelTick);
        const bool j2_cancel = attack_.id == AttackId::j2
                            && attack_.elapsed_ticks
                                   >= (attack_.connected
                                           ? kJ2HitCancelTick
                                           : kJ2WhiffCancelTick);
        if ((j1_cancel || j2_cancel) && input_buffer_.consume(Action::light)) {
            start_attack(j1_cancel ? AttackId::j2 : AttackId::j3);
            advance_vertical(true);
            return;
        }

        const AttackDefinition* definition = find_attack_definition(attack_.id);
        if (definition == nullptr) {
            finish_attack();
            advance_vertical(false);
            return;
        }

        ++attack_.elapsed_ticks;
        const AttackPhase phase =
            attack_phase_at(*definition, attack_.elapsed_ticks,
                            attack_.startup_ticks, attack_.recovery_ticks);
        if (phase == AttackPhase::finished) {
            finish_attack();
            advance_vertical(false);
            return;
        }

        player_.state = state_for_phase(phase);
        advance_vertical(true);
        return;
    }

    bool airborne = is_airborne();
    if (airborne) {
        if (player_.air_attack_available &&
            input_buffer_.consume(Action::light)) {
            start_attack(AttackId::air_j);
            advance_vertical(true);
            return;
        }
    } else if (player_.state == PlayerState::idle ||
               player_.state == PlayerState::move) {
        if (input_buffer_.consume(Action::jump)) {
            player_.velocity.z = kJumpSpeed * static_cast<float>(
                encounter_config_.player_build.values.jump_speed)
                / static_cast<float>(modifiers::kFixedOne);
            player_.state = PlayerState::jump_rise;
            airborne = true;
        } else if (input_buffer_.consume(Action::launcher)) {
            start_attack(AttackId::launcher);
            return;
        } else if (input_buffer_.consume(Action::light)) {
            start_attack(AttackId::j1);
            return;
        }
    }

    const int x_direction = direction(movement.x);
    const int y_direction = direction(movement.y);
    const float diagonal = x_direction != 0 && y_direction != 0
                               ? kDiagonal
                               : 1.0F;
    const auto& values = encounter_config_.player_build.values;
    const float movement_scale = static_cast<float>(values.movement_speed)
        / static_cast<float>(modifiers::kFixedOne);
    const float air_scale = static_cast<float>(values.air_control)
        / static_cast<float>(modifiers::kFixedOne);
    const float abyss_ground_scale = airborne
        ? 1.0F
        : scale_basis_points(
              1.0F, encounter_config_.abyss.player_ground_move_bp);
    const float speed = kGroundSpeed * movement_scale * abyss_ground_scale
        * (airborne ? kAirRatio * air_scale : 1.0F)
        * static_cast<float>(10000 - std::clamp(player_.status.slow_bp, 0, 10000))
        / 10000.0F;
    player_.velocity.x = static_cast<float>(x_direction) * speed * diagonal;
    player_.velocity.y = static_cast<float>(y_direction) * speed * diagonal;
    if (x_direction < 0) {
        player_.facing = Facing::left;
    } else if (x_direction > 0) {
        player_.facing = Facing::right;
    }

    Vec3 candidate = player_.position;
    candidate.x = std::clamp(
        candidate.x + player_.velocity.x * kTickSeconds,
        room_bounds::min_x,
        room_bounds::max_x);
    candidate.y = std::clamp(
        candidate.y + player_.velocity.y * kTickSeconds,
        room_bounds::min_y,
        room_bounds::max_y);
    if (!encounter_config_.fire_room_obstacles || !inside_fire_brazier(candidate)) {
        player_.position = candidate;
    }

    if (airborne) {
        advance_vertical(false);
        return;
    }

    player_.velocity.z = 0.0F;
    player_.state = x_direction != 0 || y_direction != 0
                        ? PlayerState::move
                        : PlayerState::idle;
}

}  // namespace arpg::combat
