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
        return;
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

    if (monster.ai_phase == MonsterAiPhase::idle) {
        monster.ai_phase = MonsterAiPhase::move;
        monster.ai_ticks = 0;
    }

    switch (monster.ai_phase) {
    case MonsterAiPhase::move: {
        face_player(monster, player_.position);
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
        return;
    }
    case MonsterAiPhase::telegraph:
        face_player(monster, player_.position);
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
        if (!monster.contact_attack_resolved) {
            resolve_monster_contact_attack(slot);
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
