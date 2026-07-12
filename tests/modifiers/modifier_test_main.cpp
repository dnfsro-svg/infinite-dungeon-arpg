#include "test_framework.hpp"

arpg::test::TestSuite modifier_math_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {modifier_math_suite()};
    return arpg::test::run_suites(suites, 7, "stage 6 task 1");
}
