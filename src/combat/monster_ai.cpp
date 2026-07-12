#include "combat/combat_world.hpp"

#include "combat/combat_collision.hpp"
#include "combat/monster_catalog.hpp"

#include <algorithm>
#include <cmath>

namespace arpg::combat {
namespace {

constexpr float kTickSeconds = 1.0F / 60.0F;
constexpr float kGravity = 24.0F;
constexpr float kRoomMinX = -8.0F;
constexpr float kRoomMaxX = 8.0F;
constexpr float kRoomMinY = -3.5F;
constexpr float kRoomMaxY = 3.5F;
constexpr float kPlayerHalfWidth = 0.45F;
constexpr float kPlayerHalfDepth = 0.35F;
constexpr float kPlayerHalfHeight = 1.60F;
constexpr float kContactHalfDepth = 0.50F;
constexpr float kContactHalfHeight = 1.50F;
constexpr std::uint16_t kLightHitstunTicks = 10;
constexpr std::uint16_t kMediumHitstunTicks = 16;
constexpr std::uint16_t kKnockdownTicks = 45;
constexpr std::uint16_t kRisingTicks = 30;
constexpr float kRangeSlack = 0.20F;
constexpr std::uint16_t kSupportFallbackCooldown = 12;
constexpr float kProjectileRadius = 0.16F;
constexpr std::uint16_t kProjectileLifetime = 120;

float reaction_scale(DummyKind kind) noexcept {
    return kind == DummyKind::light ? 1.25F : 1.0F;
}

std::uint16_t reaction_ticks(
    DummyKind kind,
    std::uint16_t base_ticks) noexcept {
    if (kind != DummyKind::light) {
        return base_ticks;
    }
    const std::uint32_t scaled =
        static_cast<std::uint32_t>(base_ticks) * 5U;
    return static_cast<std::uint16_t>((scaled + 3U) / 4U);
}

void clamp_position(Vec3& position) noexcept {
    position.x = std::clamp(position.x, kRoomMinX, kRoomMaxX);
    position.y = std::clamp(position.y, kRoomMinY, kRoomMaxY);
}

float target_distance(const Vec3& source, const Vec3& target) noexcept {
    const float dx = target.x - source.x;
    const float dy = target.y - source.y;
    return std::sqrt(dx * dx + dy * dy);
}

void face_player(MonsterRuntime& monster, const Vec3& player) noexcept {
    if (player.x < monster.position.x) {
        monster.facing = Facing::left;
    } else if (player.x > monster.position.x) {
        monster.facing = Facing::right;
    }
}

void integrate_reaction(MonsterRuntime& monster) noexcept {
    monster.position.x += monster.velocity.x * kTickSeconds;
    monster.position.y += monster.velocity.y * kTickSeconds;
    clamp_position(monster.position);
}

bool has_tag(const MonsterDefinition& definition, MonsterTag tag) noexcept {
    return (definition.tags & static_cast<std::uint16_t>(tag)) != 0U;
}

bool move_to_preferred_range(
    MonsterRuntime& monster,
    const MonsterDefinition& definition,
    const Vec3& player) noexcept {
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

void CombatWorld::resolve_monster_contact_attack(
    std::size_t slot) noexcept {
    if (slot >= monsters_.slots_.size()) {
        return;
    }
    MonsterRuntime& monster = monsters_.slots_[slot];
    const MonsterDefinition* definition = monster_definition(monster.id);
    if (definition == nullptr || !monster.active || monster.hp <= 0
        || monster.reaction == ReactionState::defeated) {
        return;
    }

    const float reach = std::max(0.45F, definition->preferred_range);
    const float facing = monster.facing == Facing::right ? 1.0F : -1.0F;
    Aabb contact{};
    if (facing > 0.0F) {
        contact.minimum.x = monster.position.x;
        contact.maximum.x = monster.position.x + reach;
    } else {
        contact.minimum.x = monster.position.x - reach;
        contact.maximum.x = monster.position.x;
    }
    contact.minimum.y = monster.position.y - kContactHalfDepth;
    contact.maximum.y = monster.position.y + kContactHalfDepth;
    contact.minimum.z = monster.position.z - kContactHalfHeight;
    contact.maximum.z = monster.position.z + kContactHalfHeight;

    const Aabb player_hurtbox{
        {player_.position.x - kPlayerHalfWidth,
         player_.position.y - kPlayerHalfDepth,
         player_.position.z - kPlayerHalfHeight},
        {player_.position.x + kPlayerHalfWidth,
         player_.position.y + kPlayerHalfDepth,
         player_.position.z + kPlayerHalfHeight},
    };
    if (overlaps_inclusive(contact, player_hurtbox)) {
        apply_player_damage(
            definition->contact_damage, monster.position,
            definition->feedback);
    }
}

void CombatWorld::simulate_monster(std::size_t slot) noexcept {
    if (slot >= monsters_.slots_.size()) {
        return;
    }
    MonsterRuntime& monster = monsters_.slots_[slot];
    if (!monster.active) {
        return;
    }
    const MonsterDefinition* definition = monster_definition(monster.id);
    if (definition == nullptr) {
        return;
    }

    if (monster.armor == ArmorState::broken
        && monster.reaction != ReactionState::defeated
        && monster.break_window_ticks != 0) {
        --monster.break_window_ticks;
        if (monster.break_window_ticks == 0) {
            monster.armor = ArmorState::armored;
            monster.break_value = monster.max_break;
        }
    }

    if (monster.hp <= 0 || monster.reaction == ReactionState::defeated) {
        monster.ai_phase = MonsterAiPhase::defeated;
        monster.velocity = Vec3{};
        remove_owned_projectiles(MonsterHandle{
            static_cast<std::uint16_t>(slot), monster.generation});
        return;
    }

    if (monster.shield_ticks != 0) {
        --monster.shield_ticks;
        if (monster.shield_ticks == 0) {
            monster.shield = 0;
        }
    }

    if (monster.reaction != ReactionState::idle) {
        switch (monster.reaction) {
        case ReactionState::hitstun:
            integrate_reaction(monster);
            if (monster.reaction_ticks != 0) {
                --monster.reaction_ticks;
            }
            if (monster.reaction_ticks == 0) {
                monster.reaction = ReactionState::idle;
                monster.velocity = Vec3{};
                monster.ai_phase = MonsterAiPhase::move;
                monster.ai_ticks = 0;
            }
            return;
        case ReactionState::airborne:
            integrate_reaction(monster);
            monster.position.z += monster.velocity.z * kTickSeconds;
            monster.velocity.z -= kGravity * kTickSeconds;
            if (monster.position.z <= 0.0F) {
                monster.position.z = 0.0F;
                monster.velocity.z = 0.0F;
                monster.reaction = ReactionState::knockdown;
                monster.reaction_ticks = reaction_ticks(
                    monster.kind, kKnockdownTicks);
                CombatEvent landing{};
                landing.kind = CombatEventKind::landing;
                landing.tick = tick_;
                landing.target_index = static_cast<std::uint8_t>(slot);
                landing.position = monster.position;
                emit_event(landing);
            }
            return;
        case ReactionState::knockdown:
            integrate_reaction(monster);
            if (monster.reaction_ticks != 0) {
                --monster.reaction_ticks;
            }
            if (monster.reaction_ticks == 0) {
                monster.reaction = ReactionState::rising;
                monster.reaction_ticks = reaction_ticks(
                    monster.kind, kRisingTicks);
                monster.velocity = Vec3{};
            }
            return;
        case ReactionState::rising:
            if (monster.reaction_ticks != 0) {
                --monster.reaction_ticks;
            }
            if (monster.reaction_ticks == 0) {
                monster.reaction = ReactionState::idle;
                monster.velocity = Vec3{};
                monster.ai_phase = MonsterAiPhase::move;
                monster.ai_ticks = 0;
            }
            return;
        case ReactionState::respawning:
            monster.reaction = ReactionState::idle;
            monster.ai_phase = MonsterAiPhase::move;
            monster.ai_ticks = 0;
            return;
        case ReactionState::idle:
        case ReactionState::defeated:
            break;
        }
    }

    const bool is_shooter = monster.id == MonsterId::lightning_shooter;
    const bool is_support = monster.id == MonsterId::water_support;
    const bool is_melee = monster.id == MonsterId::chaos_chaser
                       || monster.id == MonsterId::water_bulwark;
    const bool is_bomber = monster.id == MonsterId::fire_bomber;
    const bool is_charger = monster.id == MonsterId::fire_charger;
    const bool is_dasher = monster.id == MonsterId::lightning_dasher;
    const bool is_hazard = monster.id == MonsterId::chaos_hazard;
    if (!is_melee && !is_shooter && !is_support && !is_bomber
        && !is_charger && !is_dasher && !is_hazard) {
        monster.velocity = Vec3{};
        monster.ai_phase = MonsterAiPhase::idle;
        monster.ai_ticks = 0;
        monster.contact_attack_resolved = true;
        return;
    }

    if (monster.ai_phase == MonsterAiPhase::idle) {
        monster.ai_phase = MonsterAiPhase::move;
        monster.ai_ticks = 0;
    }

    switch (monster.ai_phase) {
    case MonsterAiPhase::move: {
        face_player(monster, player_.position);
        if (is_shooter || is_support) {
            static_cast<void>(move_to_preferred_range(
                monster, *definition, player_.position));
            return;
        }
        const float distance = target_distance(monster.position, player_.position);
        if (distance > definition->preferred_range && distance > 0.0001F) {
            const float dx = player_.position.x - monster.position.x;
            const float dy = player_.position.y - monster.position.y;
            const float speed = definition->move_speed;
            monster.velocity.x = dx / distance * speed;
            monster.velocity.y = dy / distance * speed;
            monster.position.x += monster.velocity.x;
            monster.position.y += monster.velocity.y;
            clamp_position(monster.position);
            return;
        }

        monster.velocity = Vec3{};
        monster.ai_phase = MonsterAiPhase::telegraph;
        monster.ai_ticks = definition->telegraph_ticks;
        monster.attack_target_position = player_.position;
        const float active_ticks = static_cast<float>(
            std::max<std::uint16_t>(1U, definition->active_ticks));
        monster.attack_vector = Vec3{
            (monster.attack_target_position.x - monster.position.x) / active_ticks,
            (monster.attack_target_position.y - monster.position.y) / active_ticks,
            (monster.attack_target_position.z - monster.position.z) / active_ticks,
        };
        if (is_hazard) {
            static_cast<void>(spawn_hazard(
                MonsterHandle{static_cast<std::uint16_t>(slot), monster.generation},
                monster.attack_target_position, 1.25F,
                definition->telegraph_ticks, definition->hazard_ticks,
                30U, definition->contact_damage));
        }
        return;
    }
    case MonsterAiPhase::telegraph:
        if (!is_charger && !is_dasher) {
            face_player(monster, player_.position);
        }
        monster.velocity = Vec3{};
        if (monster.ai_ticks != 0) {
            --monster.ai_ticks;
        }
        if (monster.ai_ticks == 0) {
            monster.ai_phase = MonsterAiPhase::active;
            monster.ai_ticks = std::max<std::uint16_t>(
                1U, definition->active_ticks);
            ++monster.attack_serial;
            monster.contact_attack_resolved = false;
        }
        return;
    case MonsterAiPhase::active:
        monster.velocity = Vec3{};
        if (is_bomber) {
            if (!monster.contact_attack_resolved) {
                const float distance = target_distance(
                    monster.position, player_.position);
                if (distance <= 1.60F) {
                    apply_player_damage(definition->contact_damage,
                                        monster.position, definition->feedback);
                }
                monster.contact_attack_resolved = true;
                static_cast<void>(destroy_monster(MonsterHandle{
                    static_cast<std::uint16_t>(slot), monster.generation}));
            }
            return;
        }
        if (is_charger || is_dasher) {
            monster.velocity = monster.attack_vector;
            monster.position.x += monster.attack_vector.x;
            monster.position.y += monster.attack_vector.y;
            monster.position.z += monster.attack_vector.z;
            clamp_position(monster.position);
        }
        if (!monster.contact_attack_resolved) {
            if (is_shooter) {
                const float dx = monster.attack_target_position.x
                               - monster.position.x;
                const float dy = monster.attack_target_position.y
                               - monster.position.y;
                const float magnitude = std::sqrt(dx * dx + dy * dy);
                const float speed = definition->projectile_speed > 0.0F
                    ? definition->projectile_speed : 0.14F;
                Vec3 velocity{};
                if (magnitude > 0.0001F) {
                    velocity.x = dx / magnitude * speed;
                    velocity.y = dy / magnitude * speed;
                } else {
                    velocity.x = monster.facing == Facing::right
                        ? speed : -speed;
                }
                static_cast<void>(spawn_projectile(
                    MonsterHandle{
                        static_cast<std::uint16_t>(slot), monster.generation},
                    monster.position, velocity, kProjectileLifetime,
                    definition->contact_damage, kProjectileRadius));
            } else if (is_support) {
                std::size_t target_index = monsters_.slots_.size();
                for (std::size_t index = 0; index < monsters_.slots_.size();
                     ++index) {
                    if (index == slot) {
                        continue;
                    }
                    MonsterRuntime& candidate = monsters_.slots_[index];
                    const MonsterDefinition* candidate_definition =
                        monster_definition(candidate.id);
                    if (!candidate.active || candidate.hp <= 0
                        || candidate.shield != 0
                        || candidate_definition == nullptr
                        || has_tag(*candidate_definition, MonsterTag::support)) {
                        continue;
                    }
                    target_index = index;
                    break;
                }
                if (target_index < monsters_.slots_.size()) {
                    MonsterRuntime& target = monsters_.slots_[target_index];
                    target.shield = std::min(target.max_shield,
                                             std::max(target.shield,
                                                      target.max_shield));
                    target.shield_ticks = std::min(
                        target.max_shield_ticks,
                        std::max(target.shield_ticks, target.max_shield_ticks));
                } else {
                    // No legal ally: leave the support in a bounded fallback
                    // reposition/cooldown rather than targeting the player.
                    const float distance = target_distance(
                        monster.position, player_.position);
                    if (distance < definition->preferred_range
                        && distance > 0.0001F) {
                        monster.position.x += (monster.position.x
                            - player_.position.x) / distance
                            * definition->move_speed;
                        monster.position.y += (monster.position.y
                            - player_.position.y) / distance
                            * definition->move_speed;
                        clamp_position(monster.position);
                    }
                    monster.ai_phase = MonsterAiPhase::cooldown;
                    monster.ai_ticks = kSupportFallbackCooldown;
                }
            } else if (!is_hazard) {
                resolve_monster_contact_attack(slot);
            }
            monster.contact_attack_resolved = true;
        }
        if (monster.ai_ticks != 0) {
            --monster.ai_ticks;
        }
        if (monster.ai_ticks == 0) {
            monster.ai_phase = MonsterAiPhase::recovery;
            monster.ai_ticks = definition->recovery_ticks;
        }
        return;
    case MonsterAiPhase::recovery:
        monster.velocity = Vec3{};
        if (monster.ai_ticks != 0) {
            --monster.ai_ticks;
        }
        if (monster.ai_ticks == 0) {
            monster.ai_phase = MonsterAiPhase::cooldown;
            monster.ai_ticks = definition->cooldown_ticks;
        }
        return;
    case MonsterAiPhase::cooldown:
        face_player(monster, player_.position);
        monster.velocity = Vec3{};
        if (monster.ai_ticks != 0) {
            --monster.ai_ticks;
        }
        if (monster.ai_ticks == 0) {
            monster.ai_phase = MonsterAiPhase::move;
        }
        return;
    case MonsterAiPhase::defeated:
        monster.velocity = Vec3{};
        return;
    }
}

}  // namespace arpg::combat
