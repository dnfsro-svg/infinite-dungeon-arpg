#include "test_framework.hpp"

arpg::test::TestSuite skill_loadout_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {skill_loadout_suite()};
    return arpg::test::run_suites(suites, 10, "stage 17 task 1");
}
