#include "modifiers/effect_set.hpp"

#include <algorithm>

namespace arpg::modifiers {

EffectSet::ActiveEffect* EffectSet::find(EffectId id) noexcept {
    for (auto& effect : effects_) {
        if (effect.occupied && effect.id == id) return &effect;
    }
    return nullptr;
}

const EffectSet::ActiveEffect* EffectSet::find(EffectId id) const noexcept {
    for (const auto& effect : effects_) {
        if (effect.occupied && effect.id == id) return &effect;
    }
    return nullptr;
}

void EffectSet::emit(EffectCommandTemplate command, EffectId id) noexcept {
    if (command.kind == EffectCommandKind::none) return;
    if (command_count_ == command_capacity()) {
        ++diagnostics_.command_overflows;
        return;
    }
    const std::size_t tail = (command_head_ + command_count_)
        % command_capacity();
    commands_[tail] = {command.kind, command.value, id};
    ++command_count_;
}

ApplyResult EffectSet::apply(const EffectDefinition& definition) noexcept {
    if (definition.id == 0 || definition.duration_ticks <= 0
        || definition.max_stacks == 0) {
        return ApplyResult::rejected;
    }
    if (ActiveEffect* active = find(definition.id)) {
        switch (definition.refresh_rule) {
        case RefreshRule::reject:
            return ApplyResult::rejected;
        case RefreshRule::refresh_duration:
            active->remaining_ticks = definition.duration_ticks;
            active->strength = definition.strength;
            active->modifier = definition.modifier;
            active->has_modifier = definition.has_modifier;
            active->on_expire = definition.on_expire;
            emit(definition.on_refresh, definition.id);
            return ApplyResult::refreshed;
        case RefreshRule::add_stack:
            if (active->stacks >= active->max_stacks) {
                return ApplyResult::rejected;
            }
            active->remaining_ticks = definition.duration_ticks;
            ++active->stacks;
            emit(definition.on_refresh, definition.id);
            return ApplyResult::stacked;
        case RefreshRule::replace_weaker:
            if (definition.strength <= active->strength) {
                return ApplyResult::rejected;
            }
            active->remaining_ticks = definition.duration_ticks;
            active->stacks = 1;
            active->strength = definition.strength;
            active->modifier = definition.modifier;
            active->has_modifier = definition.has_modifier;
            active->on_expire = definition.on_expire;
            emit(definition.on_refresh, definition.id);
            return ApplyResult::refreshed;
        }
    }
    for (auto& slot : effects_) {
        if (slot.occupied) continue;
        slot.id = definition.id;
        slot.remaining_ticks = definition.duration_ticks;
        slot.stacks = 1;
        slot.max_stacks = definition.max_stacks;
        slot.refresh_rule = definition.refresh_rule;
        slot.strength = definition.strength;
        slot.modifier = definition.modifier;
        slot.has_modifier = definition.has_modifier;
        slot.on_expire = definition.on_expire;
        slot.occupied = true;
        emit(definition.on_apply, definition.id);
        return ApplyResult::applied;
    }
    ++diagnostics_.effect_overflows;
    return ApplyResult::capacity_rejected;
}

void EffectSet::tick() noexcept {
    for (auto& effect : effects_) {
        if (!effect.occupied) continue;
        --effect.remaining_ticks;
        if (effect.remaining_ticks > 0) continue;
        emit(effect.on_expire, effect.id);
        effect = {};
    }
}

bool EffectSet::remove(EffectId id) noexcept {
    ActiveEffect* effect = find(id);
    if (effect == nullptr) return false;
    emit(effect->on_expire, id);
    *effect = {};
    return true;
}

void EffectSet::clear() noexcept {
    for (auto& effect : effects_) effect = {};
    command_head_ = 0;
    command_count_ = 0;
}

std::size_t EffectSet::active_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        effects_.begin(), effects_.end(),
        [](const ActiveEffect& effect) { return effect.occupied; }));
}

int EffectSet::remaining_ticks(EffectId id) const noexcept {
    const ActiveEffect* effect = find(id);
    return effect == nullptr ? 0 : effect->remaining_ticks;
}

std::uint8_t EffectSet::stack_count(EffectId id) const noexcept {
    const ActiveEffect* effect = find(id);
    return effect == nullptr ? 0 : effect->stacks;
}

std::size_t EffectSet::queued_command_count() const noexcept {
    return command_count_;
}

const EffectDiagnostics& EffectSet::diagnostics() const noexcept {
    return diagnostics_;
}

std::size_t EffectSet::copy_modifiers(
    std::array<Modifier, kCapacity>& output) const noexcept {
    std::size_t count = 0;
    for (const auto& effect : effects_) {
        if (!effect.occupied || !effect.has_modifier) continue;
        output[count++] = effect.modifier;
    }
    return count;
}

bool EffectSet::pop_command(EffectCommand& command) noexcept {
    if (command_count_ == 0) return false;
    command = commands_[command_head_];
    command_head_ = (command_head_ + 1U) % command_capacity();
    --command_count_;
    return true;
}

}  // namespace arpg::modifiers
