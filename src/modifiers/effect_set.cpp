#include "modifiers/effect_set.hpp"

#include <algorithm>

namespace arpg::modifiers {

namespace {

template <typename Effects>
auto find_effect(Effects& effects, EffectId id) noexcept
    -> decltype(&effects[0]) {
    for (auto& effect : effects) {
        if (effect.occupied && effect.id == id) return &effect;
    }
    return nullptr;
}

}  // namespace

EffectSet::ActiveEffect* EffectSet::find(EffectId id) noexcept {
    return find_effect(effects_, id);
}

const EffectSet::ActiveEffect* EffectSet::find(EffectId id) const noexcept {
    return find_effect(effects_, id);
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

bool EffectSet::same_state(const EffectSet& other) const noexcept {
    const auto same_modifier = [](const Modifier& left,
                                  const Modifier& right) noexcept {
        return left.id == right.id && left.stat == right.stat
            && left.operation == right.operation && left.value == right.value
            && left.required_tags == right.required_tags
            && left.forbidden_tags == right.forbidden_tags
            && left.required_conditions == right.required_conditions
            && left.priority == right.priority
            && left.conversion_target == right.conversion_target;
    };
    const auto same_template = [](const EffectCommandTemplate& left,
                                  const EffectCommandTemplate& right) noexcept {
        return left.kind == right.kind && left.value == right.value;
    };
    const auto same_command = [](const EffectCommand& left,
                                 const EffectCommand& right) noexcept {
        return left.kind == right.kind && left.value == right.value
            && left.effect_id == right.effect_id;
    };

    for (std::size_t index = 0U; index < effects_.size(); ++index) {
        const ActiveEffect& left = effects_[index];
        const ActiveEffect& right = other.effects_[index];
        if (left.id != right.id
            || left.remaining_ticks != right.remaining_ticks
            || left.stacks != right.stacks
            || left.max_stacks != right.max_stacks
            || left.refresh_rule != right.refresh_rule
            || left.strength != right.strength
            || !same_modifier(left.modifier, right.modifier)
            || left.has_modifier != right.has_modifier
            || !same_template(left.on_expire, right.on_expire)
            || left.occupied != right.occupied) {
            return false;
        }
    }
    for (std::size_t index = 0U; index < commands_.size(); ++index) {
        if (!same_command(commands_[index], other.commands_[index])) {
            return false;
        }
    }
    return command_head_ == other.command_head_
        && command_count_ == other.command_count_
        && diagnostics_.effect_overflows
            == other.diagnostics_.effect_overflows
        && diagnostics_.command_overflows
            == other.diagnostics_.command_overflows;
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
