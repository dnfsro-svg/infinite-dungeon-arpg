#include "test_framework.hpp"

arpg::test::TestSuite abyss_rules_suite() noexcept;
arpg::test::TestSuite abyss_rewards_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        abyss_rules_suite(),
        abyss_rewards_suite(),
    };
    return arpg::test::run_suites(suites, 32, "stage 10 task 1 abyss");
}
