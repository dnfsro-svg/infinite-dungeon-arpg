#include "test_framework.hpp"

#include <cstdio>

arpg::test::TestSuite attack_catalog_suite() noexcept;
arpg::test::TestSuite attack_state_suite() noexcept;
arpg::test::TestSuite dummy_reaction_suite() noexcept;
arpg::test::TestSuite input_buffer_suite() noexcept;
arpg::test::TestSuite movement_jump_suite() noexcept;
arpg::test::TestSuite hit_resolution_suite() noexcept;

namespace {

constexpr int kExpectedCaseCount = 25;

}  // namespace

int main() {
    const arpg::test::TestSuite suites[] = {
        attack_catalog_suite(),
        attack_state_suite(),
        dummy_reaction_suite(),
        input_buffer_suite(),
        movement_jump_suite(),
        hit_resolution_suite(),
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
            "[FAIL] stage 1 case count: actual %d != expected %d\n",
            checks,
            kExpectedCaseCount);
    }

    std::printf("%d cases, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
