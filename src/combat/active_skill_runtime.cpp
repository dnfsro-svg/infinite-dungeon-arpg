#include "combat/combat_world.hpp"

#include "combat/active_skill_runtime.hpp"
#include "combat/combat_scaling.hpp"
#include "combat/room_bounds.hpp"
#include "modifiers/modifier_math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace arpg::combat {
namespace {

constexpr float kTickSeconds = 1.0F / 60.0F;
constexpr float kGroundSpeed = 5.4F;
constexpr float kAirRatio = 0.70F;
constexpr float kGravity = 24.0F;
constexpr float kDiagonal = 0.7071067811865475F;

int direction(std::int8_t value) noexcept {
    return value < 0 ? -1 : (value > 0 ? 1 : 0);
}

}  // namespace

void CombatWorld::simulate_active_skill_movement(MovementInput movement) noexcept {
    const int x_direction = direction(movement.x);
    const int y_direction = direction(movement.y);
    const bool airborne = player_.position.z > 0.0F || player_.velocity.z != 0.0F;
    const float diagonal = x_direction != 0 && y_direction != 0
        ? kDiagonal : 1.0F;
    const auto& values = encounter_config_.player_build.values;
    const float movement_scale = static_cast<float>(values.movement_speed)
        / static_cast<float>(modifiers::kFixedOne);
    const float air_scale = static_cast<float>(values.air_control)
        / static_cast<float>(modifiers::kFixedOne);
    const float abyss_ground_scale = airborne ? 1.0F : scale_basis_points(
        1.0F, encounter_config_.abyss.player_ground_move_bp);
    const float speed = kGroundSpeed * movement_scale * abyss_ground_scale
        * (airborne ? kAirRatio * air_scale : 1.0F)
        * static_cast<float>(10000 - std::clamp(
            player_.status.slow_bp, 0, 10000)) / 10000.0F;
    player_.velocity.x = static_cast<float>(x_direction) * speed * diagonal;
    player_.velocity.y = static_cast<float>(y_direction) * speed * diagonal;
    player_.facing = active_skill_.locked_facing;
    player_.position.x = std::clamp(player_.position.x
        + player_.velocity.x * kTickSeconds,
        room_bounds::min_x, room_bounds::max_x);
    player_.position.y = std::clamp(player_.position.y
        + player_.velocity.y * kTickSeconds,
        room_bounds::min_y, room_bounds::max_y);
    if (!airborne) {
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
    }
}

void CombatWorld::tick_active_skill_cooldowns() noexcept {
    for (std::uint16_t& cooldown : active_skill_.cooldowns) {
        if (cooldown != 0U) --cooldown;
    }
}

void CombatWorld::tick_active_skill() noexcept {
    ActiveSkillSnapshot& cast = active_skill_.snapshot;
    if (cast.id == skills::ActiveSkillId::none) return;
    if (cast.id != skills::ActiveSkillId::draw_slash) {
        active_skill_.snapshot = ActiveSkillSnapshot{};
        active_skill_.hit_latch.fill(false);
        return;
    }

    ++cast.elapsed_ticks;
    if (cast.elapsed_ticks < kDrawSlashStartupTicks) {
        cast.phase = ActiveSkillPhase::startup;
        player_.state = PlayerState::attack_startup;
        return;
    }
    if (cast.elapsed_ticks == kDrawSlashStartupTicks) {
        cast.phase = ActiveSkillPhase::strikes;
        resolve_draw_slash_hits();
        player_.state = PlayerState::attack_active;
        return;
    }
    if (cast.elapsed_ticks <= kDrawSlashStartupTicks + kDrawSlashRecoveryTicks) {
        cast.phase = ActiveSkillPhase::recovery;
        player_.state = PlayerState::attack_recovery;
        return;
    }

    active_skill_.snapshot = ActiveSkillSnapshot{};
    active_skill_.hit_latch.fill(false);
}

void CombatWorld::resolve_draw_slash_hits() noexcept {
    const float facing = active_skill_.locked_facing == Facing::right
        ? 1.0F : -1.0F;
    const Vec3 center = active_skill_.snapshot.locked_center;
    const PlayerAttackHitSpec hit_spec{
        AttackId::none, kDrawSlashBasePhysical, kDrawSlashBreakDamage,
        ImpactKind::medium_hitstun, kDrawSlashKnockbackSpeed, 0.0F,
        FeedbackLevel::medium};
    for (std::size_t index = 0U; index < monsters_.slots_.size(); ++index) {
        const MonsterRuntime& monster = monsters_.slots_[index];
        if (active_skill_.hit_latch[index] || !monster.active || monster.hp <= 0
            || monster.reaction == ReactionState::defeated
            || monster.reaction == ReactionState::respawning) {
            continue;
        }

        const float forward = (monster.position.x - center.x) * facing;
        if (forward < 0.0F || forward > kDrawSlashRange) continue;
        const float half_width = kDrawSlashHalfWidthAtEnd
            * (forward / kDrawSlashRange);
        if (std::fabs(monster.position.y - center.y) > half_width) continue;

        static_cast<void>(resolve_player_attack_hit(
            index, hit_spec, active_skill_.hit_latch));
    }
}

}  // namespace arpg::combat
