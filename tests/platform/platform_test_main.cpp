#include "test_framework.hpp"

#include <cstdio>

arpg::test::TestSuite combat_view_math_suite() noexcept;
arpg::test::TestSuite combat_feedback_suite() noexcept;
arpg::test::TestSuite combat_key_bindings_suite() noexcept;

int main() {
    constexpr int kExpectedCaseCount = 7;
    const arpg::test::TestSuite suites[] = {
        combat_view_math_suite(),
        combat_feedback_suite(),
        combat_key_bindings_suite(),
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
            "[FAIL] platform case count: actual %d != expected %d\n",
            checks,
            kExpectedCaseCount);
    }

    std::printf("%d cases, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
