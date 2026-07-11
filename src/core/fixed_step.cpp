#include "core/fixed_step.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace arpg::core {
namespace {

constexpr double kToleranceFactor =
    8.0 * std::numeric_limits<double>::epsilon();

[[nodiscard]] double saturating_add(
    double current,
    double addition) noexcept {
    const double maximum = std::numeric_limits<double>::max();
    if (!std::isfinite(addition) || current > maximum - addition) {
        return maximum;
    }
    return current + addition;
}

[[nodiscard]] double snap_near_integer(double value) noexcept {
    const double nearest = std::nearbyint(value);
    const double tolerance =
        kToleranceFactor * std::max(1.0, std::fabs(value));
    return std::fabs(value - nearest) <= tolerance
        ? nearest
        : value;
}

}  // namespace

FixedStepFrame FixedStepRunner::advance(double frame_seconds) noexcept {
    if (!std::isfinite(frame_seconds) || frame_seconds < 0.0) {
        ++invalid_input_count_;
        return {
            0,
            total_ticks_,
            std::clamp(
                accumulator_seconds_ / kStepSeconds,
                0.0,
                std::nextafter(1.0, 0.0)),
            dropped_seconds_,
            invalid_input_count_,
        };
    }

    accumulator_seconds_ += frame_seconds;
    std::uint32_t steps = 0;

    while (steps < kMaxStepsPerFrame) {
        const double tolerance =
            kToleranceFactor *
            std::max(kStepSeconds, std::fabs(accumulator_seconds_));
        if (accumulator_seconds_ + tolerance < kStepSeconds) {
            break;
        }

        accumulator_seconds_ -= kStepSeconds;
        if (accumulator_seconds_ < 0.0 &&
            std::fabs(accumulator_seconds_) <= tolerance) {
            accumulator_seconds_ = 0.0;
        }
        ++steps;
        ++total_ticks_;
    }

    const double drop_tolerance =
        kToleranceFactor *
        std::max(kStepSeconds, std::fabs(accumulator_seconds_));
    if (steps == kMaxStepsPerFrame &&
        accumulator_seconds_ + drop_tolerance >= kStepSeconds) {
        const double remaining_steps =
            accumulator_seconds_ / kStepSeconds;
        double retained_seconds = 0.0;
        double dropped_now = 0.0;

        if (std::isfinite(remaining_steps)) {
            const double normalized =
                snap_near_integer(remaining_steps);
            const double whole_steps = std::floor(normalized);
            retained_seconds =
                (normalized - whole_steps) * kStepSeconds;
            dropped_now = whole_steps * kStepSeconds;
        } else if (std::isfinite(accumulator_seconds_)) {
            retained_seconds =
                std::fmod(accumulator_seconds_, kStepSeconds);
            dropped_now = accumulator_seconds_ - retained_seconds;
        } else {
            retained_seconds = 0.0;
            dropped_now = std::numeric_limits<double>::max();
        }

        const double remainder_tolerance =
            kToleranceFactor * kStepSeconds;
        if (retained_seconds >= kStepSeconds - remainder_tolerance) {
            retained_seconds = 0.0;
            dropped_now =
                saturating_add(dropped_now, kStepSeconds);
        }

        accumulator_seconds_ =
            std::clamp(
                retained_seconds,
                0.0,
                std::nextafter(kStepSeconds, 0.0));
        dropped_seconds_ =
            saturating_add(dropped_seconds_, dropped_now);
    }

    return {
        steps,
        total_ticks_,
        std::clamp(
            accumulator_seconds_ / kStepSeconds,
            0.0,
            std::nextafter(1.0, 0.0)),
        dropped_seconds_,
        invalid_input_count_,
    };
}

}  // namespace arpg::core
