#include "combat/combat_world.hpp"

#include <algorithm>

namespace arpg::combat {

modifiers::EffectSet* CombatWorld::find_effects(
    std::size_t monster_slot) noexcept {
    const auto& monster = monsters_.slots_[monster_slot];
    for (auto& owner : effect_owners_) {
        if (owner.occupied && owner.monster_slot == monster_slot
            && owner.generation == monster.generation) {
            return &owner.effects;
        }
    }
    return nullptr;
}

modifiers::EffectSet* CombatWorld::ensure_effects(
    std::size_t monster_slot) noexcept {
    if (auto* existing = find_effects(monster_slot)) return existing;
    for (auto& owner : effect_owners_) {
        if (owner.occupied) continue;
        owner.monster_slot = monster_slot;
        owner.generation = monsters_.slots_[monster_slot].generation;
        owner.effects.clear();
        owner.occupied = true;
        return &owner.effects;
    }
    return nullptr;
}

void CombatWorld::apply_effect_commands(
    MonsterRuntime& monster,
    modifiers::EffectSet& effects) noexcept {
    modifiers::EffectCommand command{};
    while (effects.pop_command(command)) {
        if (command.kind == modifiers::EffectCommandKind::set_shield) {
            monster.shield = std::min(
                monster.max_shield, static_cast<int>(command.value));
        } else if (command.kind == modifiers::EffectCommandKind::clear_shield) {
            monster.shield = 0;
        }
    }
    monster.shield_ticks = static_cast<std::uint16_t>(
        std::max(0, effects.remaining_ticks(1U)));
}

modifiers::EffectDefinition CombatWorld::water_barrier_effect(
    const MonsterRuntime& target) const noexcept {
    modifiers::EffectDefinition barrier{};
    barrier.id = 1U;
    barrier.duration_ticks = target.max_shield_ticks;
    barrier.refresh_rule = modifiers::RefreshRule::refresh_duration;
    barrier.max_stacks = 1;
    barrier.on_apply = {
        modifiers::EffectCommandKind::set_shield, target.max_shield};
    barrier.on_refresh = barrier.on_apply;
    barrier.on_expire = {modifiers::EffectCommandKind::clear_shield, 0};
    return barrier;
}

}  // namespace arpg::combat
