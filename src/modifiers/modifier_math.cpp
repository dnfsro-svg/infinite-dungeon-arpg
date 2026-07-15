#include "modifiers/modifier_math.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace arpg::modifiers {

namespace {

inline constexpr std::size_t kPlayerModifierCapacity = 256U;

bool checked_add(
    FixedValue left,
    FixedValue right,
    FixedValue& result) noexcept {
    const auto maximum = (std::numeric_limits<FixedValue>::max)();
    const auto minimum = (std::numeric_limits<FixedValue>::min)();
    if (right > 0 && left > maximum - right) {
        result = maximum;
        return false;
    }
    if (right < 0 && left < minimum - right) {
        result = minimum;
        return false;
    }
    result = left + right;
    return true;
}

bool checked_subtract(
    FixedValue left,
    FixedValue right,
    FixedValue& result) noexcept {
    const auto maximum = (std::numeric_limits<FixedValue>::max)();
    const auto minimum = (std::numeric_limits<FixedValue>::min)();
    if (right > 0 && left < minimum + right) {
        result = minimum;
        return false;
    }
    if (right < 0 && left > maximum + right) {
        result = maximum;
        return false;
    }
    result = left - right;
    return true;
}

FixedValue saturating_multiply(FixedValue left, FixedValue right) noexcept {
    const auto maximum = (std::numeric_limits<FixedValue>::max)();
    const auto minimum = (std::numeric_limits<FixedValue>::min)();
    if (left == 0 || right == 0) return 0;
    if ((left == -1 && right == minimum)
        || (right == -1 && left == minimum)) {
        return maximum;
    }
    if (left > 0) {
        if (right > 0 && left > maximum / right) return maximum;
        if (right < 0 && right < minimum / left) return minimum;
    } else {
        if (right > 0 && left < minimum / right) return minimum;
        if (right < 0 && left < maximum / right) return maximum;
    }
    return left * right;
}

FixedValue saturating_mul_div(
    FixedValue value,
    FixedValue factor,
    bool& valid) noexcept {
    const FixedValue whole = saturating_multiply(value / kFixedOne, factor);
    const FixedValue remainder = saturating_multiply(value % kFixedOne, factor)
        / kFixedOne;
    FixedValue result{};
    valid = checked_add(whole, remainder, result) && valid;
    return result;
}

bool matches(const Modifier& modifier,
    StatId stat,
    ModifierContext context) noexcept {
    return modifier.stat == stat
        && (context.tags & modifier.required_tags) == modifier.required_tags
        && (context.tags & modifier.forbidden_tags) == 0U
        && (context.conditions & modifier.required_conditions)
            == modifier.required_conditions;
}

bool precedes(const Modifier* left, const Modifier* right) noexcept {
    return left->priority < right->priority
        || (left->priority == right->priority && left->id < right->id);
}

template <std::size_t N>
bool has_duplicate_id(
    const std::array<const Modifier*, N>& values,
    std::size_t count,
    ModifierId id) noexcept {
    for (std::size_t index = 0; index < count; ++index) {
        if (values[index]->id == id) return true;
    }
    return false;
}

template <std::size_t N>
void stable_sort_by_priority(
    std::array<const Modifier*, N>& values,
    std::size_t count) noexcept {
    for (std::size_t index = 1U; index < count; ++index) {
        const Modifier* value = values[index];
        std::size_t insertion = index;
        while (insertion != 0U
            && precedes(value, values[insertion - 1U])) {
            values[insertion] = values[insertion - 1U];
            --insertion;
        }
        values[insertion] = value;
    }
}

}  // namespace

StatEvaluation evaluate_stat(
    FixedValue base,
    StatId stat,
    ModifierSpan modifiers,
    ModifierContext context,
    StatBounds bounds) noexcept {
    if (bounds.minimum > bounds.maximum
        || modifiers.size > kPlayerModifierCapacity
        || (modifiers.size != 0U && modifiers.data == nullptr)) {
        return {base, false, false};
    }

    std::array<const Modifier*, kPlayerModifierCapacity> active{};
    std::size_t count = 0U;
    for (std::size_t index = 0; index < modifiers.size; ++index) {
        const Modifier& candidate = modifiers.data[index];
        if (!matches(candidate, stat, context)) continue;
        if (has_duplicate_id(active, count, candidate.id)) {
            return {base, false, true};
        }
        active[count++] = &candidate;
    }
    stable_sort_by_priority(active, count);

    FixedValue result = base;
    for (std::size_t index = 0; index < count; ++index) {
        if (active[index]->operation == ModifierOperation::flat) {
            if (!checked_add(result, active[index]->value, result)) {
                return {result, false, false};
            }
        }
    }
    FixedValue increased = 0;
    for (std::size_t index = 0; index < count; ++index) {
        if (active[index]->operation == ModifierOperation::increased) {
            if (!checked_add(increased, active[index]->value, increased)) {
                return {result, false, false};
            }
        }
    }
    FixedValue increased_factor{};
    if (!checked_add(kFixedOne, increased, increased_factor)) {
        return {result, false, false};
    }
    bool valid = true;
    result = saturating_mul_div(result, increased_factor, valid);
    if (!valid) return {result, false, false};
    for (std::size_t index = 0; index < count; ++index) {
        if (active[index]->operation == ModifierOperation::more) {
            result = saturating_mul_div(result, active[index]->value, valid);
            if (!valid) return {result, false, false};
        }
    }
    return {std::clamp(result, bounds.minimum, bounds.maximum), true, false};
}

ConversionResult evaluate_conversions(
    StatValues values,
    ModifierSpan modifiers,
    ModifierContext context) noexcept {
    ConversionResult result{values, false, 0};
    if (modifiers.size > kPlayerModifierCapacity
        || (modifiers.size != 0U && modifiers.data == nullptr)) {
        return result;
    }
    std::array<const Modifier*, kPlayerModifierCapacity> active{};
    std::size_t count = 0U;
    for (std::size_t index = 0; index < modifiers.size; ++index) {
        const Modifier& candidate = modifiers.data[index];
        if (candidate.operation != ModifierOperation::conversion
            || !matches(candidate, candidate.stat, context)) {
            continue;
        }
        const auto source = static_cast<std::size_t>(candidate.stat);
        const auto target = static_cast<std::size_t>(candidate.conversion_target);
        if (source >= values.size() || target >= values.size()
            || source == target || candidate.value < 0) {
            return result;
        }
        if (has_duplicate_id(active, count, candidate.id)) return result;
        active[count++] = &candidate;
    }
    stable_sort_by_priority(active, count);

    const StatValues original = values;
    std::array<FixedValue,
        static_cast<std::size_t>(StatId::count)> remaining{};
    remaining.fill(kFixedOne);
    for (std::size_t index = 0; index < count; ++index) {
        const Modifier& conversion = *active[index];
        const auto source = static_cast<std::size_t>(conversion.stat);
        const auto target = static_cast<std::size_t>(conversion.conversion_target);
        const FixedValue applied = std::min(
            conversion.value, remaining[source]);
        if (!checked_add(result.truncated_basis_points,
                conversion.value - applied,
                result.truncated_basis_points)) {
            return result;
        }
        remaining[source] -= applied;
        bool valid = true;
        const FixedValue amount = saturating_mul_div(
            original[source], applied, valid);
        if (!valid
            || amount == (std::numeric_limits<FixedValue>::min)()
            || !checked_subtract(result.values[source], amount,
                result.values[source])
            || !checked_add(result.values[target], amount,
                result.values[target])) {
            return result;
        }
    }
    result.valid = true;
    return result;
}

}  // namespace arpg::modifiers
