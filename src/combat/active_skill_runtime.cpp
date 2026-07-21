#include "combat/combat_world.hpp"

#include "combat/active_skill_runtime.hpp"
#include "combat/attack_catalog.hpp"
#include "combat/combat_scaling.hpp"
#include "combat/room_bounds.hpp"
#include "modifiers/damage_types.hpp"
#include "modifiers/modifier_math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace arpg::combat {
namespace {

constexpr std::uint16_t kBreakWindowTicks = 180U;
constexpr std::uint16_t kMediumHitStopTicks = 5U;
constexpr std::int32_t kBasisPoints = 10000;
constexpr float kTickSeconds = 1.0F / 60.0F;
constexpr float kGroundSpeed = 5.4F;
constexpr float kAirRatio = 0.70F;
constexpr float kGravity = 24.0F;
constexpr float kDiagonal = 0.7071067811865475F;

int direction(std::int8_t value) noexcept {
    return value < 0 ? -1 : (value > 0 ? 1 : 0);
}

int ceil_divide(std::int64_t numerator, std::int32_t divisor) noexcept {
    if (numerator <= 0 || divisor <= 0) return 0;
    return static_cast<int>((numerator + divisor - 1) / divisor);
}

int saturating_damage_add(int left, int right) noexcept {
    const int maximum = (std::numeric_limits<int>::max)();
    if (left <= 0) return std::max(0, right);
    if (right <= 0) return left;
    return left > maximum - right ? maximum : left + right;
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

void CombatWorld::tick_active_skill() noexcept {
    for (std::uint16_t& cooldown : active_skill_.cooldowns) {
        if (cooldown != 0U) --cooldown;
    }

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
    const auto packet = build_player_hit_packet(
        kDrawSlashBasePhysical, encounter_config_.player_build);
    if (!packet.has_value()) return;

    const float facing = active_skill_.locked_facing == Facing::right
        ? 1.0F : -1.0F;
    const Vec3 center = active_skill_.snapshot.locked_center;
    const AttackDefinition impact{
        AttackId::none, 0U, 0U, 0U, kDrawSlashBasePhysical,
        kDrawSlashBreakDamage, ImpactKind::medium_hitstun, {}, 0.0F,
        kDrawSlashKnockbackSpeed, 0.0F, FeedbackLevel::medium};

    const std::size_t physical_index = modifiers::damage_index(
        modifiers::DamageType::physical);
    for (std::size_t index = 0U; index < monsters_.slots_.size(); ++index) {
        MonsterRuntime& monster = monsters_.slots_[index];
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

        active_skill_.hit_latch[index] = true;
        int hp_damage = 0;
        for (std::size_t component = 0U; component < packet->amount.size();
             ++component) {
            int value = std::max(0, packet->amount[component]);
            if (component == physical_index
                && monster.affix_profile.armor_rating > 0) {
                const std::int32_t reduction_bp =
                    modifiers::rating_to_basis_points(
                        monster.affix_profile.armor_rating);
                value = ceil_divide(
                    static_cast<std::int64_t>(value)
                        * (kBasisPoints - std::clamp(
                            reduction_bp, 0, kBasisPoints)),
                    kBasisPoints);
            }
            hp_damage = saturating_damage_add(hp_damage, value);
        }
        const int packet_total = hp_damage;
        if (monster.shield != 0 && hp_damage > 0) {
            const int absorbed = std::min(monster.shield, hp_damage);
            monster.shield -= absorbed;
            hp_damage -= absorbed;
            if (monster.shield == 0) monster.shield_ticks = 0U;
        }
        if (monster.affix_profile.shield_recharge_delay_ticks != 0U) {
            monster.shield_recharge_ticks =
                monster.affix_profile.shield_recharge_delay_ticks;
        }
        monster.hp = std::max(0, monster.hp - hp_damage);

        const float relative_x = player_.position.x - monster.position.x;
        const bool front_attack = legacy_mode_
            || (monster.facing == Facing::right ? relative_x >= 0.0F
                                                 : relative_x <= 0.0F);
        bool accepts_impact = monster.armor != ArmorState::armored
            || !front_attack;
        bool starts_break = false;
        if (monster.hp != 0 && monster.armor == ArmorState::armored
            && front_attack) {
            monster.break_value = std::max(
                0, monster.break_value - kDrawSlashBreakDamage);
            if (monster.break_value == 0) {
                monster.armor = ArmorState::broken;
                monster.break_window_ticks = kBreakWindowTicks;
                starts_break = true;
                accepts_impact = true;
            }
        }
        monster.hit_stop_ticks = std::max(
            monster.hit_stop_ticks, kMediumHitStopTicks);

        CombatEvent hit{};
        hit.kind = CombatEventKind::hit;
        hit.tick = tick_;
        hit.target_index = static_cast<std::uint8_t>(index);
        hit.hit_count = 1U;
        hit.feedback = FeedbackLevel::medium;
        hit.position = monster.position;
        hit.value = packet_total;
        emit_event(hit);

        if (starts_break) {
            CombatEvent break_started{};
            break_started.kind = CombatEventKind::break_started;
            break_started.tick = tick_;
            break_started.target_index = static_cast<std::uint8_t>(index);
            break_started.feedback = FeedbackLevel::medium;
            break_started.position = monster.position;
            emit_event(break_started);
        }
        if (monster.hp == 0 || accepts_impact) {
            apply_dummy_impact(index, impact);
        }
    }
}

}  // namespace arpg::combat
