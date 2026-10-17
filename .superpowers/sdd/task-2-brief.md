### Task 2: Complete and Freeze Fixed-Step Semantics

**Worktree:** E:\game\.worktrees\m00-foundation

**Files:**
- Modify: src/core/fixed_step.cpp
- Modify: tests/core/fixed_step_tests.cpp

**Interfaces:**
- Consumes and freezes: arpg::core::FixedStepRunner::advance(double) noexcept.
- Produces: exact 60 Hz partition invariance, 8-step cap, accumulated dropped seconds, invalid-input count, and alpha in [0, 1).
- Host worktree may consume FixedStepFrame after this task; later core tasks must not alter it.

- [ ] **Step 1: Replace the basic tests with complete fixed-step acceptance tests**

Replace tests/core/fixed_step_tests.cpp with:

~~~cpp
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
    const auto frame =
        runner.advance(std::numeric_limits<double>::max());
    ARPG_REQUIRE(frame.steps == FixedStepRunner::kMaxStepsPerFrame);
    ARPG_REQUIRE(frame.invalid_input_count == 0);
    ARPG_REQUIRE(std::isfinite(frame.dropped_seconds));
    ARPG_REQUIRE(frame.interpolation_alpha >= 0.0);
    ARPG_REQUIRE(frame.interpolation_alpha < 1.0);
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
~~~

- [ ] **Step 2: Run the expanded suite and verify the red state**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
~~~

Expected: core.units fails. The minimum implementation executes only one step per call, does not count invalid inputs, and does not record dropped time.

- [ ] **Step 3: Implement bounded fixed-step accumulation**

Replace src/core/fixed_step.cpp with:

~~~cpp
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
~~~

- [ ] **Step 4: Run core-only and full Debug tests**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\scripts\Test.ps1 -Preset windows-msvc-debug
~~~

Expected: seven fixed-step cases pass; both CTest presets report 100% tests passed.

- [ ] **Step 5: Commit the frozen timing contract**

Run:

~~~powershell
git -c safe.directory=E:/game/.worktrees/m00-foundation diff --check
git -c safe.directory=E:/game/.worktrees/m00-foundation add src/core/fixed_step.cpp tests/core/fixed_step_tests.cpp
git -c safe.directory=E:/game/.worktrees/m00-foundation -c user.name=Codex -c user.email=codex@local commit -m "feat: harden fixed timestep runtime"
~~~

Expected: the integration branch is clean and FixedStepRunner/FixedStepFrame are now frozen for parallel work.

- [ ] **Step 6: Create the two task worktrees from the frozen integration commit**

Run from E:\game:

~~~powershell
git -c safe.directory=E:/game worktree add .worktrees/m00-core-runtime -b task/m00-core-runtime milestone/m00-foundation
git -c safe.directory=E:/game worktree add .worktrees/m00-raylib-host -b task/m00-raylib-host milestone/m00-foundation
git -c safe.directory=E:/game/.worktrees/m00-core-runtime -C .worktrees/m00-core-runtime status --short --branch
git -c safe.directory=E:/game/.worktrees/m00-raylib-host -C .worktrees/m00-raylib-host status --short --branch
~~~

Expected: both task worktrees are clean and point to the same commit as milestone/m00-foundation.

---

