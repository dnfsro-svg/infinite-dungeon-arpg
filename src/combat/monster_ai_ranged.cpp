#include "combat/combat_world.hpp"

#include "combat/monster_ai_common.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/room_bounds.hpp"

#include <algorithm>
#include <array>
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

const MonsterAffixTierValues* affix_values(
    const MonsterAffixSet& affixes, MonsterAffixId id) noexcept {
    for (std::size_t index = 0; index < affixes.count
         && index < affixes.values.size(); ++index) {
        const MonsterAffixInstance& instance = affixes.values[index];
        if (instance.id != id) continue;
        const MonsterAffixDefinition* definition =
            monster_affix_definition(instance.id);
        const std::size_t tier = static_cast<std::size_t>(instance.tier);
        if (definition != nullptr && tier < definition->tiers.size()) {
            return &definition->tiers[tier];
        }
    }
    return nullptr;
}

DamagePacket scaled_multishot_packet(
    DamagePacket packet, std::int32_t percentage_bp) noexcept {
    for (int& amount : packet.amount) {
        amount = static_cast<int>((static_cast<std::int64_t>(amount)
            * percentage_bp) / 10000);
    }
    return packet;
}

bool move_to_preferred_range(
    MonsterRuntime& monster,
    const MonsterDefinition& definition,
    Vec3 player,
    const abyss::AbyssCombatConfig& abyss_config) noexcept {
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
        const float speed = abyss_monster_move_step(
            definition.move_speed, monster.affix_profile,
            abyss_config);
        monster.velocity.x = dx / magnitude * speed;
        monster.velocity.y = dy / magnitude * speed;
        monster.position.x += monster.velocity.x;
        monster.position.y += monster.velocity.y;
        clamp_position(monster.position);
        return true;
    }
    monster.velocity = Vec3{};
    monster.ai_phase = MonsterAiPhase::telegraph;
    monster.ai_ticks = abyss_monster_attack_ticks(
        definition.telegraph_ticks, monster.affix_profile,
        abyss_config);
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
        static_cast<void>(move_to_preferred_range(
            monster, definition, player_.position, encounter_config_.abyss));
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
                const MonsterAffixTierValues* multishot = affix_values(
                    monster.affixes, MonsterAffixId::multishot);
                const MonsterAffixTierValues* chain = affix_values(
                    monster.affixes, MonsterAffixId::chain_lightning);
                const std::uint8_t count = multishot == nullptr ? 1U
                    : multishot->projectile_count;
                std::array<Vec3, 4> fanned_velocities{};
                for (std::size_t projectile_index = 0U;
                     projectile_index < count
                         && projectile_index < fanned_velocities.size();
                     ++projectile_index) {
                    const float centered_index = static_cast<float>(projectile_index)
                        - (static_cast<float>(count) - 1.0F) * 0.5F;
                    const float angle = centered_index * 0.20F;
                    const float cosine = std::cos(angle);
                    const float sine = std::sin(angle);
                    fanned_velocities[projectile_index] = Vec3{
                        velocity.x * cosine - velocity.y * sine,
                        velocity.x * sine + velocity.y * cosine,
                        velocity.z};
                }
                for (std::size_t projectile_index = 0U;
                     projectile_index < count
                         && projectile_index < fanned_velocities.size();
                     ++projectile_index) {
                    DamagePacket packet = scale_monster_affix_damage(
                        definition.contact_damage, monster.affix_profile);
                    if (multishot != nullptr) {
                        packet = scaled_multishot_packet(
                            packet, multishot->primary_bp);
                    }
                    packet = scale_monster_outgoing_damage(
                        packet, encounter_config_.abyss.monster_damage_bp);
                    if (!spawn_projectile(
                            MonsterHandle{static_cast<std::uint16_t>(slot),
                                          monster.generation},
                            monster.position, fanned_velocities[projectile_index],
                            kProjectileLifetime, packet, kProjectileRadius,
                            chain != nullptr, monster.affixes)) {
                        break;
                    }
                }
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
                        const float retreat_step = abyss_monster_move_step(
                            definition.move_speed, monster.affix_profile,
                            encounter_config_.abyss);
                        monster.position.x += (monster.position.x - player_.position.x)
                            / distance * retreat_step;
                        monster.position.y += (monster.position.y - player_.position.y)
                            / distance * retreat_step;
                        clamp_position(monster.position);
                    }
                    monster.ai_phase = MonsterAiPhase::cooldown;
                    monster.ai_ticks = abyss_monster_cooldown_ticks(
                        kSupportFallbackCooldown, monster.affix_profile,
                        encounter_config_.abyss);
                }
            }
            monster.contact_attack_resolved = true;
        }
        if (tick_down(monster.ai_ticks)) {
            monster.ai_phase = MonsterAiPhase::recovery;
            monster.ai_ticks = abyss_monster_attack_ticks(
                definition.recovery_ticks, monster.affix_profile,
                encounter_config_.abyss);
        }
        return;
    case MonsterAiPhase::recovery:
        monster.velocity = Vec3{};
        if (tick_down(monster.ai_ticks)) {
            monster.ai_phase = MonsterAiPhase::cooldown;
            monster.ai_ticks = abyss_monster_cooldown_ticks(
                definition.cooldown_ticks, monster.affix_profile,
                encounter_config_.abyss);
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
