#include "test_framework.hpp"

arpg::test::TestSuite attack_catalog_suite() noexcept;
arpg::test::TestSuite attack_state_suite() noexcept;
arpg::test::TestSuite break_stress_suite() noexcept;
arpg::test::TestSuite combat_config_suite() noexcept;
arpg::test::TestSuite dummy_reaction_suite() noexcept;
arpg::test::TestSuite input_buffer_suite() noexcept;
arpg::test::TestSuite movement_jump_suite() noexcept;
arpg::test::TestSuite hit_resolution_suite() noexcept;
arpg::test::TestSuite monster_catalog_suite() noexcept;
arpg::test::TestSuite monster_pool_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        attack_catalog_suite(),
        attack_state_suite(),
        break_stress_suite(),
        combat_config_suite(),
        dummy_reaction_suite(),
        input_buffer_suite(),
        movement_jump_suite(),
        hit_resolution_suite(),
        monster_catalog_suite(),
        monster_pool_suite(),
    };

    return arpg::test::run_suites(suites, 39, "stage 4 task 3");
}
