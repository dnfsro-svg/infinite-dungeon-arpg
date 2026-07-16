#include "combat/combat_world.hpp"

#include "combat/combat_collision.hpp"
#include "combat/monster_ai_common.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/room_bounds.hpp"

#include <algorithm>
#include <cmath>

namespace arpg::combat {
namespace {

constexpr float kTickSeconds = 1.0F / 60.0F;
constexpr float kGravity = 24.0F;
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
    position.x = std::clamp(position.x, room_bounds::min_x, room_bounds::max_x);
    position.y = std::clamp(position.y, room_bounds::min_y, room_bounds::max_y);
}

void integrate_reaction(MonsterRuntime& monster) noexcept {
    monster.position.x += monster.velocity.x * kTickSeconds;
    monster.position.y += monster.velocity.y * kTickSeconds;
    clamp_position(monster.position);
}

std::int32_t blink_damage_bp(const MonsterAffixSet& affixes) noexcept {
    for (std::size_t index = 0; index < affixes.count
         && index < affixes.values.size(); ++index) {
        const MonsterAffixInstance& instance = affixes.values[index];
        if (instance.id != MonsterAffixId::blink_assault) continue;
        const MonsterAffixDefinition* definition =
            monster_affix_definition(instance.id);
        const std::size_t tier = static_cast<std::size_t>(instance.tier);
        if (definition != nullptr && tier < definition->tiers.size()) {
            return definition->tiers[tier].primary_bp;
        }
    }
    return 10000;
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
        DamagePacket packet = scale_monster_affix_damage(
            definition->contact_damage, monster.affix_profile);
        for (int& amount : packet.amount) {
            if (monster.blink_empowered) {
                amount = static_cast<int>((static_cast<std::int64_t>(amount)
                    * blink_damage_bp(monster.affixes)) / 10000);
            }
        }
        if (apply_monster_direct_hit(slot, packet,
            monster.position,
            definition->feedback)) {
            monster.blink_empowered = false;
        }
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

    if (monster.hp <= 0) {
        defeat_monster(slot, AttackId::none, true);
        return;
    }
    if (monster.reaction == ReactionState::defeated) {
        monster.ai_phase = MonsterAiPhase::defeated;
        monster.velocity = Vec3{};
        return;
    }

    if (modifiers::EffectSet* effects = find_effects(slot)) {
        effects->tick();
        apply_effect_commands(monster, *effects);
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
            if (monster.velocity.z <= 0.0F
                && monster.reaction_ticks != 0) {
                --monster.reaction_ticks;
                monster.velocity.z = 0.0F;
            } else {
                monster.position.z += monster.velocity.z * kTickSeconds;
                monster.velocity.z -= kGravity * kTickSeconds;
            }
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

    const bool is_melee = monster.id == MonsterId::chaos_chaser
                       || monster.id == MonsterId::water_bulwark;
    const bool is_ranged = monster.id == MonsterId::lightning_shooter
                        || monster.id == MonsterId::water_support;
    const bool is_special = monster.id == MonsterId::fire_bomber
                         || monster.id == MonsterId::fire_charger
                         || monster.id == MonsterId::lightning_dasher
                         || monster.id == MonsterId::chaos_hazard;
    if (!is_melee && !is_ranged && !is_special) {
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

    if (is_melee) {
        simulate_melee_ai(slot, monster, *definition);
    } else if (is_ranged) {
        simulate_ranged_ai(slot, monster, *definition);
    } else {
        simulate_special_ai(slot, monster, *definition);
    }
}

}  // namespace arpg::combat
