#include "test_framework.hpp"

#include "core/fixed_step.hpp"

#include <cmath>
#include <limits>

namespace {

using arpg::core::FixedStepRunner;

arpg::test::Failure one_full_step() noexcept {
    FixedStepRunner runner;
    const auto frame = runner.advance(FixedStepRunner::kStepSeconds);
    ARPG_REQUIRE(frame.steps == 1);
    ARPG_REQUIRE(frame.total_ticks == 1);
    ARPG_REQUIRE(arpg::test::near(frame.interpolation_alpha, 0.0));
    return {};
}

arpg::test::Failure two_half_steps() noexcept {
    FixedStepRunner runner;
    const double half = FixedStepRunner::kStepSeconds * 0.5;
    const auto first = runner.advance(half);
    ARPG_REQUIRE(first.steps == 0);
    ARPG_REQUIRE(arpg::test::near(first.interpolation_alpha, 0.5));

    const auto second = runner.advance(half);
    ARPG_REQUIRE(second.steps == 1);
    ARPG_REQUIRE(second.total_ticks == 1);
    ARPG_REQUIRE(arpg::test::near(second.interpolation_alpha, 0.0));
    return {};
}

arpg::test::Failure frame_partitioning_is_stable() noexcept {
    FixedStepRunner sixty_fps;
    FixedStepRunner thirty_fps;
    FixedStepRunner mixed;

    for (int index = 0; index < 60; ++index) {
        static_cast<void>(sixty_fps.advance(FixedStepRunner::kStepSeconds));
    }
    for (int index = 0; index < 30; ++index) {
        static_cast<void>(
            thirty_fps.advance(FixedStepRunner::kStepSeconds * 2.0));
    }
    for (int index = 0; index < 6; ++index) {
        static_cast<void>(mixed.advance(FixedStepRunner::kStepSeconds));
        static_cast<void>(
            mixed.advance(FixedStepRunner::kStepSeconds * 2.0));
        static_cast<void>(
            mixed.advance(FixedStepRunner::kStepSeconds * 3.0));
        static_cast<void>(
            mixed.advance(FixedStepRunner::kStepSeconds * 4.0));
    }

    const auto sixty = sixty_fps.advance(0.0);
    const auto thirty = thirty_fps.advance(0.0);
    const auto varied = mixed.advance(0.0);
    ARPG_REQUIRE(sixty.total_ticks == 60);
    ARPG_REQUIRE(thirty.total_ticks == 60);
    ARPG_REQUIRE(varied.total_ticks == 60);
    ARPG_REQUIRE(arpg::test::near(sixty.dropped_seconds, 0.0));
    ARPG_REQUIRE(arpg::test::near(thirty.dropped_seconds, 0.0));
    ARPG_REQUIRE(arpg::test::near(varied.dropped_seconds, 0.0));
    return {};
}

arpg::test::Failure cap_and_fraction_are_exact() noexcept {
    FixedStepRunner exact_cap;
    const auto eight = exact_cap.advance(
        FixedStepRunner::kStepSeconds * 8.0);
    ARPG_REQUIRE(eight.steps == 8);
    ARPG_REQUIRE(arpg::test::near(eight.dropped_seconds, 0.0));
    ARPG_REQUIRE(arpg::test::near(eight.interpolation_alpha, 0.0));

    FixedStepRunner over_cap;
    const auto ten_and_half = over_cap.advance(
        FixedStepRunner::kStepSeconds * 10.5);
    ARPG_REQUIRE(ten_and_half.steps == 8);
    ARPG_REQUIRE(
        arpg::test::near(
            ten_and_half.dropped_seconds,
            FixedStepRunner::kStepSeconds * 2.0));
    ARPG_REQUIRE(
        arpg::test::near(ten_and_half.interpolation_alpha, 0.5));

    FixedStepRunner nine_steps;
    const auto nine = nine_steps.advance(
        FixedStepRunner::kStepSeconds * 9.0);
    ARPG_REQUIRE(nine.steps == 8);
    ARPG_REQUIRE(
        arpg::test::near(
            nine.dropped_seconds,
            FixedStepRunner::kStepSeconds));
    ARPG_REQUIRE(arpg::test::near(nine.interpolation_alpha, 0.0));
    return {};
}

arpg::test::Failure one_second_drops_fifty_two_steps() noexcept {
    FixedStepRunner runner;
    const auto frame = runner.advance(1.0);
    ARPG_REQUIRE(frame.steps == 8);
    ARPG_REQUIRE(frame.total_ticks == 8);
    ARPG_REQUIRE(
        arpg::test::near(
            frame.dropped_seconds,
            FixedStepRunner::kStepSeconds * 52.0));
    ARPG_REQUIRE(arpg::test::near(frame.interpolation_alpha, 0.0));
    return {};
}

arpg::test::Failure invalid_input_preserves_accumulator() noexcept {
    FixedStepRunner runner;
    const auto half =
        runner.advance(FixedStepRunner::kStepSeconds * 0.5);
    ARPG_REQUIRE(arpg::test::near(half.interpolation_alpha, 0.5));

    static_cast<void>(runner.advance(-1.0));
    static_cast<void>(
        runner.advance(std::numeric_limits<double>::infinity()));
    static_cast<void>(
        runner.advance(-std::numeric_limits<double>::infinity()));
    const auto invalid =
        runner.advance(std::numeric_limits<double>::quiet_NaN());

    ARPG_REQUIRE(invalid.steps == 0);
    ARPG_REQUIRE(invalid.total_ticks == 0);
    ARPG_REQUIRE(invalid.invalid_input_count == 4);
    ARPG_REQUIRE(
        arpg::test::near(invalid.interpolation_alpha, 0.5));

    const auto negative_zero = runner.advance(-0.0);
    ARPG_REQUIRE(negative_zero.invalid_input_count == 4);
    return {};
}

arpg::test::Failure huge_finite_input_is_bounded() noexcept {
    FixedStepRunner runner;
    const double maximum = std::numeric_limits<double>::max();

    const auto first = runner.advance(maximum);
    ARPG_REQUIRE(first.steps == FixedStepRunner::kMaxStepsPerFrame);
    ARPG_REQUIRE(first.invalid_input_count == 0);
    ARPG_REQUIRE(first.dropped_seconds == maximum);
    ARPG_REQUIRE(std::isfinite(first.dropped_seconds));
    ARPG_REQUIRE(first.interpolation_alpha >= 0.0);
    ARPG_REQUIRE(first.interpolation_alpha < 1.0);

    const auto second = runner.advance(maximum);
    ARPG_REQUIRE(second.steps == FixedStepRunner::kMaxStepsPerFrame);
    ARPG_REQUIRE(second.invalid_input_count == 0);
    ARPG_REQUIRE(second.dropped_seconds == maximum);
    ARPG_REQUIRE(std::isfinite(second.dropped_seconds));
    ARPG_REQUIRE(second.interpolation_alpha >= 0.0);
    ARPG_REQUIRE(second.interpolation_alpha < 1.0);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"one full step", &one_full_step},
    {"two half steps", &two_half_steps},
    {"frame partitioning", &frame_partitioning_is_stable},
    {"cap and fraction", &cap_and_fraction_are_exact},
    {"one second regression", &one_second_drops_fifty_two_steps},
    {"invalid input", &invalid_input_preserves_accumulator},
    {"huge finite input", &huge_finite_input_is_bounded},
};

}  // namespace

arpg::test::TestSuite fixed_step_suite() noexcept {
    return {
        "fixed_step",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
