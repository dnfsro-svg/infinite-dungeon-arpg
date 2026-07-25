#include "test_framework.hpp"

arpg::test::TestSuite launcher_core_suite() noexcept;
arpg::test::TestSuite launcher_layout_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        launcher_core_suite(),
        launcher_layout_suite(),
    };

    return arpg::test::run_suites(suites, 12, "desktop launcher core");
}
