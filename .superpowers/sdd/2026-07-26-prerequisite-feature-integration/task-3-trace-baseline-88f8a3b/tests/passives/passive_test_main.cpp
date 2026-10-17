#include "test_framework.hpp"

arpg::test::TestSuite passive_tree_catalog_suite() noexcept;
arpg::test::TestSuite passive_tree_rules_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        passive_tree_catalog_suite(), passive_tree_rules_suite()};
    return arpg::test::run_suites(suites, 12, "stage 8 task 3");
}
