#include "core/fixed_step.hpp"

#include <algorithm>
#include <cmath>

namespace arpg::core {

FixedStepFrame FixedStepRunner::advance(double frame_seconds) noexcept {
    if (std::isfinite(frame_seconds) && frame_seconds >= 0.0) {
        accumulator_seconds_ += frame_seconds;
    }

    std::uint32_t steps = 0;
    if (accumulator_seconds_ >= kStepSeconds) {
        accumulator_seconds_ -= kStepSeconds;
        ++steps;
        ++total_ticks_;
    }

    const double alpha = std::clamp(
        accumulator_seconds_ / kStepSeconds,
        0.0,
        1.0);
    return {
        steps,
        total_ticks_,
        alpha,
        dropped_seconds_,
        invalid_input_count_,
    };
}

}  // namespace arpg::core
