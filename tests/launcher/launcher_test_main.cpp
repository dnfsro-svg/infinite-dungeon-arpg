#include "test_framework.hpp"

arpg::test::TestSuite launcher_core_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        launcher_core_suite(),
    };

    return arpg::test::run_suites(suites, 7, "desktop launcher core");
}
