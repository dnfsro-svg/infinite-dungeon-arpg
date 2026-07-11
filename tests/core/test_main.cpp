#include "test_framework.hpp"

#include <cstdio>

arpg::test::TestSuite fixed_step_suite() noexcept;
arpg::test::TestSuite deterministic_rng_suite() noexcept;
arpg::test::TestSuite fixed_pool_suite() noexcept;
arpg::test::TestSuite bounded_queue_suite() noexcept;

namespace {

constexpr int kExpectedCaseCount = 22;

}  // namespace

int main() {
    const arpg::test::TestSuite suites[] = {
        fixed_step_suite(),
        deterministic_rng_suite(),
        fixed_pool_suite(),
        bounded_queue_suite(),
    };

    int failures = 0;
    int checks = 0;
    for (const auto& suite : suites) {
        for (std::size_t index = 0; index < suite.count; ++index) {
            ++checks;
            const auto failure = suite.cases[index].function();
            if (failure.expression != nullptr) {
                ++failures;
                std::fprintf(
                    stderr,
                    "[FAIL] %s.%s: %s (%s:%d)\n",
                    suite.name,
                    suite.cases[index].name,
                    failure.expression,
                    failure.file,
                    failure.line);
            }
        }
    }

    if (checks != kExpectedCaseCount) {
        ++failures;
        std::fprintf(
            stderr,
            "[FAIL] stage 0 case count: actual %d != expected %d\n",
            checks,
            kExpectedCaseCount);
    }

    std::printf("%d cases, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
