#include "test_framework.hpp"

arpg::test::TestSuite progression_rules_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        progression_rules_suite(),
    };
    return arpg::test::run_suites(suites, 6, "stage 5 task 1");
}
