#pragma once

#include "modifiers/modifier_types.hpp"

namespace arpg::modifiers {

[[nodiscard]] StatEvaluation evaluate_stat(
    FixedValue base,
    StatId stat,
    ModifierSpan modifiers,
    ModifierContext context,
    StatBounds bounds) noexcept;
[[nodiscard]] ConversionResult evaluate_conversions(
    StatValues values,
    ModifierSpan modifiers,
    ModifierContext context) noexcept;

}  // namespace arpg::modifiers
