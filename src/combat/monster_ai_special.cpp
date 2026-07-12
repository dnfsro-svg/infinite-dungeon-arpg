#include "combat/combat_world.hpp"

#include "combat/monster_ai_common.hpp"
#include "combat/room_bounds.hpp"

#include <algorithm>

namespace arpg::combat {
namespace {

constexpr std::uint16_t kHazardDamageIntervalTicks = 30;

void clamp_position(Vec3& position) noexcept {
    position.x = std::clamp(position.x, room_bounds::min_x, room_bounds::max_x);
    position.y = std::clamp(position.y, room_bounds::min_y, room_bounds::max_y);
}

}  // namespace

void CombatWorld::simulate_special_ai(
    std::size_t slot,
    MonsterRuntime& monster,
    const MonsterDefinition& definition) noexcept {
    const bool is_bomber = monster.id == MonsterId::fire_bomber;
    const bool is_charger = monster.id == MonsterId::fire_charger;
    const bool is_dasher = monster.id == MonsterId::lightning_dasher;
    const bool is_hazard = monster.id == MonsterId::chaos_hazard;
    switch (monster.ai_phase) {
    case MonsterAiPhase::move: {
        face_toward(monster, player_.position);
        const float distance = target_distance(monster.position, player_.position);
        if (distance > definition.preferred_range && distance > 0.0001F) {
            monster.velocity.x = (player_.position.x - monster.position.x)
                / distance * definition.move_speed;
            monster.velocity.y = (player_.position.y - monster.position.y)
                / distance * definition.move_speed;
            monster.position.x += monster.velocity.x;
            monster.position.y += monster.velocity.y;
            clamp_position(monster.position);
            return;
        }
        monster.velocity = Vec3{};
        monster.ai_phase = MonsterAiPhase::telegraph;
        monster.ai_ticks = definition.telegraph_ticks;
        monster.attack_target_position = player_.position;
        const float active_ticks = static_cast<float>(
            std::max<std::uint16_t>(1U, definition.active_ticks));
        monster.attack_vector = Vec3{
            (monster.attack_target_position.x - monster.position.x) / active_ticks,
            (monster.attack_target_position.y - monster.position.y) / active_ticks,
            (monster.attack_target_position.z - monster.position.z) / active_ticks,
        };
        if (is_hazard) {
            static_cast<void>(spawn_hazard(
                MonsterHandle{static_cast<std::uint16_t>(slot), monster.generation},
                monster.attack_target_position, 1.25F, definition.telegraph_ticks,
                definition.hazard_ticks, kHazardDamageIntervalTicks,
                definition.contact_damage));
        }
        return;
    }
    case MonsterAiPhase::telegraph:
        if (!is_charger && !is_dasher) face_toward(monster, player_.position);
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
        if (is_bomber) {
            if (!monster.contact_attack_resolved) {
                if (target_distance(monster.position, player_.position) <= 1.60F) {
                    apply_player_damage(definition.contact_damage, monster.position,
                                        definition.feedback);
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
            if (!is_hazard) resolve_monster_contact_attack(slot);
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
