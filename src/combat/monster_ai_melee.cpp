#include "combat/combat_world.hpp"

#include "combat/monster_ai_common.hpp"
#include "combat/room_bounds.hpp"

#include <algorithm>

namespace arpg::combat {
namespace {

void clamp_position(Vec3& position) noexcept {
    position.x = std::clamp(position.x, room_bounds::min_x, room_bounds::max_x);
    position.y = std::clamp(position.y, room_bounds::min_y, room_bounds::max_y);
}

std::uint16_t frenzy_ticks(
    std::uint16_t base, const MonsterRuntime& monster) noexcept {
    return scaled_monster_ticks(base, monster.affix_profile.attack_timing_bp);
}

std::uint16_t cooldown_ticks(
    std::uint16_t base, const MonsterRuntime& monster) noexcept {
    return scaled_monster_ticks(frenzy_ticks(base, monster),
                                monster.affix_profile.cooldown_bp);
}

}  // namespace

void CombatWorld::simulate_melee_ai(
    std::size_t slot,
    MonsterRuntime& monster,
    const MonsterDefinition& definition) noexcept {
    switch (monster.ai_phase) {
    case MonsterAiPhase::move: {
        face_toward(monster, player_.position);
        const float distance = target_distance(monster.position, player_.position);
        if (distance > definition.preferred_range && distance > 0.0001F) {
            const float speed = monster_move_step(
                definition.move_speed, monster.affix_profile);
            monster.velocity.x = (player_.position.x - monster.position.x)
                / distance * speed;
            monster.velocity.y = (player_.position.y - monster.position.y)
                / distance * speed;
            monster.position.x += monster.velocity.x;
            monster.position.y += monster.velocity.y;
            clamp_position(monster.position);
            return;
        }
        monster.velocity = Vec3{};
        monster.ai_phase = MonsterAiPhase::telegraph;
        monster.ai_ticks = frenzy_ticks(definition.telegraph_ticks, monster);
        monster.attack_target_position = player_.position;
        const float active_ticks = static_cast<float>(
            std::max<std::uint16_t>(1U, definition.active_ticks));
        monster.attack_vector = Vec3{
            (monster.attack_target_position.x - monster.position.x) / active_ticks,
            (monster.attack_target_position.y - monster.position.y) / active_ticks,
            (monster.attack_target_position.z - monster.position.z) / active_ticks,
        };
        return;
    }
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
            resolve_monster_contact_attack(slot);
            monster.contact_attack_resolved = true;
        }
        if (tick_down(monster.ai_ticks)) {
            monster.ai_phase = MonsterAiPhase::recovery;
            monster.ai_ticks = frenzy_ticks(definition.recovery_ticks, monster);
        }
        return;
    case MonsterAiPhase::recovery:
        monster.velocity = Vec3{};
        if (tick_down(monster.ai_ticks)) {
            monster.ai_phase = MonsterAiPhase::cooldown;
            monster.ai_ticks = cooldown_ticks(definition.cooldown_ticks, monster);
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
