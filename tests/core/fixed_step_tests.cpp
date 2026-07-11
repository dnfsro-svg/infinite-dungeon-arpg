#include "test_framework.hpp"

#include "core/fixed_step.hpp"

namespace {

arpg::test::Failure one_full_step() noexcept {
    arpg::core::FixedStepRunner runner;
    const auto frame =
        runner.advance(arpg::core::FixedStepRunner::kStepSeconds);

    ARPG_REQUIRE(frame.steps == 1);
    ARPG_REQUIRE(frame.total_ticks == 1);
    ARPG_REQUIRE(arpg::test::near(frame.interpolation_alpha, 0.0));
    return {};
}

arpg::test::Failure two_half_steps() noexcept {
    arpg::core::FixedStepRunner runner;
    const double half =
        arpg::core::FixedStepRunner::kStepSeconds * 0.5;

    const auto first = runner.advance(half);
    ARPG_REQUIRE(first.steps == 0);
    ARPG_REQUIRE(arpg::test::near(first.interpolation_alpha, 0.5));

    const auto second = runner.advance(half);
    ARPG_REQUIRE(second.steps == 1);
    ARPG_REQUIRE(second.total_ticks == 1);
    ARPG_REQUIRE(arpg::test::near(second.interpolation_alpha, 0.0));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"one full step", &one_full_step},
    {"two half steps", &two_half_steps},
};

}  // namespace

arpg::test::TestSuite fixed_step_suite() noexcept {
    return {
        "fixed_step",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
