#include "test_framework.hpp"

arpg::test::TestSuite item_catalog_suite() noexcept;
arpg::test::TestSuite item_generation_suite() noexcept;
arpg::test::TestSuite item_modifier_suite() noexcept;
arpg::test::TestSuite item_recipe_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        item_catalog_suite(), item_generation_suite(), item_modifier_suite(),
        item_recipe_suite()};
    return arpg::test::run_suites(suites, 32, "stage 8 task 9 items");
}
