#include "test_framework.hpp"

arpg::test::TestSuite settings_types_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {settings_types_suite()};
    return arpg::test::run_suites(suites, 9, "stage 11b task 1 settings");
}
