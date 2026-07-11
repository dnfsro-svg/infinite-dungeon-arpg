#include "combat/combat_world.hpp"

#include "combat/attack_catalog.hpp"

#include <algorithm>
#include <array>

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

constexpr std::array<int, kDummyCount> kDummyHitPoints{{300, 450, 700}};
constexpr std::array<int, kDummyCount> kDummyBreakValues{{0, 0, 120}};

int direction(std::int8_t value) noexcept {
    return value < 0 ? -1 : (value > 0 ? 1 : 0);
}

}  // namespace

CombatWorld::CombatWorld(CombatLabConfig config) noexcept : config_(config) {
    reset();
}

bool CombatWorld::queue_action(Action action) noexcept {
    return input_buffer_.push(action);
}

void CombatWorld::tick(MovementInput movement) noexcept {
    ++tick_;
    simulate_player(movement);
    input_buffer_.age(player_.hit_stop_ticks != 0);
}

void CombatWorld::reset() noexcept {
    player_ = PlayerRuntime{};
    player_.position = config_.player_spawn;

    for (std::size_t index = 0; index < dummies_.size(); ++index) {
        DummyRuntime dummy{};
        dummy.kind = static_cast<DummyKind>(index);
        dummy.spawn = config_.dummy_spawns[index];
        dummy.position = dummy.spawn;
        dummy.max_hp = kDummyHitPoints[index];
        dummy.hp = dummy.max_hp;
        dummy.max_break = kDummyBreakValues[index];
        dummy.break_value = dummy.max_break;
        dummy.armor = index == 2 ? ArmorState::armored : ArmorState::none;
        dummies_[index] = dummy;
    }

    attack_ = AttackRuntime{};
    input_buffer_.clear();
    input_buffer_.reset_diagnostics();
    tick_ = 0;
    event_overflow_count_ = 0;
}

CombatSnapshot CombatWorld::snapshot() const noexcept {
    CombatSnapshot result{};
    result.tick = tick_;
    result.player = PlayerSnapshot{
        player_.position,
        player_.velocity,
        player_.facing,
        player_.state,
        attack_.id,
        attack_.id == AttackId::none
            ? AttackPhase::finished
            : attack_phase_at(
                  *find_attack_definition(attack_.id), attack_.elapsed_ticks),
        attack_.elapsed_ticks,
        player_.combo_stage,
        player_.hit_stop_ticks,
        player_.air_attack_available,
    };

    for (std::size_t index = 0; index < dummies_.size(); ++index) {
        const DummyRuntime& dummy = dummies_[index];
        result.dummies[index] = DummySnapshot{
            dummy.position,
            dummy.velocity,
            dummy.kind,
            dummy.reaction,
            dummy.armor,
            dummy.hp,
            dummy.max_hp,
            dummy.break_value,
            dummy.max_break,
            dummy.break_window_ticks,
            dummy.hit_stop_ticks,
        };
    }

    result.diagnostics = CombatDiagnostics{
        input_buffer_.size(),
        input_buffer_.expired_count(),
        input_buffer_.overflow_count(),
        event_overflow_count_,
    };
    return result;
}

void CombatWorld::simulate_player(MovementInput movement) noexcept {
    if (player_.state == PlayerState::landing) {
        player_.state = PlayerState::idle;
    }

    const bool started_airborne = player_.position.z > 0.0F;
    if (!started_airborne &&
        (player_.state == PlayerState::idle ||
         player_.state == PlayerState::move) &&
        input_buffer_.consume(Action::jump)) {
        player_.velocity.z = kJumpSpeed;
        player_.state = PlayerState::jump_rise;
    }

    const bool airborne = player_.position.z > 0.0F ||
                          player_.state == PlayerState::jump_rise ||
                          player_.state == PlayerState::jump_fall;
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
        player_.position.z += player_.velocity.z * kTickSeconds;
        player_.velocity.z -= kGravity * kTickSeconds;
        if (player_.position.z <= 0.0F) {
            player_.position.z = 0.0F;
            player_.velocity.z = 0.0F;
            player_.state = PlayerState::landing;
            player_.air_attack_available = true;
        } else {
            player_.state = player_.velocity.z > 0.0F
                                ? PlayerState::jump_rise
                                : PlayerState::jump_fall;
        }
        return;
    }

    player_.velocity.z = 0.0F;
    player_.state = x_direction != 0 || y_direction != 0
                        ? PlayerState::move
                        : PlayerState::idle;
}

}  // namespace arpg::combat
