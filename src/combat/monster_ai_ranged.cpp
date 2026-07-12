#include "combat/combat_world.hpp"

#include "combat/monster_ai_common.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/room_bounds.hpp"

#include <algorithm>
#include <cmath>

namespace arpg::combat {
namespace {

constexpr float kRangeSlack = 0.20F;
constexpr float kProjectileRadius = 0.16F;
constexpr std::uint16_t kProjectileLifetime = 120;
constexpr std::uint16_t kSupportFallbackCooldown = 12;

void clamp_position(Vec3& position) noexcept {
    position.x = std::clamp(position.x, room_bounds::min_x, room_bounds::max_x);
    position.y = std::clamp(position.y, room_bounds::min_y, room_bounds::max_y);
}

bool has_tag(const MonsterDefinition& definition, MonsterTag tag) noexcept {
    return (definition.tags & static_cast<std::uint16_t>(tag)) != 0U;
}

bool move_to_preferred_range(
    MonsterRuntime& monster,
    const MonsterDefinition& definition,
    Vec3 player) noexcept {
    const float distance = target_distance(monster.position, player);
    if (distance > definition.preferred_range + kRangeSlack
        || distance < definition.preferred_range - kRangeSlack) {
        float dx = player.x - monster.position.x;
        float dy = player.y - monster.position.y;
        if (distance < definition.preferred_range - kRangeSlack) {
            dx = -dx;
            dy = -dy;
        }
        if (distance <= 0.0001F) {
            dx = monster.facing == Facing::right ? -1.0F : 1.0F;
            dy = 0.0F;
        }
        const float magnitude = std::sqrt(dx * dx + dy * dy);
        monster.velocity.x = dx / magnitude * definition.move_speed;
        monster.velocity.y = dy / magnitude * definition.move_speed;
        monster.position.x += monster.velocity.x;
        monster.position.y += monster.velocity.y;
        clamp_position(monster.position);
        return true;
    }
    monster.velocity = Vec3{};
    monster.ai_phase = MonsterAiPhase::telegraph;
    monster.ai_ticks = definition.telegraph_ticks;
    monster.attack_target_position = player;
    return false;
}

}  // namespace

void CombatWorld::simulate_ranged_ai(
    std::size_t slot,
    MonsterRuntime& monster,
    const MonsterDefinition& definition) noexcept {
    const bool is_shooter = monster.id == MonsterId::lightning_shooter;
    switch (monster.ai_phase) {
    case MonsterAiPhase::move:
        face_toward(monster, player_.position);
        static_cast<void>(move_to_preferred_range(monster, definition, player_.position));
        return;
    case MonsterAiPhase::telegraph:
        face_toward(monster, player_.position);
        monster.velocity = Vec3{};
        if (tick_down(monster.ai_ticks)) {
            monster.ai_phase = MonsterAiPhase::active;
            monster.ai_ticks = std::max<std::uint16_t>(1U, definition.active_ticks);
            ++monster.attack_serial;
            monster.contact_attack_resolved = false;
        }
        return;
    case MonsterAiPhase::active:
        monster.velocity = Vec3{};
        if (!monster.contact_attack_resolved) {
            if (is_shooter) {
                const float dx = monster.attack_target_position.x - monster.position.x;
                const float dy = monster.attack_target_position.y - monster.position.y;
                const float magnitude = std::sqrt(dx * dx + dy * dy);
                const float speed = definition.projectile_speed > 0.0F
                    ? definition.projectile_speed : 0.14F;
                Vec3 velocity{};
                if (magnitude > 0.0001F) {
                    velocity.x = dx / magnitude * speed;
                    velocity.y = dy / magnitude * speed;
                } else {
                    velocity.x = monster.facing == Facing::right ? speed : -speed;
                }
                static_cast<void>(spawn_projectile(
                    MonsterHandle{static_cast<std::uint16_t>(slot), monster.generation},
                    monster.position, velocity, kProjectileLifetime,
                    definition.contact_damage, kProjectileRadius));
            } else {
                std::size_t target_index = monsters_.slots_.size();
                for (std::size_t index = 0; index < monsters_.slots_.size(); ++index) {
                    if (index == slot) continue;
                    MonsterRuntime& candidate = monsters_.slots_[index];
                    const MonsterDefinition* candidate_definition =
                        monster_definition(candidate.id);
                    if (!candidate.active || candidate.hp <= 0 || candidate.shield != 0
                        || candidate_definition == nullptr
                        || has_tag(*candidate_definition, MonsterTag::support)) {
                        continue;
                    }
                    target_index = index;
                    break;
                }
                if (target_index < monsters_.slots_.size()) {
                    MonsterRuntime& target = monsters_.slots_[target_index];
                    modifiers::EffectSet* effects = ensure_effects(target_index);
                    if (effects != nullptr) {
                        const modifiers::EffectDefinition barrier = water_barrier_effect(target);
                        static_cast<void>(effects->apply(barrier));
                        apply_effect_commands(target, *effects);
                    }
                } else {
                    const float distance = target_distance(monster.position, player_.position);
                    if (distance < definition.preferred_range && distance > 0.0001F) {
                        monster.position.x += (monster.position.x - player_.position.x)
                            / distance * definition.move_speed;
                        monster.position.y += (monster.position.y - player_.position.y)
                            / distance * definition.move_speed;
                        clamp_position(monster.position);
                    }
                    monster.ai_phase = MonsterAiPhase::cooldown;
                    monster.ai_ticks = kSupportFallbackCooldown;
                }
            }
            monster.contact_attack_resolved = true;
        }
        if (tick_down(monster.ai_ticks)) {
            monster.ai_phase = MonsterAiPhase::recovery;
            monster.ai_ticks = definition.recovery_ticks;
        }
        return;
    case MonsterAiPhase::recovery:
        monster.velocity = Vec3{};
        if (tick_down(monster.ai_ticks)) {
            monster.ai_phase = MonsterAiPhase::cooldown;
            monster.ai_ticks = definition.cooldown_ticks;
        }
        return;
    case MonsterAiPhase::cooldown:
        face_toward(monster, player_.position);
        monster.velocity = Vec3{};
        if (tick_down(monster.ai_ticks)) monster.ai_phase = MonsterAiPhase::move;
        return;
    case MonsterAiPhase::defeated:
        monster.velocity = Vec3{};
        return;
    case MonsterAiPhase::idle:
        return;
    }
}

}  // namespace arpg::combat
