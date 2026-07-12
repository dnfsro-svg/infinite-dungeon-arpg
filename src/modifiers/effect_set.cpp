#include "modifiers/effect_set.hpp"

#include <algorithm>

namespace arpg::modifiers {

EffectSet::ActiveEffect* EffectSet::find(EffectId id) noexcept {
    for (auto& effect : effects_) {
        if (effect.occupied && effect.definition.id == id) return &effect;
    }
    return nullptr;
}

const EffectSet::ActiveEffect* EffectSet::find(EffectId id) const noexcept {
    for (const auto& effect : effects_) {
        if (effect.occupied && effect.definition.id == id) return &effect;
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
        || definition.max_stacks == 0
        || definition.modifier_count > definition.modifiers.size()) {
        return ApplyResult::rejected;
    }
    if (ActiveEffect* active = find(definition.id)) {
        switch (definition.refresh_rule) {
        case RefreshRule::reject:
            return ApplyResult::rejected;
        case RefreshRule::refresh_duration:
            active->definition = definition;
            active->remaining_ticks = definition.duration_ticks;
            emit(definition.on_refresh, definition.id);
            return ApplyResult::refreshed;
        case RefreshRule::add_stack:
            if (active->stacks >= definition.max_stacks) {
                return ApplyResult::rejected;
            }
            active->definition = definition;
            active->remaining_ticks = definition.duration_ticks;
            ++active->stacks;
            emit(definition.on_refresh, definition.id);
            return ApplyResult::stacked;
        case RefreshRule::replace_weaker:
            if (definition.strength <= active->definition.strength) {
                return ApplyResult::rejected;
            }
            active->definition = definition;
            active->remaining_ticks = definition.duration_ticks;
            active->stacks = 1;
            emit(definition.on_refresh, definition.id);
            return ApplyResult::refreshed;
        }
    }
    for (auto& slot : effects_) {
        if (slot.occupied) continue;
        slot = {definition, definition.duration_ticks, 1, true};
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
        emit(effect.definition.on_expire, effect.definition.id);
        effect = {};
    }
}

bool EffectSet::remove(EffectId id) noexcept {
    ActiveEffect* effect = find(id);
    if (effect == nullptr) return false;
    emit(effect->definition.on_expire, id);
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

bool EffectSet::pop_command(EffectCommand& command) noexcept {
    if (command_count_ == 0) return false;
    command = commands_[command_head_];
    command_head_ = (command_head_ + 1U) % command_capacity();
    --command_count_;
    return true;
}

}  // namespace arpg::modifiers
