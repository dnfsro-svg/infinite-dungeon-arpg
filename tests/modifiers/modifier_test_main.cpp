#include "test_framework.hpp"

arpg::test::TestSuite modifier_math_suite() noexcept;
arpg::test::TestSuite effect_set_suite() noexcept;
arpg::test::TestSuite player_modifier_values_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        modifier_math_suite(), effect_set_suite(), player_modifier_values_suite()};
    return arpg::test::run_suites(suites, 16, "stage 7 task 1");
}
