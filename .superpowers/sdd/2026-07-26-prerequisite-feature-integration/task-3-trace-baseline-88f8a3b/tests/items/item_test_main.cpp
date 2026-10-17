#include "test_framework.hpp"

arpg::test::TestSuite item_catalog_suite() noexcept;
arpg::test::TestSuite item_generation_suite() noexcept;
arpg::test::TestSuite item_modifier_suite() noexcept;
arpg::test::TestSuite item_recipe_suite() noexcept;
arpg::test::TestSuite item_crafting_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        item_catalog_suite(), item_generation_suite(), item_modifier_suite(),
        item_recipe_suite(), item_crafting_suite()};
    return arpg::test::run_suites(suites, 46, "stage 16 reinforcement items");
}
