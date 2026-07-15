#include "test_framework.hpp"

arpg::test::TestSuite item_catalog_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {item_catalog_suite()};
    return arpg::test::run_suites(suites, 12, "stage 8 task 2");
}
