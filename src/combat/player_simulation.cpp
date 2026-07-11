#include "combat/combat_world.hpp"

#include "combat/attack_catalog.hpp"

#include <algorithm>

namespace arpg::combat {
namespace {

constexpr float kTickSeconds = 1.0F / 60.0F;
constexpr float kGroundSpeed = 5.4F;
constexpr float kAirRatio = 0.70F;
constexpr float kJumpSpeed = 8.5F;
constexpr float kGravity = 24.0F;
constexpr float kDiagonal = 0.7071067811865475F;
constexpr float kRoomMinX = -8.0F;
constexpr float kRoomMaxX = 8.0F;
constexpr float kRoomMinY = -3.5F;
constexpr float kRoomMaxY = 3.5F;
constexpr std::uint16_t kJ1WhiffCancelTick = 13;
constexpr std::uint16_t kJ2WhiffCancelTick = 15;

int direction(std::int8_t value) noexcept {
    return value < 0 ? -1 : (value > 0 ? 1 : 0);
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
        ++attack_.serial;
        if (attack_.serial == 0) {
            ++attack_.serial;
        }
        attack_.connected = false;
        attack_.impact_event_emitted = false;
        attack_.hit_targets.fill(false);

        player_.combo_stage = combo_stage_for(id);
        player_.velocity.x = 0.0F;
        player_.velocity.y = 0.0F;
        const float facing = player_.facing == Facing::right ? 1.0F : -1.0F;
        player_.position.x = std::clamp(
            player_.position.x + definition->lunge_distance * facing,
            kRoomMinX,
            kRoomMaxX);
        player_.state = PlayerState::attack_startup;
        if (id == AttackId::air_j) {
            player_.air_attack_available = false;
        }
    };

    const auto finish_attack = [this, &is_airborne]() noexcept {
        attack_.id = AttackId::none;
        attack_.elapsed_ticks = 0;
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

        const bool j1_whiff = attack_.id == AttackId::j1 &&
                              !attack_.connected &&
                              attack_.elapsed_ticks >= kJ1WhiffCancelTick;
        const bool j2_whiff = attack_.id == AttackId::j2 &&
                              !attack_.connected &&
                              attack_.elapsed_ticks >= kJ2WhiffCancelTick;
        if ((j1_whiff || j2_whiff) && input_buffer_.consume(Action::light)) {
            start_attack(j1_whiff ? AttackId::j2 : AttackId::j3);
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
        const std::uint32_t total_ticks =
            static_cast<std::uint32_t>(definition->startup_ticks) +
            static_cast<std::uint32_t>(definition->active_ticks) +
            static_cast<std::uint32_t>(definition->recovery_ticks);
        if (attack_.elapsed_ticks >= total_ticks) {
            finish_attack();
            advance_vertical(false);
            return;
        }

        player_.state = state_for_phase(
            attack_phase_at(*definition, attack_.elapsed_ticks));
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
            player_.velocity.z = kJumpSpeed;
            player_.state = PlayerState::jump_rise;
            airborne = true;
        } else if (input_buffer_.consume(Action::heavy)) {
            start_attack(AttackId::heavy);
            return;
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
    const float speed = kGroundSpeed * (airborne ? kAirRatio : 1.0F);
    player_.velocity.x = static_cast<float>(x_direction) * speed * diagonal;
    player_.velocity.y = static_cast<float>(y_direction) * speed * diagonal;
    if (x_direction < 0) {
        player_.facing = Facing::left;
    } else if (x_direction > 0) {
        player_.facing = Facing::right;
    }

    player_.position.x = std::clamp(
        player_.position.x + player_.velocity.x * kTickSeconds,
        kRoomMinX,
        kRoomMaxX);
    player_.position.y = std::clamp(
        player_.position.y + player_.velocity.y * kTickSeconds,
        kRoomMinY,
        kRoomMaxY);

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
